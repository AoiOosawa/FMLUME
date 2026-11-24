#pragma once
#include "UI/Advanced/OperatorGraphs.h"
#include "UI/Advanced/RoutingGraph.h"

namespace ui
{
// Advanced > Operators: the routing on top (editable; Expand gives it the whole page) and the selected
// operator's oscillator, envelope and level below.
class OperatorsTab final : public juce::Component
{
public:
    explicit OperatorsTab (Ctx&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (const fmlume::Preset&);

private:
    void select (int op);
    void applyTemplate (int index);
    void showTemplates();
    void setExpanded (bool);
    void fitMacros();
    void bindOperator (int op, fmlume::PitchMode); // points every per-operator knob at `op`
    void layoutRouting (juce::Rectangle<int> area);
    void layoutSide (juce::Rectangle<int> side);
    void layoutOperatorPanels (juce::Rectangle<int> bottom);
    void refreshRouting (const fmlume::Preset&);
    void refreshSide (const fmlume::Preset&, int op);
    void updateSideVisibility();

    Ctx& ctx;
    GraphView graph;
    AlgoGallery strip;   // the 18 templates in a row: shown while the routing is expanded
    bool expanded = false;
    juce::Label algoName, opTitle, edgeTitle, role;
    juce::ToggleButton opEnabled { "On" };
    ModelKnob index;     // depth of the selected connection
    ModelKnob sideRatio, sideLevel, sideOutput, sideFeedback; // the selected operator, while the routing is expanded
    juce::TextButton templates { "Templates..." }, expand { "Expand" }, removeEdge { "Remove" }, fitMacrosButton { "Fit macros 1-4 to routing" };
    WaveView wave;
    EnvelopeView envelope;
    BoundCombo waveform, pitchMode;
    BoundToggle keySync;
    juce::ToggleButton snap { "Snap ratio" };
    ModelKnob ratio, detune, feedback, phase, attack, decay, sustain, release, level, output, velLevel, velFm, keyTrack;
    std::vector<juce::Component*> operatorPanelParts; // hidden while the routing is expanded
    juce::Rectangle<int> routingArea, sideArea, oscArea, envArea, levelArea;
    int legendRight = 0;    // the legend in the routing header stops before the algorithm name
    bool edgeKnobFits = true;
    // Results that only depend on the routing and macros 1-4, recomputed when those change.
    juce::String routingSignature;
    int templateMatch = -1;
    bool macrosNeedFit = false;
};
} // namespace ui
