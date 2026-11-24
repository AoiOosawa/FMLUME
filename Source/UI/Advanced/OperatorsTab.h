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

// Interactive ADSR graph: the envelope of the selected operator, or the Mod Envelope.
class EnvelopeView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Source { SelectedOperator, ModEnvelope };
    explicit EnvelopeView (Ctx&, Source = Source::SelectedOperator);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void refresh (const fmlume::Preset&);
    // Draws an ADSR curve into `area`; returns the three handle points (attack, decay/sustain, release).
    // `firstCode` is the parameter code of the attack time (decay, sustain and release follow it):
    // operator and Mod Envelope times have different ranges.
    static std::array<juce::Point<float>, 3> drawEnvelope (juce::Graphics&, juce::Rectangle<float> area, const fmlume::ADSR&, juce::Colour, bool fill, float lineWidth,
                                                           int firstCode = fmlume::pc::op (0, fmlume::kOpEnvA));

private:
    int handleAt (juce::Point<float>) const;
    int firstCode() const; // attack-time code of the envelope being shown
    Ctx& ctx;
    Source source;
    fmlume::ADSR env;
    juce::Colour colour;
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
