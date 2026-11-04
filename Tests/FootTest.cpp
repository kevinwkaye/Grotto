// Footswitch and expression test: the real Grotto processor, driven only by plain MIDI
// press (127) and release (0) messages on the shipped defaults, as a footswitch sends them.
// Checks tap, hold, A+B and C+D combos, C held then D (Freeze plus Dry Kill, not Keeper),
// the 1 second C+D Keeper hold, the expression curves, the remote-control parameters, and
// that nothing else on the MIDI cable (sustain pedal, mod wheel, the wrong channel)
// changes anything.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

using namespace juce;

namespace
{
int failures = 0;

void expect (bool ok, const char* what)
{
    std::printf ("  %-62s %s\n", what, ok ? "ok" : "FAILED");
    if (! ok) ++failures;
}

struct Rig
{
    GrottoProcessor proc;
    AudioBuffer<float> buffer { 2, 64 };
    double now = 100.0;

    Rig()
    {
        // the shipped defaults, whatever is saved on this computer
        auto& s = proc.pedals();
        s.resetFootswitches();
        s.expChannel = 16; s.expCC = 11;
        s.keeperChannel = 0; s.keeperCC = -1;
        s.heel = 0; s.toe = 127; s.curve = 0;
        proc.prepareToPlay (48000.0, 64);
        proc.apvts.getParameter (ParamIDs::expSmooth)->setValueNotifyingHost (0.0f);
        settle (0.05);
    }

    void cc (int number, int value, int channel = 16)
    {
        MidiBuffer midi;
        midi.addEvent (MidiMessage::controllerEvent (channel, number, value), 0);
        buffer.clear();
        proc.processBlock (buffer, midi);
        proc.pump (now);
    }

    // let time pass, pumping the message-thread work every 10 ms as the timer does
    void settle (double seconds)
    {
        for (double t = 0.0; t < seconds; t += 0.01)
        {
            now += 0.01;
            proc.pump (now);
        }
    }

    void press (int sw) { cc (20 + sw, 127); }
    void release (int sw) { cc (20 + sw, 0); }
    void tap (int sw) { press (sw); settle (0.1); release (sw); settle (0.05); }
    void hold (int sw, double seconds) { press (sw); settle (seconds); }

    bool on (const char* id) { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    float value (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); }
    void set (const char* id, float plain)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }

    // every parameter except those a test expects to change
    std::map<String, float> snapshot()
    {
        std::map<String, float> m;
        for (auto* p : proc.getParameters())
            if (auto* rp = dynamic_cast<RangedAudioParameter*> (p)) m[rp->getParameterID()] = rp->getValue();
        return m;
    }
};
} // namespace

