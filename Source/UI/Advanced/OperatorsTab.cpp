#include "UI/Advanced/OperatorsTab.h"
#include "Core/Templates.h"

namespace ui
{
using namespace fmlume;

namespace
{
float timeNorm (int code, float seconds) { return toNorm (paramDesc (code), seconds); }
float timeFromNorm (int code, float n) { return fromNorm (paramDesc (code), juce::jlimit (0.0f, 1.0f, n)); }
static_assert (kOpEnvD == kOpEnvA + 1 && kOpEnvS == kOpEnvA + 2 && kOpEnvR == kOpEnvA + 3, "ADSR fields are consecutive");
static_assert (pc::modEnvD == pc::modEnvA + 1 && pc::modEnvS == pc::modEnvA + 2 && pc::modEnvR == pc::modEnvA + 3, "Mod Envelope codes are consecutive");
// Envelope graph geometry: each timed stage may take up to `kStage` of the width.
constexpr float kStage = 0.27f, kHold = 0.17f, kMinStage = 0.04f;
// Width : height of the two graphs. Fixed, so a curve looks the same at every window size.
constexpr float kWaveAspect = 3.0f, kEnvAspect = 2.4f;
float stageWidth (float norm, float w) { return (kMinStage + (1.0f - kMinStage) * norm) * w * kStage; }
float stageNorm (float width, float w) { return (width / (w * kStage) - kMinStage) / (1.0f - kMinStage); }

juce::String feedsText (const EnginePatch& e, int op)
{
    juce::String t;
    for (int i = 0; i < e.numEdges; ++i)
        if (e.edges[(size_t) i].from == op) t << (t.isEmpty() ? "" : ", ") << juce::String (e.edges[(size_t) i].to + 1);
    return t;
}
}

// ================================================================================= EnvelopeView
std::array<juce::Point<float>, 3> EnvelopeView::drawEnvelope (juce::Graphics& g, juce::Rectangle<float> r, const ADSR& env, juce::Colour colour, bool fill, float lineWidth, int firstCode)
{
    const float w = r.getWidth(), top = r.getY(), bottom = r.getBottom();
    const float x0 = r.getX();
    const float x1 = x0 + stageWidth (timeNorm (firstCode, env.a), w);
    const float x2 = x1 + stageWidth (timeNorm (firstCode + 1, env.d), w);
    const float x3 = x2 + w * kHold;
    const float x4 = x3 + stageWidth (timeNorm (firstCode + 3, env.r), w);
    const float ys = bottom - juce::jlimit (0.0f, 1.0f, env.s) * r.getHeight();
    juce::Path p;
    p.startNewSubPath (x0, bottom);
    p.lineTo (x1, top);
    p.quadraticTo (x1 + (x2 - x1) * 0.3f, ys, x2, ys);
    p.lineTo (x3, ys);
    p.quadraticTo (x3 + (x4 - x3) * 0.3f, bottom, x4, bottom);
    if (fill)
    {
        juce::Path area (p);
        area.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.32f), 0.0f, top, colour.withAlpha (0.03f), 0.0f, bottom, false));
        g.fillPath (area);
    }
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    return { juce::Point<float> (x1, top), juce::Point<float> (x2, ys), juce::Point<float> (x4, bottom) };
}

EnvelopeView::EnvelopeView (Ctx& c, Source s) : ctx (c), source (s), colour (col::violet)
{
    setTooltip ("Drag the dots: first = attack time, second = decay time (sideways) and sustain level (up/down), third = release time.");
}

void EnvelopeView::refresh (const Preset& p)
{
    const auto& e = p.data.sound.engine;
    if (source == Source::ModEnvelope) { env = e.modEnv; colour = col::violet; }
    else
    {
        const auto& op = e.operators[(size_t) ctx.selectedOp];
        env = op.env;
        colour = op.enabled ? roleColour (op) : col::textDim;
    }
    repaint();
}

int EnvelopeView::firstCode() const { return source == Source::ModEnvelope ? pc::modEnvA : pc::op (ctx.selectedOp, kOpEnvA); }

