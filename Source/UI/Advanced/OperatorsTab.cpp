#include "UI/Advanced/OperatorsTab.h"

namespace ui
{
using namespace fmlume;

namespace
{
float timeNorm (int field, float seconds) { return toNorm (paramDesc (pc::op (0, field)), seconds); }
float timeFromNorm (int field, float n) { return fromNorm (paramDesc (pc::op (0, field)), juce::jlimit (0.0f, 1.0f, n)); }
// Envelope graph geometry: each timed stage may take up to `kStage` of the width.
constexpr float kStage = 0.27f, kHold = 0.17f, kMinStage = 0.04f;
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
std::array<juce::Point<float>, 3> EnvelopeView::drawEnvelope (juce::Graphics& g, juce::Rectangle<float> r, const ADSR& env, juce::Colour colour, bool fill, float lineWidth)
{
    const float w = r.getWidth(), top = r.getY(), bottom = r.getBottom();
    const float x0 = r.getX();
    const float x1 = x0 + stageWidth (timeNorm (kOpEnvA, env.a), w);
    const float x2 = x1 + stageWidth (timeNorm (kOpEnvD, env.d), w);
    const float x3 = x2 + w * kHold;
    const float x4 = x3 + stageWidth (timeNorm (kOpEnvR, env.r), w);
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

EnvelopeView::EnvelopeView (Ctx& c) : ctx (c)
{
    setTooltip ("Drag the dots: first = attack time, second = decay time (sideways) and sustain level (up/down), third = release time.");
}

void EnvelopeView::refresh (const Preset& p)
{
    state = sideOf (p, ctx.sideB()).engine.operators[(size_t) ctx.selectedOp];
    repaint();
}

void EnvelopeView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (col::outline.withAlpha (0.5f));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine (juce::roundToInt (bounds.getY() + bounds.getHeight() * (float) i / 4.0f), bounds.getX() + 4.0f, bounds.getRight() - 4.0f);
    const auto r = bounds.reduced (12.0f, 12.0f);
    const juce::Colour c = state.enabled ? roleColour (state) : col::textDim;
    handles = drawEnvelope (g, r, state.env, c, true, 2.4f);
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
    const bool b = ctx.sideB();
    const int op = ctx.selectedOp;
    if (dragging == 0)
        ctx.set (pc::op (op, kOpEnvA), b, timeFromNorm (kOpEnvA, stageNorm (e.position.x - r.getX(), w)));
    else if (dragging == 1)
    {
        // both values in one validated edit
        const float decay = timeFromNorm (kOpEnvD, stageNorm (e.position.x - handles[0].x, w));
        const float sustain = juce::jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight());
        ctx.edit ([=] (Preset& p) {
            auto& env = sideOf (p, b).engine.operators[(size_t) op].env;
            env.d = decay;
            env.s = sustain;
        });
    }
    else
        ctx.set (pc::op (op, kOpEnvR), b, timeFromNorm (kOpEnvR, stageNorm (e.position.x - (handles[1].x + w * kHold), w)));
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

// ================================================================================= OpCard
OpCard::OpCard (Ctx& c, int opIndex) : ctx (c), op (opIndex)
{
    power.setTooltip ("Switch this operator on or off");
    power.onClick = [this] {
        const bool b = ctx.sideB(), v = power.getToggleState();
        if (! ctx.run ([=, this] (PatchController& pc) { return pc.setOperatorEnabled (b, op, v); }))
            power.setToggleState (! v, juce::dontSendNotification);
    };
    addAndMakeVisible (power);
    setTooltip ("Click to edit this operator below. Drag the bar to change its level.");
}

juce::Rectangle<int> OpCard::levelBar() const { return getLocalBounds().reduced (10, 9).removeFromBottom (9); }

void OpCard::resized() { power.setBounds (getLocalBounds().reduced (7, 6).removeFromTop (20).removeFromRight (34)); }

void OpCard::refresh (const Preset& p)
{
    const auto& e = sideOf (p, ctx.sideB()).engine;
    state = e.operators[(size_t) op];
    feeds = feedsText (e, op);
    power.setToggleState (state.enabled, juce::dontSendNotification);
    repaint();
}

void OpCard::paint (juce::Graphics& g)
{
    const bool selected = ctx.selectedOp == op;
    const bool carrier = state.carrierGain > 0.0f;
    const juce::Colour role = state.enabled ? roleColour (state) : col::textDim;
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (selected ? col::panelHi : col::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (selected ? role : col::outline.withAlpha (0.8f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, selected ? 2.0f : 1.0f);

    auto t = getLocalBounds().reduced (10, 7);
    auto head = t.removeFromTop (18);
    g.setColour (state.enabled ? col::text : col::textDim);
    g.setFont (font (15.0f, true));
    g.drawText ("OP " + juce::String (op + 1), head.removeFromLeft (44), juce::Justification::centredLeft);
    g.setColour (role);
    g.setFont (font (10.5f, true));
    g.drawText (! state.enabled ? "OFF" : (carrier ? "CARRIER" : "MOD"), head.withTrimmedRight (38), juce::Justification::centredLeft);

    auto bar = levelBar().toFloat();
    t.removeFromBottom (12);
    auto info = t.removeFromBottom (15);
    g.setColour (state.enabled ? col::text : col::textDim);
    g.setFont (font (12.5f, true));
    g.drawText (state.pitchMode == PitchMode::Ratio ? "x " + juce::String (state.ratio, 2) : juce::String (state.fixedHz, 0) + " Hz", info, juce::Justification::centredLeft);
    g.setColour (col::textDim);
    g.setFont (font (11.0f));
    if (state.enabled)
        g.drawText (carrier ? (feeds.isEmpty() ? juce::String ("> OUT") : "> " + feeds + " + OUT") : (feeds.isEmpty() ? juce::String ("unused") : "> OP " + feeds),
                    info, juce::Justification::centredRight);
    if (t.getHeight() > 14)
        EnvelopeView::drawEnvelope (g, t.toFloat().reduced (1.0f, 4.0f), state.env, role.withAlpha (state.enabled ? 1.0f : 0.5f), state.enabled, 1.6f);

    const float n = toNorm (paramDesc (pc::op (op, kOpLevel)), state.level);
    g.setColour (col::bg);
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (role.withAlpha (state.enabled ? 0.95f : 0.4f));
    g.fillRoundedRectangle (bar.withWidth (juce::jmax (3.0f, bar.getWidth() * n)), 3.0f);
}

void OpCard::mouseDown (const juce::MouseEvent& e)
{
    draggingLevel = levelBar().expanded (0, 6).contains (e.getPosition());
    if (onSelect) onSelect();
    if (draggingLevel) mouseDrag (e);
}

void OpCard::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingLevel) return;
    const auto bar = levelBar();
    const float n = juce::jlimit (0.0f, 1.0f, (float) (e.x - bar.getX()) / (float) juce::jmax (1, bar.getWidth()));
    ctx.set (pc::op (op, kOpLevel), ctx.sideB(), fromNorm (paramDesc (pc::op (op, kOpLevel)), n));
}

void OpCard::mouseUp (const juce::MouseEvent&)
{
    draggingLevel = false;
}

// ================================================================================= tab
OperatorsTab::OperatorsTab (Ctx& c)
    : ctx (c), envelope (c),
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
    for (int i = 0; i < kNumOps; ++i)
    {
        auto* card = cards.add (new OpCard (ctx, i));
        card->onSelect = [this, i] { select (i); };
        addAndMakeVisible (card);
    }
    for (auto* comp : std::initializer_list<juce::Component*> { &wave, &envelope, &waveform, &pitchMode, &keySync, &snap, &role,
                                                                 &ratio, &detune, &feedback, &phase, &attack, &decay, &sustain, &release,
                                                                 &level, &output, &velLevel, &velFm, &keyTrack })
        addAndMakeVisible (comp);
    level.large = output.large = true;
    snap.setToggleState (ctx.snapRatios, juce::dontSendNotification);
    snap.setTooltip ("When on, the Ratio knob lands on multiples of 0.5 (1/8 steps below 1). Type a value for anything else.");
    snap.onClick = [this] { ctx.snapRatios = snap.getToggleState(); };
    styleLabel (role, 12.5f, col::textDim, juce::Justification::topLeft);
    role.setMinimumHorizontalScale (0.85f);
}

void OperatorsTab::paint (juce::Graphics& g)
{
    const juce::String n = "OP " + juce::String (ctx.selectedOp + 1);
    paintPanel (g, oscArea, n + "  Oscillator");
    paintPanel (g, envArea, n + "  Envelope");
    paintPanel (g, levelArea, n + "  Level & Response");
}

void OperatorsTab::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto top = r.removeFromTop (juce::jlimit (92, 128, r.getHeight() * 24 / 100));
    const int cw = (top.getWidth() + 6) / kNumOps;
    for (auto* card : cards) card->setBounds (top.removeFromLeft (cw).withTrimmedRight (6));
    r.removeFromTop (8);
    oscArea = r.removeFromLeft (r.getWidth() * 35 / 100);
    r.removeFromLeft (8);
    envArea = r.removeFromLeft (r.getWidth() * 55 / 100);
    r.removeFromLeft (8);
    levelArea = r;

