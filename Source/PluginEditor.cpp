#include "PluginEditor.h"

namespace brvui
{
using namespace juce;

Font font (float height, bool bold, float kerning)
{
    Font f (FontOptions (height, bold ? Font::bold : Font::plain));
    return kerning != 0.0f ? f.withExtraKerningFactor (kerning) : f;
}

static void glow (Graphics& g, Point<float> c, float rx, float ry, Colour col)
{
    ColourGradient gr (col, c.x, c.y, col.withAlpha (0.0f), c.x + rx, c.y, true);
    g.setGradientFill (gr);
    g.saveState();
    g.addTransform (AffineTransform::scale (1.0f, ry / rx, c.x, c.y));
    g.fillEllipse (c.x - rx, c.y - rx, rx * 2, rx * 2);
    g.restoreState();
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (PopupMenu::backgroundColourId, Colours::field);
    setColour (PopupMenu::textColourId, Colours::text);
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xFF3A2766));
    setColour (PopupMenu::highlightedTextColourId, Colours::text);
    setColour (ComboBox::textColourId, Colours::text);
    setColour (ComboBox::arrowColourId, Colours::textDim);
    setColour (TooltipWindow::backgroundColourId, Colours::field);
    setColour (TooltipWindow::textColourId, Colours::text);
}

void LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider&)
{
    const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);
    const float th = jmax (2.5f, r * 0.085f);

    // track
    Path track;
    track.addCentredArc (c.x, c.y, r - th, r - th, 0.0f, a0, a1, true);
    g.setColour (Colour (0xFF29223A));
    g.strokePath (track, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));

    // value arc + glow
    Path val;
    val.addCentredArc (c.x, c.y, r - th, r - th, 0.0f, a0, ang, true);
    g.setColour (Colours::accent.withAlpha (0.18f));
    g.strokePath (val, PathStrokeType (th * 3.2f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setGradientFill (ColourGradient (Colours::accentLo, c.x - r, c.y + r, Colours::accentHi, c.x + r, c.y - r, false));
    g.strokePath (val, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));

    // body
    const float br = r - th * 2.6f;
    g.setColour (Colours::bg.withAlpha (0.7f));
    g.fillEllipse (c.x - br - 1, c.y - br + 2, (br + 1) * 2, (br + 1) * 2);
    g.setGradientFill (ColourGradient (Colour (0xFF2B2438), c.x, c.y - br, Colour (0xFF120F18), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.16f), c.x, c.y - br, Colours::white.withAlpha (0.02f), c.x, c.y + br, false));
    g.drawEllipse (c.x - br, c.y - br, br * 2, br * 2, 1.0f);

    // pointer
    const float p0 = br * 0.28f, p1 = br * 0.82f;
    Line<float> ln (c.getPointOnCircumference (p0, ang), c.getPointOnCircumference (p1, ang));
    g.setColour (Colours::accentHi.withAlpha (0.25f));
    g.drawLine (ln, 5.0f);
    g.setColour (Colours::white);
    g.drawLine (ln, 2.0f);
}

void LookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto b = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (Colours::field);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (box.isMouseOver (true) ? Colours::accent.withAlpha (0.55f) : Colours::stroke);
    g.drawRoundedRectangle (b, 6.0f, 1.0f);

    const float cx = (float) w - 13.0f, cy = (float) h * 0.5f;
    Path arrow;
    arrow.startNewSubPath (cx - 3.5f, cy - 1.5f);
    arrow.lineTo (cx, cy + 2.0f);
    arrow.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (Colours::textDim);
    g.strokePath (arrow, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
}

Font LookAndFeel::getComboBoxFont (ComboBox& b) { return font (jmin (12.0f, b.getHeight() * 0.5f), true, 0.04f); }

void LookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (Justification::centred);
}

Font LookAndFeel::getPopupMenuFont() { return font (13.0f); }

void LookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (Colours::field);
    g.setColour (Colours::stroke);
    g.drawRect (0, 0, w, h);
}

void LookAndFeel::drawToggleButton (Graphics& g, ToggleButton& t, bool highlighted, bool)
{
    const auto area = t.getLocalBounds().toFloat();
    auto b = Rectangle<float> (40.0f, 20.0f).withCentre (area.getCentre());
    const bool on = t.getToggleState();
    const float rr = b.getHeight() * 0.5f;

    if (on)
    {
        g.setColour (Colours::accent.withAlpha (0.25f));
        g.fillRoundedRectangle (b.expanded (2.5f), rr + 2.5f);
        g.setGradientFill (ColourGradient (Colours::accentLo, b.getX(), b.getY(), Colours::accent, b.getRight(), b.getBottom(), false));
    }
    else
        g.setColour (Colour (0xFF1D1827));
    g.fillRoundedRectangle (b, rr);
    g.setColour (on ? Colours::accentHi.withAlpha (0.6f) : (highlighted ? Colours::accent.withAlpha (0.5f) : Colours::stroke));
    g.drawRoundedRectangle (b, rr, 1.0f);

    const float d = b.getHeight() - 6.0f;
    const auto knob = Rectangle<float> (d, d).withCentre ({ on ? b.getRight() - rr : b.getX() + rr, b.getCentreY() });
    g.setColour (on ? Colours::white : Colour (0xFF8C849E));
    g.fillEllipse (knob);

    g.setFont (font (7.5f, true, 0.08f));
    g.setColour (on ? Colours::white : Colours::textDim);
    g.drawText (on ? "ON" : "OFF",
                on ? b.withTrimmedRight (d + 4).withTrimmedLeft (3) : b.withTrimmedLeft (d + 5).withTrimmedRight (3),
                Justification::centred);
}