void EnvelopeView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (col::outline.withAlpha (0.5f));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine (juce::roundToInt (bounds.getY() + bounds.getHeight() * (float) i / 4.0f), bounds.getX() + 4.0f, bounds.getRight() - 4.0f);
    const auto r = bounds.reduced (12.0f, 12.0f);
    const juce::Colour c = colour;
    handles = drawEnvelope (g, r, env, c, true, 2.4f, firstCode());
    static const char* const names[3] = { "A", "D / S", "R" };
    for (int i = 0; i < 3; ++i)
    {
        const bool hot = i == dragging || i == hover;
        g.setColour (col::bg);
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (handles[(size_t) i]));
        g.setColour (hot ? col::text : c);
        g.drawEllipse (juce::Rectangle<float> (hot ? 13.0f : 11.0f, hot ? 13.0f : 11.0f).withCentre (handles[(size_t) i]), 2.2f);
        g.setColour (col::textDim);
        g.setFont (font (10.5f, true));
        if (i == 0) g.drawText (names[i], juce::Rectangle<float> (handles[0].x + 10.0f, handles[0].y - 4.0f, 20.0f, 13.0f), juce::Justification::centredLeft);
        else g.drawText (names[i], juce::Rectangle<float> (handles[(size_t) i].x - 22.0f, handles[(size_t) i].y - 22.0f, 44.0f, 13.0f), juce::Justification::centred);
    }
}

