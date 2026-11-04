#pragma once

// The swarm: a reverb made of short delay lines, written for Grotto and kept free of
// anything Grotto-specific so other Flip Fella plugins (Vesper first) can copy it.
//
// 16 delay lines, 8 per side, with unrelated lengths of about 3 to 45 ms. Each pass round
// a line goes through a damping low-pass and a low shelf, then a lossless feedback mix: a
// Householder reflection, rotations between neighbouring lines that run from none (distinct,
// metallic taps) to strong (a smooth wash), and a rotation between the sides (ping-pong). Every line write is
// soft clipped, so a loop that grows saturates into a steady roar instead of exploding.
//
// The stretch (Grotto's Drag) scales every line's length together. Changing it moves the
// read points through sound already in the lines, which bends its pitch like a tape delay.
// The hold factor puts what goes into and comes out of the swarm on a slower
// sample-and-hold clock with no filtering, so it aliases and gets gritty, as a reverb
// running on a slower clock would. It sits outside the loop so the tail keeps its length.
//
// Plain C++17, no JUCE.

#include "DspUtil.h"
#include <array>

namespace gr
{
class Swarm
{
public:
    static constexpr int N = 16, Half = 8;

    struct Settings
    {
        float decayDbPerSec = 20.0f;  // positive dies away (60 / RT60), negative grows
        float diffuse = 0.5f;         // 0 distinct taps, 1 smooth wash
        float dampDbPerSec = 40.0f;   // extra decay at the very top of the range: higher is darker
        float lowCutHz = 60.0f;       // below this, the tail dies away fast
        float lowCutDbPerSec = 90.0f; // how fast it dies away down there
        float scatter = 0.0f;         // 0 sides kept apart, 1 every pass swaps sides
        float hold = 1.0f;            // >= 1: sample-and-hold factor on the way in and out
        bool freeze = false;          // lossless: the lines loop as they are
    };

    void prepare (double sampleRate, float maxStretch)
    {
        sr = sampleRate;
        const int maxLen = (int) (kBaseMs[N - 1] * 0.001 * maxStretch * sr + kWanderMs * 0.001 * sr * 2.0) + 64;
        for (int i = 0; i < N; ++i)
        {
            lines[(size_t) i].prepare (maxLen);
            wander[(size_t) i].prepare (sr, 0.17f + 0.031f * (float) i, 0x51ed270bu + 7919u * (uint32_t) i);
        }
        for (int s = 0; s < 2; ++s)
            for (int k = 0; k < kAllpasses; ++k)
                ap[(size_t) s][(size_t) k].line.prepare ((int) (kApMs[k] * 0.001 * maxStretch * sr) + 64);
        clear();
        setBlock (Settings {}, 1.0f);
    }

    void clear()
    {
        for (auto& l : lines) l.clear();
        damp.fill (0.0f);
        for (auto& f : cut) f.reset();
        for (auto& side : ap) for (auto& a : side) a.line.clear();
        heldIn.fill (0.0f);
        heldOut.fill (0.0f);
        holdPhase = 1.0f;
    }

    // Once per block: everything that depends on the settings and the stretch.
    void setBlock (const Settings& s, float stretch)
    {
        set = s;
        // The loop filters are set per second, not per pass, so a 3 ms line and a 45 ms line
        // darken and lose their lows at the same rate (the usual way to damp a delay-network reverb).
        for (int i = 0; i < N; ++i)
        {
            const float seconds = kBaseMs[i] * 0.001f * stretch;
            gain[(size_t) i] = s.freeze ? 1.0f : std::pow (10.0f, -s.decayDbPerSec * seconds / 20.0f);
            // one-pole low-pass with unity at DC and the extra loss at Nyquist: b = (1 - r) / (1 + r)
            const float r = std::pow (10.0f, -std::max (0.0f, s.dampDbPerSec) * seconds / 20.0f);
            dampB[(size_t) i] = (1.0f - r) / (1.0f + r);
            // low shelf: everything below the cut loses lowCutDbPerSec more
            cut[(size_t) i].setCutoff (s.lowCutHz, sr);
            cutK[(size_t) i] = 1.0f - std::pow (10.0f, -std::max (0.0f, s.lowCutDbPerSec) * seconds / 20.0f);
        }
        const float d = clampf (s.diffuse, 0.0f, 1.0f);
        mixC = std::cos (d * kPi * 0.25f);
        mixS = std::sin (d * kPi * 0.25f);
        swap = s.freeze ? 0.0f : clampf (s.scatter, 0.0f, 1.0f);
        swapC = std::cos (swap * kPi * 0.5f);
        swapS = std::sin (swap * kPi * 0.5f);
        apGain = 0.72f * clampf (s.diffuse, 0.0f, 1.0f);
        holdStep = 1.0f / std::max (1.0f, s.hold);
    }

