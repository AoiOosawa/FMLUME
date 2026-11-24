#include "UI/Advanced/OperatorsTab.h"
#include "Core/Templates.h"

namespace ui
{
using namespace fmlume;

namespace
{
juce::String feedsText (const EnginePatch& e, int op)
{
    juce::String t;
    for (int i = 0; i < e.numEdges; ++i)
        if (e.edges[(size_t) i].from == op) t << (t.isEmpty() ? "" : ", ") << juce::String (e.edges[(size_t) i].to + 1);
    return t;
}

// True when assignCommonMacros would change what macros 1-4 point at.
bool commonMacrosNeedFit (const Preset& now)
{
    Preset fitted = now;
    assignCommonMacros (fitted.data);
    const auto same = [] (const Macro& a, const Macro& b) {
        if (a.numMappings != b.numMappings) return false;
        for (int k = 0; k < a.numMappings; ++k)
        {
            const auto& x = a.mappings[(size_t) k];
            const auto& y = b.mappings[(size_t) k];
            if (x.target != y.target || x.amount != y.amount || x.min != y.min || x.max != y.max || x.curve != y.curve || x.polarity != y.polarity) return false;
        }
        return true;
    };
    for (int i = 0; i < 4; ++i)
        if (! same (now.data.sound.macros[(size_t) i], fitted.data.sound.macros[(size_t) i])) return true;
    return false;
}

// Everything the template match and the macro fit depend on.
juce::String routingSignatureOf (const Preset& p)
{
    const auto& s = p.data.sound;
    juce::String sig;
    for (auto& op : s.engine.operators) sig << (op.enabled ? 'E' : 'e') << (op.carrierGain > 0.0f ? 'C' : 'c') << (int) op.waveform << (op.feedback > 0.0f ? 'F' : 'f');
    for (int i = 0; i < s.engine.numEdges; ++i) sig << ';' << (int) s.engine.edges[(size_t) i].from << '>' << (int) s.engine.edges[(size_t) i].to;
    sig << '|' << (int) s.engine.filter.type;
    for (int m = 0; m < 4; ++m)
        for (int k = 0; k < s.macros[(size_t) m].numMappings; ++k)
        {
            const auto& mp = s.macros[(size_t) m].mappings[(size_t) k];
            sig << ',' << mp.target << ':' << juce::String (mp.amount, 4) << ':' << juce::String (mp.min, 3) << ':' << juce::String (mp.max, 3) << (int) mp.curve << (int) mp.polarity;
        }
    return sig;
}

const char* const kFitTip = "Point Brightness / Attack / Release / Character (macros 1-4) at the current routing. "
                            "Choosing a template does this by itself; use it after connecting, removing or switching operators by hand.";
const char* const kFittedTip = "Macros 1-4 already fit the current routing. This becomes available after you change the connections by hand.";
} // namespace

// ================================================================================= tab
OperatorsTab::OperatorsTab (Ctx& c)
    : ctx (c), graph (c), index (c, -1, "FM Amount"),
      sideRatio (c, pc::op (0, kOpRatio), "Ratio"), sideLevel (c, pc::op (0, kOpLevel), "Level"),
      sideOutput (c, pc::op (0, kOpCarrierGain), "Output"), sideFeedback (c, pc::op (0, kOpFeedback), "Feedback"),
      envelope (c),
      waveform (c, "Waveform", labels::list (labels::waveform),
                [this] (const Preset& p) { return (int) selectedOperator (p, ctx).waveform; },
                [this] (Preset& p, int v) { selectedOperator (p, ctx).waveform = (Waveform) v; }),
      pitchMode (c, "Pitch mode", labels::list (labels::pitchMode),
                 [this] (const Preset& p) { return (int) selectedOperator (p, ctx).pitchMode; },
                 [this] (Preset& p, int v) {
                     auto& o = selectedOperator (p, ctx);
                     o.pitchMode = (PitchMode) v;
                     o.keyTracking = v == (int) PitchMode::Fixed ? 0.0f : 1.0f; // fixed frequencies ignore the keyboard by default
                 }),
      keySync (c, "Key sync", "On: the wave restarts at the Phase value on every note. Off: free-running phase.",
               [this] (const Preset& p) { return selectedOperator (p, ctx).keySync; },
               [this] (Preset& p, bool v) { selectedOperator (p, ctx).keySync = v; }),
      ratio (c, pc::op (0, kOpRatio), "Ratio"), detune (c, pc::op (0, kOpDetune), "Detune"), feedback (c, pc::op (0, kOpFeedback), "Feedback"),
      phase (c, pc::op (0, kOpPhaseOffset), "Phase"), attack (c, pc::op (0, kOpEnvA), "Attack"), decay (c, pc::op (0, kOpEnvD), "Decay"),
      sustain (c, pc::op (0, kOpEnvS), "Sustain"), release (c, pc::op (0, kOpEnvR), "Release"), level (c, pc::op (0, kOpLevel), "Level"),
      output (c, pc::op (0, kOpCarrierGain), "Output"), velLevel (c, pc::op (0, kOpVelToLevel), "Vel > Level"),
      velFm (c, pc::op (0, kOpVelToIndex), "Vel > Tone"), keyTrack (c, pc::op (0, kOpKeyTracking), "Key Track")
{
    graph.onSelection = [this] { select (ctx.selectedOp); };
    expand.setTooltip ("Give the whole page to the routing (bigger boxes, all 18 templates). Click again to return.");
    expand.onClick = [this] { setExpanded (! expanded); };
    strip.onChoose = [this] (int i) { applyTemplate (i); };
    templates.setTooltip ("Choose one of the 18 routing templates. The operators keep their tone settings.");
    templates.onClick = [this] { showTemplates(); };
    styleLabel (algoName, 13.0f, col::accent, juce::Justification::centredRight);
    algoName.setFont (font (13.0f, true));
    for (auto* l : { &opTitle, &edgeTitle })
    {
        styleLabel (*l, 14.0f, col::text);
        l->setFont (font (14.0f, true));
    }
    opEnabled.setTooltip ("Switch this operator on or off");
    opEnabled.onClick = [this] {
        const bool v = opEnabled.getToggleState();
        const int op = ctx.selectedOp;
        if (! ctx.run ([=] (PatchController& pc) { return pc.setOperatorEnabled (op, v); })) refresh (ctx.snapshot());
    };
    index.setAccent (col::accentB);
    removeEdge.setTooltip ("Remove the selected connection");
    removeEdge.onClick = [this] {
        if (graph.selectedEdgeFrom < 0) return;
        const int from = graph.selectedEdgeFrom, to = graph.selectedEdgeTo;
        ctx.run ([=] (PatchController& pc) { return pc.removeEdge (from, to); });
    };
    fitMacrosButton.setTooltip (kFitTip);
    fitMacrosButton.onClick = [this] { fitMacros(); };
    snap.setToggleState (ctx.snapRatios, juce::dontSendNotification);
    snap.setTooltip ("When on, the Ratio knob lands on multiples of 0.5 (1/8 steps below 1). Type a value for anything else.");
    snap.onClick = [this] { ctx.snapRatios = snap.getToggleState(); };
    styleLabel (role, 12.5f, col::textDim, juce::Justification::topLeft);
    role.setMinimumHorizontalScale (0.85f);

    operatorPanelParts = { &wave, &envelope, &waveform, &pitchMode, &keySync, &snap, &ratio, &detune, &feedback, &phase,
                           &attack, &decay, &sustain, &release, &level, &output, &velLevel, &velFm, &keyTrack };
    for (auto* comp : std::initializer_list<juce::Component*> { &graph, &expand, &templates, &algoName, &opTitle, &opEnabled, &role, &edgeTitle,
                                                                 &index, &removeEdge, &fitMacrosButton })
        addAndMakeVisible (comp);
    for (auto* comp : operatorPanelParts) addAndMakeVisible (comp);
    for (auto* comp : std::initializer_list<juce::Component*> { &strip, &sideRatio, &sideLevel, &sideOutput, &sideFeedback })
        addChildComponent (comp); // expanded mode only
}

void OperatorsTab::paint (juce::Graphics& g)
{
    paintPanel (g, routingArea, "Routing");
    drawRoutingLegend (g, juce::Rectangle<int> (routingArea.getX() + 118, routingArea.getY() + 10, juce::jmax (0, legendRight - routingArea.getX() - 118), 16));
    g.setColour (col::outline.withAlpha (0.7f));
    g.drawVerticalLine (sideArea.getX() - 7, (float) sideArea.getY() + 4.0f, (float) sideArea.getBottom() - 4.0f);
    if (expanded) return;
    const juce::String n = "OP " + juce::String (ctx.selectedOp + 1);
    paintPanel (g, oscArea, n + "  Oscillator");
    paintPanel (g, envArea, n + "  Envelope");
    paintPanel (g, levelArea, n + "  Level & Response");
}

namespace
{
juce::Rectangle<int> inner (juce::Rectangle<int> a) { return panelContent (a, 12, 10, 2); }

// Heights in the oscillator panel besides its graph and its knob row.
constexpr int kComboRow = 26, kToggleRow = 22, kGap = 6, kGraphGap = 8;
constexpr int kOscFixed = kGraphGap + kComboRow + kGap + 4 + kToggleRow; // under the graph: combos, gaps, toggles
constexpr int kMinKnobRow = 66, kMaxKnobRow = 104;
constexpr int kMinRouting = 200; // the routing band never gets less than this

// Takes the graph's strip off the top of `area` and returns the graph bounds inside it (fixed aspect).
juce::Rectangle<int> takeGraph (juce::Rectangle<int>& area, float aspect, int controlsBelow)
{
    const int h = juce::jmax (30, juce::jmin (juce::roundToInt ((float) area.getWidth() / aspect), area.getHeight() - controlsBelow));
    const auto band = area.removeFromTop (h);
    return band.withSizeKeepingCentre (juce::jmin (band.getWidth(), juce::roundToInt ((float) h * aspect)), h);
}
} // namespace

void OperatorsTab::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    if (expanded)
    {
        routingArea = r;
        oscArea = envArea = levelArea = {};
    }
    else
    {
        // Bottom row: oscillator | envelope | level, exactly as tall as their contents (the graphs keep
        // their shape, so this row has a natural height). The rest goes to the routing.
        const int colW = (r.getWidth() - 16) / 3;
        const auto probe = juce::Rectangle<int> (0, 0, colW, 1000);
        const int chrome = probe.getHeight() - inner (probe).getHeight(); // panel title + padding
        const int waveH = juce::roundToInt ((float) inner (probe).getWidth() / WaveView::kAspect);
        int knobRow = kMaxKnobRow;
        while (knobRow > kMinKnobRow && r.getHeight() - (waveH + kOscFixed + knobRow + chrome) - 8 < kMinRouting) knobRow -= 2;
        // still too little room: the graphs give way (keeping their shape)
        const int rowH = juce::jmax (chrome + 150, juce::jmin (waveH + kOscFixed + knobRow + chrome, r.getHeight() - 8 - kMinRouting));
        auto bottom = r.removeFromBottom (rowH);
        r.removeFromBottom (8);
        routingArea = r;
        layoutOperatorPanels (bottom);
    }
    layoutRouting (routingArea);
}

