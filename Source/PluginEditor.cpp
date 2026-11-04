#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

// =====================================================================================
// Palette helpers
// =====================================================================================
namespace ff
{
Font font (float height, float kerning)
{
    static Typeface::Ptr typeface = Typeface::createSystemTypefaceFor (BinaryData::RobotoCondensedBold_ttf,
                                                                       BinaryData::RobotoCondensedBold_ttfSize);
    return Font (FontOptions (typeface).withHeight (height).withKerningFactor (kerning));
}

void label (Graphics& g, const String& text, Rectangle<int> r, Justification j, Colour c, float size)
{
    g.setColour (c);
    g.setFont (font (size, 0.08f));
    g.drawText (text, r, j, false);
}
} // namespace ff

namespace ui
{
static double nowSeconds() { return Time::getMillisecondCounterHiRes() * 0.001; }
static float raw (GrottoProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
static bool isOn (GrottoProcessor& p, const char* id) { return raw (p, id) > 0.5f; }
static const String dot = String::fromUTF8 (" \xc2\xb7 ");

static void setParam (RangedAudioParameter* p, float plain)
{
    if (p == nullptr) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (plain));
    p->endChangeGesture();
}

void pill (Graphics& g, Rectangle<float> b, const String& text, bool on, float size, Colour onColour)
{
    const float r = b.getHeight() * 0.5f;
    if (on) { g.setColour (onColour); g.fillRoundedRectangle (b, r); }
    else { g.setColour (ff::line); g.drawRoundedRectangle (b.reduced (1.0f), r, 2.0f); }
    g.setColour (on ? ff::bg : ff::ink);
    g.setFont (ff::font (size, 0.06f));
    g.drawText (text, b, Justification::centred, false);
}

// =====================================================================================
// Flip card
// =====================================================================================
void FlipCard::setValue (const String& v)
{
    if (v == current) return;
    previous = current;
    current = v;
    progress = previous.isEmpty() ? 1.0f : 0.0f;
    repaint();
}

void FlipCard::advance (double dt)
{
    if (progress < 1.0f)
    {
        progress = jmin (1.0f, progress + (float) (dt / 0.26));
        repaint();
    }
}

void FlipCard::drawHalf (Graphics& g, Rectangle<float> card, const String& text, bool top, float scale, bool shade)
{
    const float hinge = card.getCentreY();
    const float halfH = card.getHeight() * 0.5f;
    if (scale <= 0.001f) return;

    Graphics::ScopedSaveState save (g);
    auto clip = top ? Rectangle<float> (card.getX(), hinge - halfH * scale, card.getWidth(), halfH * scale)
                    : Rectangle<float> (card.getX(), hinge, card.getWidth(), halfH * scale);
    g.reduceClipRegion (clip.getSmallestIntegerContainer());
    g.addTransform (AffineTransform::scale (1.0f, scale, 0.0f, hinge));

    if (top) g.setGradientFill (ColourGradient (ff::cardHi, 0, card.getY(), ff::card, 0, hinge, false));
    else     g.setGradientFill (ColourGradient (ff::card, 0, hinge, ff::cardLow, 0, card.getBottom(), false));
    g.fillRoundedRectangle (card, 7.0f);
    g.setColour (inkColour);
    g.setFont (ff::font (textHeight, 0.02f));
    g.drawFittedText (text, card.reduced (6.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.7f);

    if (shade)
    {
        g.setColour (Colours::black.withAlpha (0.45f * (1.0f - scale)));
        g.fillRoundedRectangle (card, 7.0f);
    }
}

void FlipCard::paint (Graphics& g)
{
    auto outer = getLocalBounds().toFloat();
    g.setColour (ff::housing);
    g.fillRoundedRectangle (outer, 10.0f);
    auto card = outer.reduced (6.0f);

    if (progress >= 1.0f)
    {
        drawHalf (g, card, current, true, 1.0f, false);
        drawHalf (g, card, current, false, 1.0f, false);
    }
    else
    {
        drawHalf (g, card, current, true, 1.0f, false);
        drawHalf (g, card, previous, false, 1.0f, false);
        const float eased = 1.0f - std::pow (1.0f - progress, 2.0f);
        if (eased < 0.5f) drawHalf (g, card, previous, true, 1.0f - eased * 2.0f, true);
        else              drawHalf (g, card, current, false, (eased - 0.5f) * 2.0f, true);
    }

    g.setColour (Colours::black);
    g.fillRect (card.getX(), card.getCentreY() - 1.5f, card.getWidth(), 3.0f);
    g.setColour (ff::pin);
    g.fillRoundedRectangle (outer.getX() + 1.0f, card.getCentreY() - 6.0f, 3.0f, 12.0f, 1.0f);
    g.fillRoundedRectangle (outer.getRight() - 4.0f, card.getCentreY() - 6.0f, 3.0f, 12.0f, 1.0f);
}

// =====================================================================================
// Pills and chips
// =====================================================================================
ToggleChip::ToggleChip (AudioProcessorValueTreeState& s, const String& paramId, const String& t, const String& tOn, float size, Colour c)
    : text (t), onText (tOn.isEmpty() ? t : tOn), textSize (size), onColour (c)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    attachment = std::make_unique<ParameterAttachment> (*s.getParameter (paramId), [this] (float v) { on = v > 0.5f; repaint(); });
    attachment->sendInitialUpdate();
}

void ToggleChip::paint (Graphics& g) { pill (g, getLocalBounds().toFloat().reduced (1.0f), on ? onText : text, on, textSize, onColour); }
void ToggleChip::mouseDown (const MouseEvent&) { attachment->setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

// =====================================================================================
// Look and feel
// =====================================================================================
void FlipLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider& s)
{
    const float cy = (float) y + (float) h * 0.5f;
    const float left = (float) x + 7.0f, right = (float) (x + w) - 7.0f;
    g.setColour (ff::ring);
    g.fillRoundedRectangle (left, cy - 3.0f, right - left, 6.0f, 3.0f);
    g.setColour (ff::ink.withAlpha (s.isMouseOverOrDragging() ? 1.0f : 0.85f));
    g.fillRoundedRectangle (left, cy - 3.0f, jmax (0.0f, pos - left), 6.0f, 3.0f);
    g.setColour (ff::ink);
    g.fillEllipse (pos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
}

void FlipLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    if (s.getProperties()["hidden"]) return;   // the hero dials draw themselves
    const float size = (float) jmin (w, h);
    const Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float r = size * 0.5f - 1.0f;

    g.setColour (ff::ring);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (ff::bg);
    g.fillEllipse (c.x - r * 0.9f, c.y - r * 0.9f, r * 1.8f, r * 1.8f);

    g.setColour (ff::ink);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * (float) i / 10.0f;
        g.drawLine ({ { c.x + std::sin (a) * r * 0.80f, c.y - std::cos (a) * r * 0.80f },
                      { c.x + std::sin (a) * r * 0.92f, c.y - std::cos (a) * r * 0.92f } }, 2.0f);
    }

    const bool red = s.getProperties()["red"];
    g.setColour (red ? ff::red : ff::ink);
    const float a = start + pos * (end - start);
    Path hand;
    hand.addRoundedRectangle (-3.0f, -r * 0.68f, 6.0f, r * 0.68f + r * 0.15f, 2.0f);
    g.fillPath (hand, AffineTransform::rotation (a).translated (c));
    g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
    g.setColour (ff::bg);
    g.fillEllipse (c.x - 1.8f, c.y - 1.8f, 3.6f, 3.6f);
}

