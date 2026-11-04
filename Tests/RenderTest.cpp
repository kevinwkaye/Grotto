// Offline engine test: no plugin host, no JUCE. Plays a short phrase through every
// scenario at three sample rates, checks for NaNs, levels above full scale and dead
// output, checks that Reflect really self-oscillates (and that the defaults don't), that
// Tame, Freeze, bypass, Dry Kill and Bass mode do their jobs, then measures CPU per
// instance at 48 kHz with 64 and 128 sample buffers (a house rule for every release).
//
// Pass a folder to also write a WAV of each scenario.

#include "GrottoEngine.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace gr;

namespace
{
struct Scene
{
    std::string name;
    EngineParams p;
    double seconds = 8.0;
    double playUntil = 4.0;   // input stops here
    double tameAt = -1.0;     // press Tame (A+B) here
    bool bassNote = false;    // play a low E on bass instead of the guitar phrase
    double startAt = 0.0;     // input starts here
    std::function<void (double t, EngineParams&)> script;
    std::function<bool (const std::vector<float>& wet, const std::vector<float>& out, double sr, std::string& why)> check;
};

// a plucked chord, retriggered every 1.25 s
float phrase (double t)
{
    static const double freqs[4] = { 110.0, 164.81, 220.0, 277.18 };
    const double local = std::fmod (t, 1.25);
    const double env = std::exp (-local * 2.6) * std::min (1.0, local * 400.0);
    double s = 0.0;
    for (int k = 0; k < 4; ++k)
    {
        const double ph = 2.0 * 3.14159265358979 * freqs[k] * t;
        s += std::sin (ph) + 0.35 * std::sin (2.0 * ph) + 0.15 * std::sin (3.0 * ph);
    }
    return (float) (0.12 * env * s);
}

// a bass's open E (41.2 Hz) with its harmonics, plucked every 2 s
float bassNote (double t)
{
    const double local = std::fmod (t, 2.0);
    const double env = std::exp (-local * 0.9) * std::min (1.0, local * 300.0);
    const double ph = 2.0 * 3.14159265358979 * 41.2 * t;
    return (float) (0.35 * env * (std::sin (ph) + 0.5 * std::sin (2.0 * ph) + 0.3 * std::sin (3.0 * ph) + 0.15 * std::sin (4.0 * ph)));
}

double rms (const std::vector<float>& x, double sr, double from, double to)
{
    const size_t a = (size_t) (from * sr), b = std::min (x.size(), (size_t) (to * sr));
    double s = 0.0;
    for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
    return b > a ? std::sqrt (s / (double) (b - a)) : 0.0;
}

// level of one frequency (Goertzel), as an amplitude
double tone (const std::vector<float>& x, double sr, double from, double to, double hz)
{
    const size_t a = (size_t) (from * sr), b = std::min (x.size(), (size_t) (to * sr));
    const double w = 2.0 * 3.14159265358979 * hz / sr, c = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = a; i < b; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
    return b > a ? 2.0 * std::sqrt (std::max (0.0, p)) / (double) (b - a) : 0.0;
}

void writeWav (const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const uint32_t frames = (uint32_t) l.size(), dataBytes = frames * 4;
    auto u32 = [f] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [f] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (1); u16 (2); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (dataBytes);
    for (uint32_t i = 0; i < frames; ++i)
    {
        const int16_t a = (int16_t) std::lround (clampf (l[i], -1.0f, 1.0f) * 32767.0f);
        const int16_t b = (int16_t) std::lround (clampf (r[i], -1.0f, 1.0f) * 32767.0f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

bool run (const Scene& s, double sr, const std::string& wavDir, std::string& why)
{
    auto eng = std::make_unique<GrottoEngine>();
    eng->prepare (sr);
    const int block = 128;
    const size_t total = (size_t) (s.seconds * sr);
    std::vector<float> outL (total), outR (total), wet (total);
    std::vector<float> bl (block), br (block), dry (block);
    EngineParams p = s.p;
    for (size_t pos = 0; pos < total; pos += block)
    {
        const int n = (int) std::min ((size_t) block, total - pos);
        const double t = (double) pos / sr;
        if (s.script) s.script (t, p);
        if (s.tameAt >= 0.0 && t <= s.tameAt && t + block / sr > s.tameAt) eng->tame();
        for (int i = 0; i < n; ++i)
        {
            const double ti = (double) (pos + (size_t) i) / sr;
            const float x = ti >= s.startAt && ti < s.playUntil ? (s.bassNote ? bassNote (ti) : phrase (ti)) : 0.0f;
            bl[(size_t) i] = br[(size_t) i] = dry[(size_t) i] = x;
        }
        eng->process (bl.data(), br.data(), bl.data(), br.data(), n, p);
        // what the plugin adds on top of the dry signal as it would be at this Mix
        const float mix = clampf (p.mix, 0.0f, 1.0f);
        const float dryMix = ! p.active ? 1.0f : (p.dryKill ? 0.0f : (mix <= 0.5f ? 1.0f : 1.0f - 0.684f * (mix - 0.5f) / 0.5f));
        for (int i = 0; i < n; ++i)
        {
            outL[pos + (size_t) i] = bl[(size_t) i];
            outR[pos + (size_t) i] = br[(size_t) i];
            wet[pos + (size_t) i] = 0.5f * (bl[(size_t) i] + br[(size_t) i]) - dry[(size_t) i] * dryMix;
        }
    }
    for (size_t i = 0; i < total; ++i)
    {
        if (! std::isfinite (outL[i]) || ! std::isfinite (outR[i])) { why = "NaN or infinity at sample " + std::to_string (i); return false; }
        if (std::abs (outL[i]) > 1.0001f || std::abs (outR[i]) > 1.0001f) { why = "level above full scale"; return false; }
    }
    if (! wavDir.empty() && sr == 48000.0) writeWav (wavDir + "/" + s.name + ".wav", outL, outR, (int) sr);
    std::vector<float> out (total);
    for (size_t i = 0; i < total; ++i) out[i] = 0.5f * (outL[i] + outR[i]);
    return s.check ? s.check (wet, out, sr, why) : true;
}

double cpuPercent (int block)
{
    const double sr = 48000.0;
    GrottoEngine eng;
    eng.prepare (sr);
    EngineParams p;
    p.drag = 0.3f; p.reflect = 0.7f; p.length = 0.8f;   // long lines and the grit clock: the heaviest case
    const int seconds = 20;
    std::vector<float> l ((size_t) block), r ((size_t) block);
    const auto start = std::chrono::steady_clock::now();
    for (long pos = 0; pos < (long) (seconds * sr); pos += block)
    {
        for (int i = 0; i < block; ++i) l[(size_t) i] = r[(size_t) i] = phrase ((double) (pos + i) / sr);
        eng.process (l.data(), r.data(), l.data(), r.data(), block, p);
    }
    const double used = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    return 100.0 * used / (double) seconds;
}

using Check = std::function<bool (const std::vector<float>&, const std::vector<float>&, double, std::string&)>;

Check hasWet (double minRms)
{
    return [minRms] (const std::vector<float>& w, const std::vector<float>&, double sr, std::string& why)
    {
        const double r = rms (w, sr, 0.5, 4.0);
        if (r < minRms) { why = "wet signal too quiet: " + std::to_string (r); return false; }
        return true;
    };
}

// the tail has died away by a time after the playing stops: it isn't singing on its own
Check diesBy (double from, double to, double maxRms)
{
    return [=] (const std::vector<float>& w, const std::vector<float>&, double sr, std::string& why)
    {
        const double during = rms (w, sr, 0.5, 4.0), after = rms (w, sr, from, to);
        if (during < 0.01) { why = "wet signal too quiet: " + std::to_string (during); return false; }
        if (after > maxRms) { why = "tail still sounding: " + std::to_string (after); return false; }
        return true;
    };
}
} // namespace

int main (int argc, char** argv)
{
    const std::string wavDir = argc > 1 ? argv[1] : "";
    std::vector<Scene> scenes;

    // ---- the defaults, and each knob ----
    { Scene s; s.name = "01_defaults"; s.seconds = 14.0; s.check = diesBy (12.0, 14.0, 0.002); scenes.push_back (s); }
    { Scene s; s.name = "02_length_short"; s.p.length = 0.0f; s.check = diesBy (5.5, 8.0, 0.001); scenes.push_back (s); }
    { Scene s; s.name = "03_length_long"; s.p.length = 1.0f; s.p.reflect = 0.0f; s.seconds = 10.0;
      s.check = [] (const std::vector<float>& w, const std::vector<float>&, double sr, std::string& why)
      { const double r = rms (w, sr, 7.0, 9.0); if (r < 0.003) { why = "long Length died too soon: " + std::to_string (r); return false; } return true; };
      scenes.push_back (s); }
    { Scene s; s.name = "04_diffuse_sharp"; s.p.diffuse = 0.0f; s.check = hasWet (0.01); scenes.push_back (s); }
    { Scene s; s.name = "05_diffuse_wash"; s.p.diffuse = 1.0f; s.check = hasWet (0.01); scenes.push_back (s); }
    { Scene s; s.name = "06_dampen_dark"; s.p.dampen = 1.0f; s.check = hasWet (0.003); scenes.push_back (s); }
    { Scene s; s.name = "07_dampen_bright"; s.p.dampen = 0.0f; s.check = hasWet (0.01); scenes.push_back (s); }
    { Scene s; s.name = "08_drag_tight"; s.p.drag = 1.0f; s.check = hasWet (0.01); scenes.push_back (s); }
    { Scene s; s.name = "09_drag_scattered"; s.p.drag = 0.0f; s.check = hasWet (0.005); scenes.push_back (s); }
    { Scene s; s.name = "10_mix_zero_is_dry"; s.p.mix = 0.0f;
      s.check = [] (const std::vector<float>& w, const std::vector<float>&, double sr, std::string& why)
      { const double r = rms (w, sr, 0.5, 8.0); if (r > 0.001) { why = "Mix at zero still wet: " + std::to_string (r); return false; } return true; };
      scenes.push_back (s); }
    { Scene s; s.name = "11_mix_full"; s.p.mix = 1.0f; s.check = hasWet (0.02); scenes.push_back (s); }

    // ---- Drag's pitch warp and the modes ----
    {
        Scene s; s.name = "12_drag_sweep_warp"; s.p.reflect = 0.7f; s.p.length = 0.7f;
        s.script = [] (double t, EngineParams& p) { p.drag = (float) (0.55 + 0.4 * std::sin (t * 1.7)); };
        s.check = hasWet (0.01);
        scenes.push_back (s);
    }
    for (int m = 0; m < NumModes; ++m)
    {
        Scene s; s.name = std::string ("13_mode_") + std::to_string (m) + "_sweep"; s.p.mode = m; s.p.reflect = 0.6f;
        s.script = [] (double t, EngineParams& p) { p.drag = (float) (0.5 + 0.45 * std::sin (t * 1.3)); };
        s.check = hasWet (0.005);
        scenes.push_back (s);
    }

    // ---- Reflect: singing, the runaway, Tame ----
    {
        // Reflect up high with Length at noon: it keeps sounding long after you stop
        Scene s; s.name = "14_reflect_sings"; s.seconds = 10.0; s.playUntil = 1.5; s.p.reflect = 0.95f; s.p.length = 0.5f;
        s.check = [] (const std::vector<float>& w, const std::vector<float>&, double sr, std::string& why)
        { const double r = rms (w, sr, 8.0, 10.0); if (r < 0.05) { why = "Reflect didn't self-oscillate: " + std::to_string (r); return false; } return true; };
        scenes.push_back (s);
    }
    {
        // Reflect just under the edge: long, but it still dies away
        Scene s; s.name = "15_reflect_below_edge"; s.seconds = 14.0; s.playUntil = 1.5; s.p.reflect = 0.7f; s.p.length = 0.5f;
        s.check = [] (const std::vector<float>& w, const std::vector<float>&, double sr, std::string& why)
        { const double r = rms (w, sr, 12.0, 14.0); if (r > 0.003) { why = "sang below the edge: " + std::to_string (r); return false; } return true; };
        scenes.push_back (s);
    }
    {
        // hold B: it takes off on its own once the playing stops, and settles on release
        Scene s; s.name = "16_runaway_oscillates"; s.seconds = 14.0; s.playUntil = 1.5;
        s.script = [] (double t, EngineParams& p) { p.runaway = t > 1.0 && t < 7.0; };
        s.check = [] (const std::vector<float>& w, const std::vector<float>&, double sr, std::string& why)
        {
            const double during = rms (w, sr, 5.5, 6.9), after = rms (w, sr, 13.0, 14.0);
            if (during < 0.05) { why = "runaway didn't self-oscillate: " + std::to_string (during); return false; }
            if (after > during * 0.1) { why = "runaway didn't settle after release: " + std::to_string (after); return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        Scene s; s.name = "17_runaway_scattered_bend"; s.seconds = 10.0; s.playUntil = 1.5; s.p.drag = 0.4f;
        s.script = [] (double t, EngineParams& p) { p.runaway = t > 1.0 && t < 8.0; p.drag = (float) (0.6 + 0.3 * std::sin (t)); };
        s.check = [] (const std::vector<float>& w, const std::vector<float>&, double sr, std::string& why)
        { const double r = rms (w, sr, 5.0, 7.5); if (r < 0.02) { why = "runaway with Drag moving went quiet: " + std::to_string (r); return false; } return true; };
        scenes.push_back (s);
    }
    {
        Scene s; s.name = "18_tame_stops_it"; s.seconds = 8.0; s.playUntil = 1.5; s.p.reflect = 1.0f;
        s.script = [] (double t, EngineParams& p) { p.runaway = t > 1.0 && t < 6.0; };
        s.tameAt = 4.0;   // Tame while B is still held: it must still go quiet, then may build again
        s.check = [] (const std::vector<float>& w, const std::vector<float>&, double sr, std::string& why)
        {
            const double before = rms (w, sr, 3.5, 3.95), after = rms (w, sr, 4.05, 4.25);
            if (before < 0.05) { why = "nothing to tame: " + std::to_string (before); return false; }
            if (after > before * 0.1) { why = "Tame didn't cut the feedback: " + std::to_string (after); return false; }
            return true;
        };
        scenes.push_back (s);
    }

    // ---- Freeze, bypass, Dry Kill ----
    {
        Scene s; s.name = "19_freeze_holds"; s.seconds = 10.0; s.playUntil = 3.0;
        s.script = [] (double t, EngineParams& p) { p.freeze = t > 2.5; };
        s.check = [] (const std::vector<float>& w, const std::vector<float>&, double sr, std::string& why)
        {
            const double a = rms (w, sr, 5.0, 6.0), b = rms (w, sr, 9.0, 10.0);
            if (b < 0.005) { why = "freeze didn't hold the sound: " + std::to_string (b); return false; }
            if (b > a * 1.5) { why = "freeze grew"; return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        Scene s; s.name = "20_bypass_cuts"; s.p.trails = false;
        s.script = [] (double t, EngineParams& p) { p.active = t < 3.0; };
        s.check = [] (const std::vector<float>& w, const std::vector<float>&, double sr, std::string& why)
        { const double r = rms (w, sr, 3.2, 8.0); if (r > 0.001) { why = "wet still sounding after bypass: " + std::to_string (r); return false; } return true; };
        scenes.push_back (s);
    }
    {
        Scene s; s.name = "21_bypass_trails"; s.seconds = 14.0;
        s.script = [] (double t, EngineParams& p) { p.active = t < 3.0; };
        s.check = [] (const std::vector<float>& w, const std::vector<float>&, double sr, std::string& why)
        {
            const double ringing = rms (w, sr, 3.2, 4.0), later = rms (w, sr, 12.0, 14.0);
            if (ringing < 0.005) { why = "no trails after bypass: " + std::to_string (ringing); return false; }
            if (later > 0.002) { why = "bypassed trails didn't die away: " + std::to_string (later); return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        // Dry Kill: none of the dry note left at its own attack
        Scene s; s.name = "22_dry_kill"; s.p.dryKill = true; s.p.mix = 0.45f; s.p.drag = 0.3f; s.startAt = 0.2;
        s.check = [] (const std::vector<float>&, const std::vector<float>& out, double sr, std::string& why)
        {
            const double first = rms (out, sr, 0.2, 0.22);   // the first note's attack, before its first echo
            if (first > 0.003) { why = "dry still heard: " + std::to_string (first); return false; }
            return true;
        };
        scenes.push_back (s);
    }

    // ---- bass ----
    {
        // the dry bass passes untouched, and the reverb adds next to nothing at 41 Hz
        Scene s; s.name = "23_bass_low_end_clean"; s.bassNote = true; s.p.length = 0.85f; s.p.reflect = 0.6f;
        s.check = [] (const std::vector<float>& w, const std::vector<float>& out, double sr, std::string& why)
        {
            const double dryE = tone (out, sr, 0.5, 4.0, 41.2), wetE = tone (w, sr, 0.5, 4.0, 41.2), wetMid = tone (w, sr, 0.5, 4.0, 164.8);
            if (dryE < 0.08) { why = "the low E lost level: " + std::to_string (dryE); return false; }
            if (wetE > dryE * 0.08) { why = "reverb muddies the low E: " + std::to_string (wetE / dryE); return false; }
            if (wetMid < wetE) { why = "reverb has more lows than mids"; return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        // a runaway on bass must not fill up with sub
        Scene s; s.name = "24_bass_runaway_no_sub"; s.bassNote = true; s.seconds = 8.0; s.playUntil = 3.0; s.p.bass = true; s.p.reflect = 1.0f; s.p.length = 0.9f;
        s.check = [] (const std::vector<float>& w, const std::vector<float>&, double sr, std::string& why)
        {
            const double sub = tone (w, sr, 5.0, 8.0, 41.2), all = rms (w, sr, 5.0, 8.0);
            if (all < 0.02) { why = "nothing ringing: " + std::to_string (all); return false; }
            if (sub > all * 0.1) { why = "runaway full of sub: " + std::to_string (sub / all); return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        // Bass mode with Dry Kill: the fundamental stays under the wet wash
        Scene s; s.name = "25_bass_dry_kill_keeps_lows"; s.bassNote = true; s.p.bass = true; s.p.dryKill = true;
        s.check = [] (const std::vector<float>&, const std::vector<float>& out, double sr, std::string& why)
        {
            const double low = tone (out, sr, 0.5, 4.0, 41.2);
            if (low < 0.1) { why = "Dry Kill took the fundamental: " + std::to_string (low); return false; }
            return true;
        };
        scenes.push_back (s);
    }
    { Scene s; s.name = "26_mono"; s.p.mono = true; s.p.drag = 0.2f; s.check = hasWet (0.005); scenes.push_back (s); }

    int failed = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (const auto& s : scenes)
        {
            std::string why;
            const bool ok = run (s, sr, wavDir, why);
            if (! ok) ++failed;
            if (! ok || sr == 48000.0)
                std::printf ("%-30s %6.0f Hz  %s %s\n", s.name.c_str(), sr, ok ? "ok" : "FAILED", why.c_str());
        }

    std::printf ("\nCPU per instance at 48 kHz (one core, scattered Drag with the grit clock, Reflect high):\n");
    for (int block : { 64, 128 })
        std::printf ("  %3d samples: %.2f%%\n", block, cpuPercent (block));

    std::printf ("\n%s: %d failed\n", failed ? "FAILED" : "PASSED", failed);
    return failed ? 1 : 0;
}
