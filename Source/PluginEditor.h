#pragma once

#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

// Flip Fella palette, shared across all of Kevin's builds.
namespace ff
{
const juce::Colour bg        { 0xff050505 };
const juce::Colour face      { 0xff0d0d0d };
const juce::Colour housing   { 0xff060606 };
const juce::Colour card      { 0xff1b1b1b };
const juce::Colour cardHi    { 0xff252525 };
const juce::Colour cardLow   { 0xff141414 };
const juce::Colour ink       { 0xfff3f0e8 };
const juce::Colour dim       { 0xffa29e95 };
const juce::Colour faint     { 0xff5a5852 };
const juce::Colour line      { 0xff3a3a3a };
const juce::Colour ring      { 0xff1a1a1a };
const juce::Colour pin       { 0xff3c3c3c };
const juce::Colour red       { 0xffe8392f };
const juce::Colour teal      { 0xff3fd6c8 };   // the cave's glow
const juce::Colour ice       { 0xffbfeff5 };   // Freeze
const juce::Colour green     { 0xff5fd38a };
const juce::Colour blob      { 0xff4d5bff };
const juce::Colour blobShade { 0xff2329a8 };
const juce::Colour eye       { 0xffffffff };
const juce::Colour faceInk   { 0xff0b0b12 };

juce::Font font (float height, float kerning = 0.04f);
void label (juce::Graphics&, const juce::String& text, juce::Rectangle<int> r,
            juce::Justification j = juce::Justification::centredLeft, juce::Colour c = dim, float size = 13.0f);
} // namespace ff

namespace ui
{
void pill (juce::Graphics&, juce::Rectangle<float> b, const juce::String& text, bool on, float size, juce::Colour onColour = ff::ink);

// A split-flap card that flips like the Karlsson clock when its value changes.
class FlipCard : public juce::Component
{
public:
    void setValue (const juce::String& v);
    void setTextHeight (float h) { textHeight = h; }
    void setInk (juce::Colour c) { if (c != inkColour) { inkColour = c; repaint(); } }
    void advance (double dt);
    void paint (juce::Graphics&) override;

private:
    void drawHalf (juce::Graphics&, juce::Rectangle<float> card, const juce::String& text, bool top, float scale, bool shade);
    juce::String current, previous;
    float progress = 1.0f;
    float textHeight = 34.0f;
    juce::Colour inkColour = ff::ink;
};

// A single pill that turns ink-white when its switch is on.
class ToggleChip : public juce::Component
{
public:
    ToggleChip (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& text,
                const juce::String& onText = {}, float textSize = 13.0f, juce::Colour onColour = ff::ink);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::String text, onText;
    float textSize;
    juce::Colour onColour;
    bool on = false;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// A plain pill button that runs a function.
class ActionChip : public juce::Component
{
public:
    ActionChip (const juce::String& t, std::function<void()> fn, float textSize = 12.0f)
        : text (t), action (std::move (fn)), size (textSize) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics& g) override { pill (g, getLocalBounds().toFloat().reduced (1.0f), text, lit, size, colour); }
    void mouseDown (const juce::MouseEvent&) override { if (action) action(); }
    void setLit (bool l) { if (l != lit) { lit = l; repaint(); } }
    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void setColour (juce::Colour c) { colour = c; repaint(); }

private:
    juce::String text;
    std::function<void()> action;
    float size;
    bool lit = false;
    juce::Colour colour = ff::ink;
};

class FlipLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
};