static void setupRotary (Slider& s)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    s.setMouseCursor (MouseCursor::PointingHandCursor);
}

// =====================================================================================
// Dials and sliders
// =====================================================================================
Dial::Dial (GrottoProcessor& p, const String& paramId, const String& c, bool redHand, int size)
    : caption (c), knobSize (size)
{
    param = p.apvts.getParameter (paramId);
    setupRotary (slider);
    slider.getProperties().set ("red", redHand);
    slider.onValueChange = [this] { repaint(); };
    slider.onDragStart = [&p] { p.focusHere(); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramId, slider);
    slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
}

void Dial::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (18).withTrimmedBottom (22).withSizeKeepingCentre (knobSize, knobSize));
}

void Dial::paint (Graphics& g)
{
    auto r = getLocalBounds();
    ff::label (g, caption.toUpperCase(), r.removeFromTop (18), Justification::centred, ff::dim, 12.0f);
    const String value = valueText ? valueText() : param->getCurrentValueAsText();
    ff::label (g, value.toUpperCase(), r.removeFromBottom (20), Justification::centred, ff::ink, 13.0f);
}

LabelledSlider::LabelledSlider (AudioProcessorValueTreeState& s, const String& paramId, const String& c) : caption (c)
{
    param = s.getParameter (paramId);
    slider.setSliderStyle (Slider::LinearHorizontal);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setMouseCursor (MouseCursor::PointingHandCursor);
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
    slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}

void LabelledSlider::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (16)); }

void LabelledSlider::paint (Graphics& g)
{
    auto top = getLocalBounds().removeFromTop (16);
    ff::label (g, caption.toUpperCase(), top, Justification::centredLeft, ff::dim, 11.0f);
    ff::label (g, param->getCurrentValueAsText().toUpperCase(), top, Justification::centredRight, ff::ink, 11.0f);
}


void FootInputLabel::paint (Graphics& g)
{
    g.setColour (proc.pedalsDirect() ? ff::green : ff::pin);
    g.fillEllipse (0.0f, (float) getHeight() * 0.5f - 5.0f, 10.0f, 10.0f);
    ff::label (g, proc.pedalsDirect() ? "FOOTSWITCHES READY  v" : "FOOTSWITCHES  v", getLocalBounds().withTrimmedLeft (16), Justification::centredLeft, ff::dim, 12.0f);
}

void FootInputLabel::mouseDown (const MouseEvent&)
{
    PopupMenu m;
    m.addSectionHeader ("Footswitch input");
    m.addItem (-1, proc.footInputStatus(), false, false);
    const auto chosen = proc.footDevice();
    m.addItem (10, "Find my footswitch automatically", true, chosen.isEmpty());
    const auto devices = DirectMidiInput::deviceNames();
    for (int i = 0; i < devices.size() && i < 40; ++i)
        m.addItem (100 + i, "Use " + devices[i], true, chosen == devices[i]);
    m.addItem (11, "Only from a MIDI track in Ableton", true, chosen == "none");

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this, devices] (int r)
    {
        if (r == 10) proc.setFootDevice ({});
        else if (r == 11) proc.setFootDevice ("none");
        else if (r >= 100 && r - 100 < devices.size()) proc.setFootDevice (devices[r - 100]);
        repaint();
    });
}

// =====================================================================================
// Drag: the hero dial. Clockwise is a tight reverb, anticlockwise the swarm scatters.
// =====================================================================================
static constexpr float kStart = -2.35619449f, kSpan = 4.71238898f;   // -135 degrees, 270 degrees

DragDial::DragDial (GrottoProcessor& p) : proc (p)
{
    setupRotary (slider);
    slider.getProperties().set ("hidden", true);
    slider.onDragStart = [&p] { p.focusHere(); };
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ParamIDs::drag, slider);
    slider.setDoubleClickReturnValue (true, 0.75);
}

Rectangle<float> DragDial::modeChip() const
{
    return Rectangle<float> (170.0f, 28.0f).withCentre ({ (float) getWidth() * 0.5f, (float) getHeight() - 18.0f });
}

void DragDial::resized()
{
    const auto c = Point<int> (getWidth() / 2, (int) (getHeight() * 0.46f));
    slider.setBounds (Rectangle<int> (220, 220).withCentre (c));
}

void DragDial::mouseDown (const MouseEvent& e)
{
    if (! modeChip().contains (e.position)) return;
    proc.focusHere();
    auto* mp = proc.apvts.getParameter (ParamIDs::mode);
    const int cur = roundToInt (raw (proc, ParamIDs::mode));
    if (e.mods.isPopupMenu())
    {
        PopupMenu m;
        m.addSectionHeader ("How Drag responds");
        for (int i = 0; i < gr::NumModes; ++i) m.addItem (1 + i, names::modeLabel (i), true, i == cur);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [mp] (int r) { if (r > 0) setParam (mp, (float) (r - 1)); });
        return;
    }
    setParam (mp, (float) ((cur + 1) % gr::NumModes));
}

void DragDial::tick()
{
    const int m = roundToInt (raw (proc, ParamIDs::mode));
    // the hand shows the stretch you hear, mapped back onto the knob's travel
    const float s = proc.getEngine().uiStretch.load();
    const float pos = (m == gr::Smooth || m == gr::Glide) ? gr::stretchToDrag (s, gr::Smooth) : gr::stretchToDrag (s, gr::Even);
    if (std::abs (pos - shown) > 0.002f || m != lastMode) { shown = pos; lastMode = m; repaint(); }
}

void DragDial::paint (Graphics& g)
{
    const Point<float> c ((float) getWidth() * 0.5f, (float) getHeight() * 0.46f);
    const float r = 112.0f;
    g.setColour (ff::ring);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (ff::bg);
    g.fillEllipse (c.x - r * 0.9f, c.y - r * 0.9f, r * 1.8f, r * 1.8f);

    for (int i = 0; i <= 30; ++i)
    {
        const float a = kStart + kSpan * (float) i / 30.0f;
        const bool major = i % 5 == 0;
        g.setColour (major ? ff::ink : ff::faint);
        const float in = major ? 0.78f : 0.84f;
        g.drawLine ({ { c.x + std::sin (a) * r * in, c.y - std::cos (a) * r * in },
                      { c.x + std::sin (a) * r * 0.92f, c.y - std::cos (a) * r * 0.92f } }, major ? 3.0f : 2.0f);
    }

    // where the stepped modes land
    const int mode = roundToInt (raw (proc, ParamIDs::mode));
    const auto& deg = gr::modeDegrees (mode);
    g.setColour (ff::teal);
    for (int oct = 0; oct <= 4 && ! deg.empty(); ++oct)
        for (int d : deg)
        {
            const float semis = 12.0f * (float) oct + (float) d;
            if (semis > gr::kStretchSemis + 0.01f) continue;
            const float pos = 1.0f - semis / gr::kStretchSemis;
            const float a = kStart + kSpan * pos;
            g.fillEllipse (c.x + std::sin (a) * r * 0.70f - 2.5f, c.y - std::cos (a) * r * 0.70f - 2.5f, 5.0f, 5.0f);
        }

    ff::label (g, "DRAG", Rectangle<float> (120.0f, 20.0f).withCentre ({ c.x, c.y - r - 22.0f }).toNearestInt(), Justification::centred, ff::ink, 16.0f);
    ff::label (g, "SCATTER", Rectangle<float> (80.0f, 16.0f).withCentre ({ c.x - r * 0.82f, c.y + r * 1.02f }).toNearestInt(), Justification::centred, ff::dim, 11.0f);
    ff::label (g, "REVERB", Rectangle<float> (80.0f, 16.0f).withCentre ({ c.x + r * 0.82f, c.y + r * 1.02f }).toNearestInt(), Justification::centred, ff::dim, 11.0f);

    // a ghost hand marks the knob when the mode or the expression pedal moved what you hear
    const float knob = (float) slider.getValue();
    if (std::abs (knob - shown) > 0.03f)
    {
        Path ghostHand;
        ghostHand.addRoundedRectangle (-2.0f, -r * 0.66f, 4.0f, r * 0.5f, 1.5f);
        g.setColour (ff::faint);
        g.fillPath (ghostHand, AffineTransform::rotation (kStart + kSpan * knob).translated (c));
    }
    Path hand;
    hand.addRoundedRectangle (-4.5f, -r * 0.68f, 9.0f, r * 0.68f + r * 0.15f, 3.0f);
    g.setColour (ff::red);
    g.fillPath (hand, AffineTransform::rotation (kStart + kSpan * shown).translated (c));
    g.fillEllipse (c.x - 9.0f, c.y - 9.0f, 18.0f, 18.0f);
    g.setColour (ff::bg);
    g.fillEllipse (c.x - 3.0f, c.y - 3.0f, 6.0f, 6.0f);

    const auto chip = modeChip();
    g.setColour (ff::card);
    g.fillRoundedRectangle (chip, chip.getHeight() * 0.5f);
    g.setColour (ff::line);
    g.drawRoundedRectangle (chip.reduced (1.0f), chip.getHeight() * 0.5f, 1.5f);
    ff::label (g, "MODE" + dot + gr::modeName (mode), chip.toNearestInt(), Justification::centred, ff::teal, 12.0f);
}