int EnvelopeView::handleAt (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 16.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float d = handles[(size_t) i].getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void EnvelopeView::mouseMove (const juce::MouseEvent& e)
{
    const int h = handleAt (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void EnvelopeView::mouseDown (const juce::MouseEvent& e) { dragging = handleAt (e.position); repaint(); }

void EnvelopeView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    const auto r = getLocalBounds().toFloat().reduced (12.0f, 12.0f);
    const float w = r.getWidth();
    const int code = firstCode();
    const bool mod = source == Source::ModEnvelope;
    const int op = ctx.selectedOp;
    if (dragging == 0)
        ctx.set (code, timeFromNorm (code, stageNorm (e.position.x - r.getX(), w)));
    else if (dragging == 1)
    {
        // both values in one validated edit
        const float decay = timeFromNorm (code + 1, stageNorm (e.position.x - handles[0].x, w));
        const float sustain = juce::jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight());
        ctx.edit ([=] (Preset& p) {
            auto& target = mod ? p.data.sound.engine.modEnv : p.data.sound.engine.operators[(size_t) op].env;
            target.d = decay;
            target.s = sustain;
        });
    }
    else
        ctx.set (code + 3, timeFromNorm (code + 3, stageNorm (e.position.x - (handles[1].x + w * kHold), w)));
}

void EnvelopeView::mouseUp (const juce::MouseEvent&)
{
    dragging = -1;
    repaint();
}

// ================================================================================= WaveView
void WaveView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (bounds, 6.0f);
    const auto r = bounds.reduced (8.0f, 8.0f);
    g.setColour (col::outline.withAlpha (0.6f));
    g.drawHorizontalLine (juce::roundToInt (r.getCentreY()), r.getX(), r.getRight());
    const int n = juce::jmax (64, (int) r.getWidth());
    juce::Path p;
    float y1 = 0.0f, y2 = 0.0f;
    for (int i = 0; i <= n; ++i)
    {
        float ph = 2.0f * (float) i / (float) n + state.phaseOffset; // two cycles
        ph -= std::floor (ph);
        float y = 0.0f;
        switch (state.waveform)
        {
            case Waveform::Triangle: y = ph < 0.25f ? ph * 4.0f : (ph < 0.75f ? 2.0f - ph * 4.0f : ph * 4.0f - 4.0f); break;
            case Waveform::Saw: y = 2.0f * ph - 1.0f; break;
            case Waveform::Pulse: y = ph < 0.5f ? 1.0f : -1.0f; break;
            default: y = std::sin (juce::MathConstants<float>::twoPi * ph + state.feedback * 0.5f * (y1 + y2)); break;
        }
        y2 = y1;
        y1 = y;
        const juce::Point<float> pt (r.getX() + r.getWidth() * (float) i / (float) n, r.getCentreY() - y * r.getHeight() * 0.46f);
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    const juce::Colour c = state.enabled ? roleColour (state) : col::textDim;
    g.setGradientFill (juce::ColourGradient (c, r.getX(), 0.0f, c.interpolatedWith (col::hot, 0.45f), r.getRight(), 0.0f, false));
    g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ================================================================================= tab
OperatorsTab::OperatorsTab (Ctx& c)
    : ctx (c), graph (c), index (c, -1, "FM Amount"),
      sideRatio (c, pc::op (0, kOpRatio), "Ratio"), sideLevel (c, pc::op (0, kOpLevel), "Level"),
      sideOutput (c, pc::op (0, kOpCarrierGain), "Output"), sideFeedback (c, pc::op (0, kOpFeedback), "Feedback"), envelope (c),
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
    graph.showLegend = false; // drawn in the panel header
    expand.setTooltip ("Give the whole page to the routing (bigger boxes, all 18 templates). Click again to return.");
    expand.onClick = [this] { setExpanded (! expanded); };
    strip.onChoose = [this] (int i) {
        if (ctx.run ([=] (PatchController& pc) { return pc.applyAlgorithmTemplate (i); }))
            ctx.say (juce::String ("Routing: ") + algorithmTemplate (i).name + ". Macros 1-4 were re-assigned for the new routing.");
    };
    templates.setTooltip ("Choose one of the 18 routing templates. The operators keep their tone settings.");
    templates.onClick = [this] { showTemplates(); };
    styleLabel (algoName, 13.0f, col::accent);
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
    resetMacros.setTooltip ("Point Brightness / Attack / Release / Character (macros 1-4) at the current routing. "
                            "Choosing a template does this by itself; use it after connecting, removing or switching operators by hand.");
    resetMacros.onClick = [this] { fitMacros(); };
    for (auto* comp : std::initializer_list<juce::Component*> { &graph, &strip, &expand, &templates, &algoName, &opTitle, &opEnabled, &edgeTitle, &index, &sideRatio, &sideLevel, &sideOutput, &sideFeedback, &removeEdge, &resetMacros, &wave, &envelope, &waveform, &pitchMode, &keySync, &snap, &role,
                                                                 &ratio, &detune, &feedback, &phase, &attack, &decay, &sustain, &release,
                                                                 &level, &output, &velLevel, &velFm, &keyTrack })
        addAndMakeVisible (comp);
    snap.setToggleState (ctx.snapRatios, juce::dontSendNotification);
    snap.setTooltip ("When on, the Ratio knob lands on multiples of 0.5 (1/8 steps below 1). Type a value for anything else.");
    snap.onClick = [this] { ctx.snapRatios = snap.getToggleState(); };
    styleLabel (role, 12.5f, col::textDim, juce::Justification::topLeft);
    role.setJustificationType (juce::Justification::topLeft);
    role.setMinimumHorizontalScale (0.85f);
}

void OperatorsTab::paint (juce::Graphics& g)
{
    const juce::String n = "OP " + juce::String (ctx.selectedOp + 1);
    paintPanel (g, routingArea, "Routing");
    GraphView::drawLegend (g, routingArea.withTrimmedLeft (118).removeFromTop (34).withTrimmedTop (10).withHeight (16).withWidth (400));
    g.setColour (col::outline.withAlpha (0.7f));
    g.drawVerticalLine (sideArea.getX() - 7, (float) sideArea.getY() + 4.0f, (float) sideArea.getBottom() - 4.0f);
    if (expanded) return;
    paintPanel (g, oscArea, n + "  Oscillator");
    paintPanel (g, envArea, n + "  Envelope");
    paintPanel (g, levelArea, n + "  Level & Response");
}

void OperatorsTab::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    const auto inner = [] (juce::Rectangle<int> a) { return panelContent (a, 12, 10, 2); };

    // Bottom row: oscillator | envelope | level & response, exactly as tall as their contents.
    // The graphs keep their shape at every window size (kWaveAspect / kEnvAspect), so this row
    // has a natural height; whatever is left above it goes to the routing diagram, which looks
    // right at any size.
    const int colW = (r.getWidth() - 16) / 3;
    const int innerW = inner (juce::Rectangle<int> (0, 0, colW, 400)).getWidth();
    const int chrome = 400 - inner (juce::Rectangle<int> (0, 0, colW, 400)).getHeight(); // title + padding of a panel
    const int waveH = juce::roundToInt ((float) innerW / kWaveAspect);
    const int fixedOsc = waveH + 8 + 26 + 6 + 4 + 22; // graph, combos, toggles and the gaps (knob row comes on top)
    int knobH = 104;
    const int minRouting = 200;
    while (knobH > 66 && r.getHeight() - (fixedOsc + knobH + chrome) - 8 < minRouting) knobH -= 2;
    // still too little room for the diagram: the graphs give way (keeping their shape)
    const int rowH = juce::jmax (chrome + 150, juce::jmin (fixedOsc + knobH + chrome, r.getHeight() - 8 - minRouting));
    auto bottom = expanded ? juce::Rectangle<int>() : r.removeFromBottom (rowH);
    if (! expanded) r.removeFromBottom (8);
    routingArea = r;
    for (auto* c : std::initializer_list<juce::Component*> { &wave, &envelope, &waveform, &pitchMode, &keySync, &snap, &ratio, &detune, &feedback, &phase,
                                                             &attack, &decay, &sustain, &release, &level, &output, &velLevel, &velFm, &keyTrack })
        c->setVisible (! expanded);
    strip.setVisible (expanded);
    for (auto* k : { &sideRatio, &sideLevel, &sideOutput, &sideFeedback }) k->setVisible (expanded);
    templates.setVisible (! expanded);
    expand.setButtonText (expanded ? "Collapse" : "Expand");
    graph.maxBox = expanded ? 96.0f : 76.0f;
    oscArea = bottom.removeFromLeft (colW);
    bottom.removeFromLeft (8);
    envArea = bottom.removeFromLeft (colW);
    bottom.removeFromLeft (8);
    levelArea = bottom;

    // Routing band: the editable diagram, and beside it the selected operator / connection.
    auto top = inner (routingArea);
    {
        auto head = routingArea.reduced (12, 0).removeFromTop (34).withTrimmedTop (6);
        expand.setBounds (head.removeFromRight (96).withSizeKeepingCentre (96, 24));
        head.removeFromRight (6);
        if (! expanded) { templates.setBounds (head.removeFromRight (116).withSizeKeepingCentre (116, 24)); head.removeFromRight (10); }
        algoName.setBounds (head.removeFromRight (juce::jmin (260, head.getWidth() / 3)));
    }
    if (expanded)
    {
        strip.setBounds (top.removeFromTop (juce::jlimit (64, 92, top.getHeight() * 16 / 100)));
        top.removeFromTop (8);
    }
    sideArea = top.removeFromRight (juce::jlimit (220, 300, top.getWidth() * 24 / 100));
    top.removeFromRight (14);
    graph.setBounds (top);
    auto side = sideArea;
    auto row = side.removeFromTop (24);
    opEnabled.setBounds (row.removeFromRight (64));
    opTitle.setBounds (row);
    side.removeFromTop (2);
    resetMacros.setBounds (side.removeFromBottom (24));
    side.removeFromBottom (6);
    const int knobRow = juce::jlimit (0, 78, side.getHeight() - 24 - 26);
    auto edgeRow = side.removeFromBottom (knobRow);
    index.setBounds (edgeRow.removeFromLeft (juce::jmin (96, edgeRow.getWidth() / 2)));
    removeEdge.setBounds (edgeRow.withSizeKeepingCentre (juce::jmin (edgeRow.getWidth() - 10, 110), 24));
    index.setVisible (index.isVisible() && knobRow >= 56);
    edgeTitle.setBounds (side.removeFromBottom (22));
    side.removeFromBottom (2);
    if (expanded)
    {
        // what the selected operator does in the routing: its description, then 2 x 2 knobs right under it
        const int knobsH = juce::jmin (side.getHeight() - 40, 2 * 100);
        role.setBounds (side.removeFromTop (juce::jmin (80, side.getHeight() - juce::jmax (0, knobsH) - 8)));
        side.removeFromTop (8);
        auto knobs = side.removeFromTop (juce::jmax (0, knobsH));
        auto upper = knobs.removeFromTop (knobs.getHeight() / 2);
        layoutKnobRow (upper.withTrimmedBottom (4), { &sideRatio, &sideLevel }, 100);
        layoutKnobRow (knobs.withTrimmedTop (4), { &sideOutput, &sideFeedback }, 100);
    }
    else role.setBounds (side);
    side = role.getBounds();
    role.setVisible (side.getHeight() >= 30);

    // Takes the graph's strip off the top of `area` and returns the graph bounds inside it.
    const auto takeGraph = [] (juce::Rectangle<int>& area, float aspect, int controls) {
        const int h = juce::jmax (30, juce::jmin (juce::roundToInt ((float) area.getWidth() / aspect), area.getHeight() - controls));
        const auto strip = area.removeFromTop (h);
        return strip.withSizeKeepingCentre (juce::jmin (strip.getWidth(), juce::roundToInt ((float) h * aspect)), h);
    };
    auto o = inner (oscArea);
    wave.setBounds (takeGraph (o, kWaveAspect, 8 + 26 + 6 + 66 + 4 + 22));
    o.removeFromTop (8);
    auto combos = o.removeFromTop (26);
    waveform.setBounds (combos.removeFromLeft (combos.getWidth() * 40 / 100));
    combos.removeFromLeft (6);
    pitchMode.setBounds (combos);
    o.removeFromTop (6);
    auto toggles = o.removeFromBottom (22);
    keySync.setBounds (toggles.removeFromLeft (toggles.getWidth() / 2));
    snap.setBounds (toggles);
    o.removeFromBottom (4);
    layoutKnobRow (o, { &ratio, &detune, &feedback, &phase }, 100);

    auto e = inner (envArea);
    envelope.setBounds (takeGraph (e, kEnvAspect, 8 + 66));
    e.removeFromTop (8);
    layoutKnobRow (e, { &attack, &decay, &sustain, &release }, 100);

    auto l = inner (levelArea);
    layoutKnobRow (l.removeFromTop ((l.getHeight() - 6) / 2), { &level, &output }, 120);
    l.removeFromTop (6);
    layoutKnobRow (l, { &velLevel, &velFm, &keyTrack }, 100);
}