void OperatorsTab::layoutRouting (juce::Rectangle<int> area)
{
    auto head = area.reduced (12, 0).removeFromTop (34).withTrimmedTop (6);
    expand.setBounds (head.removeFromRight (96).withSizeKeepingCentre (96, 24));
    head.removeFromRight (6);
    if (! expanded)
    {
        templates.setBounds (head.removeFromRight (116).withSizeKeepingCentre (116, 24));
        head.removeFromRight (10);
    }
    algoName.setBounds (head.removeFromRight (juce::jmin (260, head.getWidth() / 3)));
    legendRight = algoName.getX() - 12;

    auto top = inner (area);
    if (expanded)
    {
        strip.setBounds (top.removeFromTop (juce::jlimit (64, 92, top.getHeight() * 16 / 100)));
        top.removeFromTop (8);
    }
    sideArea = top.removeFromRight (juce::jlimit (220, 300, top.getWidth() * 24 / 100));
    top.removeFromRight (14);
    graph.setBounds (top);
    layoutSide (sideArea);
}

void OperatorsTab::layoutSide (juce::Rectangle<int> side)
{
    auto row = side.removeFromTop (24);
    opEnabled.setBounds (row.removeFromRight (64));
    opTitle.setBounds (row);
    side.removeFromTop (2);
    fitMacrosButton.setBounds (side.removeFromBottom (24));
    side.removeFromBottom (6);
    // the connection: its title and the depth knob come before the description, which may be dropped
    const int knobRow = juce::jlimit (0, 78, side.getHeight() - 22 - 4);
    edgeKnobFits = knobRow >= 56;
    auto edgeRow = side.removeFromBottom (knobRow);
    index.setBounds (edgeRow.removeFromLeft (juce::jmin (96, edgeRow.getWidth() / 2)));
    removeEdge.setBounds (edgeRow.withSizeKeepingCentre (juce::jmin (edgeRow.getWidth() - 10, 110), 24));
    edgeTitle.setBounds (side.removeFromBottom (22));
    side.removeFromBottom (2);
    if (expanded)
    {
        // what the selected operator does in the routing: its description, then 2 x 2 knobs right under it
        const int knobsH = juce::jmax (0, juce::jmin (side.getHeight() - 40, 2 * 100));
        role.setBounds (side.removeFromTop (juce::jmax (0, juce::jmin (80, side.getHeight() - knobsH - 8))));
        side.removeFromTop (8);
        auto knobs = side.removeFromTop (knobsH);
        auto upper = knobs.removeFromTop (knobs.getHeight() / 2);
        layoutKnobRow (upper.withTrimmedBottom (4), { &sideRatio, &sideLevel }, 100);
        layoutKnobRow (knobs.withTrimmedTop (4), { &sideOutput, &sideFeedback }, 100);
    }
    else role.setBounds (side);
    updateSideVisibility();
}