//==============================================================================
Knob::Knob (AudioProcessorValueTreeState& s, const String& id, const String& t, int dec, bool show)
    : title (t), decimals (dec), showTitle (show)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.setPopupDisplayEnabled (false, false, nullptr);
}

void Knob::resized()
{
    auto b = getLocalBounds();
    if (showTitle) b.removeFromTop (22);
    b.removeFromBottom (18);
    const int d = jmin (b.getWidth(), b.getHeight());
    slider.setBounds (b.withSizeKeepingCentre (d, d));
}

void Knob::paint (Graphics& g)
{
    if (showTitle)
    {
        g.setColour (Colours::text.withAlpha (0.85f));
        g.setFont (font (10.5f, true, 0.14f));
        g.drawText (title, getLocalBounds().removeFromTop (18), Justification::centred);
    }
    g.setColour (Colours::text.withAlpha (0.9f));
    g.setFont (font (11.5f, true, 0.02f));
    g.drawText (String (slider.getValue(), decimals), getLocalBounds().removeFromBottom (16), Justification::centred);
}

//==============================================================================
SourceSwitch::SourceSwitch (RangedAudioParameter& p)
    : att (p, [this] (float v) { value = roundToInt (v); repaint(); })
{
    att.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void SourceSwitch::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    const float rr = b.getHeight() * 0.5f;
    g.setColour (Colours::field);
    g.fillRoundedRectangle (b, rr);
    g.setColour (Colours::stroke);
    g.drawRoundedRectangle (b, rr, 1.0f);

    auto seg = b.reduced (2.5f);
    auto left = seg.removeFromLeft (seg.getWidth() * 0.42f), right = seg;
    auto sel = value == 0 ? left : right;
    g.setColour (Colours::accent.withAlpha (0.3f));
    g.fillRoundedRectangle (sel.expanded (1.5f), sel.getHeight() * 0.5f + 1.5f);
    g.setGradientFill (ColourGradient (Colours::accent, sel.getX(), sel.getY(), Colours::accentLo, sel.getRight(), sel.getBottom(), false));
    g.fillRoundedRectangle (sel, sel.getHeight() * 0.5f);

    g.setFont (font (10.5f, true, 0.1f));
    g.setColour (value == 0 ? Colours::white : Colours::textDim);
    g.drawText ("VOCAL", left, Justification::centred);
    g.setColour (value == 1 ? Colours::white : Colours::textDim);
    g.drawText ("INSTRUMENT", right, Justification::centred);
}

void SourceSwitch::mouseDown (const MouseEvent& e)
{
    const int v = e.position.x < getWidth() * 0.42f ? 0 : 1;
    if (v != value) att.setValueAsCompleteGesture ((float) v);
}

