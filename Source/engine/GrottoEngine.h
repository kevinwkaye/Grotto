#pragma once

// Grotto's engine: a cavernous reverb made of a swarm of short delays, recreating how the
// EarthQuaker Devices Afterneath V3 behaves. Original code, written from public
// descriptions of the pedal. Plain C++17 with no JUCE, so it builds anywhere
// (Windows x64, Linux ARM64, macOS) and can move to a headless host.

#include "Swarm.h"
#include <atomic>

namespace gr
{
// V3's nine Drag modes
enum DragMode { Smooth = 0, Glide, Even, Chromatic, Major, Minor, Pentatonic, OctFifth, Octaves, NumModes };

constexpr float kMaxStretch = 12.0f;                 // Drag full anticlockwise: every line 12 times longer
constexpr float kStretchSemis = 43.02f;              // 12 times longer is about 43 semitones of clock
constexpr float kLowCutMin = 20.0f, kLowCutMax = 300.0f;
constexpr float kBassLowCut = 120.0f;                // Bass mode: the reverb never goes below this
constexpr float kGritFrom = 6.5f;                    // the stretch at noon on the knob: grit starts below it

struct EngineParams
{
    float length = 0.55f;     // 0..1: decay, about 0.4 s to 15 s
    float diffuse = 0.55f;    // 0 sharp taps, 1 washy
    float dampen = 0.4f;      // 0 bright, 1 dark (clockwise darker)
    float drag = 0.75f;       // 0 scattered and long, 1 a tight reverb
    float reflect = 0.4f;     // regeneration; self-oscillates up high
    float mix = 0.45f;        // never fully wet on its own
    float lowCutHz = 90.0f;   // high-pass on the reverb only
    int mode = Smooth;        // how Drag responds
    bool active = true;       // false when bypassed
    bool trails = true;       // the cave rings out when bypassed
    bool freeze = false;
    bool runaway = false;     // footswitch B: Reflect climbs into self-oscillation
    bool dryKill = false;     // footswitch D: wet only
    bool bass = false;        // reverb low cut at least 120 Hz; Dry Kill keeps the dry lows
    bool mono = false;        // pedal-exact: mono in, mono out
};

// Length to decay time in seconds
inline float lengthSeconds (float length) { return 0.4f * std::pow (15.0f / 0.4f, clampf (length, 0.0f, 1.0f)); }

// Dampen to the extra decay of the highest frequencies, in dB per second: clockwise darker.
// Each pass round the cave loses a little more top, like sound in a real cave.
inline float dampenDbPerSec (float dampen) { const float d = clampf (dampen, 0.0f, 1.0f); return 3.0f + 260.0f * d * d; }

// How much Reflect pushes against the decay, in dB per second. Where it matches the
// decay the swarm stops dying away and sings on its own. With Length at noon that is
// about 80% of the knob; with Length up it comes sooner, as on the pedal.
inline float reflectDbPerSec (float reflect) { return 38.0f * reflect * reflect; }

// Semitones of stretch below a tight reverb that each stepped mode allows
inline const std::vector<int>& modeDegrees (int mode)
{
    static const std::vector<int> none {}, chrom { 0,1,2,3,4,5,6,7,8,9,10,11 }, major { 0,2,4,5,7,9,11 },
        minor { 0,2,3,5,7,8,10 }, penta { 0,3,5,7,10 }, fifth { 0,7 }, oct { 0 };
    switch (mode)
    {
        case Chromatic: return chrom;
        case Major: return major;
        case Minor: return minor;
        case Pentatonic: return penta;
        case OctFifth: return fifth;
        case Octaves: return oct;
        default: return none;
    }
}

// Drag knob (0..1) to the stretch of the swarm, through the mode.
// Smooth and Glide: the knob moves the delay time evenly, as the original pedal's knob.
// Even: equal travel gives equal pitch change. The stepped modes snap that to intervals.
inline float dragToStretch (float drag, int mode)
{
    drag = clampf (drag, 0.0f, 1.0f);
    if (mode == Smooth || mode == Glide) return 1.0f + (kMaxStretch - 1.0f) * (1.0f - drag);
    float semis = kStretchSemis * (1.0f - drag);
    const auto& deg = modeDegrees (mode);
    if (! deg.empty())
    {
        float best = 0.0f, bestDist = 1.0e9f;
        for (int oct = 0; oct <= 4; ++oct)
            for (int d : deg)
            {
                const float c = 12.0f * (float) oct + (float) d;
                if (c > kStretchSemis + 0.01f) continue;
                if (std::abs (c - semis) < bestDist) { bestDist = std::abs (c - semis); best = c; }
            }
        semis = best;
    }
    return std::exp2 (semis / 12.0f);
}

// The other way: the Drag knob that gives a stretch in a mode (for the expression pedal,
// so it sweeps evenly in pitch whatever the mode)
inline float stretchToDrag (float stretch, int mode)
{
    stretch = clampf (stretch, 1.0f, kMaxStretch);
    if (mode == Smooth || mode == Glide) return 1.0f - (stretch - 1.0f) / (kMaxStretch - 1.0f);
    return 1.0f - 12.0f * std::log2 (stretch) / kStretchSemis;
}

inline const char* modeName (int m)
{
    static const char* const n[NumModes] = { "SMOOTH", "GLIDE", "EVEN", "CHROMATIC", "MAJOR", "MINOR", "PENTATONIC", "8VE + 5TH", "OCTAVES" };
    return n[m < 0 || m >= NumModes ? 0 : m];
}

class GrottoEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        swarm.prepare (sr, kMaxStretch + 0.5f);
        stretchSm.reset (dragToStretch (0.75f, Smooth));
        inGainSm.setTime (0.01f, sr);   inGainSm.reset (1.0f);
        wetGainSm.setTime (0.02f, sr);  wetGainSm.reset (1.0f);
        dryGainSm.setTime (0.02f, sr);  dryGainSm.reset (1.0f);
        dryLowSm.setTime (0.02f, sr);   dryLowSm.reset (0.0f);
        monoSm.setTime (0.03f, sr);     monoSm.reset (0.0f);
        env.setTime (0.08f, sr);
        inEnv.setTime (0.03f, sr);
        fastEnv.setTime (0.004f, sr);
        for (auto& f : inCut) f.reset();
        for (auto& f : inCut2) f.reset();
        for (auto& f : dryLow) f.reset();
        for (auto& d : dc) d.reset();
        reflectDb = 0.0f;
        cleared = true;
        lastLowCut = -1.0f;
    }

    // Clears the feedback and the tail at once: footswitches A+B
    void tame()
    {
        tamePending.store (true);
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const EngineParams& p)
    {
        if (tamePending.exchange (false)) { swarm.clear(); reflectDb = 0.0f; runawayAmount = 0.0f; }

        // ---- per block ----
        const float blockSec = (float) n / (float) sr;
        const float decay = 60.0f / lengthSeconds (p.length);

        // Reflect, and the runaway: B held pushes Reflect past the point where the swarm
        // sings, over about a second and a half, then lets it fall back on release.
        const float knobDb = reflectDbPerSec (p.reflect);
        const float runDb = std::max (knobDb, decay + 9.0f);
        const float target = p.runaway ? runDb : knobDb;
        const float rate = p.runaway ? (runDb - knobDb) / 1.5f + 1.0f : 30.0f;
        if (reflectDb < target) reflectDb = std::min (target, reflectDb + rate * blockSec);
        else reflectDb = std::max (target, reflectDb - 60.0f * blockSec);
        runawayAmount = clampf ((reflectDb - knobDb) / std::max (1.0f, runDb - knobDb), 0.0f, 1.0f);

        // Drag through the mode, with each mode's own glide
        const float stretchTarget = dragToStretch (p.drag, p.mode);
        const float glide = p.mode == Glide ? 0.35f : (modeDegrees (p.mode).empty() ? 0.07f : 0.025f);
        stretchSm.setTime (glide, sr);

        const float lowCut = std::max (clampf (p.lowCutHz, kLowCutMin, kLowCutMax), p.bass ? kBassLowCut : 0.0f);
        if (std::abs (lowCut - lastLowCut) > 0.5f)
        {
            lastLowCut = lowCut;
            // 24 dB per octave (Butterworth), so a bass's fundamental stays out of the cave
            for (int c = 0; c < 2; ++c) { inCut[(size_t) c].setHighPass (lowCut, 0.5412f, sr); inCut2[(size_t) c].setHighPass (lowCut, 1.3066f, sr); }
            for (auto& f : dryLow) f.setLowPass (kBassLowCut, 0.707f, sr);
        }

        const float stretchNow = stretchSm.y;
        Swarm::Settings s;
        s.decayDbPerSec = std::max (-12.0f, decay - reflectDb - kSwarmLoss);
        s.diffuse = p.diffuse;
        s.dampDbPerSec = dampenDbPerSec (p.dampen);
        s.lowCutHz = lowCut;
        s.lowCutDbPerSec = 90.0f;
        const float drift = std::log2 (stretchNow) / std::log2 (kMaxStretch);   // 0 tight, 1 scattered
        s.scatter = 0.9f * std::pow (drift, 1.5f);
        s.hold = 1.0f + 5.0f * clampf ((stretchNow - kGritFrom) / (kMaxStretch - kGritFrom), 0.0f, 1.0f);   // grit below noon
        s.freeze = p.freeze;
        swarm.setBlock (s, stretchNow);

        // levels: the dry stays at unity to noon then dips, so it never goes fully wet on its own
        const float mix = clampf (p.mix, 0.0f, 1.0f);
        const float dryMix = mix <= 0.5f ? 1.0f : 1.0f - 0.684f * (mix - 0.5f) / 0.5f;
        const float wetMix = std::sin (mix * kPi * 0.5f);
        const bool feedSwarm = p.active && ! p.freeze;
        const float inTarget = feedSwarm ? 1.0f : 0.0f;
        const float wetTarget = (p.active || p.trails) ? wetMix * kWetLevel : 0.0f;
        const float dryTarget = ! p.active ? 1.0f : (p.dryKill ? 0.0f : dryMix);
        const float dryLowTarget = p.active && p.dryKill && p.bass ? 1.0f : 0.0f;   // Bass: keep the fundamental

        if (p.active && cleared) cleared = false;
        const float stretchStart = stretchSm.y;

        float bendSum = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float l = inL[i], r = inR[i];
            const float monoAmt = monoSm.next (p.mono ? 1.0f : 0.0f);
            if (monoAmt > 0.0f) { const float m = 0.5f * (l + r); l += monoAmt * (m - l); r += monoAmt * (m - r); }

            const float stretch = stretchSm.next (stretchTarget);
            const float ig = inGainSm.next (inTarget);
            const float sl = inCut2[0].process (inCut[0].process (l)) * ig, sr_ = inCut2[1].process (inCut[1].process (r)) * ig;
            float wl, wr;
            swarm.process (sl, sr_, stretch, wl, wr);
            wl = safetyLimit (dc[0].process (wl));
            wr = safetyLimit (dc[1].process (wr));
            if (monoAmt > 0.0f) { const float m = 0.5f * (wl + wr); wl += monoAmt * (m - wl); wr += monoAmt * (m - wr); }

            const float wg = wetGainSm.next (wetTarget), dg = dryGainSm.next (dryTarget), lg = dryLowSm.next (dryLowTarget);
            const float lowL = dryLow[0].process (l), lowR = dryLow[1].process (r);
            outL[i] = safetyLimit (l * dg + lowL * lg + wl * wg);
            outR[i] = safetyLimit (r * dg + lowR * lg + wr * wg);

            const float w = 0.5f * (std::abs (wl) + std::abs (wr)) * wg;
            env.next (w);
            const float x = 0.5f * (std::abs (l) + std::abs (r));
            inEnv.next (x);
            fastEnv.next (x);
        }
        bendSum = 12.0f * std::log2 (stretchSm.y / std::max (1.0e-3f, stretchStart));

        // bypassed with no trails: once quiet, empty the cave so it starts fresh
        if (! p.active && ! p.trails && wetGainSm.y < 1.0e-4f && ! cleared) { swarm.clear(); cleared = true; }

        // ---- for the panel and Pebble ----
        uiStretch.store (stretchSm.y);
        uiReflect.store (reflectDb / std::max (1.0f, decay));   // 1 is where it starts singing
        uiRunaway.store (p.runaway ? std::max (0.05f, runawayAmount) : runawayAmount);
        uiFrozen.store (p.freeze);
        uiWet.store (env.y);
        uiInput.store (inEnv.y);
        // pitch bend: the tail goes down as Drag stretches, up as it tightens (semitones per second)
        const float bendRate = -bendSum / std::max (1.0e-4f, blockSec);
        uiBend.store (uiBend.load() + 0.2f * (bendRate - uiBend.load()));
        if (fastEnv.y > inEnv.y * 1.6f + 0.01f && ! onsetArmed) { onsetArmed = true; uiOnsets.fetch_add (1); }
        if (fastEnv.y < inEnv.y * 1.1f) onsetArmed = false;
    }

    // ---- what the panel and Pebble read (written once per block) ----
    std::atomic<float> uiStretch { 1.0f }, uiReflect { 0.0f }, uiRunaway { 0.0f }, uiWet { 0.0f }, uiInput { 0.0f }, uiBend { 0.0f };
    std::atomic<bool> uiFrozen { false };
    std::atomic<int> uiOnsets { 0 };

private:
    static constexpr float kWetLevel = 1.4f;
    static constexpr float kSwarmLoss = 2.0f;   // dB per second the lines lose on their own (interpolation, drift)

    double sr = 48000.0;
    Swarm swarm;
    Smoother stretchSm, inGainSm, wetGainSm, dryGainSm, dryLowSm, monoSm, env, inEnv, fastEnv;
    std::array<Biquad, 2> inCut, inCut2, dryLow;
    std::array<DcBlock, 2> dc;
    float reflectDb = 0.0f, runawayAmount = 0.0f, lastLowCut = -1.0f;
    bool cleared = true, onsetArmed = false;
    std::atomic<bool> tamePending { false };
};
} // namespace gr
