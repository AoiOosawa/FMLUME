#pragma once
#include "UI/Common/Ctx.h"
#include "UI/Common/Theme.h"

namespace ui
{
// Slider with FMLUME's interaction rules: Shift = fine, Option/Alt-click = default,
// double-click = type a value, right-click = context menu.
class LumeSlider : public juce::Slider
{
public:
    LumeSlider();
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    std::function<void()> onReset;                          // restore the default value
    std::function<void()> onDoubleClick;                    // replaces "type a value" when set
};

// Rotary knob with caption above and editable value below.
class Knob : public juce::Component
{
public:
    explicit Knob (const juce::String& caption = {});
    void resized() override;
    void setCaption (const juce::String& text) { caption.setText (text, juce::dontSendNotification); }
    void setAccent (juce::Colour c) { slider.setColour (juce::Slider::rotarySliderFillColourId, c); }
    LumeSlider slider;
    juce::Label caption;
    bool large = false;
};

// Knob attached to a host (APVTS) parameter.
class HostKnob final : public Knob
{
public:
    HostKnob (FmlumeAudioProcessor& p, const juce::String& paramId, const juce::String& caption);

private:
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
};

// Knob bound to one canonical-model parameter code (edits go through the PatchController).
class ModelKnob final : public Knob
{
public:
    ModelKnob (Ctx& c, int paramCode, const juce::String& caption = {});
    void setCode (int paramCode);
    int getCode() const { return code; }
    void refresh();
    std::function<bool()> useSideB; // default: the slot being edited

private:
    bool side() { return useSideB ? useSideB() : ctx.sideB(); }
    Ctx& ctx;
    int code = -1;
    bool updating = false;
};

// Toggle attached to a host bool parameter.
class HostToggle final : public juce::ToggleButton
{
public:
    HostToggle (FmlumeAudioProcessor& p, const juce::String& paramId, const juce::String& text);

private:
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;
};

// Wheel next to the on-screen keyboard (same control as POLYBRISK's). Pitch bend is centred and
// springs back on release; modulation starts at the bottom and stays where it is left.
// Not a parameter: values are reported through onChange (bend -1..1, modulation 0..1).
class PerformanceWheel final : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Kind { pitchBend, modulation };
    PerformanceWheel (Kind kind, const juce::String& labelText);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    // Shows a value that arrived over MIDI (does not call onChange; ignored while dragging).
    void setDisplayValue (float newValue);
    std::function<void (float)> onChange;
    static constexpr int labelHeight = 14;

private:
    juce::Rectangle<float> track() const;
    void setValue (float newValue);
    Kind kind;
    juce::String label;
    float value = 0.0f, valueAtDown = 0.0f;
};

// Lays `items` out side by side in equal cells of `row`; each is centred in its cell and at most
// `maxSize` wide and high (and `pad` narrower than the cell).
void layoutKnobRow (juce::Rectangle<int> row, const std::vector<juce::Component*>& items, int maxSize, int pad = 4);

// ---- Leaving the keyboard to the host (same mechanism as POLYBRISK) ----------------------------
// JUCE buttons and combo boxes take keyboard focus when clicked and then handle Return / arrow
// keys themselves; in a plug-in that steals the host's shortcuts. Policy: only text fields may
// hold the keyboard. When anything else receives focus, the editor moves it at once to a "sink"
// component that handles no keys, so the keys reach the host.
// Setting setWantsKeyboardFocus (false) per component is not enough: JUCE restores the flag when
// a component is enabled / disabled, and components of hidden tabs are not in the hierarchy.
void markAsFocusSink (juce::Component& component);
bool isFocusSink (const juce::Component& component);
// May this component keep the keyboard? True for text fields (and anything inside one) and sinks.
bool mayHoldKeyboardFocus (const juce::Component& component);
// Makes every text field under `root` give the keyboard back when typing ends (Esc, and Return
// in single-line fields). Existing handlers are kept. Safe to call repeatedly.
void releaseKeysAfterTextEntry (juce::Component& root);

// Model strings are UTF-8 std::strings.
inline juce::String utf8 (const std::string& s) { return juce::String (juce::CharPointer_UTF8 (s.c_str())); }
inline juce::String joinTags (const std::vector<std::string>& tags)
{
    juce::String t;
    for (size_t i = 0; i < tags.size(); ++i) t << (i ? " / " : "") << utf8 (tags[i]);
    return t;
}

// Sets a host parameter (normalised 0..1) as one complete automation gesture.
inline void setHostParam (juce::RangedAudioParameter& p, float normalised)
{
    p.beginChangeGesture();
    p.setValueNotifyingHost (normalised);
    p.endChangeGesture();
}

void styleCombo (juce::ComboBox& c, const juce::StringArray& items, const juce::String& tooltip = {});
void styleLabel (juce::Label& l, float size = 12.0f, juce::Colour colour = col::textDim, juce::Justification j = juce::Justification::centredLeft);
} // namespace ui
