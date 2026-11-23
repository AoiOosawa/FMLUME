#pragma once
#include "UI/Common/Bindings.h"

namespace ui
{
// 16-step gate pattern, one lane (mono) or two (stereo: L above R). Click / drag paints steps
// on or off; Alt-drag sets a step's level.
class StepGrid final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepGrid (Ctx&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent& e) override { apply (e); }
    void mouseUp (const juce::MouseEvent&) override;
    void refresh (const fmlume::Preset&);

private:
    void apply (const juce::MouseEvent&);
    Ctx& ctx;
    juce::Rectangle<float> laneBounds (int lane) const;
    std::array<float, fmlume::kGateSteps> steps[2] {}; // [0] = left / mono, [1] = right
    bool stereo = false;
    int dragLane = 0;
    float paintValue = 1.0f;
    bool levelMode = false;
};

class RhythmTab final : public juce::Component
{
public:
    explicit RhythmTab (Ctx&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (const fmlume::Preset&);

private:
    juce::Component* add (juce::Component*, const juce::String& caption);
    Ctx& ctx;
    HostToggle gateOn, arpOn;
    StepGrid grid;
    GatePatternBox patterns;
    juce::TextButton randomise { "Random" }, clear { "Clear" }, invert { "Invert" };
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<juce::Label> captions;
    std::vector<Refreshable*> bound;
    std::vector<std::pair<juce::Component*, juce::Label*>> gateRow, arpRow, chordRow; // control + caption above it
    juce::Label patternCaption;
    juce::Component* widthKnob = nullptr; // owned by `owned`; dimmed while the gate is mono
    juce::Rectangle<int> gateArea, arpArea, chordArea;
};
} // namespace ui
