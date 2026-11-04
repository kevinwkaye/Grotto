#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/GrottoEngine.h"

// Every parameter lives in this one list, the pedal's controls, the footswitch jobs and
// Keeper included, so a host, an automation lane or a remote controller can read and set
// everything the panel and the footswitches can.
namespace ParamIDs
{
// the pedal
inline constexpr const char* length    = "length";
inline constexpr const char* diffuse   = "diffuse";
inline constexpr const char* dampen    = "dampen";
inline constexpr const char* drag      = "drag";
inline constexpr const char* reflect   = "reflect";
inline constexpr const char* mix       = "mix";
inline constexpr const char* mode      = "mode";
inline constexpr const char* bypass    = "bypass";
inline constexpr const char* trails    = "trails";
// extras
inline constexpr const char* lowCut    = "lowCut";
inline constexpr const char* freeze    = "freeze";
inline constexpr const char* runaway   = "runaway";
inline constexpr const char* dryKill   = "dryKill";
inline constexpr const char* bass      = "bass";
inline constexpr const char* mono      = "mono";
// expression
inline constexpr const char* expTarget = "expTarget";
inline constexpr const char* expMin    = "expMin";
inline constexpr const char* expMax    = "expMax";
inline constexpr const char* expSmooth = "expSmooth";
// actions for a remote controller: set to 1 to press, 0 to release
inline constexpr const char* foot[4]   = { "footA", "footB", "footC", "footD" };
inline constexpr const char* keeper    = "keeper";
inline constexpr const char* tame      = "tame";
} // namespace ParamIDs

namespace names
{
struct ExpTarget { const char* id; const char* label; };
inline const std::vector<ExpTarget>& expTargets()
{
    static const std::vector<ExpTarget> t {
        { ParamIDs::drag, "Drag" }, { ParamIDs::reflect, "Reflect" }, { ParamIDs::length, "Length" },
        { ParamIDs::mix, "Mix" }, { ParamIDs::dampen, "Dampen" }, { ParamIDs::diffuse, "Diffuse" } };
    return t;
}
enum { ExpDrag = 0, ExpReflect = 1 };

inline juce::String modeLabel (int m)
{
    static const char* const n[gr::NumModes] = { "Smooth", "Glide", "Even", "Chromatic", "Major", "Minor", "Pentatonic", "Octaves and fifths", "Octaves" };
    return n[juce::jlimit (0, gr::NumModes - 1, m)];
}

inline juce::String dragText (float drag, int mode)
{
    const float ms = gr::Swarm::meanMs (gr::dragToStretch (drag, mode));
    return juce::String (juce::roundToInt (ms)) + " ms";
}
} // namespace names

// Heel to toe on Drag: an even sweep in pitch, whatever the mode. Heel is full
// anticlockwise (scattered, long, the tail bent furthest down), toe is a tight reverb.
// Equal travel gives equal pitch change, so the dive feels the same all the way along.
inline float expressionToDrag (float x, int mode)
{
    const float stretch = std::pow (gr::kMaxStretch, 1.0f - juce::jlimit (0.0f, 1.0f, x));
    return gr::stretchToDrag (stretch, mode);
}
inline float dragToExpression (float drag, int mode)
{
    return 1.0f - std::log (gr::dragToStretch (drag, mode)) / std::log (gr::kMaxStretch);
}

// Heel to toe on Reflect: most of the travel sits near the edge where the cave starts to
// sing. Heel is a short wash, the last fifth goes past the edge.
inline float expressionToReflect (float x)
{
    if (x <= 0.8f) return 0.15f + 0.65f * std::pow (x / 0.8f, 0.8f);
    return 0.8f + 0.2f * (x - 0.8f) / 0.2f;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto add = [&layout] (auto p) { layout.add (std::move (p)); };
    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; };

    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::length, 1 }, "Length",
         NormalisableRange<float> (0.0f, 1.0f), 0.55f,
         AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
         {
             const float s = gr::lengthSeconds (v);
             return String (s, s < 10.0f ? 1 : 0) + " s";
         })));
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::diffuse, 1 }, "Diffuse",
         NormalisableRange<float> (0.0f, 1.0f), 0.55f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::dampen, 1 }, "Dampen",
         NormalisableRange<float> (0.0f, 1.0f), 0.4f,
         AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
         {
             if (v < 0.15f) return String ("Bright");
             if (v > 0.85f) return String ("Dark");
             return String (roundToInt (v * 100.0f)) + "% dark";
         })));
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::drag, 1 }, "Drag",
         NormalisableRange<float> (0.0f, 1.0f), 0.75f,
         AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return names::dragText (v, gr::Smooth); })));
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::reflect, 1 }, "Reflect",
         NormalisableRange<float> (0.0f, 1.0f), 0.4f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mix, 1 }, "Mix",
         NormalisableRange<float> (0.0f, 1.0f), 0.45f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    StringArray modes;
    for (int m = 0; m < gr::NumModes; ++m) modes.add (names::modeLabel (m));
    add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Drag Mode", modes, gr::Smooth));
    add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));
    add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::trails, 1 }, "Trails", true));

    NormalisableRange<float> cut (gr::kLowCutMin, gr::kLowCutMax);
    cut.setSkewForCentre (90.0f);
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::lowCut, 1 }, "Low Cut", cut, 90.0f,
         AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " Hz"; })));
    add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::freeze, 1 }, "Freeze", false));
    add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::runaway, 1 }, "Runaway", false));
    add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::dryKill, 1 }, "Dry Kill", false));
    add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::bass, 1 }, "Bass", false));
    add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::mono, 1 }, "Mono", false));

    StringArray targets;
    for (const auto& t : names::expTargets()) targets.add (t.label);
    add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::expTarget, 1 }, "Expression Target", targets, names::ExpDrag));
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::expMin, 1 }, "Expression Min",
         NormalisableRange<float> (0.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::expMax, 1 }, "Expression Max",
         NormalisableRange<float> (0.0f, 1.0f), 1.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::expSmooth, 1 }, "Expression Smoothing",
         NormalisableRange<float> (0.0f, 1.0f), 0.25f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    const char* footNames[4] = { "Footswitch A", "Footswitch B", "Footswitch C", "Footswitch D" };
    for (int i = 0; i < 4; ++i)
        add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::foot[i], 1 }, footNames[i], false));
    add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::keeper, 1 }, "Keeper", false));
    add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::tame, 1 }, "Tame", false));
    return layout;
}
