// The two graphs of an operator: its waveform and its (draggable) ADSR envelope. The envelope graph
// also edits the Mod Envelope (Filter & Mod tab).
#pragma once
#include "UI/Common/Bindings.h"
#include "UI/Advanced/AlgoDiagram.h"

namespace ui
{
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
    static constexpr float kAspect = 2.4f; // width : height, fixed so a curve looks the same at every size

private:
    // Draws the ADSR curve into `area`; returns the three handle points (attack, decay/sustain, release).
    // `firstCode` is the parameter code of the attack time (decay, sustain and release follow it):
    // operator and Mod Envelope times have different ranges.
    static std::array<juce::Point<float>, 3> drawEnvelope (juce::Graphics&, juce::Rectangle<float> area, const fmlume::ADSR&, juce::Colour, int firstCode);
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
    static constexpr float kAspect = 3.0f; // width : height
    fmlume::OperatorState state;
};
} // namespace ui
