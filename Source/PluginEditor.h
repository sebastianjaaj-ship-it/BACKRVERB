#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace brvui
{
namespace Colours
{
    const juce::Colour bg        { 0xFF07060B };
    const juce::Colour panel     { 0xFF110D18 };
    const juce::Colour panelTop  { 0xFF1B1428 };
    const juce::Colour well      { 0xFF0D0A13 };
    const juce::Colour field     { 0xFF16121E };
    const juce::Colour accent    { 0xFFA979FF };
    const juce::Colour accentHi  { 0xFFD9C7FF };
    const juce::Colour accentLo  { 0xFF6D3FE0 };
    const juce::Colour text      { 0xFFEDE8F7 };
    const juce::Colour textDim   { 0xFF8E87A3 };
    const juce::Colour stroke    { 0x2EB9A2FF };
    const juce::Colour white     { 0xFFFFFFFF };
}

juce::Font font (float height, bool bold = false, float kerning = 0.0f);

//==============================================================================
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& title,
          int decimals, bool showTitle = true);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String title;
    int decimals;
    bool showTitle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

//==============================================================================
class SourceSwitch : public juce::Component
{
public:
    explicit SourceSwitch (juce::RangedAudioParameter&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    int value = 0;
    juce::ParameterAttachment att;
};

//==============================================================================
class PadButton : public juce::Button
{
public:
    enum class Icon { Mic, Play, Stop, Download, None };
    PadButton (const juce::String& text, Icon icon, bool primary);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void setIcon (Icon i)          { icon = i; repaint(); }
    void setActive (bool a)        { if (a != active) { active = a; repaint(); } }
    void setPulse (float p)        { pulse = p; repaint(); }
    static void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float>, juce::Colour);
protected:
    Icon icon;
    bool primary, active = false;
    float pulse = 0.0f;
};

class DragButton : public PadButton
{
public:
    explicit DragButton (BackrverbProcessor&);
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void clicked() override;
    std::function<void (const juce::String&)> onStatus;
    static juce::File exportsFolder();
private:
    juce::File makeExport();
    BackrverbProcessor& proc;
    bool dragging = false;
};

class GearButton : public juce::Button
{
public:
    GearButton() : juce::Button ("settings") {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

//==============================================================================
class Display : public juce::Component
{
public:
    explicit Display (BackrverbProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void refresh();
    void setStatus (const juce::String& s) { status = s; statusTicks = 90; }
private:
    void drawWaves (juce::Graphics&, juce::Rectangle<float>);
    BackrverbProcessor& proc;
    BackrverbProcessor::DisplayData data;
    float anim = 0.0f;
    juce::String status;
    int statusTicks = 0;
};

//==============================================================================
class MainPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 640, H = 660;
    explicit MainPanel (BackrverbProcessor&);
    ~MainPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void showSettings();

    BackrverbProcessor& proc;
    LookAndFeel laf;

    SourceSwitch source;
    Display display;
    PadButton captureBtn { "CAPTURE", PadButton::Icon::Mic, true },
              playBtn    { "PLAY", PadButton::Icon::Play, false },
              previewBtn { "PREVIEW", PadButton::Icon::Play, false };
    DragButton dragBtn;
    GearButton gear;

    juce::ComboBox lengthBox, tremRateBox;
    Knob size, tone, width, tremolo, output;
    juce::ToggleButton tremOn, lowCut;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lengthAtt, tremRateAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tremOnAtt, lowCutAtt;

    juce::TooltipWindow tooltips { this, 700 };
    juce::String bpmText { "--" }, tsText { "4/4" };
    float phase = 0.0f;
};

} // namespace brvui

//==============================================================================
class BackrverbEditor : public juce::AudioProcessorEditor
{
public:
    explicit BackrverbEditor (BackrverbProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    brvui::MainPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BackrverbEditor)
};
