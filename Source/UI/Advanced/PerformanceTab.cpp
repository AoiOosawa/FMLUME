#include "UI/Advanced/PerformanceTab.h"
#include "DSP/FM/SynthEngine.h"

namespace ui
{
using namespace fmlume;

// ------------------------------------------------------------------------------------ Performance
PerformanceTab::PerformanceTab (Ctx& c)
    : ctx (c), portamento (c, pc::portamento, "Glide"),
      bend (c, "Bend Range", 1.0f, 24.0f, 1.0f, 2.0f, " st",
            [this] (const Preset& p) { return p.data.sound.engine.pitchBendRange; },
            [this] (Preset& p, float v) { p.data.sound.engine.pitchBendRange = v; }),
      presetLevel (c, "Preset Level", -24.0f, 12.0f, 0.1f, 0.0f, " dB",
                   [] (const Preset& p) { return p.data.out.levelDb; }, [] (Preset& p, float v) { p.data.out.levelDb = v; }),
      mono (c, "Mono", "One note at a time, last-note priority",
            [] (const Preset& p) { return p.data.perf.mono; }, [] (Preset& p, bool v) { p.data.perf.mono = v; }),
      legato (c, "Legato", "In mono mode, overlapping notes glide without restarting the envelopes",
              [] (const Preset& p) { return p.data.perf.legato; }, [] (Preset& p, bool v) { p.data.perf.legato = v; })
{
    static const char* const ids[6] = { "master.polyphony", "master.unison", "master.detune", "master.spread", "master.gainDb", "master.pan" };
    static const char* const caps[6] = { "Polyphony", "Unison", "Detune", "Spread", "Master", "Pan" };
    for (int i = 0; i < 6; ++i) addAndMakeVisible (hostKnobs.add (new HostKnob (ctx.proc, ids[i], caps[i])));
    quality.addItemList (labels::list (labels::quality), 1);
    quality.setTooltip ("Oversampling of the FM engine. Higher = less aliasing, more CPU and a few samples more latency (reported to the host). Switching fades the sound briefly.");
    if (auto* p = ctx.proc.apvts.getParameter ("master.quality")) qualityAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (*p, quality);
    styleLabel (qualityLabel, 12.0f, col::textDim, juce::Justification::centred);
    qualityLabel.setText ("Quality", juce::dontSendNotification);
    styleLabel (voiceInfo, 12.5f, col::text);
    styleLabel (hint, 12.5f, col::textDim, juce::Justification::topLeft);
    hint.setText ("Master, Pan and Quality belong to the project, not to the preset. Preset Level is stored with the sound and is meant for matching loudness between presets.",
                  juce::dontSendNotification);
    for (auto* comp : std::initializer_list<juce::Component*> { &portamento, &bend, &presetLevel, &mono, &legato, &quality, &qualityLabel, &voiceInfo, &hint })
        addAndMakeVisible (comp);
}

void PerformanceTab::paint (juce::Graphics& g)
{
    paintPanel (g, voiceArea, "Voices");
    paintPanel (g, outputArea, "Output");
}

void PerformanceTab::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    const int h = juce::jmin (r.getHeight(), 230);
    auto top = r.removeFromTop (h);
    voiceArea = top.removeFromLeft (top.getWidth() * 60 / 100);
    top.removeFromLeft (8);
    outputArea = top;

    auto v = panelContent (voiceArea, 12, 10);
    voiceInfo.setBounds (v.removeFromBottom (20));
    v.removeFromBottom (4);
    auto toggles = v.removeFromRight (70);
    mono.setBounds (toggles.removeFromTop (toggles.getHeight() / 2).withSizeKeepingCentre (64, juce::jmin (toggles.getHeight(), 80)));
    legato.setBounds (toggles.withSizeKeepingCentre (64, juce::jmin (toggles.getHeight(), 80)));
    layoutKnobRow (v, { hostKnobs[0], hostKnobs[1], hostKnobs[2], hostKnobs[3], &portamento, &bend }, 104);

    auto o = panelContent (outputArea, 12, 10);
    hint.setBounds (o.removeFromBottom (juce::jmin (48, o.getHeight() / 3)));
    o.removeFromBottom (4);
    auto q = o.removeFromRight (juce::jmin (150, o.getWidth() / 3)).withSizeKeepingCentre (juce::jmin (150, o.getWidth() / 3), 52);
    qualityLabel.setBounds (q.removeFromTop (16));
    q.removeFromTop (8);
    quality.setBounds (q.removeFromTop (28));
    layoutKnobRow (o, { hostKnobs[4], hostKnobs[5], &presetLevel }, 104);
}

void PerformanceTab::refresh (const Preset& p)
{
    portamento.refresh();
    bend.refreshFrom (p);
    presetLevel.refreshFrom (p);
    mono.refreshFrom (p);
    legato.refreshFrom (p);
    const HostParams h = ctx.proc.currentHostParams();
    juce::String t;
    t << "Max simultaneous notes: " << (p.data.perf.mono ? 1 : SynthEngine::effectiveMaxNotes (h.polyphony, h.unison))
      << "   (budget " << kMaxSubVoices << " unison voices)";
    voiceInfo.setText (t, juce::dontSendNotification);
}
} // namespace ui
