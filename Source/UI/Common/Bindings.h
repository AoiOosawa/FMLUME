// Small controls bound to arbitrary model fields through getter / setter lambdas.
#pragma once
#include "UI/Common/Widgets.h"
#include "UI/Common/EnumLabels.h"
#include "DSP/Sequencer/GatePatterns.h"

namespace ui
{
struct Refreshable
{
    virtual ~Refreshable() = default;
    virtual void refreshFrom (const fmlume::Preset&) = 0;
};

// The operator selected in the Operators / Algorithm tabs.
inline fmlume::OperatorState& selectedOperator (fmlume::Preset& p, Ctx& ctx) { return p.data.sound.engine.operators[(size_t) ctx.selectedOp]; }
inline const fmlume::OperatorState& selectedOperator (const fmlume::Preset& p, Ctx& ctx) { return p.data.sound.engine.operators[(size_t) ctx.selectedOp]; }

// Horizontal slider with caption for a float field.
class BoundSlider final : public juce::Component, public Refreshable
{
public:
    using Getter = std::function<float (const fmlume::Preset&)>;
    using Setter = std::function<void (fmlume::Preset&, float)>;
    BoundSlider (Ctx& c, const juce::String& captionText, float lo, float hi, float step, float def, const juce::String& suffix, Getter g, Setter s)
        : ctx (c), get (std::move (g)), set (std::move (s))
    {
        styleLabel (caption);
        caption.setText (captionText, juce::dontSendNotification);
        addAndMakeVisible (caption);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        // set here because panels are built before the editor installs its look-and-feel
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxTextColourId, col::text);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 18);
        slider.setRange (lo, hi, step);
        slider.setTextValueSuffix (suffix);
        slider.getProperties().set ("bipolar", lo < 0.0f);
        slider.setTooltip (captionText);
        slider.onValueChange = [this] {
            if (updating) return;
            const float v = (float) slider.getValue();
            ctx.edit ([this, v] (fmlume::Preset& p) { set (p, v); });
        };
        slider.onReset = [this, def] { slider.setValue (def, juce::sendNotificationSync); };
        addAndMakeVisible (slider);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        caption.setBounds (r.removeFromLeft (juce::jmin (captionWidth, r.getWidth() / 2)));
        slider.setBounds (r);
    }
    void refreshFrom (const fmlume::Preset& p) override
    {
        if (slider.isMouseButtonDown()) return;
        updating = true;
        slider.setValue (get (p), juce::dontSendNotification);
        updating = false;
    }
    LumeSlider slider;
    juce::Label caption;
    int captionWidth = 86;

private:
    Ctx& ctx;
    Getter get;
    Setter set;
    bool updating = false;
};

// Rotary knob for a float field (same look as the parameter knobs).
class BoundKnob final : public Knob, public Refreshable
{
public:
    using Getter = std::function<float (const fmlume::Preset&)>;
    using Setter = std::function<void (fmlume::Preset&, float)>;
    BoundKnob (Ctx& c, const juce::String& captionText, float lo, float hi, float step, float def, const juce::String& suffix, Getter g, Setter s)
        : Knob (captionText), ctx (c), get (std::move (g)), set (std::move (s))
    {
        slider.setRange (lo, hi, step);
        slider.setTextValueSuffix (suffix);
        slider.getProperties().set ("bipolar", lo < 0.0f);
        slider.setTooltip (captionText);
        slider.onValueChange = [this] {
            if (updating) return;
            const float v = (float) slider.getValue();
            ctx.edit ([this, v] (fmlume::Preset& p) { set (p, v); });
        };
        slider.onReset = [this, def] { slider.setValue (def, juce::sendNotificationSync); };
    }
    void refreshFrom (const fmlume::Preset& p) override
    {
        if (slider.isMouseButtonDown()) return;
        updating = true;
        slider.setValue (get (p), juce::dontSendNotification);
        updating = false;
    }

private:
    Ctx& ctx;
    Getter get;
    Setter set;
    bool updating = false;
};

