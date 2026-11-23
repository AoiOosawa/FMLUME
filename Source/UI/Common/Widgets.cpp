#include "UI/Common/Widgets.h"

namespace ui
{
bool Ctx::run (const std::function<bool (fmlume::PatchController&)>& command, const juce::String& errorPrefix)
{
    juce::String error;
    const bool ok = proc.withModel ([&] (fmlume::PatchController& c) {
        const bool r = command (c);
        if (! r) error = c.lastError();
        return r;
    });
    if (! ok) say (errorPrefix + error, true);
    return ok;
}

bool Ctx::edit (const std::function<void (fmlume::Preset&)>& fn, bool forceNew)
{
    return run ([&] (fmlume::PatchController& c) { return c.edit (fn, forceNew); });
}

// --------------------------------------------------------------------------------- LumeSlider
LumeSlider::LumeSlider()
{
    setColour (juce::Slider::rotarySliderFillColourId, col::accent);
    setColour (juce::Slider::trackColourId, col::accent);
    setWantsKeyboardFocus (false); // keys belong to the host
}

void LumeSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Reset to default", onReset != nullptr);
        menu.addItem (2, "Type a value...");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe = juce::Component::SafePointer<LumeSlider> (this)] (int r) {
            if (safe == nullptr || r == 0) return;
            if (r == 1 && safe->onReset) safe->onReset();
            else if (r == 2) safe->showTextBox();
        });
        return;
    }
    if (e.mods.isAltDown())
    {
        if (onReset) onReset();
        return;
    }
    setMouseDragSensitivity (e.mods.isShiftDown() ? 1200 : 220);
    juce::Slider::mouseDown (e);
}

void LumeSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || e.mods.isAltDown()) return;
    juce::Slider::mouseDrag (e);
}

void LumeSlider::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onDoubleClick) onDoubleClick(); else showTextBox();
}

// --------------------------------------------------------------------------------- Knob
Knob::Knob (const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    slider.setDoubleClickReturnValue (false, 0.0);
    addAndMakeVisible (slider);
    caption.setText (text, juce::dontSendNotification);
    caption.setJustificationType (juce::Justification::centred);
    caption.setFont (font (12.0f));
    caption.setColour (juce::Label::textColourId, col::textDim);
    caption.setInterceptsMouseClicks (false, false);
    caption.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (caption);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int capH = large ? 20 : 15;
    caption.setFont (font (large ? 15.0f : 12.0f, large));
    caption.setBounds (r.removeFromTop (capH));
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmin (r.getWidth(), 92), large ? 20 : 16);
    slider.setBounds (r);
}

// --------------------------------------------------------------------------------- HostKnob
HostKnob::HostKnob (FmlumeAudioProcessor& p, const juce::String& paramId, const juce::String& text) : Knob (text)
{
    param = p.apvts.getParameter (paramId);
    jassert (param != nullptr);
    if (param == nullptr) return;
    attachment = std::make_unique<juce::SliderParameterAttachment> (*param, slider);
    slider.textFromValueFunction = [this] (double v) { return param->getText (param->convertTo0to1 ((float) v), 16) + (param->getLabel().isNotEmpty() ? " " + param->getLabel() : juce::String()); };
    slider.updateText();
    slider.onReset = [this] {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getDefaultValue());
        param->endChangeGesture();
    };
    slider.setTooltip (param->getName (64) + " (host automatable)");
}

// --------------------------------------------------------------------------------- ModelKnob
ModelKnob::ModelKnob (Ctx& c, int paramCode, const juce::String& text) : Knob (text), ctx (c)
{
    // The slider works in the normalised 0..1 domain; text is shown in physical units.
    slider.setRange (0.0, 1.0, 0.0);
    slider.textFromValueFunction = [this] (double v) {
        return code < 0 ? juce::String() : juce::String (fmlume::formatParamValue (code, fmlume::fromNorm (fmlume::paramDesc (code), (float) v)));
    };
    slider.valueFromTextFunction = [this] (const juce::String& t) {
        if (code < 0) return 0.0;
        return (double) fmlume::toNorm (fmlume::paramDesc (code), t.trim().getFloatValue());
    };
    slider.onValueChange = [this] {
        if (updating || code < 0) return;
        const auto& d = fmlume::paramDesc (code);
        float v = fmlume::fromNorm (d, (float) slider.getValue());
        // Snap applies to dragging only, so a typed value is taken as typed.
        if (slider.isMouseButtonDown() && ctx.snapRatios && fmlume::pc::isOp (code) && code % fmlume::kOpFieldCount == fmlume::kOpRatio)
            v = v < 1.0f ? std::round (v * 8.0f) / 8.0f : std::round (v * 2.0f) / 2.0f; // 0.5 steps (finer below 1)
        if (! ctx.set (code, side(), v)) refresh();
    };
    slider.onDragEnd = [this] {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ModelKnob> (this)] { if (safe != nullptr) safe->refresh(); }); // show the stored (snapped) value
    };
    slider.onReset = [this] {
        if (code < 0) return;
        ctx.set (code, side(), fmlume::paramDesc (code).def);
        refresh();
    };
    setCode (paramCode);
}