// Macros 1-4 are re-assigned from the routing (assignCommonMacros). Whatever they point at now is
// replaced, including targets set by hand in the Macro Editor, so that is confirmed first.
void OperatorsTab::fitMacros()
{
    const Preset now = ctx.snapshot();
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
    bool changes = false;
    for (int i = 0; i < 4; ++i) changes = changes || ! same (now.data.sound.macros[(size_t) i], fitted.data.sound.macros[(size_t) i]);
    if (! changes) { ctx.say ("Macros 1-4 already fit the current routing."); return; }

    juce::String names;
    for (int i = 0; i < 4; ++i) names << (i ? ", " : "") << utf8 (now.meta.macroNames[(size_t) i]);
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
    resized();
    refresh (ctx.snapshot());
}

void OperatorsTab::showTemplates()
{
    auto gallery = std::make_unique<AlgoGallery>();
    gallery->columns = 6;
    gallery->setSize (6 * 92, 3 * 84);
    bool renumbered = false;
    gallery->current = matchAlgorithmShape (ctx.snapshot().data.sound.engine, &renumbered);
    auto* g = gallery.get();
    gallery->onChoose = [this, g] (int i) {
        if (ctx.run ([=] (PatchController& pc) { return pc.applyAlgorithmTemplate (i); }))
            ctx.say (juce::String ("Routing: ") + algorithmTemplate (i).name + ". Macros 1-4 were re-assigned for the new routing.");
        if (auto* box = g->findParentComponentOfClass<juce::CallOutBox>()) box->dismiss();
    };
    juce::CallOutBox::launchAsynchronously (std::move (gallery), templates.getScreenBounds(), nullptr);
}