class BoundCombo final : public juce::ComboBox, public Refreshable
{
public:
    using Getter = std::function<int (const fmlume::Preset&)>;
    using Setter = std::function<void (fmlume::Preset&, int)>;
    BoundCombo (Ctx& c, const juce::String& name, const juce::StringArray& items, Getter g, Setter s, bool forceNewGeneration = false)
        : ctx (c), get (std::move (g)), set (std::move (s))
    {
        styleCombo (*this, items, name);
        onChange = [this, forceNewGeneration] {
            const int i = getSelectedItemIndex();
            if (i < 0) return;
            if (! ctx.edit ([this, i] (fmlume::Preset& p) { set (p, i); }, forceNewGeneration))
                refreshFrom (ctx.snapshot());
        };
    }
    void refreshFrom (const fmlume::Preset& p) override { setSelectedItemIndex (get (p), juce::dontSendNotification); }

private:
    Ctx& ctx;
    Getter get;
    Setter set;
};

class BoundToggle final : public juce::ToggleButton, public Refreshable
{
public:
    using Getter = std::function<bool (const fmlume::Preset&)>;
    using Setter = std::function<void (fmlume::Preset&, bool)>;
    BoundToggle (Ctx& c, const juce::String& text, const juce::String& tooltip, Getter g, Setter s) : ctx (c), get (std::move (g)), set (std::move (s))
    {
        setButtonText (text);
        setTooltip (tooltip);
        onClick = [this] {
            const bool v = getToggleState();
            if (! ctx.edit ([this, v] (fmlume::Preset& p) { set (p, v); }))
                refreshFrom (ctx.snapshot());
        };
    }
    void refreshFrom (const fmlume::Preset& p) override { setToggleState (get (p), juce::dontSendNotification); }

private:
    Ctx& ctx;
    Getter get;
    Setter set;
};
// Names of the tempo divisions first..last (indices into syncDivName).
inline juce::StringArray divisionNames (int first, int last)
{
    juce::StringArray a;
    for (int i = first; i <= last; ++i) a.add (fmlume::syncDivName (i));
    return a;
}
inline juce::StringArray rateNames() { return divisionNames (fmlume::kRateFirst, fmlume::kArpRateLast); }
inline juce::StringArray gateRateNames() { return divisionNames (fmlume::kRateFirst, fmlume::kGateRateLast); }

// Trance-gate pattern chooser: the factory patterns shared with POLYBRISK, grouped. Shows
// "Custom" when the steps were edited by hand. Choosing a pattern also sets its rate.
class GatePatternBox final : public juce::ComboBox, public Refreshable
{
public:
    explicit GatePatternBox (Ctx& c) : ctx (c)
    {
        const char* group = "";
        for (int i = 0; i < fmlume::kNumFactoryGatePatterns; ++i)
        {
            const auto& f = fmlume::kFactoryGatePatterns[(size_t) i];
            if (juce::String (group) != f.group) { group = f.group; addSectionHeading (group); }
            addItem (f.name, i + 1);
        }
        setTextWhenNothingSelected ("Custom");
        setTooltip ("Ready-made gate patterns (the same set as POLYBRISK). Choosing one also sets the gate rate it was written for; the Stereo group switches the gate to stereo.");
        onChange = [this] {
            const int i = getSelectedId() - 1;
            if (i < 0) return;
            ctx.edit ([i] (fmlume::Preset& p) { fmlume::applyGatePattern (p.data.perf.gate, i); });
        };
    }
    void refreshFrom (const fmlume::Preset& p) override
    {
        const int i = fmlume::matchGatePattern (p.data.perf.gate);
        setSelectedId (i >= 0 ? i + 1 : 0, juce::dontSendNotification);
    }

private:
    Ctx& ctx;
};
} // namespace ui
