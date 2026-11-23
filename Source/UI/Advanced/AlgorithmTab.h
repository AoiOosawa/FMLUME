#pragma once
#include "UI/Common/Bindings.h"
#include "UI/Advanced/AlgoDiagram.h"

namespace ui
{
class GraphView final : public juce::Component, public juce::SettableTooltipClient
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

// The routing templates as a strip of thumbnails (like the algorithm chart on a classic FM synth).
class AlgoGallery final : public juce::Component, public juce::SettableTooltipClient
{
public:
    AlgoGallery();
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

class AlgorithmTab final : public juce::Component
{
public:
    explicit AlgorithmTab (Ctx&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (const fmlume::Preset&);

private:
    void selectionChanged();
    Ctx& ctx;
    AlgoGallery gallery;
    GraphView graph;
    juce::Label algoName, opTitle, edgeTitle, hint;
    juce::ToggleButton opEnabled { "On" };
    ModelKnob level, carrier, feedback, ratio, index;
    juce::TextButton removeEdge { "Remove connection" }, resetMacros { "Rebuild macros 1-4" };
    juce::Rectangle<int> sideArea;
    int shownOp = -1;
};
} // namespace ui
