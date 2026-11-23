#pragma once
#include "UI/Common/Bindings.h"

namespace ui
{
class PerformanceTab final : public juce::Component
{
public:
    explicit PerformanceTab (Ctx&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (const fmlume::Preset&);

private:
    Ctx& ctx;
    juce::OwnedArray<HostKnob> hostKnobs; // polyphony, unison, detune, spread, master, pan
    ModelKnob portamento;
    BoundKnob bend, presetLevel;
    BoundToggle mono, legato;
    juce::ComboBox quality;
    std::unique_ptr<juce::ComboBoxParameterAttachment> qualityAttachment;
    juce::Label qualityLabel, voiceInfo, hint;
    juce::Rectangle<int> voiceArea, outputArea;
};
} // namespace ui