// =====================================================================================
// The footswitch strip
// =====================================================================================
void FootButton::tick()
{
    String b;
    Colour l = ff::pin;
    bool lt = false;
    switch (sw)
    {
        case 0: { const bool on = ! isOn (proc, ParamIDs::bypass); b = on ? "ON" : "BYPASSED"; lt = on; if (on) l = ff::green; break; }
        case 1:
        {
            const bool run = isOn (proc, ParamIDs::runaway) || proc.getEngine().uiRunaway.load() > 0.05f;
            b = run ? "RUNNING AWAY" : "OFF"; lt = run;
            if (run) l = std::fmod (nowSeconds() * 3.0, 1.0) < 0.5 ? ff::red : ff::ink;
            break;
        }
        case 2: { const bool fr = isOn (proc, ParamIDs::freeze); b = fr ? "FROZEN" : "OFF"; lt = fr; if (fr) l = ff::ice; break; }
        default: { const bool dk = isOn (proc, ParamIDs::dryKill); b = dk ? "WET ONLY" : "OFF"; lt = dk; if (dk) l = ff::teal; break; }
    }
    const bool d = (proc.uiFootDown.load() >> sw) & 1;
    if (b != big || l != led || lt != lit || d != down)
    {
        big = b; led = l; lit = lt; down = d;
        repaint();
    }
}

void FootButton::paint (Graphics& g)
{
    static const char* const taps[4] = { "BYPASS", "RUNAWAY", "FREEZE", "DRY KILL" };
    static const char* const holds[4] = { "ON WHILE HELD", "RUN AWAY", "FREEZE", "DRY KILL" };
    auto r = getLocalBounds().toFloat().reduced (1.0f).translated (0.0f, down ? 2.0f : 0.0f);
    g.setColour (down ? ff::cardHi : ff::card);
    g.fillRoundedRectangle (r, 8.0f);
    if (lit)
    {
        g.setColour (sw == 1 ? ff::red : ff::ink);
        g.drawRoundedRectangle (r.reduced (1.0f), 8.0f, 2.0f);
    }
    g.setColour (led);
    g.fillEllipse (r.getRight() - 22.0f, r.getY() + 14.0f, 10.0f, 10.0f);
    auto t = r.reduced (12.0f, 8.0f).toNearestInt();
    auto top = t.removeFromTop (34);
    ff::label (g, String::charToString ((juce_wchar) ('A' + sw)), top, Justification::centredLeft, ff::ink, 30.0f);
    ff::label (g, big, top.withTrimmedRight (18), Justification::centredRight, lit ? ff::ink : ff::faint, 11.0f);
    t.removeFromTop (4);
    ff::label (g, String ("TAP  ") + taps[sw], t.removeFromTop (18), Justification::centredLeft, ff::dim, 12.0f);
    ff::label (g, String ("HOLD  ") + holds[sw], t.removeFromTop (18), Justification::centredLeft, ff::faint, 12.0f);
}

ExpressionBar::ExpressionBar (GrottoProcessor& p) : proc (p)
{
    target = p.apvts.getParameter (ParamIDs::expTarget);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void ExpressionBar::tick()
{
    const int t = roundToInt (raw (proc, ParamIDs::expTarget));
    const float v = proc.expressionLevel();
    if (t != lastTarget || std::abs (v - level) > 0.002f) { lastTarget = t; level = v; repaint(); }
    else if (proc.isCalibrating() || proc.isLearning()) repaint();
}

void ExpressionBar::paint (Graphics& g)
{
    const auto& t = names::expTargets();
    const int which = jlimit (0, (int) t.size() - 1, roundToInt (raw (proc, ParamIDs::expTarget)));
    String head = "EXPRESSION";
    if (proc.isCalibrating()) head = "ROCK HEEL TO TOE";
    else if (proc.learningWhat() == GrottoProcessor::LearnExpression) head = "MOVE THE PEDAL";
    ff::label (g, head, { 0, 0, 120, 18 }, Justification::centredLeft, proc.isCalibrating() ? ff::ink : ff::dim, 11.0f);
    ff::label (g, String (t[(size_t) which].label).toUpperCase() + "  v", { 100, 0, 160, 18 }, Justification::centredLeft, ff::teal, 11.0f);

    const float x0 = 10.0f, w = (float) getWidth() - 20.0f, y = 34.0f;
    g.setColour (ff::card);
    g.fillRoundedRectangle (x0 - 10.0f, y - 9.0f, w + 20.0f, 18.0f, 9.0f);
    g.setColour (ff::pin);
    g.fillRoundedRectangle (x0 - 10.0f, y - 9.0f, jmax (18.0f, (w + 20.0f) * level), 18.0f, 9.0f);
    const bool fresh = nowSeconds() - proc.lastExpressionMove() < 1.5;
    g.setColour (fresh ? ff::teal : ff::ink);
    g.fillEllipse (x0 + w * level - 7.0f, y - 7.0f, 14.0f, 14.0f);

    ff::label (g, "HEEL", { 0, 46, 60, 14 }, Justification::centredLeft, ff::faint, 10.0f);
    ff::label (g, "TOE", { getWidth() - 60, 46, 60, 14 }, Justification::centredRight, ff::faint, 10.0f);
    const String note = which == names::ExpDrag ? "HEEL SCATTERS AND DIVES" + dot + "EVEN IN PITCH"
                      : which == names::ExpReflect ? "MOST OF THE TRAVEL NEAR THE EDGE" : "CLICK TO CALIBRATE";
    ff::label (g, note, { 40, 46, getWidth() - 80, 14 }, Justification::centred, ff::faint, 10.0f);
    pill (g, Rectangle<float> (0.0f, 64.0f, (float) getWidth(), 24.0f).withSizeKeepingCentre (176.0f, 24.0f),
          proc.isCalibrating() ? "ROCK IT NOW" : "CALIBRATE HEEL AND TOE", proc.isCalibrating(), 11.0f, ff::teal);
}

void ExpressionBar::mouseDown (const MouseEvent& e)
{
    proc.focusHere();
    if (e.position.y > 62.0f && std::abs (e.position.x - (float) getWidth() * 0.5f) < 90.0f) { proc.calibrateExpression(); return; }
    PopupMenu targets;
    const auto& t = names::expTargets();
    const int cur = roundToInt (raw (proc, ParamIDs::expTarget));
    for (int i = 0; i < (int) t.size(); ++i) targets.addItem (100 + i, t[(size_t) i].label, true, i == cur);
    PopupMenu curves;
    const char* const curveNames[3] = { "Linear", "Slow start", "Fast start" };
    for (int i = 0; i < 3; ++i) curves.addItem (200 + i, curveNames[i], true, proc.pedals().curve == i);

    PopupMenu m;
    m.addSubMenu ("Expression moves", targets);
    m.addItem (1, "Calibrate heel and toe");
    m.addItem (2, "Learn expression pedal");
    m.addSubMenu ("Curve", curves);
    m.addItem (3, "Min, max and smoothing on the Pedal page");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this] (int r)
    {
        if (r == 1) proc.calibrateExpression();
        else if (r == 2) proc.learnExpression();
        else if (r == 3) { if (showPedalPage) showPedalPage(); }
        else if (r >= 100 && r < 200) setParam (target, (float) (r - 100));
        else if (r >= 200 && r < 300) proc.setCurve (r - 200);
        repaint();
    });
}

