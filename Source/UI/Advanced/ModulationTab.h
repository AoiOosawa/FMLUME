#pragma once
#include "UI/Common/Bindings.h"

namespace ui
{
class ModulationTab final : public juce::Component
{
public:
    explicit ModulationTab (Ctx&);
    ~ModulationTab() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (const fmlume::Preset&);

private:
    struct LfoBox;
    struct MatrixRow;
    struct MappingRow;
    void rebuildTargets (const fmlume::Preset&);
    Ctx& ctx;
    std::unique_ptr<LfoBox> lfos[2];
    juce::OwnedArray<ModelKnob> envKnobs, filterKnobs;
    std::unique_ptr<BoundCombo> filterType;
    juce::OwnedArray<MatrixRow> matrix;
    juce::ComboBox macroSelect;
    juce::TextEditor macroName;
    juce::OwnedArray<MappingRow> mappings;
    juce::Label macroHint;
    juce::String targetSignature, macroNameOwner;
    int selectedMacro = 0;
    juce::Rectangle<int> lfoArea, envArea, matrixArea, macroArea, filterArea;
};
} // namespace ui