    const auto inner = [] (juce::Rectangle<int> a) { return panelContent (a, 12, 10, 2); };
    auto o = inner (oscArea);
    const int knobH = juce::jlimit (70, 96, o.getHeight() * 30 / 100);
    auto toggles = o.removeFromBottom (22);
    keySync.setBounds (toggles.removeFromLeft (toggles.getWidth() / 2));
    snap.setBounds (toggles);
    o.removeFromBottom (4);
    layoutKnobRow (o.removeFromBottom (knobH), { &ratio, &detune, &feedback, &phase }, 96);
    o.removeFromBottom (6);
    auto combos = o.removeFromBottom (26);
    waveform.setBounds (combos.removeFromLeft (combos.getWidth() * 40 / 100));
    combos.removeFromLeft (6);
    pitchMode.setBounds (combos);
    o.removeFromBottom (6);
    wave.setBounds (o);

    auto e = inner (envArea);
    layoutKnobRow (e.removeFromBottom (knobH), { &attack, &decay, &sustain, &release }, 96);
    e.removeFromBottom (6);
    envelope.setBounds (e);

    auto l = inner (levelArea);
    role.setBounds (l.removeFromTop (juce::jmin (64, l.getHeight() / 4)));
    l.removeFromTop (4);
    layoutKnobRow (l.removeFromBottom (knobH), { &velLevel, &velFm, &keyTrack }, 92);
    l.removeFromBottom (6);
    layoutKnobRow (l, { &level, &output }, 132);
}

