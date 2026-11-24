#pragma once
#include "UI/Common/Bindings.h"
#include "UI/Advanced/AlgoDiagram.h"

namespace ui
{
// Editable routing: drag from one operator to another to connect them, click a box or a line to select it.
class GraphView final : public juce::Component
{
public:
    explicit GraphView (Ctx&);
    void paint (juce::Graphics&) override;
    void resized() override { relayout(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh (const fmlume::Preset&);
    int selectedEdgeFrom = -1, selectedEdgeTo = -1;
    std::function<void()> onSelection;
    bool showLegend = true;
    float maxBox = 76.0f;
    // the carrier / modulator / feedback key, for drawing it elsewhere (e.g. a panel header)
    static void drawLegend (juce::Graphics&, juce::Rectangle<int> area);

private:
    void relayout();
    int nodeAt (juce::Point<float> p) const;
    bool edgeAt (juce::Point<float> p, int& from, int& to) const;
    Ctx& ctx;
    fmlume::EnginePatch engine;
    AlgoLayout layout;
    int dragFrom = -1;
    juce::Point<float> dragPos;
    bool dragging = false;
};

// The routing templates as thumbnails (like the algorithm chart on a classic FM synth).
class AlgoGallery final : public juce::Component, public juce::SettableTooltipClient
{
public:
    AlgoGallery();
    int columns = 0; // 0: one row
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseUp (const juce::MouseEvent&) override;
    int current = -1;
    std::function<void (int)> onChoose;

private:
    juce::Rectangle<int> cell (int i) const;
    std::vector<fmlume::EnginePatch> engines;
    int hover = -1;
};

} // namespace ui