//==============================================================================
PadButton::PadButton (const String& text, Icon i, bool p) : Button (text), icon (i), primary (p)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void PadButton::drawIcon (Graphics& g, Icon icon, Rectangle<float> r, Colour c)
{
    g.setColour (c);
    const auto cx = r.getCentreX(), cy = r.getCentreY();
    switch (icon)
    {
        case Icon::Mic:
        {
            g.fillRoundedRectangle (cx - 3.0f, cy - 7.0f, 6.0f, 10.0f, 3.0f);
            Path p;
            p.addCentredArc (cx, cy - 1.0f, 5.5f, 5.5f, 0.0f, MathConstants<float>::pi * 0.5f, MathConstants<float>::pi * 1.5f, true);
            g.strokePath (p, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
            g.drawLine (cx, cy + 4.5f, cx, cy + 7.0f, 1.4f);
            g.drawLine (cx - 3.0f, cy + 7.0f, cx + 3.0f, cy + 7.0f, 1.4f);
            break;
        }
        case Icon::Play:
        {
            Path p;
            p.addTriangle (cx - 3.5f, cy - 5.0f, cx - 3.5f, cy + 5.0f, cx + 5.0f, cy);
            g.fillPath (p);
            break;
        }
        case Icon::Stop:
            g.fillRoundedRectangle (cx - 4.5f, cy - 4.5f, 9.0f, 9.0f, 1.5f);
            break;
        case Icon::Download:
        {
            g.drawLine (cx, cy - 7.0f, cx, cy + 2.5f, 1.5f);
            Path p;
            p.startNewSubPath (cx - 4.0f, cy - 1.5f);
            p.lineTo (cx, cy + 2.5f);
            p.lineTo (cx + 4.0f, cy - 1.5f);
            g.strokePath (p, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
            g.drawLine (cx - 6.0f, cy + 6.5f, cx + 6.0f, cy + 6.5f, 1.5f);
            break;
        }
        case Icon::None: break;
    }
}

void PadButton::paintButton (Graphics& g, bool hi, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (4.0f);
    if (down) b = b.reduced (1.0f);
    const float rr = 9.0f;
    Colour txt;

    if (primary)
    {
        const float gl = 0.25f + 0.25f * pulse + (hi ? 0.1f : 0.0f);
        g.setColour (Colours::accent.withAlpha (gl));
        g.fillRoundedRectangle (b.expanded (4.0f), rr + 4.0f);
        g.setGradientFill (ColourGradient (Colours::accentHi.interpolatedWith (Colours::white, 0.25f + 0.2f * pulse), b.getX(), b.getY(),
                                           Colour (0xFFB08CFF), b.getRight(), b.getBottom(), false));
        g.fillRoundedRectangle (b, rr);
        g.setColour (Colours::white.withAlpha (0.55f));
        g.drawRoundedRectangle (b, rr, 1.0f);
        txt = Colour (0xFF1C1034);
    }
    else
    {
        g.setGradientFill (ColourGradient (Colour (0xFF1F1A2A), b.getX(), b.getY(), Colour (0xFF15111D), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, rr);
        if (active)
        {
            g.setColour (Colours::accent.withAlpha (0.18f));
            g.fillRoundedRectangle (b, rr);
        }
        g.setColour (active ? Colours::accent.withAlpha (0.8f) : (hi ? Colours::accent.withAlpha (0.45f) : Colours::stroke));
        g.drawRoundedRectangle (b, rr, 1.0f);
        txt = (active || hi) ? Colours::text : Colour (0xFFB8B1C8);
        if (! isEnabled()) txt = txt.withAlpha (0.4f);
    }

    g.setFont (font (11.5f, true, 0.12f));
    const auto text = getButtonText();
    const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), text);
    const float total = (icon != Icon::None ? 22.0f : 0.0f) + tw;
    float x = b.getCentreX() - total * 0.5f;
    if (icon != Icon::None)
    {
        drawIcon (g, icon, { x, b.getY(), 16.0f, b.getHeight() }, txt);
        x += 22.0f;
    }
    g.setColour (txt);
    g.drawText (text, Rectangle<float> (x, b.getY(), tw + 4.0f, b.getHeight()), Justification::centredLeft);
}

//==============================================================================
DragButton::DragButton (BackrverbProcessor& p) : PadButton ("DRAG TO DAW", Icon::Download, false), proc (p)
{
    setTooltip ("Drag onto the FL Studio playlist (or click to save the WAV)");
}

File DragButton::exportsFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("BACKRVERB Exports");
}

File DragButton::makeExport()
{
    auto f = exportsFolder().getChildFile (proc.suggestedFileName());
    return proc.exportRender (f) ? f : File();
}

void DragButton::mouseDown (const MouseEvent& e)
{
    dragging = false;
    PadButton::mouseDown (e);
}

void DragButton::mouseDrag (const MouseEvent& e)
{
    PadButton::mouseDrag (e);
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    dragging = true;

    if (! proc.hasCapture()) { if (onStatus) onStatus ("Nothing to drag yet - capture a sound first"); return; }
    auto f = makeExport();
    if (! f.existsAsFile()) { if (onStatus) onStatus ("Export failed"); return; }

    if (onStatus) onStatus ("Drop it on the playlist: " + f.getFileName());
    DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
}

void DragButton::clicked()
{
    if (dragging) { dragging = false; return; }
    if (! proc.hasCapture()) { if (onStatus) onStatus ("Nothing to export yet - capture a sound first"); return; }
    auto f = makeExport();
    if (f.existsAsFile())
    {
        if (onStatus) onStatus ("Saved " + f.getFileName());
        f.revealToUser();
    }
    else if (onStatus) onStatus ("Export failed");
}

//==============================================================================
void GearButton::paintButton (Graphics& g, bool hi, bool)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float ro = 8.0f, ri = 6.0f;
    Path p;
    for (int i = 0; i < 8; ++i)
    {
        Path tooth;
        tooth.addRoundedRectangle (c.x - 1.8f, c.y - ro - 1.5f, 3.6f, 4.5f, 0.8f);
        tooth.applyTransform (AffineTransform::rotation (MathConstants<float>::twoPi * i / 8.0f, c.x, c.y));
        p.addPath (tooth);
    }
    p.addEllipse (c.x - ri - 0.5f, c.y - ri - 0.5f, (ri + 0.5f) * 2, (ri + 0.5f) * 2);
    g.setColour (hi ? Colours::text : Colours::textDim.brighter (0.2f));
    g.fillPath (p);
    g.setColour (Colours::panelTop);
    g.fillEllipse (c.x - 2.8f, c.y - 2.8f, 5.6f, 5.6f);
}