// --snapshot <folder>: render the panel in a few states as PNGs (needs a display, e.g. xvfb-run)
static int snapshots (const File& dir)
{
    dir.createDirectory();
    Rig r;
    std::unique_ptr<AudioProcessorEditor> ed (r.proc.createEditor());
    auto* editor = dynamic_cast<GrottoEditor*> (ed.get());
    ed->setVisible (true);
    double phase = 0.0;
    auto play = [&] (double seconds)
    {
        for (double t = 0.0; t < seconds; t += 64.0 / 48000.0)
        {
            for (int i = 0; i < 64; ++i)
            {
                const float v = (float) (0.3 * std::sin (phase) * std::exp (-std::fmod (phase / 1382.0, 1.0) * 3.0));
                phase += 2.0 * MathConstants<double>::pi * 220.0 / 48000.0;
                r.buffer.setSample (0, i, v);
                r.buffer.setSample (1, i, v);
            }
            MidiBuffer none;
            r.proc.processBlock (r.buffer, none);
        }
        r.settle (0.01);
        MessageManager::getInstance()->runDispatchLoopUntil (400);
    };
    auto shot = [&] (const char* name)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        File f = dir.getChildFile (String (name) + ".png");
        f.deleteFile();
        FileOutputStream out (f);
        PNGImageFormat().writeImageToStream (img, out);
        std::printf ("  wrote %s\n", f.getFullPathName().toRawUTF8());
    };
    play (1.5); shot ("1_echoing");
    r.set (ParamIDs::drag, 0.15f); play (1.0); shot ("2_scattered");
    r.set (ParamIDs::drag, 0.75f); r.set (ParamIDs::runaway, 1.0f); play (2.5); shot ("3_runaway");
    r.set (ParamIDs::runaway, 0.0f); r.set (ParamIDs::freeze, 1.0f); play (1.0); shot ("4_freeze");
    r.set (ParamIDs::freeze, 0.0f); r.set (ParamIDs::dryKill, 1.0f); play (1.0); shot ("5_dry_kill");
    r.set (ParamIDs::dryKill, 0.0f); r.set (ParamIDs::bypass, 1.0f); play (1.0); shot ("6_bypassed");
    r.set (ParamIDs::bypass, 0.0f); editor->setPage (1); play (0.5); shot ("7_deep_page");
    editor->setPage (2); play (0.5); shot ("8_pedal_page");
    return 0;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI juce;
    if (argc > 2 && String (argv[1]) == "--snapshot") return snapshots (File (argv[2]));

    // every switch: tap latches its job, hold does it only while held
    const char* jobs[4] = { ParamIDs::bypass, ParamIDs::runaway, ParamIDs::freeze, ParamIDs::dryKill };
    const char* names_[4] = { "A: bypass", "B: runaway", "C: freeze", "D: dry kill" };
    for (int sw = 0; sw < 4; ++sw)
    {
        std::printf ("Footswitch %s\n", names_[sw]);
        Rig r;
        r.press (sw); r.settle (0.02);
        expect (r.on (jobs[sw]), "acts on the press");
        r.release (sw); r.settle (0.05);
        expect (r.on (jobs[sw]), "tap latches it");
        r.tap (sw);
        expect (! r.on (jobs[sw]), "tap again switches it back");
        r.hold (sw, 1.0);
        expect (r.on (jobs[sw]), "hold: on while held");
        r.release (sw); r.settle (0.05);
        expect (! r.on (jobs[sw]), "release: back as it was");
    }

    std::printf ("Pairs\n");
    {
        Rig r;
        const int tamesBefore = r.proc.uiTamePulse.load();
        r.press (0); r.settle (0.03); r.press (1); r.settle (0.1); r.release (0); r.release (1); r.settle (0.1);
        expect (r.proc.uiTamePulse.load() == tamesBefore + 1, "A+B tames");
        expect (! r.on (ParamIDs::bypass), "A+B leaves bypass as it was");
        expect (! r.on (ParamIDs::runaway), "A+B leaves no runaway");

        r.press (1); r.settle (0.03); r.press (0); r.settle (0.1); r.release (0); r.release (1); r.settle (0.1);
        expect (r.proc.uiTamePulse.load() == tamesBefore + 2, "B+A (B first) tames too");
        expect (! r.on (ParamIDs::bypass) && ! r.on (ParamIDs::runaway), "B+A leaves nothing behind");

        const int keepsBefore = r.proc.uiKeepPulse.load();
        r.press (3); r.settle (0.03); r.press (2); r.settle (0.1); r.release (2); r.release (3); r.settle (0.1);
        expect (! r.on (ParamIDs::freeze) && ! r.on (ParamIDs::dryKill), "C+D tap (D first) leaves freeze and dry kill as they were");
        expect (r.proc.uiKeepPulse.load() == keepsBefore, "C+D tap doesn't keep");

        r.press (2); r.settle (0.03); r.press (3); r.settle (1.2);
        expect (r.proc.uiKeepPulse.load() == keepsBefore + 1, "C+D held 1 second keeps");
        r.release (2); r.release (3); r.settle (0.1);
        expect (! r.on (ParamIDs::freeze) && ! r.on (ParamIDs::dryKill), "C+D hold leaves freeze and dry kill alone");
        expect (r.proc.uiKeepPulse.load() == keepsBefore + 1, "only one Keeper per hold");
    }

    std::printf ("C held, then D: a frozen wet-only bed, not Keeper\n");
    {
        Rig r;
        const int keeps = r.proc.uiKeepPulse.load();
        r.hold (2, 0.6);
        r.press (3); r.settle (1.3);
        expect (r.on (ParamIDs::freeze) && r.on (ParamIDs::dryKill), "frozen and wet only together");
        expect (r.proc.uiKeepPulse.load() == keeps, "no Keeper");
        r.release (3); r.release (2); r.settle (0.1);
        expect (! r.on (ParamIDs::freeze) && ! r.on (ParamIDs::dryKill), "both let go: both back as they were");
    }

    std::printf ("Expression on Drag (even in pitch, heel scatters)\n");
    {
        Rig r;
        r.cc (11, 0); r.settle (0.05);
        expect (r.value (ParamIDs::drag) < 0.01f, "heel is full anticlockwise");
        r.cc (11, 127); r.settle (0.05);
        expect (r.value (ParamIDs::drag) > 0.99f, "toe is a tight reverb");
        // equal travel gives equal pitch change, in every mode
        bool even = true;
        for (int m = 0; m < gr::NumModes; ++m)
        {
            if (! gr::modeDegrees (m).empty()) continue;   // stepped modes snap, so only the smooth ones can be even
            float last = 0.0f;
            for (int v = 1; v <= 10; ++v)
            {
                const float x = (float) v / 10.0f;
                const float semis = 12.0f * std::log2 (gr::dragToStretch (expressionToDrag (x, m), m));
                const float prev = 12.0f * std::log2 (gr::dragToStretch (expressionToDrag (x - 0.1f, m), m));
                const float step = prev - semis;
                if (v > 1 && std::abs (step - last) > 0.05f) even = false;
                last = step;
            }
        }
        expect (even, "the sweep moves evenly in pitch in the smooth modes");
        r.set (ParamIDs::mode, (float) gr::Chromatic);
        r.cc (11, 40); r.settle (0.05);
        const float semis = 12.0f * std::log2 (gr::dragToStretch (r.value (ParamIDs::drag), gr::Chromatic));
        expect (std::abs (semis - std::round (semis)) < 0.01f, "Chromatic mode lands on semitones");
    }

    std::printf ("Expression on Reflect\n");
    {
        Rig r;
        r.set (ParamIDs::expTarget, (float) names::ExpReflect);
        const float dragBefore = r.value (ParamIDs::drag);
        r.cc (11, 127); r.settle (0.05);
        expect (r.value (ParamIDs::reflect) > 0.98f, "toe is full Reflect");
        r.cc (11, 64); r.settle (0.05);
        expect (r.value (ParamIDs::reflect) > 0.45f && r.value (ParamIDs::reflect) < 0.7f, "half travel is a long wash");
        expect (std::abs (r.value (ParamIDs::drag) - dragBefore) < 0.001f, "Drag is left alone");
    }

    std::printf ("Remote control through the parameter list\n");
    {
        Rig r;
        r.set (ParamIDs::foot[0], 1.0f); r.settle (0.05); r.set (ParamIDs::foot[0], 0.0f); r.settle (0.05);
        expect (r.on (ParamIDs::bypass), "footA parameter taps A");
        r.set (ParamIDs::foot[1], 1.0f); r.settle (0.8);
        expect (r.on (ParamIDs::runaway), "footB parameter held runs away");
        r.set (ParamIDs::foot[1], 0.0f); r.settle (0.05);
        expect (! r.on (ParamIDs::runaway), "footB released reels it in");
        const int keeps = r.proc.uiKeepPulse.load();
        r.set (ParamIDs::keeper, 1.0f); r.settle (0.05);
        expect (r.proc.uiKeepPulse.load() == keeps + 1 && ! r.on (ParamIDs::keeper), "keeper parameter keeps once and resets");
        const int tames = r.proc.uiTamePulse.load();
        r.set (ParamIDs::tame, 1.0f); r.settle (0.05);
        expect (r.proc.uiTamePulse.load() == tames + 1 && ! r.on (ParamIDs::tame), "tame parameter tames once and resets");
    }

    std::printf ("No clashes\n");
    {
        Rig r;
        const auto before = r.snapshot();
        // sustain pedal and mod wheel on the footswitch channel
        r.cc (64, 127); r.settle (0.6); r.cc (64, 0);
        r.cc (1, 100);
        // the footswitch and expression numbers on the wrong channel
        for (int ch : { 1, 2, 15 })
        {
            for (int n : { 20, 21, 22, 23 }) { r.cc (n, 127, ch); r.settle (0.6); r.cc (n, 0, ch); }
            r.cc (11, 0, ch);
        }
        r.settle (0.2);
        expect (r.snapshot() == before, "sustain, mod wheel and other channels change nothing");
    }

    std::printf ("\n%s: %d failed\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
