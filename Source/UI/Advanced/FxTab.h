#pragma once
#include "UI/Common/Bindings.h"

namespace ui
{
class FxTab final : public juce::Component
{
public:
    explicit FxTab (Ctx&);
    ~FxTab() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (const fmlume::Preset&);
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    struct Row;
    int rowAt (juce::Point<int>) const;
    void select (int type);
    void move (int fromPos, int toPos);
    juce::Rectangle<int> rowBounds (int pos) const;
    Ctx& ctx;
    juce::OwnedArray<Row> rows; // indexed by chain position
    juce::OwnedArray<ModelKnob> knobs;
    std::unique_ptr<BoundToggle> delaySync;
    std::unique_ptr<BoundCombo> delayDivision;
    HostToggle bypassAll;
    juce::Label title, hint;
    std::array<uint8_t, fmlume::kNumFx> order { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    int selected = 0;
    int dragPos = -1, dragTarget = -1, dragY = 0; // row being dragged, where it would land, pointer y
    bool dragging = false;
    juce::Rectangle<int> listArea, paramArea;
};
} // namespace ui