//==============================================================================
void Display::refresh()
{
    anim += 0.035f;
    if (statusTicks > 0) --statusTicks;
    if (proc.getRenderVersion() != data.version) data = proc.getDisplayData();
    repaint();
}

void Display::drawWaves (Graphics& g, Rectangle<float> r)
{
    const float cy = r.getCentreY();
    for (int k = 0; k < 4; ++k)
    {
        Path p;
        const float amp = r.getHeight() * (0.10f + 0.05f * k);
        const float freq = 1.6f + 0.55f * k;
        for (float x = r.getX(); x <= r.getRight(); x += 3.0f)
        {
            const float t = (x - r.getX()) / r.getWidth();
            const float env = std::exp (-std::pow ((t - 0.5f) * 2.6f, 2.0f));
            const float y = cy + amp * env * std::sin (t * freq * MathConstants<float>::twoPi + anim * (0.6f + 0.25f * k) + k * 1.3f);
            if (x == r.getX()) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setGradientFill (ColourGradient (Colours::accent.withAlpha (0.0f), r.getX(), cy,
                                           Colours::accent.withAlpha (0.0f), r.getRight(), cy, false));
        ColourGradient cg (Colours::accent.withAlpha (0.0f), r.getX(), cy, Colours::accent.withAlpha (0.0f), r.getRight(), cy, false);
        cg.addColour (0.5, (k % 2 ? Colours::accentHi : Colours::accent).withAlpha (0.42f - 0.07f * k));
        g.setGradientFill (cg);
        g.strokePath (p, PathStrokeType (1.3f));
    }
}

void Display::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    const float rr = 14.0f;
    g.setGradientFill (ColourGradient (Colour (0xFF15101F), b.getX(), b.getY(), Colours::well, b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, rr);
    {
        Graphics::ScopedSaveState ss (g);
        Path clip; clip.addRoundedRectangle (b, rr);
        g.reduceClipRegion (clip);
        glow (g, { b.getCentreX(), b.getY() }, b.getWidth() * 0.35f, 14.0f, Colours::accent.withAlpha (0.45f));
        glow (g, b.getCentre(), b.getWidth() * 0.45f, b.getHeight() * 0.45f, Colours::accentLo.withAlpha (0.10f));
    }
    g.setColour (Colours::stroke);
    g.drawRoundedRectangle (b, rr, 1.0f);

    if (proc.getRenderVersion() != data.version) data = proc.getDisplayData();
    const int state = proc.getCaptureState();
    const bool has = proc.hasCapture() && data.swellLen > 0;
    auto inner = b.reduced (18.0f, 14.0f);

    // ---- status pill
    String pill; Colour dot = Colours::textDim;
    if (state == BackrverbProcessor::Armed)          { pill = "ARMED - PLAY YOUR SOUND"; dot = Colours::accentHi.withAlpha (0.5f + 0.5f * std::abs (std::sin (anim * 3))); }
    else if (state == BackrverbProcessor::Recording) { pill = "RECORDING"; dot = Colour (0xFFFF5C7A); }
    else if (proc.isRendering())                     { pill = "RENDERING"; dot = Colours::accent; }
    else if (has)                                    { pill = "CAPTURED"; dot = Colour (0xFF7CF2C2); }
    else                                             { pill = "NO CAPTURE"; }
    {
        g.setFont (font (9.0f, true, 0.1f));
        const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), pill);
        auto pr = Rectangle<float> (inner.getX(), inner.getY(), tw + 30.0f, 20.0f);
        g.setColour (Colours::field.brighter (0.05f));
        g.fillRoundedRectangle (pr, 10.0f);
        g.setColour (Colours::stroke);
        g.drawRoundedRectangle (pr, 10.0f, 1.0f);
        g.setColour (dot);
        g.fillEllipse (pr.getX() + 9.0f, pr.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (Colours::text.withAlpha (0.85f));
        g.drawText (pill, pr.withTrimmedLeft (20.0f), Justification::centredLeft);
    }

    if (statusTicks > 0 && status.isNotEmpty())
    {
        g.setFont (font (10.0f, false));
        g.setColour (Colours::accentHi.withAlpha (jmin (1.0f, statusTicks / 20.0f)));
        g.drawText (status, inner.withTrimmedLeft (inner.getWidth() * 0.35f).removeFromTop (20.0f), Justification::centredRight);
    }

    // ---- live input meter while capturing
    if (state == BackrverbProcessor::Armed || state == BackrverbProcessor::Recording)
    {
        const float lv = jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (proc.getInputLevel(), -60.0f) + 60.0f) / 60.0f);
        auto m = Rectangle<float> (inner.getX(), inner.getBottom() - 4.0f, inner.getWidth(), 3.0f);
        g.setColour (Colour (0xFF231C30));
        g.fillRoundedRectangle (m, 1.5f);
        g.setGradientFill (ColourGradient (Colours::accentLo, m.getX(), 0, Colours::accentHi, m.getRight(), 0, false));
        g.fillRoundedRectangle (m.withWidth (m.getWidth() * lv), 1.5f);
    }

    if (! has)
    {
        auto wr = inner.withTrimmedTop (18.0f).withTrimmedBottom (26.0f);
        drawWaves (g, wr);

        // centre icon
        const auto c = wr.getCentre().translated (0, -6.0f);
        const float hs[] = { 12.0f, 22.0f, 34.0f, 22.0f, 12.0f };
        glow (g, c, 40.0f, 30.0f, Colours::accent.withAlpha (0.35f));
        for (int i = 0; i < 5; ++i)
        {
            const float x = c.x - 14.0f + i * 7.0f;
            g.setGradientFill (ColourGradient (Colours::accentHi, x, c.y - hs[i] * 0.5f, Colours::accent, x, c.y + hs[i] * 0.5f, false));
            g.fillRoundedRectangle (x - 1.6f, c.y - hs[i] * 0.5f, 3.2f, hs[i], 1.6f);
        }

        AttributedString as;
        as.setJustification (Justification::centred);
        const auto dim = Colours::text.withAlpha (0.75f);
        as.append ("Press ", font (11.5f), dim);
        as.append ("CAPTURE", font (11.5f, true, 0.06f), Colours::text);
        as.append (state == BackrverbProcessor::Idle ? ", then play the sound" : " again to cancel", font (11.5f), dim);
        as.draw (g, Rectangle<float> (inner.getX(), inner.getBottom() - 30.0f, inner.getWidth(), 18.0f));
        return;
    }

    // ---- waveform: swell -> hit -> dry
    auto wr = inner.withTrimmedTop (26.0f).withTrimmedBottom (8.0f);
    const float total = (float) (data.swellLen + data.dryLen);
    const float split = wr.getX() + wr.getWidth() * (float) data.swellLen / total;
    const float cy = wr.getCentreY(), hh = wr.getHeight() * 0.5f;

    auto drawPeaks = [&] (const std::vector<float>& pk, float x0, float x1, Colour top, Colour bottom, float alpha)
    {
        if (pk.empty() || x1 - x0 < 2.0f) return;
        Path p;
        const int n = (int) pk.size();
        const int steps = jmax (2, (int) (x1 - x0));
        p.startNewSubPath (x0, cy);
        for (int i = 0; i <= steps; ++i)
        {
            const float v = pk[(size_t) jmin (n - 1, i * n / steps)];
            p.lineTo (x0 + (float) i, cy - v * hh);
        }
        for (int i = steps; i >= 0; --i)
        {
            const float v = pk[(size_t) jmin (n - 1, i * n / steps)];
            p.lineTo (x0 + (float) i, cy + v * hh);
        }
        p.closeSubPath();
        g.setGradientFill (ColourGradient (top.withAlpha (alpha), x0, cy - hh, bottom.withAlpha (alpha), x0, cy + hh, false));
        g.fillPath (p);
        g.setColour (top.withAlpha (alpha * 0.5f));
        g.strokePath (p, PathStrokeType (1.0f));
    };

    glow (g, { split, cy }, wr.getWidth() * 0.25f, hh * 1.1f, Colours::accent.withAlpha (0.25f));
    drawPeaks (data.swellPeaks, wr.getX(), split, Colours::accentHi, Colours::accentLo, 0.95f);
    drawPeaks (data.dryPeaks, split, wr.getRight(), Colour (0xFF9C95B0), Colour (0xFF4C455E), 0.6f);

    g.setColour (Colours::white.withAlpha (0.8f));
    g.drawLine (split, wr.getY() - 4.0f, split, wr.getBottom() + 2.0f, 1.0f);
    g.setFont (font (8.5f, true, 0.12f));
    g.setColour (Colours::text.withAlpha (0.7f));
    g.drawText ("HIT", Rectangle<float> (split + 4.0f, wr.getY() - 6.0f, 30.0f, 12.0f), Justification::centredLeft);
    g.drawText ("SWELL", Rectangle<float> (wr.getX(), wr.getBottom() - 4.0f, 60.0f, 12.0f), Justification::centredLeft);

    // playhead
    const int mode = proc.getPlayMode();
    if (mode != BackrverbProcessor::Stopped)
    {
        const float span = mode == BackrverbProcessor::PlaySwell ? (float) data.swellLen : total;
        const float x = wr.getX() + wr.getWidth() * proc.getPlayProgress() * span / total;
        g.setColour (Colours::accentHi.withAlpha (0.3f));
        g.fillRect (x - 3.0f, wr.getY(), 6.0f, wr.getHeight());
        g.setColour (Colours::white);
        g.fillRect (x - 0.75f, wr.getY(), 1.5f, wr.getHeight());
    }
}