void OperatorsTab::layoutOperatorPanels (juce::Rectangle<int> bottom)
{
    const int colW = (bottom.getWidth() - 16) / 3;
    oscArea = bottom.removeFromLeft (colW);
    bottom.removeFromLeft (8);
    envArea = bottom.removeFromLeft (colW);
    bottom.removeFromLeft (8);
    levelArea = bottom;

    auto o = inner (oscArea);
    wave.setBounds (takeGraph (o, WaveView::kAspect, kOscFixed + kMinKnobRow));
    o.removeFromTop (kGraphGap);
    auto combos = o.removeFromTop (kComboRow);
    waveform.setBounds (combos.removeFromLeft (combos.getWidth() * 40 / 100));
    combos.removeFromLeft (kGap);
    pitchMode.setBounds (combos);
    o.removeFromTop (kGap);
    auto toggles = o.removeFromBottom (kToggleRow);
    keySync.setBounds (toggles.removeFromLeft (toggles.getWidth() / 2));
    snap.setBounds (toggles);
    o.removeFromBottom (4);
    layoutKnobRow (o, { &ratio, &detune, &feedback, &phase }, 100);

    auto e = inner (envArea);
    envelope.setBounds (takeGraph (e, EnvelopeView::kAspect, kGraphGap + kMinKnobRow));
    e.removeFromTop (kGraphGap);
    layoutKnobRow (e, { &attack, &decay, &sustain, &release }, 100);

    auto l = inner (levelArea);
    layoutKnobRow (l.removeFromTop ((l.getHeight() - kGap) / 2), { &level, &output }, 120);
    l.removeFromTop (kGap);
    layoutKnobRow (l, { &velLevel, &velFm, &keyTrack }, 100);
}