// =====================================================================================
// Pedal page
// =====================================================================================
FootswitchCard::FootswitchCard (GrottoProcessor& p, int s)
    : proc (p), sw (s),
      learn ("LEARN", [this] { proc.learnFootswitch (sw); }, 11.0f),
      reset ("DEFAULTS", [this] { proc.resetFootswitches(); }, 11.0f)
{
    letter.setTextHeight (40.0f);
    letter.setValue (String::charToString ((juce_wchar) ('A' + s)));
    addAndMakeVisible (letter);
    addAndMakeVisible (learn);
    addAndMakeVisible (reset);
}

void FootswitchCard::resized()
{
    letter.setBounds (12, 28, getWidth() - 24, 62);
    learn.setBounds (12, getHeight() - 38, (getWidth() - 30) / 2, 28);
    reset.setBounds (18 + (getWidth() - 30) / 2, getHeight() - 38, (getWidth() - 30) / 2, 28);
}

void FootswitchCard::paint (Graphics& g)
{
    g.setColour (ff::face);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 12.0f);
    ff::label (g, "FOOTSWITCH", { 12, 8, 200, 16 }, Justification::centredLeft, ff::dim, 11.0f);

    static const char* const taps[4] = { "BYPASS", "RUNAWAY", "FREEZE", "DRY KILL" };
    static const char* const holds[4] = { "ON WHILE HELD", "WHILE HELD", "WHILE HELD", "WHILE HELD" };
    static const char* const notes[4] = { "THE CAVE RINGS OUT", "REFLECT PAST THE EDGE", "PLAY OVER THE CAVE", "WET-ONLY WASH" };
    static const char* const combos[4] = { "A+B  TAME", "A+B  TAME", "C+D  NEXT COPY", "C+D  NEXT COPY" };
    struct Row { const char* k; String v; };
    const Row rows[3] = { { "TAP", taps[sw] }, { "HOLD", holds[sw] }, { "HEARS", sends } };
    const int w = getWidth() - 24;
    int y = 98;
    for (const auto& r : rows)
    {
        g.setColour (ff::ring);
        g.fillRect (12, y, w, 2);
        ff::label (g, r.k, { 12, y + 3, 60, 26 }, Justification::centredLeft, ff::dim, 11.0f);
        ff::label (g, r.v, { 12, y + 3, w, 26 }, Justification::centredRight, ff::ink, 12.0f);
        y += 30;
    }
    g.setColour (ff::ring);
    g.fillRect (12, y, w, 2);
    ff::label (g, notes[sw], { 12, y + 6, w, 14 }, Justification::centredLeft, ff::dim, 10.5f);
    ff::label (g, combos[sw], { 12, y + 24, w, 14 }, Justification::centredLeft, ff::dim, 10.5f);
    if (sw >= 2) ff::label (g, "C+D HELD 1 S  KEEPER", { 12, y + 42, w, 14 }, Justification::centredLeft, ff::dim, 10.5f);
}

void FootswitchCard::tick (double dt)
{
    const auto& s = proc.pedals();
    const String t = "CH " + String (s.fsChannel[(size_t) sw]) + dot + "CC " + String (s.fsCC[(size_t) sw]);
    if (t != sends) { sends = t; repaint(); }
    learn.setLit (proc.learningWhat() == sw);
    learn.setText (proc.learningWhat() == sw ? "PRESS IT" : "LEARN");
    const bool down = (proc.uiFootDown.load() >> sw) & 1;
    letter.setAlpha (down ? 0.6f : 1.0f);
    letter.advance (dt);
}