//==============================================================================
MainPanel::MainPanel (BackrverbProcessor& p)
    : proc (p),
      source (*p.apvts.getParameter (ParamID::source)),
      display (p),
      dragBtn (p),
      size (p.apvts, ParamID::size, "SIZE", 2),
      tone (p.apvts, ParamID::tone, "TONE", 2),
      width (p.apvts, ParamID::width, "WIDTH", 2),
      tremolo (p.apvts, ParamID::tremolo, "TREMOLO", 2),
      output (p.apvts, ParamID::output, "OUTPUT", 1, false)
{
    setLookAndFeel (&laf);
    setSize (W, H);

    for (Component* c : std::initializer_list<Component*> { &source, &display, &captureBtn, &playBtn, &previewBtn, &dragBtn, &gear,
                                                            &lengthBox, &tremRateBox, &size, &tone, &width, &tremolo, &output,
                                                            &tremOn, &lowCut })
        addAndMakeVisible (c);

    lengthBox.addItemList (brv::lengthNames(), 1);
    tremRateBox.addItemList (brv::tremRateNames(), 1);
    lengthAtt   = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamID::length, lengthBox);
    tremRateAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamID::tremRate, tremRateBox);
    tremOnAtt   = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamID::tremOn, tremOn);
    lowCutAtt   = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamID::lowCut, lowCut);

    captureBtn.onClick = [this] { proc.toggleCapture(); };
    playBtn.onClick = [this]
    {
        proc.requestPlay (proc.getPlayMode() == BackrverbProcessor::PlaySwell ? BackrverbProcessor::Stopped : BackrverbProcessor::PlaySwell);
    };
    previewBtn.onClick = [this]
    {
        proc.requestPlay (proc.getPlayMode() == BackrverbProcessor::PlayPreview ? BackrverbProcessor::Stopped : BackrverbProcessor::PlayPreview);
    };
    dragBtn.onStatus = [this] (const String& s) { display.setStatus (s); };
    gear.onClick = [this] { showSettings(); };
    gear.setMouseCursor (MouseCursor::PointingHandCursor);

    captureBtn.setTooltip ("Arm capture: BACKRVERB records the next sound that comes in");
    playBtn.setTooltip ("Play the reverse-reverb swell");
    previewBtn.setTooltip ("Play the swell followed by the captured sound");

    startTimerHz (30);
}