void ModelKnob::setCode (int paramCode)
{
    code = paramCode;
    if (code >= 0)
    {
        const auto& d = fmlume::paramDesc (code);
        slider.getProperties().set ("bipolar", d.min < 0.0f);
        slider.setTooltip (juce::String (fmlume::paramDisplayName (code)) + "  [" + juce::String (fmlume::paramCodeToId (code)) + "]");
    }
    refresh();
}

void ModelKnob::refresh()
{
    const bool present = code >= 0 && ctx.exists (code, side());
    setEnabled (present);
    slider.setEnabled (present);
    if (! present || slider.isMouseButtonDown()) return;
    updating = true;
    slider.setValue (fmlume::toNorm (fmlume::paramDesc (code), ctx.get (code, side())), juce::dontSendNotification);
    slider.updateText();
    updating = false;
}

// --------------------------------------------------------------------------------- misc
HostToggle::HostToggle (FmlumeAudioProcessor& p, const juce::String& paramId, const juce::String& text)
{
    setButtonText (text);
    if (auto* param = p.apvts.getParameter (paramId))
    {
        attachment = std::make_unique<juce::ButtonParameterAttachment> (*param, *this);
        setTooltip (param->getName (64) + " (host automatable)");
    }
}

// --------------------------------------------------------------------------------- wheel
PerformanceWheel::PerformanceWheel (Kind k, const juce::String& labelText) : kind (k), label (labelText)
{
    setWantsKeyboardFocus (false);
}

juce::Rectangle<float> PerformanceWheel::track() const
{
    return getLocalBounds().toFloat().withTrimmedBottom ((float) labelHeight).reduced (5.0f, 4.0f);
}

void PerformanceWheel::setValue (float v)
{
    v = juce::jlimit (kind == Kind::pitchBend ? -1.0f : 0.0f, 1.0f, v);
    if (juce::approximatelyEqual (v, value)) return;
    value = v;
    repaint();
    if (onChange) onChange (value);
}

void PerformanceWheel::setDisplayValue (float v)
{
    if (isMouseButtonDown()) return;
    v = juce::jlimit (kind == Kind::pitchBend ? -1.0f : 0.0f, 1.0f, v);
    if (juce::approximatelyEqual (v, value)) return;
    value = v;
    repaint();
}

void PerformanceWheel::paint (juce::Graphics& g)
{
    const auto area = track();
    const bool bend = kind == Kind::pitchBend;
    const auto colour = bend ? col::accent : col::violet;

    g.setColour (col::bg.darker (0.4f));
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (col::outline);
    g.drawRoundedRectangle (area.reduced (0.5f), 5.0f, 1.0f);

    const float thumbHeight = 14.0f;
    const float travel = area.getHeight() - thumbHeight - 4.0f;
    const float position = bend ? 0.5f * (value + 1.0f) : value;
    const float thumbY = area.getBottom() - 2.0f - thumbHeight - position * travel;

    // fill from the zero position to the thumb
    const float zeroY = area.getBottom() - 2.0f - thumbHeight * 0.5f - (bend ? 0.5f : 0.0f) * travel;
    const float thumbCentre = thumbY + thumbHeight * 0.5f;
    g.setColour (colour.withAlpha (0.35f));
    g.fillRoundedRectangle ({ area.getX() + 3.0f, juce::jmin (zeroY, thumbCentre), area.getWidth() - 6.0f, std::abs (zeroY - thumbCentre) }, 2.0f);
    if (bend)
    {
        g.setColour (col::textDim.withAlpha (0.5f));
        g.fillRect (juce::Rectangle<float> (area.getX() + 2.0f, zeroY - 0.5f, area.getWidth() - 4.0f, 1.0f));
    }

    // thumb with three grooves
    const juce::Rectangle<float> thumb (area.getX() + 2.0f, thumbY, area.getWidth() - 4.0f, thumbHeight);
    g.setColour (col::panelHi.brighter (isMouseButtonDown() ? 0.35f : 0.15f));
    g.fillRoundedRectangle (thumb, 3.0f);
    g.setColour (colour);
    g.fillRect (juce::Rectangle<float> (thumb.getX() + 3.0f, thumb.getCentreY() - 1.0f, thumb.getWidth() - 6.0f, 2.0f));
    g.setColour (col::bg.withAlpha (0.6f));
    g.fillRect (juce::Rectangle<float> (thumb.getX() + 3.0f, thumb.getY() + 3.0f, thumb.getWidth() - 6.0f, 1.0f));
    g.fillRect (juce::Rectangle<float> (thumb.getX() + 3.0f, thumb.getBottom() - 4.0f, thumb.getWidth() - 6.0f, 1.0f));

    g.setColour (col::textDim);
    g.setFont (font (9.5f, true));
    g.drawText (label, getLocalBounds().removeFromBottom (labelHeight), juce::Justification::centred);
}