// The side panel's optional parts, from the current selection and the space the layout gave them.
void OperatorsTab::updateSideVisibility()
{
    const bool edge = graph.selectedEdgeFrom >= 0;
    index.setVisible (edge && edgeKnobFits);
    removeEdge.setVisible (edge);
    role.setVisible (role.getHeight() >= 30);
    for (auto* k : { &sideRatio, &sideLevel, &sideOutput, &sideFeedback }) k->setVisible (expanded);
}

// Macros 1-4 are re-assigned from the routing (assignCommonMacros). Whatever they point at now is
// replaced, including targets set by hand in the Macro Editor, so that is confirmed first.
void OperatorsTab::fitMacros()
{
    const Preset now = ctx.snapshot();
    if (! commonMacrosNeedFit (now)) { ctx.say ("Macros 1-4 already fit the current routing."); return; }

    juce::String names;
    for (int i = 0; i < 4; ++i)
    {
        juce::String n = utf8 (now.meta.macroNames[(size_t) i]).trim();
        names << (i ? ", " : "") << (n.isEmpty() ? "Macro " + juce::String (i + 1) : n);
    }
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Fit macros 1-4 to the routing?")
                                      .withMessage ("What " + names + " control will be re-assigned for the current routing.\n"
                                                    "Targets you set for these four in the Macro Editor are replaced. Macros 5-8 and the knob positions stay as they are.")
                                      .withButton ("Fit", 1)
                                      .withButton ("Cancel", 0)
                                      .withAssociatedComponent (this),
                                  [safe = juce::Component::SafePointer<OperatorsTab> (this)] (int r) {
                                      if (safe == nullptr || r != 1) return;
                                      if (safe->ctx.edit ([] (Preset& p) { assignCommonMacros (p.data); }))
                                          safe->ctx.say ("Macros 1-4 now follow the current routing.");
                                  });
}