MainPanel::~MainPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MainPanel::resized()
{
    display.setBounds (36, 146, 568, 168);
    source.setBounds (102, 108, 176, 26);
    gear.setBounds (566, 50, 30, 30);

    const int by = 326, bh = 46, bw = 142;
    captureBtn.setBounds (34, by, bw, bh);
    playBtn.setBounds (34 + bw, by, bw, bh);
    previewBtn.setBounds (34 + bw * 2, by, bw, bh);
    dragBtn.setBounds (34 + bw * 3, by, bw, bh);

    // controls row: LENGTH | SIZE | TONE | WIDTH | TREMOLO | LOW CUT
    const int x0 = 36, top = 394;
    lengthBox.setBounds (x0 + 10, top + 36, 88, 26);
    size.setBounds  (x0 + 108, top, 84, 96);
    tone.setBounds  (x0 + 192, top, 84, 96);
    width.setBounds (x0 + 276, top, 84, 96);
    tremolo.setBounds (x0 + 368, top, 104, 82);
    tremRateBox.setBounds (x0 + 388, top + 84, 64, 22);
    tremOn.setBounds (x0 + 396, top + 110, 48, 24);
    lowCut.setBounds (x0 + 486, top + 34, 70, 30);

    output.setBounds (W / 2 - 40, 556, 80, 86);
}

