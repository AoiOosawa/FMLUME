#include "UI/Advanced/OperatorGraphs.h"

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
float stageWidth (float norm, float w) { return (kMinStage + (1.0f - kMinStage) * norm) * w * kStage; }
float stageNorm (float width, float w) { return (width / (w * kStage) - kMinStage) / (1.0f - kMinStage); }
} // namespace

// ================================================================================= EnvelopeView
std::array<juce::Point<float>, 3> EnvelopeView::drawEnvelope (juce::Graphics& g, juce::Rectangle<float> r, const ADSR& env, juce::Colour colour, int firstCode)
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
    juce::Path area (p);
    area.closeSubPath();
    g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.32f), 0.0f, top, colour.withAlpha (0.03f), 0.0f, bottom, false));
    g.fillPath (area);
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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
    handles = drawEnvelope (g, r, env, c, firstCode());
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
        else g.drawText (names[i], juce::Rectangle<float> (handles[(size_t) i].x - 22.0f, juce::jmax (bounds.getY() + 2.0f, handles[(size_t) i].y - 22.0f), 44.0f, 13.0f),
                         juce::Justification::centred); // kept inside the graph when the sustain is near the top
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
} // namespace ui
