#include "UI/Advanced/RhythmTab.h"
#include "Core/Pcg32.h"

namespace ui
{
using namespace fmlume;

// ------------------------------------------------------------------------------------ StepGrid
StepGrid::StepGrid (Ctx& c) : ctx (c)
{
    steps[0].fill (1.0f);
    steps[1].fill (1.0f);
    setTooltip ("Gate pattern: click or drag across steps to switch them on / off. Alt-drag up / down sets a step's level (accent). "
                "With Stereo on, the upper row gates the left channel and the lower row the right channel.");
}

juce::Rectangle<float> StepGrid::laneBounds (int lane) const
{
    auto r = getLocalBounds().toFloat();
    if (! stereo) return r;
    const float h = (r.getHeight() - 5.0f) * 0.5f;
    return lane == 0 ? r.removeFromTop (h) : r.removeFromBottom (h);
}

void StepGrid::paint (juce::Graphics& g)
{
    for (int lane = 0; lane < (stereo ? 2 : 1); ++lane)
    {
        const auto r = laneBounds (lane);
        const float w = r.getWidth() / kGateSteps;
        const juce::Colour c1 = lane == 0 ? col::accent : col::accentB, c2 = lane == 0 ? col::violet : col::hot;
        for (int i = 0; i < kGateSteps; ++i)
        {
            const juce::Rectangle<float> cell (r.getX() + (float) i * w + 2.0f, r.getY(), w - 4.0f, r.getHeight());
            g.setColour (i % 4 == 0 ? col::bg.brighter (0.07f) : col::bg);
            g.fillRoundedRectangle (cell, 4.0f);
            const float v = steps[lane][(size_t) i];
            if (v > 0.0f)
            {
                const auto fill = cell.withTop (cell.getBottom() - cell.getHeight() * v);
                g.setGradientFill (juce::ColourGradient (c1.withAlpha (0.55f + 0.45f * v), 0.0f, fill.getY(), c2.withAlpha (0.55f + 0.45f * v), 0.0f, cell.getBottom() + cell.getHeight(), false));
                g.fillRoundedRectangle (fill, 4.0f);
            }
            g.setColour (v > 0.2f ? col::onAccent.withAlpha (0.8f) : col::textDim.withAlpha (0.7f));
            g.setFont (font (10.5f, true));
            if (stereo && i == 0) g.drawText (lane == 0 ? "L" : "R", juce::Rectangle<float> (cell).removeFromTop (16.0f).withTrimmedLeft (6.0f), juce::Justification::centredLeft);
            g.drawText (juce::String (i + 1), juce::Rectangle<float> (cell).removeFromBottom (15.0f), juce::Justification::centred);
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    levelMode = e.mods.isAltDown() || e.mods.isPopupMenu();
    dragLane = stereo && e.position.y > laneBounds (0).getBottom() + 2.5f ? 1 : 0;
    const int i = juce::jlimit (0, kGateSteps - 1, e.x * kGateSteps / juce::jmax (1, getWidth()));
    paintValue = steps[dragLane][(size_t) i] > 0.0f ? 0.0f : 1.0f;
    apply (e);
}

void StepGrid::apply (const juce::MouseEvent& e)
{
    const int i = juce::jlimit (0, kGateSteps - 1, e.x * kGateSteps / juce::jmax (1, getWidth()));
    float v = paintValue;
    if (levelMode)
    {
        const auto lane = laneBounds (dragLane);
        v = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - lane.getY()) / juce::jmax (1.0f, lane.getHeight()));
        if (v > 0.93f) v = 1.0f;
        if (v < 0.07f) v = 0.0f;
    }
    if (steps[dragLane][(size_t) i] == v) return;
    steps[dragLane][(size_t) i] = v;
    const bool right = dragLane == 1, both = ! stereo;
    ctx.edit ([i, v, right, both] (Preset& p) {
        auto& gate = p.data.perf.gate;
        if (right) gate.stepsRight[(size_t) i] = v; else gate.steps[(size_t) i] = v;
        if (both) gate.stepsRight[(size_t) i] = v; // mono: the hidden right lane follows the left one
    });
    repaint();
}

void StepGrid::mouseUp (const juce::MouseEvent&) {}

void StepGrid::refresh (const Preset& p)
{
    if (isMouseButtonDown()) return;
    steps[0] = p.data.perf.gate.steps;
    steps[1] = p.data.perf.gate.stepsRight;
    stereo = p.data.perf.gate.stereo;
    repaint();
}

// ------------------------------------------------------------------------------------ Rhythm
juce::Component* RhythmTab::add (juce::Component* comp, const juce::String& caption)
{
    owned.add (comp);
    addAndMakeVisible (comp);
    if (auto* r = dynamic_cast<Refreshable*> (comp)) bound.push_back (r);
    if (caption.isNotEmpty())
    {
        auto* label = captions.add (new juce::Label());
        styleLabel (*label, 12.0f, col::textDim, juce::Justification::centred);
        label->setText (caption, juce::dontSendNotification);
        addAndMakeVisible (label);
    }
    return comp;
}

RhythmTab::RhythmTab (Ctx& c)
    : ctx (c), gateOn (c.proc, "tranceGate.enabled", "Power"), arpOn (c.proc, "arp.enabled", "Power"), grid (c), patterns (c)
{
    for (auto* comp : std::initializer_list<juce::Component*> { &gateOn, &arpOn, &grid, &patterns, &randomise, &clear, &invert, &patternCaption }) addAndMakeVisible (comp);
    styleLabel (patternCaption, 12.0f, col::textDim, juce::Justification::centred);
    patternCaption.setText ("Pattern", juce::dontSendNotification);

    const auto item = [this] (std::vector<std::pair<juce::Component*, juce::Label*>>& row, juce::Component* comp, const juce::String& caption) {
        const int before = captions.size();
        add (comp, caption);
        row.push_back ({ comp, captions.size() > before ? captions.getLast() : nullptr });
    };

    // Trance gate
    item (gateRow, new BoundCombo (ctx, "Gate rate", gateRateNames(),
                                   [] (const Preset& p) { return (int) p.data.perf.gate.rate - kRateFirst; }, [] (Preset& p, int v) { p.data.perf.gate.rate = (uint8_t) (v + kRateFirst); }), "Rate");
    item (gateRow, new BoundKnob (ctx, "Depth", 0.0f, 100.0f, 1.0f, 100.0f, " %",
                                  [] (const Preset& p) { return p.data.perf.gate.mix * 100.0f; }, [] (Preset& p, float v) { p.data.perf.gate.mix = v * 0.01f; }), {});
    item (gateRow, new BoundKnob (ctx, "Smooth", 0.0f, 30.0f, 0.5f, 5.0f, " ms",
                                  [] (const Preset& p) { return p.data.perf.gate.smoothMs; }, [] (Preset& p, float v) { p.data.perf.gate.smoothMs = v; }), {});
    item (gateRow, new BoundKnob (ctx, "Swing", 0.0f, 60.0f, 1.0f, 0.0f, " %",
                                  [] (const Preset& p) { return p.data.perf.gate.swing * 100.0f; }, [] (Preset& p, float v) { p.data.perf.gate.swing = v * 0.01f; }), {});

    item (gateRow, new BoundToggle (ctx, "Stereo", "Separate patterns for the left and right channel (upper row = left, lower row = right)",
                                    [] (const Preset& p) { return p.data.perf.gate.stereo; },
                                    [] (Preset& p, bool v) {
                                        auto& gate = p.data.perf.gate;
                                        if (v && ! gate.stereo) gate.stepsRight = gate.steps; // start from the mono pattern
                                        gate.stereo = v;
                                    }), {});
    widthKnob = new BoundKnob (ctx, "Width", 0.0f, 100.0f, 1.0f, 100.0f, " %",
                               [] (const Preset& p) { return p.data.perf.gate.width * 100.0f; }, [] (Preset& p, float v) { p.data.perf.gate.width = v * 0.01f; });
    item (gateRow, widthKnob, {});

    // Arpeggiator
    item (arpRow, new BoundCombo (ctx, "Arp mode", labels::list (labels::arpMode),
                                  [] (const Preset& p) { return (int) p.data.perf.arp.mode; }, [] (Preset& p, int v) { p.data.perf.arp.mode = (ArpMode) v; }), "Mode");
    item (arpRow, new BoundCombo (ctx, "Arp rate", rateNames(),
                                  [] (const Preset& p) { return (int) p.data.perf.arp.rate - kRateFirst; }, [] (Preset& p, int v) { p.data.perf.arp.rate = (uint8_t) (v + kRateFirst); }), "Rate");
    {
        auto* order = new BoundCombo (ctx, "Octave order", labels::list (labels::arpOctaveMode),
                                      [] (const Preset& p) { return (int) p.data.perf.arp.octaveMode; }, [] (Preset& p, int v) { p.data.perf.arp.octaveMode = (ArpOctaveMode) v; });
        order->setTooltip ("Across range: the note order runs through all octaves (C E G C' E' G'). "
                           "Step up / down / up-down: the figure is played inside one octave, then the whole figure moves by an octave (C E G E, C' E' G' E').");
        item (arpRow, order, "Octave Order");
    }
    item (arpRow, new BoundKnob (ctx, "Octaves", 1.0f, 4.0f, 1.0f, 1.0f, "",
                                 [] (const Preset& p) { return (float) p.data.perf.arp.octaves; }, [] (Preset& p, float v) { p.data.perf.arp.octaves = juce::roundToInt (v); }), {});
    item (arpRow, new BoundKnob (ctx, "Gate Length", 5.0f, 100.0f, 1.0f, 80.0f, " %",
                                 [] (const Preset& p) { return p.data.perf.arp.gate * 100.0f; }, [] (Preset& p, float v) { p.data.perf.arp.gate = v * 0.01f; }), {});
    item (arpRow, new BoundKnob (ctx, "Swing", 0.0f, 60.0f, 1.0f, 0.0f, " %",
                                 [] (const Preset& p) { return p.data.perf.arp.swing * 100.0f; }, [] (Preset& p, float v) { p.data.perf.arp.swing = v * 0.01f; }), {});
    item (arpRow, new BoundToggle (ctx, "Reset", "Restart the arpeggio pattern when the host transport stops",
                                   [] (const Preset& p) { return p.data.perf.arp.resetOnStop; }, [] (Preset& p, bool v) { p.data.perf.arp.resetOnStop = v; }), {});

    // Chord
    item (chordRow, new BoundCombo (ctx, "Chord type", labels::list (labels::chordType),
                                    [] (const Preset& p) { return (int) p.data.perf.chord.type; }, [] (Preset& p, int v) { p.data.perf.chord.type = (ChordType) v; }), "Chord");
    item (chordRow, new BoundCombo (ctx, "Voicing", labels::list (labels::voicing),
                                    [] (const Preset& p) { return (int) p.data.perf.chord.voicing; }, [] (Preset& p, int v) { p.data.perf.chord.voicing = (Voicing) v; }), "Voicing");
    item (chordRow, new BoundKnob (ctx, "Octave", -2.0f, 2.0f, 1.0f, 0.0f, "",
                                   [] (const Preset& p) { return (float) p.data.perf.chord.octave; }, [] (Preset& p, float v) { p.data.perf.chord.octave = juce::roundToInt (v); }), {});
    item (chordRow, new BoundKnob (ctx, "Strum", 0.0f, 100.0f, 1.0f, 0.0f, " ms",
                                   [] (const Preset& p) { return p.data.perf.chord.strumMs; }, [] (Preset& p, float v) { p.data.perf.chord.strumMs = v; }), {});

    randomise.setTooltip ("A new random pattern (the first step always stays open)");
    randomise.onClick = [this] {
        const uint32_t seed = (uint32_t) juce::Time::getMillisecondCounter();
        ctx.edit ([seed] (Preset& p) {
            auto& gate = p.data.perf.gate;
            Pcg32 rng (seed);
            const auto randomStep = [&rng] { return rng.nextFloat() < 0.6f ? 1.0f : (rng.nextFloat() < 0.5f ? 0.0f : 0.5f); };
            for (auto& s : gate.steps) s = randomStep();
            gate.steps[0] = 1.0f;
            if (gate.stereo) for (auto& s : gate.stepsRight) s = randomStep();
            else gate.stepsRight = gate.steps;
        });
    };
    clear.setTooltip ("Open every step");
    clear.onClick = [this] { ctx.edit ([] (Preset& p) { p.data.perf.gate.steps.fill (1.0f); p.data.perf.gate.stepsRight.fill (1.0f); }); };
    invert.setTooltip ("Swap open and closed steps");
    invert.onClick = [this] { ctx.edit ([] (Preset& p) { for (auto& s : p.data.perf.gate.steps) s = 1.0f - s; for (auto& s : p.data.perf.gate.stepsRight) s = 1.0f - s; }); };
}

void RhythmTab::paint (juce::Graphics& g)
{
    paintPanel (g, gateArea, "Trance Gate");
    paintPanel (g, arpArea, "Arpeggiator");
    paintPanel (g, chordArea, "Chord");
}

void RhythmTab::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    gateArea = r.removeFromTop (r.getHeight() * 56 / 100);
    r.removeFromTop (8);
    arpArea = r.removeFromLeft (r.getWidth() * 62 / 100);
    r.removeFromLeft (8);
    chordArea = r;

