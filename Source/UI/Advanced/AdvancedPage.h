// The Advanced view: tab strip plus the A / B slot switch.
#pragma once
#include "UI/Advanced/AdvancedTabs.h"

namespace ui
{
class AdvancedPage final : public juce::Component
{
public:
    explicit AdvancedPage (Ctx&);
    void resized() override;
    void refresh (const fmlume::Preset&);
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

private:
    Ctx& ctx;
    OperatorsTab operators;
    AlgorithmTab algorithm;
    ModulationTab modulation;
    FxTab fx;
    RhythmTab rhythm;
    PerformanceTab performance;
    juce::TextButton slotA { "Edit A" }, slotB { "Edit B" };
};
} // namespace ui