void OperatorsTab::select (int op)
{
    ctx.selectedOp = juce::jlimit (0, kNumOps - 1, op);
    refresh (ctx.snapshot());
}

void OperatorsTab::refresh (const Preset& p)
{
    const auto& e = p.data.sound.engine;
    const int op = juce::jlimit (0, kNumOps - 1, ctx.selectedOp);
    const auto& o = e.operators[(size_t) op];
    const int ratioCode = pc::op (op, o.pitchMode == PitchMode::Ratio ? kOpRatio : kOpFixedHz);
    if (shown != op || ratio.getCode() != ratioCode)
    {
        shown = op;
        ratio.setCode (ratioCode);
        ratio.setCaption (o.pitchMode == PitchMode::Ratio ? "Ratio" : "Fixed Hz");
        detune.setCode (pc::op (op, kOpDetune));
        feedback.setCode (pc::op (op, kOpFeedback));
        phase.setCode (pc::op (op, kOpPhaseOffset));
        attack.setCode (pc::op (op, kOpEnvA));
        decay.setCode (pc::op (op, kOpEnvD));
        sustain.setCode (pc::op (op, kOpEnvS));
        release.setCode (pc::op (op, kOpEnvR));
        level.setCode (pc::op (op, kOpLevel));
        output.setCode (pc::op (op, kOpCarrierGain));
        velLevel.setCode (pc::op (op, kOpVelToLevel));
        velFm.setCode (pc::op (op, kOpVelToIndex));
        keyTrack.setCode (pc::op (op, kOpKeyTracking));
    }
    graph.refresh (p);
    {
        bool renumbered = false;
        const int match = matchAlgorithmShape (e, &renumbered);
        algoName.setText (match < 0 ? juce::String ("Custom routing") : "Algorithm " + juce::String (match + 1) + "  " + algorithmTemplate (match).name,
                          juce::dontSendNotification);
        algoName.setColour (juce::Label::textColourId, match < 0 ? col::warm : col::accent);
        algoName.setJustificationType (juce::Justification::centredRight);
        if (strip.current != match) { strip.current = match; strip.repaint(); }
        opTitle.setText ("OP " + juce::String (op + 1) + (o.enabled ? (o.carrierGain > 0.0f ? "  carrier" : "  modulator") : "  off"), juce::dontSendNotification);
        opEnabled.setToggleState (o.enabled, juce::dontSendNotification);
        const bool edge = graph.selectedEdgeFrom >= 0;
        const int edgeCode = edge ? pc::edge (graph.selectedEdgeFrom, graph.selectedEdgeTo) : -1;
        if (index.getCode() != edgeCode) index.setCode (edgeCode);
        index.setVisible (edge && index.getHeight() >= 56);
        removeEdge.setVisible (edge);
        edgeTitle.setText (edge ? "Connection " + juce::String (graph.selectedEdgeFrom + 1) + " > " + juce::String (graph.selectedEdgeTo + 1)
                                : juce::String ("Drag between boxes to connect. Click a line to edit it."), juce::dontSendNotification);
        edgeTitle.setColour (juce::Label::textColourId, edge ? col::text : col::textDim);
        edgeTitle.setFont (font (edge ? 14.0f : 12.0f, edge));
        index.refresh();
        const int ratioCode = pc::op (op, o.pitchMode == PitchMode::Ratio ? kOpRatio : kOpFixedHz);
        if (sideRatio.getCode() != ratioCode)
        {
            sideRatio.setCode (ratioCode);
            sideRatio.setCaption (o.pitchMode == PitchMode::Ratio ? "Ratio" : "Fixed Hz");
        }
        if (sideLevel.getCode() != pc::op (op, kOpLevel))
        {
            sideLevel.setCode (pc::op (op, kOpLevel));
            sideOutput.setCode (pc::op (op, kOpCarrierGain));
            sideFeedback.setCode (pc::op (op, kOpFeedback));
        }
        for (auto* k : { &sideRatio, &sideLevel, &sideOutput, &sideFeedback })
        {
            k->refresh();
            k->setAlpha (o.enabled ? 1.0f : 0.45f);
        }
    }
    for (auto* k : { &ratio, &detune, &feedback, &phase, &attack, &decay, &sustain, &release, &level, &output, &velLevel, &velFm, &keyTrack })
    {
        k->refresh();
        k->setAlpha (o.enabled ? 1.0f : 0.45f);
    }
    waveform.refreshFrom (p);
    pitchMode.refreshFrom (p);
    keySync.refreshFrom (p);
    wave.state = o;
    wave.repaint();
    envelope.refresh (p);

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
    repaint();
}
} // namespace ui