// =====================================================================================
// The cave and Pebble
// =====================================================================================
Cave::Cave (GrottoProcessor& p) : proc (p)
{
    Random rnd (7);
    for (int i = 0; i < 16; ++i) rocks.push_back ({ (float) i / 15.0f, 18.0f + rnd.nextFloat() * 46.0f, 14.0f + rnd.nextFloat() * 22.0f });
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void Cave::mouseDown (const MouseEvent&)
{
    proc.focusHere();
    spawn (now);   // a ping into the cave, to see the echoes at these settings
}

void Cave::spawn (double t)
{
    // each note sends a run of echoes round the cave: more with Length and Reflect,
    // further apart (and swapping sides) as Drag scatters the swarm
    const float g1 = jlimit (0.3f, 0.97f, 0.5f + 0.25f * raw (proc, ParamIDs::length) + 0.3f * jmin (1.0f, proc.getEngine().uiReflect.load()));
    const double gap = 0.06 + 0.3 * scatter;
    for (int k = 1; k <= 12; ++k)
        echoes.push_back ({ t + k * gap, k, (k & 1) ? -1.0f : 1.0f, std::pow (g1, (float) k), 0.0f });
    if (echoes.size() > 240) echoes.erase (echoes.begin(), echoes.begin() + (long) (echoes.size() - 240));
}

void Cave::update (double t, double dt)
{
    now = t;
    auto& eng = proc.getEngine();
    asleep = isOn (proc, ParamIDs::bypass);
    const bool fr = eng.uiFrozen.load();
    if (fr && ! frozen) frozenAt = t;
    frozen = fr;
    ghost = isOn (proc, ParamIDs::dryKill) && ! asleep;
    stretch = eng.uiStretch.load();
    scatter = std::log (stretch) / std::log (gr::kMaxStretch);
    bend += (eng.uiBend.load() - bend) * (float) jmin (1.0, dt * 10.0);
    wet = eng.uiWet.load();
    input = eng.uiInput.load();
    const float sc = asleep ? 0.0f : jmax (eng.uiRunaway.load(), jlimit (0.0f, 1.0f, (eng.uiReflect.load() - 0.95f) * 8.0f));
    scream += (sc - scream) * (float) jmin (1.0, dt * 6.0);

    if (eng.uiOnsets.load() != lastOnset) { lastOnset = eng.uiOnsets.load(); if (! asleep && ! frozen) spawn (t); mouth = 1.0f; }
    if (scream > 0.5f && ! frozen && Random::getSystemRandom().nextFloat() < (float) dt * 3.0f) spawn (t);   // the runaway keeps feeding the cave

    // echoes slide down as Drag stretches the swarm and the pitch falls, up as it tightens
    if (! frozen)
        for (auto& e : echoes)
            if (t >= e.born) e.y += -bend * 3.0f * (float) dt * (float) e.k * 0.6f;

    const double life = 1.2 + gr::lengthSeconds (raw (proc, ParamIDs::length)) * 0.3;
    if (! frozen && scream < 0.5f)
        echoes.erase (std::remove_if (echoes.begin(), echoes.end(), [t, life] (const Echo& e) { return t - e.born > life + 1.0; }), echoes.end());

    const float leanTarget = t - proc.lastExpressionMove() < 1.5 ? (proc.expressionLevel() - 0.5f) * 0.3f : 0.0f;
    lean += (leanTarget - lean) * (float) jmin (1.0, dt * 6.0);
    if (proc.uiFootPulse.load() != lastFoot) { lastFoot = proc.uiFootPulse.load(); tap = 1.0f; }
    if (proc.uiKeepPulse.load() != lastKeep) { lastKeep = proc.uiKeepPulse.load(); nod = 1.0f; }
    if (proc.uiTamePulse.load() != lastTame) { lastTame = proc.uiTamePulse.load(); flinch = 1.0f; echoes.clear(); }
    tap = jmax (0.0f, tap - (float) dt * 5.0f);
    nod = jmax (0.0f, nod - (float) dt * 0.8f);
    flinch = jmax (0.0f, flinch - (float) dt * 2.0f);
    mouth = jmax (0.0f, mouth - (float) dt * 1.5f);

    // Pebble stretches wide as Drag scatters the swarm, and more while Drag is moving
    const float stretchGoal = 1.0f + scatter * 0.35f + jlimit (0.0f, 0.5f, std::abs (bend) * 0.02f);
    pebStretch += (stretchGoal - pebStretch) * (float) jmin (1.0, dt * 8.0);

    if (t > nextBlink) { blinking = true; blinkUntil = t + 0.12; nextBlink = t + 2.0 + Random::getSystemRandom().nextDouble() * 3.0; }
    if (blinking && t > blinkUntil) blinking = false;
    repaint();
}

String Cave::mood() const
{
    const double ago = nowSeconds() - proc.uiMessageAt;
    if (ago < 2.5 && proc.uiMessage.isNotEmpty()) return proc.uiMessage;
    if (asleep) return "ASLEEP";
    if (flinch > 0.3f) return "TAMED";
    if (scream > 0.5f) return "SCREAMING";
    if (frozen) return "FROZEN";
    if (ghost) return "WET ONLY";
    if (std::abs (bend) > 2.0f) return "WARPING";
    if (wet > 0.01f) return "ECHOING";
    return "LISTENING";
}

void Cave::drawPebble (Graphics& g, Point<float> c, float s, float sx, float sy, float tilt, Colour body, Colour shade,
                       float alpha, bool withFace, bool asGhost) const
{
    // Pebble from Flip Fella, drawn in its 200 unit space
    const auto tf = AffineTransform::translation (-100.0f, -100.0f).scaled (s * sx, s * sy).rotated (tilt).translated (c);
    auto ell = [&] (float cx, float cy, float rx, float ry, Colour col)
    {
        Path p; p.addEllipse (cx - rx, cy - ry, rx * 2, ry * 2);
        g.setColour (col.withMultipliedAlpha (alpha)); g.fillPath (p, tf);
    };
    auto stroke = [&] (Path& p, float w)
    {
        g.setColour (ff::faceInk.withMultipliedAlpha (alpha));
        g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded), tf);
    };
    if (asGhost)
    {
        Path p; p.addEllipse (42, 52, 116, 96);
        Path dashed;
        const float dashes[2] = { 10.0f, 8.0f };
        PathStrokeType (5.0f).createDashedStroke (dashed, p, dashes, 2, tf);
        g.setColour (body.withMultipliedAlpha (alpha));
        g.fillPath (dashed);
    }
    else
    {
        ell (107, 109, 58, 48, shade);
        ell (100, 100, 58, 48, body);
    }
    if (! withFace) return;

    const float ey = 92.0f;
    if (asleep || blinking)
    {
        for (float ex : { 80.0f, 120.0f }) { Path lid; lid.startNewSubPath (ex - 12, ey + 2); lid.quadraticTo (ex, ey + 10, ex + 12, ey + 2); stroke (lid, 6.0f); }
    }
    else if (frozen && scream < 0.5f)   // holding its breath: eyes screwed shut, cheeks puffed
    {
        for (float ex : { 80.0f, 120.0f }) { Path lid; lid.startNewSubPath (ex - 12, ey + 6); lid.quadraticTo (ex, ey - 8, ex + 12, ey + 6); stroke (lid, 6.0f); }
        ell (66, 112, 9, 7, Colours::white.withAlpha (0.18f));
        ell (134, 112, 9, 7, Colours::white.withAlpha (0.18f));
    }
    else
    {
        const float er = 14.0f + 3.0f * scream, pr = 7.0f - 2.0f * scream;
        const float look = jlimit (-1.0f, 1.0f, lean * 4.0f);
        for (float ex : { 80.0f, 120.0f }) { ell (ex, ey, er, er, ff::eye); ell (ex + look * 5.0f, ey + 2, pr, pr, ff::faceInk); }
    }

    const float my = 120.0f, t = (float) now;
    if (asleep) { Path m; m.startNewSubPath (94, my + 4); m.lineTo (106, my + 4); stroke (m, 6.0f); }
    else if (scream > 0.3f) ell (100, my + 4, 8.0f + 5.0f * scream, 5.0f + 11.0f * scream * (0.8f + 0.2f * std::sin (t * 13.0f)), ff::faceInk);
    else if (frozen) ell (100, my + 4, 6.0f, 6.0f, ff::faceInk);
    else if (mouth > 0.15f) ell (100, my + 4, 7.0f + 3.0f * mouth, 4.0f + 6.0f * mouth, ff::faceInk);
    else { Path m; m.startNewSubPath (86, my); m.quadraticTo (100, my + 10, 114, my); stroke (m, 6.0f); }
}