void OperatorsTab::setExpanded (bool e)
{
    expanded = e;
    for (auto* c : operatorPanelParts) c->setVisible (! expanded);
    strip.setVisible (expanded);
    templates.setVisible (! expanded);
    expand.setButtonText (expanded ? "Collapse" : "Expand");
    graph.maxBox = expanded ? 96.0f : 76.0f;
    resized();
    refresh (ctx.snapshot());
}

void OperatorsTab::applyTemplate (int i)
{
    if (ctx.run ([=] (PatchController& pc) { return pc.applyAlgorithmTemplate (i); }))
        ctx.say (juce::String ("Routing: ") + algorithmTemplate (i).name + ". Macros 1-4 were re-assigned for the new routing.");
}

void OperatorsTab::showTemplates()
{
    // Shown inside the editor (not as a separate desktop window): it then uses the editor's look,
    // shows its tooltips, and goes away with the editor.
    auto* parent = getTopLevelComponent();
    auto gallery = std::make_unique<AlgoGallery>();
    gallery->columns = 6;
    gallery->setSize (6 * 92, 3 * 84);
    gallery->current = matchAlgorithmShape (ctx.snapshot().data.sound.engine);
    auto* g = gallery.get();
    gallery->onChoose = [safe = juce::Component::SafePointer<OperatorsTab> (this), g] (int i) {
        if (safe != nullptr) safe->applyTemplate (i);
        if (auto* box = g->findParentComponentOfClass<juce::CallOutBox>()) box->dismiss();
    };
    juce::CallOutBox::launchAsynchronously (std::move (gallery), parent->getLocalArea (&templates, templates.getLocalBounds()), parent);
}

void OperatorsTab::select (int op)
{
    ctx.selectedOp = juce::jlimit (0, kNumOps - 1, op);
    refresh (ctx.snapshot());
}

void OperatorsTab::bindOperator (int op, PitchMode mode)
{
    const bool ratioMode = mode == PitchMode::Ratio;
    for (auto* k : { &ratio, &sideRatio })
    {
        k->setCode (pc::op (op, ratioMode ? kOpRatio : kOpFixedHz));
        k->setCaption (ratioMode ? "Ratio" : "Fixed Hz");
    }
    const std::pair<ModelKnob*, int> knobs[] = { { &detune, kOpDetune }, { &feedback, kOpFeedback }, { &phase, kOpPhaseOffset },
                                                 { &attack, kOpEnvA }, { &decay, kOpEnvD }, { &sustain, kOpEnvS }, { &release, kOpEnvR },
                                                 { &level, kOpLevel }, { &output, kOpCarrierGain }, { &velLevel, kOpVelToLevel },
                                                 { &velFm, kOpVelToIndex }, { &keyTrack, kOpKeyTracking },
                                                 { &sideLevel, kOpLevel }, { &sideOutput, kOpCarrierGain }, { &sideFeedback, kOpFeedback } };
    for (auto& [knob, field] : knobs) knob->setCode (pc::op (op, field));
}