void MainPanel::paint (Graphics& g)
{
    g.fillAll (Colours::bg);
    glow (g, { W * 0.5f, H * 0.5f }, W * 0.6f, H * 0.55f, Colours::accentLo.withAlpha (0.10f));

    // main glass panel
    const auto pr = Rectangle<float> (14.0f, 14.0f, W - 28.0f, H - 28.0f);
    g.setColour (Colours::accent.withAlpha (0.08f));
    g.fillRoundedRectangle (pr.expanded (3.0f), 26.0f);
    g.setGradientFill (ColourGradient (Colours::panelTop, pr.getX(), pr.getY(), Colours::panel, pr.getX(), pr.getY() + 260.0f, false));
    g.fillRoundedRectangle (pr, 24.0f);
    {
        Graphics::ScopedSaveState ss (g);
        Path clip; clip.addRoundedRectangle (pr, 24.0f);
        g.reduceClipRegion (clip);
        glow (g, { pr.getX() + 40.0f, pr.getY() }, 260.0f, 120.0f, Colours::accent.withAlpha (0.22f));
        glow (g, { pr.getRight() - 120.0f, pr.getY() + 20.0f }, 220.0f, 60.0f, Colours::accentLo.withAlpha (0.18f));
        glow (g, { W * 0.5f, pr.getBottom() }, 300.0f, 70.0f, Colours::accent.withAlpha (0.20f));

        // light streaks
        for (int i = 0; i < 3; ++i)
        {
            Path s;
            const float y = 40.0f + i * 10.0f;
            s.startNewSubPath (pr.getX() + 180.0f, y + 20.0f);
            s.cubicTo (pr.getX() + 320.0f, y - 10.0f, pr.getX() + 440.0f, y + 25.0f, pr.getRight(), y - 5.0f);
            ColourGradient cg (Colours::accent.withAlpha (0.0f), pr.getX() + 180.0f, 0, Colours::accent.withAlpha (0.0f), pr.getRight(), 0, false);
            cg.addColour (0.55, Colours::accentHi.withAlpha (0.10f - 0.025f * i));
            g.setGradientFill (cg);
            g.strokePath (s, PathStrokeType (1.0f));
        }
    }
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f), pr.getX(), pr.getY(), Colours::accent.withAlpha (0.12f), pr.getX(), pr.getBottom(), false));
    g.drawRoundedRectangle (pr, 24.0f, 1.0f);

    // ---- logo
    {
        const float lx = 44.0f, ly = 62.0f;
        const float hs[] = { 34.0f, 28.0f, 20.0f, 14.0f };
        glow (g, { lx + 10.0f, ly }, 30.0f, 26.0f, Colours::accent.withAlpha (0.35f));
        for (int i = 0; i < 4; ++i)
        {
            const float x = lx + i * 7.0f;
            g.setGradientFill (ColourGradient (Colours::accentHi, x, ly - hs[i] * 0.5f, Colours::accentLo, x, ly + hs[i] * 0.5f, false));
            g.fillRoundedRectangle (x, ly - hs[i] * 0.5f, 3.6f, hs[i], 1.8f);
        }
        g.setColour (Colours::text);
        g.setFont (font (31.0f, true, 0.32f));
        g.drawText ("BACKRVERB", Rectangle<float> (90.0f, 38.0f, 330.0f, 34.0f), Justification::centredLeft);
        g.setColour (Colours::textDim.brighter (0.25f));
        g.setFont (font (9.5f, true, 0.32f));
        g.drawText ("REVERSE REVERB. FASTER.", Rectangle<float> (92.0f, 72.0f, 300.0f, 14.0f), Justification::centredLeft);
    }

    // ---- BPM / TIME SIG
    auto box = [&] (float x, const String& label, const String& value)
    {
        g.setFont (font (8.0f, true, 0.14f));
        g.setColour (Colours::textDim);
        g.drawText (label, Rectangle<float> (x, 38.0f, 66.0f, 12.0f), Justification::centred);
        auto r = Rectangle<float> (x, 52.0f, 66.0f, 26.0f);
        g.setColour (Colours::field);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Colours::stroke);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        g.setColour (Colours::text);
        g.setFont (font (12.0f, true, 0.04f));
        g.drawText (value, r, Justification::centred);
    };
    box (410.0f, "BPM", bpmText);
    box (488.0f, "TIME SIG", tsText);

    // ---- source label
    g.setFont (font (9.0f, true, 0.14f));
    g.setColour (Colours::textDim.brighter (0.2f));
    g.drawText ("SOURCE", Rectangle<float> (40.0f, 108.0f, 60.0f, 26.0f), Justification::centredLeft);

    // ---- separator + column titles/dividers
    {
        ColourGradient sep (Colours::stroke.withAlpha (0.0f), 36.0f, 0, Colours::stroke.withAlpha (0.0f), W - 36.0f, 0, false);
        sep.addColour (0.5, Colours::accent.withAlpha (0.35f));
        g.setGradientFill (sep);
        g.fillRect (36.0f, 384.0f, W - 72.0f, 1.0f);

        g.setColour (Colours::text.withAlpha (0.85f));
        g.setFont (font (10.5f, true, 0.14f));
        g.drawText ("LENGTH", Rectangle<float> (36.0f, 394.0f, 108.0f, 18.0f), Justification::centred);
        g.drawText ("LOW CUT", Rectangle<float> (36.0f + 476.0f, 394.0f, 92.0f, 18.0f), Justification::centred);

        g.setColour (Colours::stroke.withAlpha (0.12f));
        for (float x : { 36.0f + 108.0f, 36.0f + 360.0f, 36.0f + 476.0f })
            g.fillRect (x, 398.0f, 1.0f, 120.0f);
        for (float x : { 36.0f + 192.0f, 36.0f + 276.0f })
            g.fillRect (x, 398.0f, 1.0f, 86.0f);
    }

    // ---- output tab (notch at the bottom)
    {
        const float tw = 200.0f, tx = W * 0.5f - tw * 0.5f, ty = 534.0f, bottom = pr.getBottom();
        Path tab;
        tab.startNewSubPath (tx - 30.0f, bottom);
        tab.cubicTo (tx - 6.0f, bottom, tx + 4.0f, ty, tx + 30.0f, ty);
        tab.lineTo (tx + tw - 30.0f, ty);
        tab.cubicTo (tx + tw - 4.0f, ty, tx + tw + 6.0f, bottom, tx + tw + 30.0f, bottom);
        tab.closeSubPath();
        g.setGradientFill (ColourGradient (Colour (0xFF1C1529), W * 0.5f, ty, Colour (0xFF120E1A), W * 0.5f, bottom, false));
        g.fillPath (tab);
        g.setColour (Colours::accent.withAlpha (0.35f));
        g.strokePath (tab, PathStrokeType (1.0f));

        g.setColour (Colours::text.withAlpha (0.85f));
        g.setFont (font (10.5f, true, 0.18f));
        g.drawText ("OUTPUT", Rectangle<float> (W * 0.5f - 50.0f, 540.0f, 100.0f, 14.0f), Justification::centred);
    }

    // ---- footer
    g.setFont (font (8.0f, true, 0.14f));
    g.setColour (Colours::textDim.withAlpha (0.6f));
    g.drawText ("V1.0", Rectangle<float> (40.0f, H - 44.0f, 80.0f, 12.0f), Justification::centredLeft);
    g.drawText ("TURN TIME AROUND.", Rectangle<float> (W - 240.0f, H - 44.0f, 200.0f, 12.0f), Justification::centredRight);
}