    // One stereo sample. stretch is the current (smoothed) length multiplier, >= 1.
    void process (float inL, float inR, float stretch, float& outL, float& outR)
    {
        const float msToSamples = 0.001f * (float) sr;

        // input diffusion: allpasses that grow with the stretch, smearing the attack
        const float apStretch = std::sqrt (stretch);
        float x[2] = { inL, inR };
        for (int s = 0; s < 2; ++s)
            for (int k = 0; k < kAllpasses; ++k)
            {
                auto& a = ap[(size_t) s][(size_t) k];
                const float d = a.line.read (kApMs[k] * apStretch * msToSamples * (s == 0 ? 1.0f : 1.07f));
                const float w = x[s] + apGain * d;
                a.line.write (w);
                x[s] = d - apGain * w;
            }

        // the slow clock, on the way in
        holdPhase += holdStep;
        const bool fresh = holdPhase >= 1.0f;
        if (fresh) { holdPhase -= 1.0f; heldIn[0] = x[0]; heldIn[1] = x[1]; }
        x[0] = heldIn[0]; x[1] = heldIn[1];

        // read every line
        std::array<float, N> o {};
        float sum = 0.0f, oL = 0.0f, oR = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const float len = kBaseMs[i] * stretch * msToSamples + (1.0f + wander[(size_t) i].next()) * kWanderMs * msToSamples;
            float v = lines[(size_t) i].read (len);
            if (! set.freeze)
            {
                float& z = damp[(size_t) i];
                z = v + dampB[(size_t) i] * (z - v);   // (1 - b) v + b z: unity at DC, r at Nyquist
                v = z;
                v -= cutK[(size_t) i] * cut[(size_t) i].process (v);
            }
            o[(size_t) i] = v;
            sum += v;
            const float signed_ = (i & 1) ? -v : v;
            if (i < Half) oL += signed_; else oR += signed_;
        }
        if (fresh) { heldOut[0] = oL * kOut; heldOut[1] = oR * kOut; }
        outL = heldOut[0];
        outR = heldOut[1];

        // the feedback mix, every step lossless: Householder, neighbour rotations, then the side swap
        const float h = 2.0f / (float) N * sum;
        for (int i = 0; i < N; ++i) o[(size_t) i] -= h;
        if (mixS > 0.0f)
            for (int i = 0; i < N; i += 2)
            {
                const float a = o[(size_t) i], b = o[(size_t) (i + 1)];
                o[(size_t) i] = mixC * a + mixS * b;
                o[(size_t) (i + 1)] = mixC * b - mixS * a;
            }
        if (swap > 0.0f)   // a rotation between each left line and its right partner: lossless at any amount
            for (int i = 0; i < Half; ++i)
            {
                const float a = o[(size_t) i], b = o[(size_t) (i + Half)];
                o[(size_t) i] = swapC * a + swapS * b;
                o[(size_t) (i + Half)] = swapC * b - swapS * a;
            }

        // write
        for (int i = 0; i < N; ++i)
        {
            const float in = (i < Half ? x[0] : x[1]) * ((i & 2) ? -kIn : kIn);
            lines[(size_t) i].write (softClip (gain[(size_t) i] * o[(size_t) i] + in));
        }
    }

    // average length of the lines at a stretch, for the panel
    static float meanMs (float stretch)
    {
        float s = 0.0f;
        for (float ms : kBaseMs) s += ms;
        return s / (float) N * stretch;
    }
    static float longestMs (float stretch) { return kBaseMs[N - 1] * stretch; }

private:
    // unrelated lengths, left lines then right lines, sorted so the last is the longest
    static constexpr float kBaseMs[N] = { 3.1f, 5.3f, 7.9f, 11.3f, 15.7f, 21.1f, 29.3f, 41.9f,
                                          3.7f, 6.1f, 8.9f, 12.7f, 17.3f, 23.9f, 32.9f, 44.3f };
    static constexpr int kAllpasses = 4;
    static constexpr float kApMs[kAllpasses] = { 1.3f, 2.3f, 3.7f, 5.9f };
    static constexpr float kWanderMs = 0.18f;   // a little drift, so the swarm isn't a perfect comb
    static constexpr float kIn = 0.5f, kOut = 0.32f;

    struct Allpass { DelayLine line; };

    double sr = 48000.0;
    Settings set;
    std::array<DelayLine, N> lines;
    std::array<Wander, N> wander;
    std::array<float, N> damp {};
    std::array<OnePoleLP, N> cut;
    std::array<float, N> gain {}, dampB {}, cutK {};
    std::array<float, 2> heldIn {}, heldOut {};
    std::array<std::array<Allpass, kAllpasses>, 2> ap;
    float mixC = 1.0f, mixS = 0.0f, swap = 0.0f, swapC = 1.0f, swapS = 0.0f, apGain = 0.3f, holdStep = 1.0f, holdPhase = 1.0f;
};
} // namespace gr