    // A row of controls: rocker, then combos (caption above) and knobs (own caption).
    const auto layoutRow = [] (juce::Rectangle<int> row, juce::Component* power, std::vector<std::pair<juce::Component*, juce::Label*>>& items) {
        if (power != nullptr)
        {
            power->setBounds (row.removeFromLeft (58).withSizeKeepingCentre (58, juce::jmin (row.getHeight(), 92)));
            row.removeFromLeft (10);
        }
        int combos = 0, others = 0;
        for (auto& it : items) (dynamic_cast<juce::ComboBox*> (it.first) != nullptr ? combos : others)++;
        const int comboW = juce::jmin (150, juce::jmax (84, row.getWidth() / juce::jmax (1, combos + others)));
        const int otherW = others > 0 ? juce::jmin (104, (row.getWidth() - combos * (comboW + 8)) / others) : 0;
        for (auto& it : items)
        {
            if (dynamic_cast<juce::ComboBox*> (it.first) != nullptr)
            {
                auto cell = row.removeFromLeft (comboW).withSizeKeepingCentre (comboW, 52);
                if (it.second != nullptr) it.second->setBounds (cell.removeFromTop (16));
                cell.removeFromTop (8);
                it.first->setBounds (cell.removeFromTop (28));
                row.removeFromLeft (8);
            }
            else if (dynamic_cast<juce::ToggleButton*> (it.first) != nullptr)
                it.first->setBounds (row.removeFromLeft (otherW).withSizeKeepingCentre (58, juce::jmin (row.getHeight(), 92)));
            else
                it.first->setBounds (row.removeFromLeft (otherW).withSizeKeepingCentre (otherW - 4, juce::jmin (row.getHeight(), 98)));
        }
        return row;
    };