void MainPanel::timerCallback()
{
    phase += 0.12f;

    const String b = proc.hostProvidesBpm() ? String (proc.getHostBpm(), proc.getHostBpm() == std::floor (proc.getHostBpm()) ? 0 : 1) : String ("--");
    const String ts = String (proc.getTsNum()) + "/" + String (proc.getTsDen());
    if (b != bpmText || ts != tsText) { bpmText = b; tsText = ts; repaint (400, 36, 160, 46); }

    const int st = proc.getCaptureState();
    if (st == BackrverbProcessor::Armed)          { captureBtn.setButtonText ("ARMED");  captureBtn.setIcon (PadButton::Icon::Mic);  captureBtn.setPulse (0.5f + 0.5f * std::sin (phase * 2.0f)); }
    else if (st == BackrverbProcessor::Recording) { captureBtn.setButtonText ("STOP");   captureBtn.setIcon (PadButton::Icon::Stop); captureBtn.setPulse (1.0f); }
    else                                          { captureBtn.setButtonText ("CAPTURE"); captureBtn.setIcon (PadButton::Icon::Mic); captureBtn.setPulse (0.0f); }

    const int pm = proc.getPlayMode();
    const bool has = proc.hasCapture();
    playBtn.setEnabled (has);
    previewBtn.setEnabled (has);
    playBtn.setActive (pm == BackrverbProcessor::PlaySwell);
    previewBtn.setActive (pm == BackrverbProcessor::PlayPreview);
    playBtn.setButtonText (pm == BackrverbProcessor::PlaySwell ? "STOP" : "PLAY");
    playBtn.setIcon (pm == BackrverbProcessor::PlaySwell ? PadButton::Icon::Stop : PadButton::Icon::Play);
    previewBtn.setButtonText (pm == BackrverbProcessor::PlayPreview ? "STOP" : "PREVIEW");
    previewBtn.setIcon (pm == BackrverbProcessor::PlayPreview ? PadButton::Icon::Stop : PadButton::Icon::Play);

    display.refresh();
}

void MainPanel::showSettings()
{
    PopupMenu m;
    m.setLookAndFeel (&laf);
    m.addItem (1, "Normalize render (-1 dBFS)", true, proc.normalize.load());
    m.addItem (2, "Auto-stop capture on silence", true, proc.autoStop.load());
    m.addSeparator();
    m.addItem (3, "Open exports folder");
    m.addItem (4, "Clear capture", proc.hasCapture());
    m.addSeparator();
    m.addItem (5, "BACKRVERB v1.0", false);

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&gear),
                     [this] (int r)
                     {
                         if (r == 1) proc.normalize.store (! proc.normalize.load());
                         if (r == 2) proc.autoStop.store (! proc.autoStop.load());
                         if (r == 3) { auto f = DragButton::exportsFolder(); f.createDirectory(); f.startAsProcess(); }
                         if (r == 4) proc.clearCapture();
                     });
}

} // namespace brvui

//==============================================================================
BackrverbEditor::BackrverbEditor (BackrverbProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) brvui::MainPanel::W / brvui::MainPanel::H);
    setResizeLimits (brvui::MainPanel::W * 7 / 10, brvui::MainPanel::H * 7 / 10,
                     brvui::MainPanel::W * 8 / 5, brvui::MainPanel::H * 8 / 5);
    setSize (brvui::MainPanel::W * 92 / 100, brvui::MainPanel::H * 92 / 100);
}

void BackrverbEditor::paint (juce::Graphics& g) { g.fillAll (brvui::Colours::bg); }

void BackrverbEditor::resized()
{
    const float s = (float) getWidth() / (float) brvui::MainPanel::W;
    panel.setTransform (juce::AffineTransform::scale (s));
    panel.setBounds (0, 0, brvui::MainPanel::W, brvui::MainPanel::H);
}
