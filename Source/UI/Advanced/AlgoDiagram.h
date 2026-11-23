// DX-style algorithm diagram: numbered boxes, carriers on the bottom row joined by an output
// bar, modulators stacked above the operators they feed, feedback drawn as a loop.
#pragma once
#include "UI/Common/Theme.h"
#include "Core/Model.h"

namespace ui
{
struct AlgoLayout
{
    juce::Point<float> pos[fmlume::kNumOps];
    bool shown[fmlume::kNumOps] {};
    float box = 40.0f;   // box edge length
    float outY = 0.0f;   // y of the output bar
    // Polyline of a connection (from the modulator's bottom edge to the target's top edge).
    juce::Path edgePath (int from, int to) const;
    juce::Rectangle<float> boxOf (int k) const { return juce::Rectangle<float> (box, box).withCentre (pos[k]); }
};

// includeDisabled: also place switched-off operators (dimmed, bottom row) so they can be selected.
AlgoLayout layoutAlgorithm (const fmlume::EnginePatch& e, juce::Rectangle<float> area, bool includeDisabled, float maxBox = 54.0f);

struct AlgoStyle
{
    bool mini = false;
    int selectedOp = -1, selectedFrom = -1, selectedTo = -1;
};
void drawAlgorithm (juce::Graphics& g, const fmlume::EnginePatch& e, const AlgoLayout& l, const AlgoStyle& style);

inline juce::Colour roleColour (const fmlume::OperatorState& op) { return op.carrierGain > 0.0f ? col::accent : col::violet; }
} // namespace ui
