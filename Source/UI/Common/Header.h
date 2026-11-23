#pragma once
#include "UI/Common/Widgets.h"

namespace ui
{
// Same header as POLYBRISK: logo, preset stepper + name, Save, SIMPLE / ADVANCED, output level.
class Header final : public juce::Component
{
public:
    explicit Header (Ctx&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void refresh (const fmlume::Preset&);
    void tick(); // output meter, 30 Hz
    juce::TextButton simpleBtn { "SIMPLE" }, advancedBtn { "ADVANCED" };
    std::function<void()> onSave, onBrowse;
    bool browserOpen = false;

private:
    Ctx& ctx;
    juce::TextButton prev { "<" }, next { ">" }, save { "Save" };
    LumeSlider out;
    std::unique_ptr<juce::SliderParameterAttachment> outAttachment;
    juce::String presetName;
    bool dirty = false, overName = false;
    float peak[2] {};
    juce::Rectangle<int> logoArea, nameArea, outLabelArea, outValueArea, meterArea;
};
} // namespace ui