    auto gA = panelContent (gateArea, 12, 10);
    auto top = gA.removeFromTop (juce::jlimit (78, 100, gA.getHeight() * 36 / 100));
    // pattern chooser + pattern tools keep their room on the right; the row shares the rest
    auto rest = top.removeFromRight (juce::jlimit (230, 330, top.getWidth() * 28 / 100));
    top.removeFromRight (8);
    layoutRow (top, &gateOn, gateRow);
    auto buttons = rest.removeFromRight (80).withSizeKeepingCentre (80, 84);
    randomise.setBounds (buttons.removeFromTop (24));
    buttons.removeFromTop (6);
    clear.setBounds (buttons.removeFromTop (24));
    buttons.removeFromTop (6);
    invert.setBounds (buttons.removeFromTop (24));
    rest.removeFromRight (10);
    auto pat = rest.withSizeKeepingCentre (rest.getWidth(), 52);
    patternCaption.setBounds (pat.removeFromTop (16));
    pat.removeFromTop (8);
    patterns.setBounds (pat.removeFromTop (28));
    gA.removeFromTop (8);
    grid.setBounds (gA);

    layoutRow (panelContent (arpArea, 12, 10), &arpOn, arpRow);
    layoutRow (panelContent (chordArea, 12, 10), nullptr, chordRow);
}

void RhythmTab::refresh (const Preset& p)
{
    for (auto* b : bound) b->refreshFrom (p);
    patterns.refreshFrom (p);
    grid.refresh (p);
    if (widthKnob != nullptr) widthKnob->setAlpha (p.data.perf.gate.stereo ? 1.0f : 0.4f); // Width only matters in stereo
}
} // namespace ui