void Cave::paint (Graphics& g)
{
    const auto a = getLocalBounds().toFloat();
    const float t = (float) now;
    const float quake = scream > 0.3f ? scream * 1.6f * (std::sin (t * 61.0f) + std::sin (t * 37.0f)) : 0.0f;

    Graphics::ScopedSaveState save (g);
    Path clip; clip.addRoundedRectangle (a, 10.0f);
    g.reduceClipRegion (clip);
    g.setColour (ff::face);
    g.fillRect (a);
    const Colour glow = scream > 0.3f ? ff::red.withAlpha (0.18f + 0.1f * std::sin (t * 9.0f))
                      : frozen ? ff::ice.withAlpha (0.12f) : ff::teal.withAlpha (0.08f + jmin (0.12f, wet * 2.0f));
    g.setGradientFill (ColourGradient (glow, a.getCentreX(), a.getY() + a.getHeight() * 0.62f,
                                       Colours::transparentBlack, a.getCentreX() + a.getWidth() * 0.75f, a.getY() + a.getHeight() * 0.62f, true));
    g.fillRect (a);

    // stalactites and floor
    g.setColour (ff::cardLow);
    for (const auto& r : rocks)
    {
        const float rx = a.getX() + r.x * a.getWidth() + quake;
        Path p; p.addTriangle (rx - r.w, a.getY() - 2.0f, rx + r.w, a.getY() - 2.0f, rx + 2.0f, a.getY() + r.h);
        g.fillPath (p);
    }
    g.setColour (ff::card);
    Path floor;
    floor.startNewSubPath (a.getX(), a.getBottom());
    floor.quadraticTo (a.getX() + a.getWidth() * 0.25f, a.getBottom() - 46.0f, a.getCentreX(), a.getBottom() - 30.0f);
    floor.quadraticTo (a.getX() + a.getWidth() * 0.78f, a.getBottom() - 18.0f, a.getRight(), a.getBottom() - 52.0f);
    floor.lineTo (a.getRight(), a.getBottom());
    floor.closeSubPath();
    g.fillPath (floor);

    // the echoes
    const Point<float> c (a.getCentreX() + quake, a.getY() + a.getHeight() * 0.56f);
    const float spread = 10.0f + 16.0f * scatter;
    const double life = 1.2 + gr::lengthSeconds (raw (proc, ParamIDs::length)) * 0.3;
    const Colour echoCol = frozen ? ff::ice : (scream > 0.3f ? ff::red : ff::teal);
    for (const auto& e : echoes)
    {
        if (now < e.born) continue;
        double age = now - e.born;
        if (frozen) age = jmin (age, jmax (0.0, frozenAt - e.born));
        const float decay = scream > 0.5f ? 1.0f : (float) jmax (0.0, 1.0 - age / life);
        const float alpha = jmin (1.0f, e.amp * decay);
        if (alpha < 0.02f) continue;
        const Point<float> p (c.x + e.side * (float) e.k * spread * (0.4f + scatter), c.y + e.y);
        drawPebble (g, p, 0.5f * std::pow (0.93f, (float) e.k), 1.0f + scatter * 0.5f, 1.0f - scatter * 0.15f, 0.0f,
                    echoCol, Colours::transparentBlack, alpha, false, false);
    }

    // Pebble
    const float shake = (scream > 0.3f ? std::sin (t * 47.0f) * 0.07f * scream : 0.0f) + flinch * std::sin (t * 40.0f) * 0.15f;
    const float bob = asleep ? 0.0f : std::sin (t * (scream > 0.3f ? 9.0f : 2.4f)) * (scream > 0.3f ? 2.5f : 1.5f);
    const float nodDip = nod * 10.0f * std::sin ((1.0f - nod) * 12.0f);
    const float dy = bob - tap * 6.0f + jmax (0.0f, nodDip);
    const float breathe = asleep ? 1.0f + 0.025f * std::sin (t * 1.6f) : 1.0f;
    drawPebble (g, { c.x + (scream > 0.3f ? std::sin (t * 53.0f) * 2.5f * scream : 0.0f), c.y + dy }, 0.78f,
                pebStretch * breathe, 1.0f / std::sqrt (pebStretch) / breathe, lean + shake,
                ff::blob, ff::blobShade, 1.0f, true, ghost);

    if (asleep)
    {
        g.setFont (ff::font (16.0f, 0.0f));
        for (int k = 0; k < 3; ++k)
        {
            const float ph = (float) std::fmod (now * 0.5 + k / 3.0, 1.0);
            g.setColour (ff::ink.withAlpha (1.0f - ph));
            g.drawText ("z", Rectangle<float> (c.x + 50.0f + ph * 10.0f, c.y - 50.0f - ph * 22.0f, 16.0f, 16.0f), Justification::centred, false);
        }
    }
    ff::label (g, "CAVE" + dot + "CLICK TO PING IT", getLocalBounds().reduced (12, 8).removeFromBottom (14), Justification::centredLeft, ff::faint, 10.5f);
}