void OperatorsTab::select (int op)
{
    ctx.selectedOp = juce::jlimit (0, kNumOps - 1, op);
    refresh (ctx.snapshot());
}

void OperatorsTab::refresh (const Preset& p)
{
    const int op = juce::jlimit (0, kNumOps - 1, ctx.selectedOp);
    const auto& e = sideOf (p, ctx.sideB()).engine;
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
    for (auto* card : cards) card->refresh (p);
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
    if (! o.enabled) t = "This operator is switched off. Turn it on with the switch on its card.";
    else if (o.carrierGain > 0.0f)
        t = "CARRIER: you hear this operator directly. Level and Output set how loud it is; its envelope shapes the volume."
            + (feeds.isNotEmpty() ? " It also modulates OP " + feeds + "." : juce::String());
    else if (feeds.isNotEmpty())
        t = "MODULATOR: not heard by itself. It colours OP " + feeds + ". Level = how much brightness it adds; its envelope shapes how the tone changes over time; Ratio sets the character.";
    else t = "This operator is on but not connected to anything. Connect it in the Algorithm tab or raise Output to hear it.";
    role.setText (t, juce::dontSendNotification);
    role.setColour (juce::Label::textColourId, o.enabled ? col::text.withAlpha (0.85f) : col::textDim);
    repaint();
}
} // namespace ui