void OperatorsTab::refresh (const Preset& p)
{
    const auto& e = p.data.sound.engine;
    const int op = juce::jlimit (0, kNumOps - 1, ctx.selectedOp);
    const auto& o = e.operators[(size_t) op];
    if (ratio.getCode() != pc::op (op, o.pitchMode == PitchMode::Ratio ? kOpRatio : kOpFixedHz) || detune.getCode() != pc::op (op, kOpDetune))
        bindOperator (op, o.pitchMode);

    refreshRouting (p);
    refreshSide (p, op);
    for (auto* k : { &ratio, &detune, &feedback, &phase, &attack, &decay, &sustain, &release, &level, &output, &velLevel, &velFm, &keyTrack,
                     &sideRatio, &sideLevel, &sideOutput, &sideFeedback })
    {
        if (! k->isVisible()) continue; // hidden knobs are refreshed when they are shown again (setExpanded refreshes)
        k->refresh();
        k->setAlpha (o.enabled ? 1.0f : 0.45f);
    }
    if (! expanded)
    {
        waveform.refreshFrom (p);
        pitchMode.refreshFrom (p);
        keySync.refreshFrom (p);
        wave.state = o;
        wave.repaint();
        envelope.refresh (p);
    }
    repaint();
}

void OperatorsTab::refreshRouting (const Preset& p)
{
    graph.refresh (p);
    const juce::String sig = routingSignatureOf (p);
    if (sig != routingSignature)
    {
        routingSignature = sig;
        templateMatch = matchAlgorithmShape (p.data.sound.engine);
        macrosNeedFit = commonMacrosNeedFit (p);
    }
    algoName.setText (templateMatch < 0 ? juce::String ("Custom routing") : "Algorithm " + juce::String (templateMatch + 1) + "  " + algorithmTemplate (templateMatch).name,
                      juce::dontSendNotification);
    algoName.setColour (juce::Label::textColourId, templateMatch < 0 ? col::warm : col::accent);
    if (strip.current != templateMatch) { strip.current = templateMatch; strip.repaint(); }
    // only clickable when it would change something (a click that does nothing looks broken)
    fitMacrosButton.setEnabled (macrosNeedFit);
    fitMacrosButton.setTooltip (macrosNeedFit ? kFitTip : kFittedTip);
}

void OperatorsTab::refreshSide (const Preset& p, int op)
{
    const auto& e = p.data.sound.engine;
    const auto& o = e.operators[(size_t) op];
    opTitle.setText ("OP " + juce::String (op + 1) + (o.enabled ? (o.carrierGain > 0.0f ? "  carrier" : "  modulator") : "  off"), juce::dontSendNotification);
    opEnabled.setToggleState (o.enabled, juce::dontSendNotification);

    const bool edge = graph.selectedEdgeFrom >= 0;
    const int edgeCode = edge ? pc::edge (graph.selectedEdgeFrom, graph.selectedEdgeTo) : -1;
    if (index.getCode() != edgeCode) index.setCode (edgeCode);
    edgeTitle.setText (edge ? "Connection " + juce::String (graph.selectedEdgeFrom + 1) + " > " + juce::String (graph.selectedEdgeTo + 1)
                            : juce::String ("Drag between boxes to connect. Click a line to edit it."), juce::dontSendNotification);
    edgeTitle.setColour (juce::Label::textColourId, edge ? col::text : col::textDim);
    edgeTitle.setFont (font (edge ? 14.0f : 12.0f, edge));
    updateSideVisibility();
    if (edge) index.refresh();

    const juce::String feeds = feedsText (e, op);
    juce::String t;
    if (! o.enabled) t = "Switched off: not part of the sound. Turn it on with the switch above.";
    else if (o.carrierGain > 0.0f)
        t = "CARRIER: you hear this operator directly. Level and Output set how loud it is; its envelope shapes the volume."
            + (feeds.isNotEmpty() ? " It also modulates OP " + feeds + "." : juce::String());
    else if (feeds.isNotEmpty())
        t = "MODULATOR: not heard by itself. It colours OP " + feeds + ". Level = how much brightness it adds; its envelope shapes how the tone changes over time; Ratio sets the character.";
    else t = "On but not connected to anything. Drag from it to another box, or raise Output to hear it.";
    role.setText (t, juce::dontSendNotification);
    role.setColour (juce::Label::textColourId, o.enabled ? col::text.withAlpha (0.85f) : col::textDim);
}
} // namespace ui