// =====================================================================================
// Editor
// =====================================================================================
GrottoEditor::GrottoEditor (GrottoProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      mainTab ("MAIN", [this] { setPage (0); }),
      deepTab ("DEEP", [this] { setPage (1); }),
      pedalTab ("PEDAL", [this] { setPage (2); }),
      keepButton ("KEEP", [this] { proc.keepNow(); }),
      keepMenu ("v", [this] { keeperMenu(); }),
      cave (p),
      dragDial (p),
      length (p, ParamIDs::length, "Length"),
      diffuse (p, ParamIDs::diffuse, "Diffuse"),
      dampen (p, ParamIDs::dampen, "Dampen"),
      reflect (p, ParamIDs::reflect, "Reflect", true),
      mix (p, ParamIDs::mix, "Mix"),
      lowCut (p, ParamIDs::lowCut, "Low cut"),
      footInput (p),
      expression (p),
      bassChip (p.apvts, ParamIDs::bass, "BASS OFF", "BASS ON", 12.0f, ff::teal),
      monoChip (p.apvts, ParamIDs::mono, "STEREO", "MONO", 12.0f),
      trailsChip (p.apvts, ParamIDs::trails, "CUT TAILS", "TRAILS", 12.0f),
      tameChip ("TAME", [this] { proc.tame(); }, 12.0f),
      liveChipDeep ("STUDIO", [this] { proc.setKeeperLive (! proc.keeperLive()); }, 12.0f),
      expMin (p.apvts, ParamIDs::expMin, "Heel sets"),
      expMax (p.apvts, ParamIDs::expMax, "Toe sets"),
      expSmooth (p.apvts, ParamIDs::expSmooth, "Smoothing"),
      calibrate ("CALIBRATE", [this] { proc.calibrateExpression(); }, 11.0f),
      learnExp ("LEARN", [this] { proc.learnExpression(); }, 11.0f),
      curve ("CURVE", [this] { proc.setCurve ((proc.pedals().curve + 1) % 3); }, 11.0f),
      liveChip ("STUDIO", [this] { proc.setKeeperLive (! proc.keeperLive()); }, 11.0f),
      learnKeep ("LEARN SWITCH", [this] { proc.learnKeeper(); }, 11.0f),
      exportKeep ("EXPORT", [this] { proc.exportKeepers(); }, 11.0f),
      folderKeep ("FOLDER", [this] { proc.openKeepersFolder(); }, 11.0f)
{
    setLookAndFeel (&lnf);
    keepButton.setColour (ff::green);

    for (auto* c : std::initializer_list<Component*> { &mainTab, &deepTab, &pedalTab, &keepButton, &keepMenu,
                                                       &cave, &dragDial, &length, &diffuse, &dampen, &reflect, &mix, &lowCut,
                                                       &footInput, &expression,
                                                       &bassChip, &monoChip, &trailsChip, &tameChip, &liveChipDeep,
                                                       &expMin, &expMax, &expSmooth, &calibrate, &learnExp, &curve,
                                                       &liveChip, &learnKeep, &exportKeep, &folderKeep })
        addAndMakeVisible (c);
    for (auto& c : cards) { c.setTextHeight (24.0f); addAndMakeVisible (c); }
    expression.showPedalPage = [this] { setPage (2); };

    for (int i = 0; i < 4; ++i)
    {
        feet[(size_t) i] = std::make_unique<FootButton> (p, i);
        addAndMakeVisible (*feet[(size_t) i]);
        footCards[(size_t) i] = std::make_unique<FootswitchCard> (p, i);
        addChildComponent (*footCards[(size_t) i]);
    }
    for (int m = 0; m < gr::NumModes; ++m)
    {
        modeChips[(size_t) m] = std::make_unique<ActionChip> (gr::modeName (m),
            [this, m] { setParam (proc.apvts.getParameter (ParamIDs::mode), (float) m); }, 11.0f);
        modeChips[(size_t) m]->setColour (ff::teal);
        addChildComponent (*modeChips[(size_t) m]);
    }
    const auto& t = names::expTargets();
    for (int i = 0; i < 6; ++i)
    {
        targetChips[(size_t) i] = std::make_unique<ActionChip> (String (t[(size_t) i].label).toUpperCase(),
            [this, i] { setParam (proc.apvts.getParameter (ParamIDs::expTarget), (float) i); }, 10.5f);
        targetChips[(size_t) i]->setColour (ff::teal);
        addChildComponent (*targetChips[(size_t) i]);
    }

    noteEditor.setFont (ff::font (14.0f));
    noteEditor.setColour (TextEditor::backgroundColourId, ff::card);
    noteEditor.setColour (TextEditor::textColourId, ff::ink);
    noteEditor.setColour (TextEditor::outlineColourId, ff::line);
    noteEditor.setTextToShowWhenEmpty ("A NOTE FOR THIS KEEPER, THEN ENTER", ff::dim);
    noteEditor.onReturnKey = [this] { closeNote (true); };
    noteEditor.onEscapeKey = [this] { closeNote (false); };
    addChildComponent (noteEditor);

    setPage (0);
    setSize (960, 640);
    start = lastTick = nowSeconds();
    startTimerHz (30);
}

GrottoEditor::~GrottoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GrottoEditor::setPage (int p)
{
    page = p;
    for (auto* c : std::initializer_list<Component*> { &cave, &dragDial, &length, &diffuse, &dampen, &reflect, &mix, &lowCut })
        c->setVisible (p == 0);
    for (auto* c : std::initializer_list<Component*> { &bassChip, &monoChip, &trailsChip, &tameChip, &liveChipDeep })
        c->setVisible (p == 1);
    for (auto& c : modeChips) c->setVisible (p == 1);
    for (auto& c : footCards) c->setVisible (p == 2);
    for (auto& c : targetChips) c->setVisible (p == 2);
    for (auto* c : std::initializer_list<Component*> { &expMin, &expMax, &expSmooth, &calibrate, &learnExp, &curve,
                                                       &liveChip, &learnKeep, &exportKeep, &folderKeep })
        c->setVisible (p == 2);
    mainTab.setLit (p == 0);
    deepTab.setLit (p == 1);
    pedalTab.setLit (p == 2);
    repaint();
}

void GrottoEditor::keeperMenu()
{
    PopupMenu m;
    m.addItem (1, "Add a note to Keeper No. " + String (proc.lastKeeper()), proc.lastKeeper() > 0);
    m.addItem (2, "Export for Claude");
    m.addItem (3, "Open the Keepers folder");
    m.addSeparator();
    m.addItem (5, "Live", true, proc.keeperLive());
    m.addItem (6, "Studio", true, ! proc.keeperLive());
    m.addSeparator();
    m.addItem (4, "Learn a Keeper footswitch");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&keepMenu), [this] (int r)
    {
        if (r == 1)
        {
            noteNumber = proc.lastKeeper();
            noteEditor.clear();
            noteEditor.setVisible (true);
            noteEditor.grabKeyboardFocus();
            noteUntil = nowSeconds() + 20.0;
        }
        else if (r == 2) proc.exportKeepers();
        else if (r == 3) proc.openKeepersFolder();
        else if (r == 4) proc.learnKeeper();
        else if (r == 5 || r == 6) proc.setKeeperLive (r == 5);
    });
}

void GrottoEditor::closeNote (bool save)
{
    if (save) proc.addKeeperNote (noteNumber, noteEditor.getText());
    if (save && noteEditor.getText().trim().isNotEmpty())
    {
        proc.uiMessage = "NOTED";
        proc.uiMessageAt = nowSeconds();
    }
    noteEditor.setVisible (false);
    noteEditor.clear();
    noteUntil = 0.0;
}

void GrottoEditor::section (Graphics& g, Rectangle<int> r) const
{
    g.setColour (ff::face);
    g.fillRoundedRectangle (r.toFloat(), 12.0f);
}

void GrottoEditor::paint (Graphics& g)
{
    g.fillAll (ff::bg);
    g.setColour (ff::ink);
    g.setFont (ff::font (34.0f, 0.1f));
    g.drawText ("GROTTO", 28, 18, 150, 44, Justification::centredLeft, false);
    ff::label (g, "CAVERNOUS REVERB", { 178, 30, 200, 24 }, Justification::centredLeft, ff::faint, 12.0f);

    static const char* const cardLabels[6] = { "LENGTH", "DRAG", "MODE", "REFLECT", "MIX", "PEBBLE" };
    for (int i = 0; i < 6; ++i)
        ff::label (g, cardLabels[i], cards[(size_t) i].getBounds().translated (2, -18).withHeight (16), Justification::centredLeft, ff::faint, 11.0f);

    // the footswitch strip
    ff::label (g, "A+B TAME" + dot + "C+D NEXT COPY" + dot + "HOLD C+D 1 S KEEPER", { 220, 500, 400, 20 }, Justification::centredLeft, ff::faint, 11.0f);

    if (page == 1)
    {
        section (g, { 20, 172, 450, 318 });
        section (g, { 490, 172, 450, 318 });
        ff::label (g, "FOR BASS AND THE LIVE RIG", { 36, 182, 400, 18 });
        ff::label (g, "DRAG MODE" + dot + "HOW DRAG AND THE EXPRESSION PEDAL MOVE", { 506, 182, 420, 18 });
        auto note = [&g] (int y, const String& a, const String& b)
        {
            ff::label (g, a, { 176, y, 280, 16 }, Justification::centredLeft, ff::ink, 12.0f);
            ff::label (g, b, { 176, y + 17, 280, 16 }, Justification::centredLeft, ff::dim, 11.0f);
        };
        note (214, "REVERB LOW CUT AT LEAST 120 HZ", "DRY KILL KEEPS YOUR FUNDAMENTAL");
        note (270, "MONO IN AND OUT, AS THE PEDAL", "STEREO SPREADS THE SWARM");
        note (326, "THE CAVE RINGS OUT WHEN BYPASSED", "CUT TAILS STOPS IT DEAD");
        note (382, "KILL REFLECT AND THE TAIL NOW", "SAME AS FOOTSWITCHES A+B");
        note (438, "KEEPERS ARE TAGGED LIVE OR STUDIO", "REMEMBERED BY EVERY PLUGIN");
        static const char* const modeNotes[gr::NumModes] = {
            "THE KNOB MOVES THE DELAY TIME EVENLY, LIKE THE ORIGINAL",
            "THE SAME, GLIDING SLOWLY BETWEEN SETTINGS",
            "EQUAL TRAVEL GIVES EQUAL PITCH CHANGE",
            "STEPS IN SEMITONES", "STEPS IN A MAJOR SCALE", "STEPS IN A MINOR SCALE",
            "STEPS IN A PENTATONIC SCALE", "STEPS IN OCTAVES AND FIFTHS", "STEPS IN OCTAVES" };
        const int m = roundToInt (raw (proc, ParamIDs::mode));
        ff::label (g, modeNotes[jlimit (0, gr::NumModes - 1, m)], { 506, 336, 420, 18 }, Justification::centredLeft, ff::ink, 12.0f);
        ff::label (g, "THE PEDAL TURNS DRAG ON THE TEAL STEPS. THE EXPRESSION", { 506, 366, 420, 16 }, Justification::centredLeft, ff::dim, 11.0f);
        ff::label (g, "PEDAL ALWAYS SWEEPS EVENLY IN PITCH, WHATEVER THE MODE.", { 506, 382, 420, 16 }, Justification::centredLeft, ff::dim, 11.0f);
        ff::label (g, "RIGHT-CLICK THE MODE CHIP UNDER DRAG FOR THIS LIST TOO.", { 506, 410, 420, 16 }, Justification::centredLeft, ff::dim, 11.0f);
    }
    else if (page == 2)
    {
        section (g, { 700, 172, 240, 204 });
        section (g, { 700, 386, 240, 104 });
        ff::label (g, "EXPRESSION MOVES", { 712, 178, 200, 18 }, Justification::centredLeft, ff::dim, 11.0f);
        ff::label (g, "KEEPER", { 712, 390, 100, 18 }, Justification::centredLeft, ff::dim, 11.0f);
        ff::label (g, "LAST No. " + String (proc.lastKeeper()), { 800, 390, 128, 18 }, Justification::centredRight, ff::ink, 11.0f);
    }
}