// A knob drawn as a small clock face, with its name above and its value below.
class Dial : public juce::Component
{
public:
    Dial (GrottoProcessor&, const juce::String& paramId, const juce::String& caption, bool redHand = false, int knobSize = 58);
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<juce::String()> valueText;   // overrides the parameter's own text
    juce::Slider slider;

private:
    juce::RangedAudioParameter* param;
    juce::String caption;
    int knobSize;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class LabelledSlider : public juce::Component
{
public:
    LabelledSlider (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    juce::RangedAudioParameter* param;
    juce::String caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// The hero: Drag on a Karlsson face. Clockwise a tight reverb, anticlockwise the swarm
// scatters. Teal dots mark where the stepped modes land; the hand shows what you hear.
class DragDial : public juce::Component
{
public:
    explicit DragDial (GrottoProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void tick();

private:
    juce::Rectangle<float> modeChip() const;
    GrottoProcessor& proc;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    float shown = 0.75f;
    int lastMode = -1;
};

// One footswitch in the bottom strip. Works exactly like the real one: tap, hold, pairs.
class FootButton : public juce::Component
{
public:
    FootButton (GrottoProcessor& p, int s) : proc (p), sw (s) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { proc.footDown (sw); }
    void mouseUp (const juce::MouseEvent&) override { proc.footUp (sw); }
    void tick();

private:
    GrottoProcessor& proc;
    int sw;
    juce::String big;
    juce::Colour led = ff::pin;
    bool lit = false, down = false;
};

// The expression pedal: live position, the target, and a menu for calibration and the rest.
class ExpressionBar : public juce::Component
{
public:
    explicit ExpressionBar (GrottoProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void tick();
    std::function<void()> showPedalPage;

private:
    GrottoProcessor& proc;
    juce::RangedAudioParameter* target;
    float level = 0.0f;
    int lastTarget = -1;
};

// "FOOTSWITCHES": a dot that lights when the footswitch is heard directly, and the input menu.
class FootInputLabel : public juce::Component
{
public:
    explicit FootInputLabel (GrottoProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    GrottoProcessor& proc;
};

// One footswitch on the Pedal page: its jobs, what it listens for, Learn and Defaults.
class FootswitchCard : public juce::Component
{
public:
    FootswitchCard (GrottoProcessor&, int sw);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick (double dt);

private:
    GrottoProcessor& proc;
    int sw;
    FlipCard letter;
    ActionChip learn, reset;
    juce::String sends;
};

// Pebble in the cave. Every note echoes round the walls as fading Pebbles; Drag stretches
// Pebble and scatters the echoes into a ping-pong, and they slide as the pitch bends.
// In a runaway Pebble shakes and screams. Holds its breath for Freeze, goes see-through
// for Dry Kill, dozes when bypassed, taps its foot on a press, leans with the expression
// pedal, flinches at Tame and nods for Keeper.
class Cave : public juce::Component
{
public:
    explicit Cave (GrottoProcessor& p);
    void update (double t, double dt);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::String mood() const;

private:
    struct Echo { double born = 0.0; int k = 1; float side = 1.0f, amp = 1.0f, y = 0.0f; };
    void drawPebble (juce::Graphics&, juce::Point<float> c, float scale, float sx, float sy, float tilt, juce::Colour body,
                     juce::Colour shade, float alpha, bool face, bool ghost) const;
    void spawn (double t);

    GrottoProcessor& proc;
    std::vector<Echo> echoes;
    struct Rock { float x, h, w; };
    std::vector<Rock> rocks;
    double now = 0.0;
    float stretch = 1.0f, scatter = 0.0f, bend = 0.0f, wet = 0.0f, input = 0.0f, scream = 0.0f, lean = 0.0f;
    float pebStretch = 1.0f, tap = 0.0f, nod = 0.0f, flinch = 0.0f, mouth = 0.0f;
    bool asleep = false, frozen = false, ghost = false, blinking = false;
    double nextBlink = 2.0, blinkUntil = 0.0, frozenAt = 0.0;
    int lastOnset = 0, lastFoot = 0, lastKeep = 0, lastTame = 0;
};

class GrottoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GrottoEditor (GrottoProcessor&);
    ~GrottoEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override { proc.focusHere(); }
    void setPage (int p);

private:
    void timerCallback() override;
    void keeperMenu();
    void closeNote (bool save);
    void section (juce::Graphics&, juce::Rectangle<int>) const;

    GrottoProcessor& proc;
    FlipLookAndFeel lnf;
    int page = 0;   // 0 Main, 1 Deep, 2 Pedal

    // header
    ActionChip mainTab, deepTab, pedalTab, keepButton, keepMenu;

    // the flip card row
    std::array<FlipCard, 6> cards;

    // main
    Cave cave;
    DragDial dragDial;
    Dial length, diffuse, dampen, reflect, mix, lowCut;

    // strip
    FootInputLabel footInput;
    std::array<std::unique_ptr<FootButton>, 4> feet;
    ExpressionBar expression;

    // deep page
    ToggleChip bassChip, monoChip, trailsChip;
    std::array<std::unique_ptr<ActionChip>, gr::NumModes> modeChips;
    ActionChip tameChip, liveChipDeep;

    // pedal page
    std::array<std::unique_ptr<FootswitchCard>, 4> footCards;
    std::array<std::unique_ptr<ActionChip>, 6> targetChips;
    LabelledSlider expMin, expMax, expSmooth;
    ActionChip calibrate, learnExp, curve, liveChip, learnKeep, exportKeep, folderKeep;

    juce::TextEditor noteEditor;
    int noteNumber = 0;
    double noteUntil = 0.0;

    double lastTick = 0.0, start = 0.0;
};
} // namespace ui

using GrottoEditor = ui::GrottoEditor;
