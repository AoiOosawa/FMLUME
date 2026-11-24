// FMLUME look. The chassis (background, panels, knob / switch / tab shapes, section headers)
// is shared with POLYBRISK so the two plug-ins read as one series; the hero colour differs:
// POLYBRISK leads with orange, FMLUME with cyan -> violet (assets/branding).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
namespace col
{
const juce::Colour bg { 0xff12131c };
const juce::Colour panel { 0xff1b1d2b };
const juce::Colour panelHi { 0xff262940 };
const juce::Colour outline { 0xff34385a };
const juce::Colour text { 0xffe8e9f3 };
const juce::Colour textDim { 0xff9a9db8 };
const juce::Colour accent { 0xff3cd6ff };  // cyan: FMLUME hero colour
const juce::Colour violet { 0xff8a6bff };  // gradient partner of the hero colour
const juce::Colour accentB { 0xffff6a3c }; // orange: secondary emphasis
const juce::Colour hot { 0xffff3c8e };     // pink
const juce::Colour warm { 0xffffd23c };    // yellow
const juce::Colour good { 0xff5fe0a0 };
const juce::Colour bad { 0xffff5470 };
const juce::Colour onAccent { 0xff0d0e17 }; // text on an accent-filled button
} // namespace col

inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

// Logo mark: three stacked sine waves on the series' rounded tile.
void drawLogoMark (juce::Graphics& g, juce::Rectangle<float> area);

class LumeLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LumeLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (13.0f, (float) h * 0.6f), true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
    juce::Font getPopupMenuFont() override { return font (13.5f); }
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool over, bool down) override;
    int getTabButtonBestWidth (juce::TabBarButton&, int) override;
};

// Rounded panel with the series' section header (accent bar + bold caps title).
void paintPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title = {});
constexpr int kPanelHeader = 26; // space a titled panel reserves above its content
// Content rectangle of a titled panel: inset by the padding, below the header (less `headerTrim`).
inline juce::Rectangle<int> panelContent (juce::Rectangle<int> area, int padX, int padY, int headerTrim = 4)
{
    return area.reduced (padX, padY).withTrimmedTop (kPanelHeader - headerTrim);
}
} // namespace ui