void PerformanceWheel::mouseDown (const juce::MouseEvent&) { valueAtDown = value; repaint(); }

void PerformanceWheel::mouseDrag (const juce::MouseEvent& e)
{
    // relative drag: the full track height covers the full range
    const float travel = juce::jmax (1.0f, track().getHeight() - 18.0f);
    setValue (valueAtDown - (float) e.getDistanceFromDragStartY() / travel * (kind == Kind::pitchBend ? 2.0f : 1.0f));
}

void PerformanceWheel::mouseUp (const juce::MouseEvent&)
{
    if (kind == Kind::pitchBend) setValue (0.0f); // spring back to the centre
    repaint();
}

void PerformanceWheel::mouseDoubleClick (const juce::MouseEvent&) { setValue (0.0f); }

void layoutKnobRow (juce::Rectangle<int> row, const std::vector<juce::Component*>& items, int maxSize, int pad)
{
    if (items.empty()) return;
    const int w = row.getWidth() / (int) items.size();
    for (auto* item : items)
        item->setBounds (row.removeFromLeft (w).withSizeKeepingCentre (juce::jmin (w - pad, maxSize), juce::jmin (row.getHeight(), maxSize)));
}

namespace
{
const juce::Identifier focusSinkProperty { "fmlumeFocusSink" };
const juce::Identifier focusReleaseProperty { "fmlumeReleasesFocus" };
}

void markAsFocusSink (juce::Component& component)
{
    component.getProperties().set (focusSinkProperty, true);
    component.setWantsKeyboardFocus (true);
}

bool isFocusSink (const juce::Component& component) { return component.getProperties().contains (focusSinkProperty); }

bool mayHoldKeyboardFocus (const juce::Component& component)
{
    if (isFocusSink (component)) return true;
    // a text field or something inside one (knob value entry and editable combos are TextEditors too)
    for (const auto* c = &component; c != nullptr; c = c->getParentComponent())
        if (dynamic_cast<const juce::TextEditor*> (c) != nullptr) return true;
    return false;
}

void releaseKeysAfterTextEntry (juce::Component& root)
{
    if (auto* editor = dynamic_cast<juce::TextEditor*> (&root))
    {
        if (! editor->getProperties().contains (focusReleaseProperty))
        {
            editor->getProperties().set (focusReleaseProperty, true);
            const juce::Component::SafePointer<juce::TextEditor> safe (editor);
            const auto thenRelease = [safe] (std::function<void()> existing) {
                return [safe, handler = std::move (existing)] {
                    if (handler != nullptr) handler();
                    if (safe != nullptr) safe->giveAwayKeyboardFocus();
                };
            };
            editor->onEscapeKey = thenRelease (editor->onEscapeKey);
            if (! editor->isMultiLine()) editor->onReturnKey = thenRelease (editor->onReturnKey); // Return is a line break in multi-line fields
        }
        return; // nothing inside a text field is touched
    }
    for (auto* child : root.getChildren()) releaseKeysAfterTextEntry (*child);
}

void styleCombo (juce::ComboBox& c, const juce::StringArray& items, const juce::String& tooltip)
{
    c.clear (juce::dontSendNotification);
    c.addItemList (items, 1);
    c.setTooltip (tooltip);
}

void styleLabel (juce::Label& l, float size, juce::Colour colour, juce::Justification j)
{
    l.setFont (font (size));
    l.setColour (juce::Label::textColourId, colour);
    l.setJustificationType (j);
    l.setMinimumHorizontalScale (0.7f);
}
} // namespace ui