void GrottoEditor::resized()
{
    // header
    mainTab.setBounds (560, 24, 64, 30);
    deepTab.setBounds (630, 24, 64, 30);
    pedalTab.setBounds (700, 24, 72, 30);
    keepButton.setBounds (796, 24, 100, 30);
    keepMenu.setBounds (900, 24, 32, 30);

    // the flip card row
    const int xs[6] = { 22, 166, 310, 496, 640, 784 }, ws[6] = { 138, 138, 180, 138, 138, 154 };
    for (int i = 0; i < 6; ++i) cards[(size_t) i].setBounds (xs[i], 92, ws[i], 62);

    // main
    cave.setBounds (28, 172, 340, 318);
    dragDial.setBounds (386, 172, 260, 318);
    const int kx[2] = { 654, 796 };
    Dial* dials[6] = { &length, &diffuse, &dampen, &reflect, &mix, &lowCut };
    for (int i = 0; i < 6; ++i) dials[i]->setBounds (kx[i % 2], 172 + (i / 2) * 106, 140, 106);
    noteEditor.setBounds (40, 456, 316, 28);

    // strip
    footInput.setBounds (28, 500, 190, 20);
    for (int i = 0; i < 4; ++i) feet[(size_t) i]->setBounds (28 + i * 152, 526, 144, 92);
    expression.setBounds (652, 526, 280, 92);

    // deep page
    bassChip.setBounds (36, 214, 124, 32);
    monoChip.setBounds (36, 270, 124, 32);
    trailsChip.setBounds (36, 326, 124, 32);
    tameChip.setBounds (36, 382, 124, 32);
    liveChipDeep.setBounds (36, 438, 124, 32);
    for (int m = 0; m < gr::NumModes; ++m) modeChips[(size_t) m]->setBounds (506 + (m % 3) * 140, 214 + (m / 3) * 38, 132, 30);

    // pedal page
    for (int i = 0; i < 4; ++i) footCards[(size_t) i]->setBounds (20 + i * 170, 172, 160, 318);
    for (int i = 0; i < 6; ++i) targetChips[(size_t) i]->setBounds (712 + (i % 3) * 74, 200 + (i / 3) * 30, 70, 26);
    expMin.setBounds (712, 256, 216, 28);
    expMax.setBounds (712, 284, 216, 28);
    expSmooth.setBounds (712, 312, 216, 28);
    calibrate.setBounds (712, 348, 70, 24);
    learnExp.setBounds (785, 348, 70, 24);
    curve.setBounds (858, 348, 70, 24);
    liveChip.setBounds (712, 414, 106, 30);
    learnKeep.setBounds (822, 414, 106, 30);
    exportKeep.setBounds (712, 450, 106, 30);
    folderKeep.setBounds (822, 450, 106, 30);
}

void GrottoEditor::timerCallback()
{
    const double now = nowSeconds();
    const double dt = jlimit (0.0, 0.2, now - lastTick);
    lastTick = now;

    for (auto& f : feet) f->tick();
    expression.tick();
    footInput.repaint();

    const bool kept = proc.uiMessage == "KEPT" && now - proc.uiMessageAt < 1.5;
    keepButton.setLit (kept);
    keepButton.setText (kept ? "KEPT" : (proc.learningWhat() == GrottoProcessor::LearnKeeper ? "PRESS IT" : "KEEP"));
    if (noteUntil > 0.0 && now > noteUntil) closeNote (true);

    // the flip card row
    auto& eng = proc.getEngine();
    const int mode = roundToInt (raw (proc, ParamIDs::mode));
    const bool scream = eng.uiRunaway.load() > 0.05f || eng.uiReflect.load() >= 0.98f;
    const float lenS = gr::lengthSeconds (raw (proc, ParamIDs::length));
    cards[0].setValue (String (lenS, lenS < 10.0f ? 1 : 0) + " S");
    cards[1].setValue (String (roundToInt (gr::Swarm::meanMs (eng.uiStretch.load()))) + " MS");
    cards[2].setValue (gr::modeName (mode));
    cards[3].setValue (isOn (proc, ParamIDs::runaway) ? "RUN AWAY" : scream ? "SINGING" : String (roundToInt (raw (proc, ParamIDs::reflect) * 100.0f)) + "%");
    cards[3].setInk (scream ? ff::red : ff::ink);
    cards[4].setValue (isOn (proc, ParamIDs::dryKill) ? "WET" : String (roundToInt (raw (proc, ParamIDs::mix) * 100.0f)) + "%");
    cards[5].setValue (cave.mood());
    cards[5].setInk (scream ? ff::red : ff::ink);
    for (auto& c : cards) c.advance (dt);

    cave.update (now - start, dt);   // keeps Pebble's mood current on every page
    if (page == 0)
    {
        dragDial.tick();
    }
    else if (page == 1)
    {
        for (int m = 0; m < gr::NumModes; ++m) modeChips[(size_t) m]->setLit (m == mode);
        liveChipDeep.setText (proc.keeperLive() ? "LIVE" : "STUDIO");
        repaint (500, 330, 430, 30);
    }
    else
    {
        for (auto& c : footCards) c->tick (dt);
        const int t = roundToInt (raw (proc, ParamIDs::expTarget));
        for (int i = 0; i < 6; ++i) targetChips[(size_t) i]->setLit (i == t);
        const char* const curveNames[3] = { "LINEAR", "SLOW", "FAST" };
        curve.setText (curveNames[jlimit (0, 2, proc.pedals().curve)]);
        calibrate.setLit (proc.isCalibrating());
        calibrate.setText (proc.isCalibrating() ? "ROCK IT" : "CALIBRATE");
        learnExp.setLit (proc.learningWhat() == GrottoProcessor::LearnExpression);
        liveChip.setText (proc.keeperLive() ? "LIVE" : "STUDIO");
        learnKeep.setLit (proc.learningWhat() == GrottoProcessor::LearnKeeper);
        repaint (700, 386, 240, 30);
    }
}
} // namespace ui
