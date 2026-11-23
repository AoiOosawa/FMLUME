#pragma once
#include "UI/Common/Bindings.h"
#include "UI/Advanced/AlgoDiagram.h"

namespace ui
{
// Compact overview of one operator: click to select, drag the bar to set its level.
class OpCard final : public juce::Component, public juce::SettableTooltipClient
{
public:
    OpCard (Ctx&, int opIndex);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh (const fmlume::Preset&);
    std::function<void()> onSelect;

private:
    juce::Rectangle<int> levelBar() const;
    Ctx& ctx;
    int op;
    fmlume::OperatorState state;
    juce::String feeds;
    juce::ToggleButton power;
    bool draggingLevel = false;
};

// Interactive ADSR graph of the selected operator.
class EnvelopeView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit EnvelopeView (Ctx&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void refresh (const fmlume::Preset&);
    // Draws an ADSR curve into `area`; returns the three handle points (attack, decay/sustain, release).
    static std::array<juce::Point<float>, 3> drawEnvelope (juce::Graphics&, juce::Rectangle<float> area, const fmlume::ADSR&, juce::Colour, bool fill, float lineWidth);

private:
    int handleAt (juce::Point<float>) const;
    Ctx& ctx;
    fmlume::OperatorState state;
    std::array<juce::Point<float>, 3> handles {};
    int dragging = -1, hover = -1;
};

// One or two cycles of the selected operator's own waveform (including its feedback).
class WaveView final : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    fmlume::OperatorState state;
};

class OperatorsTab final : public juce::Component
{
public:
    explicit OperatorsTab (Ctx&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (const fmlume::Preset&);

private:
    void select (int op);
    Ctx& ctx;
    juce::OwnedArray<OpCard> cards;
    WaveView wave;
    EnvelopeView envelope;
    BoundCombo waveform, pitchMode;
    BoundToggle keySync;
    juce::ToggleButton snap { "Snap ratio" };
    ModelKnob ratio, detune, feedback, phase, attack, decay, sustain, release, level, output, velLevel, velFm, keyTrack;
    juce::Label role;
    juce::Rectangle<int> oscArea, envArea, levelArea;
    int shown = -1;
};
} // namespace ui
