#include "UI/Advanced/AlgoDiagram.h"
#include <algorithm>

namespace ui
{
using namespace fmlume;

juce::Path AlgoLayout::edgePath (int from, int to) const
{
    juce::Path p;
    const auto a = pos[from].translated (0.0f, box * 0.5f), b = pos[to].translated (0.0f, -box * 0.5f);
    p.startNewSubPath (a);
    if (std::abs (a.x - b.x) > 1.0f)
    {
        // elbow just above the target; offset slightly by source so parallel lines stay apart
        const float busY = b.y - juce::jmin (box * 0.32f, (b.y - a.y) * 0.5f);
        p.lineTo (a.x, busY);
        p.lineTo (b.x, busY);
    }
    p.lineTo (b);
    return p;
}

AlgoLayout layoutAlgorithm (const EnginePatch& e, juce::Rectangle<float> area, bool includeDisabled, float maxBox)
{
    AlgoLayout l;
    int depth[kNumOps] {};
    for (int pass = 0; pass < kNumOps; ++pass)
        for (int i = 0; i < e.numEdges; ++i)
        {
            const auto& ed = e.edges[(size_t) i];
            if (e.operators[(size_t) ed.from].enabled && e.operators[(size_t) ed.to].enabled)
                depth[ed.from] = juce::jmax (depth[ed.from], juce::jmin (depth[ed.to] + 1, kNumOps - 1));
        }
    float x[kNumOps] {};
    int maxDepth = 0;
    for (int k = 0; k < kNumOps; ++k)
    {
        l.shown[k] = e.operators[(size_t) k].enabled || includeDisabled;
        if (! e.operators[(size_t) k].enabled) depth[k] = 0;
        if (l.shown[k]) maxDepth = juce::jmax (maxDepth, depth[k]);
    }
    // Row 0: enabled operators in index order, then the switched-off ones.
    float next = 0.0f;
    for (int pass = 0; pass < 2; ++pass)
        for (int k = 0; k < kNumOps; ++k)
            if (l.shown[k] && depth[k] == 0 && e.operators[(size_t) k].enabled == (pass == 0)) x[k] = next++;
    // Upper rows: each operator sits above the mean position of what it feeds.
    for (int d = 1; d <= maxDepth; ++d)
    {
        struct Item { int op; float want; };
        std::vector<Item> row;
        for (int k = 0; k < kNumOps; ++k)
        {
            if (! l.shown[k] || depth[k] != d) continue;
            float sum = 0.0f;
            int n = 0;
            for (int i = 0; i < e.numEdges; ++i)
                if (e.edges[(size_t) i].from == k && e.operators[(size_t) e.edges[(size_t) i].to].enabled) { sum += x[e.edges[(size_t) i].to]; ++n; }
            row.push_back ({ k, n > 0 ? sum / (float) n : 0.0f });
        }
        std::stable_sort (row.begin(), row.end(), [] (const Item& a, const Item& b) { return a.want < b.want; });
        float prev = -100.0f, wantSum = 0.0f, gotSum = 0.0f;
        for (auto& it : row)
        {
            x[it.op] = juce::jmax (it.want, prev + 1.0f);
            prev = x[it.op];
            wantSum += it.want;
            gotSum += x[it.op];
        }
        const float shift = row.empty() ? 0.0f : (wantSum - gotSum) / (float) row.size();
        for (auto& it : row) x[it.op] += shift;
    }
    float minX = 1.0e9f, maxX = -1.0e9f;
    for (int k = 0; k < kNumOps; ++k)
        if (l.shown[k]) { minX = juce::jmin (minX, x[k]); maxX = juce::jmax (maxX, x[k]); }
    if (minX > maxX) { minX = maxX = 0.0f; }
    const float cols = maxX - minX + 1.0f, rows = (float) maxDepth + 1.0f;
    // cell = box + gap; leave room below for the output bar.
    const float cell = juce::jmin (area.getWidth() / (cols + 0.25f), area.getHeight() / (rows + 0.45f));
    l.box = juce::jmin (maxBox, cell * 0.62f);
    const float pitchX = juce::jmin (cell, l.box * 1.75f), pitchY = juce::jmin (cell, l.box * 1.6f);
    const float totalW = (cols - 1.0f) * pitchX, totalH = (rows - 1.0f) * pitchY + l.box * 0.55f;
    const float left = area.getCentreX() - totalW * 0.5f;
    const float bottomRowY = area.getCentreY() + totalH * 0.5f - l.box * 0.55f;
    for (int k = 0; k < kNumOps; ++k)
        l.pos[k] = { left + (x[k] - minX) * pitchX, bottomRowY - (float) depth[k] * pitchY };
    l.outY = bottomRowY + l.box * 0.5f + l.box * 0.38f;
    return l;
}

void drawAlgorithm (juce::Graphics& g, const EnginePatch& e, const AlgoLayout& l, const AlgoStyle& st)
{
    const float lineW = st.mini ? 1.3f : 2.2f;
    // output bar under the carriers
    float outMin = 1.0e9f, outMax = -1.0e9f;
    for (int k = 0; k < kNumOps; ++k)
    {
        const auto& op = e.operators[(size_t) k];
        if (! l.shown[k] || ! op.enabled || op.carrierGain <= 0.0f) continue;
        outMin = juce::jmin (outMin, l.pos[k].x);
        outMax = juce::jmax (outMax, l.pos[k].x);
        g.setColour (col::accent.withAlpha (0.9f));
        g.drawLine (l.pos[k].x, l.pos[k].y + l.box * 0.5f, l.pos[k].x, l.outY, lineW);
    }
    if (outMin <= outMax)
    {
        g.setColour (col::accent.withAlpha (0.9f));
        g.drawLine (outMin - l.box * 0.3f, l.outY, outMax + l.box * 0.3f, l.outY, lineW + (st.mini ? 0.4f : 1.0f));
        if (! st.mini)
        {
            g.setFont (font (11.0f, true));
            g.drawText ("OUT", juce::Rectangle<float> (outMax + l.box * 0.3f + 6.0f, l.outY - 8.0f, 40.0f, 16.0f), juce::Justification::centredLeft);
        }
    }
    // connections
    for (int i = 0; i < e.numEdges; ++i)
    {
        const auto& ed = e.edges[(size_t) i];
        if (! l.shown[ed.from] || ! l.shown[ed.to]) continue;
        const bool sel = ed.from == st.selectedFrom && ed.to == st.selectedTo;
        const bool live = e.operators[(size_t) ed.from].enabled && e.operators[(size_t) ed.to].enabled;
        if (! live && st.mini) continue;
        const auto path = l.edgePath (ed.from, ed.to);
        g.setColour (sel ? col::accentB : (live ? col::violet.brighter (0.15f) : col::outline));
        g.strokePath (path, juce::PathStrokeType (sel ? lineW + 1.4f : lineW, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        if (! st.mini && live)
        {
            const auto a = l.pos[ed.from].translated (0.0f, l.box * 0.5f);
            g.setColour (sel ? col::accentB : col::textDim);
            g.setFont (font (11.0f));
            g.drawText (juce::String (ed.index, 1), juce::Rectangle<float> (a.x + 5.0f, a.y + 1.0f, 34.0f, 13.0f), juce::Justification::centredLeft);
        }
    }
    // operator boxes
    for (int k = 0; k < kNumOps; ++k)
    {
        if (! l.shown[k]) continue;
        const auto& op = e.operators[(size_t) k];
        const auto b = l.boxOf (k);
        const bool sel = k == st.selectedOp && st.selectedFrom < 0;
        const juce::Colour role = op.enabled ? roleColour (op) : col::outline;
        const float corner = st.mini ? 2.0f : 6.0f;
        if (op.enabled && op.feedback > 0.0f)
        {
            // feedback: a loop leaving the bottom-right corner and re-entering at the top-right
            juce::Path loop;
            const float o = l.box * 0.24f;
            loop.startNewSubPath (b.getRight(), b.getBottom() - l.box * 0.22f);
            loop.lineTo (b.getRight() + o, b.getBottom() - l.box * 0.22f);
            loop.lineTo (b.getRight() + o, b.getY() - o);
            loop.lineTo (b.getCentreX() + l.box * 0.2f, b.getY() - o);
            loop.lineTo (b.getCentreX() + l.box * 0.2f, b.getY());
            g.setColour (col::warm.withAlpha (0.9f));
            g.strokePath (loop, juce::PathStrokeType (st.mini ? 1.0f : 1.8f, juce::PathStrokeType::mitered));
        }
        g.setColour (op.enabled ? role.withAlpha (st.mini ? 0.85f : 0.22f) : col::panel);
        g.fillRoundedRectangle (b, corner);
        if (! st.mini)
        {
            g.setColour (sel ? col::text : role);
            g.drawRoundedRectangle (b.reduced (0.5f), corner, sel ? 2.6f : 1.5f);
            g.setColour (op.enabled ? col::text : col::textDim);
            g.setFont (font (l.box * 0.42f, true));
            g.drawText (juce::String (k + 1), b.withTrimmedBottom (l.box * 0.28f), juce::Justification::centred);
            g.setFont (font (juce::jmax (9.5f, l.box * 0.2f)));
            g.setColour (op.enabled ? col::textDim.brighter (0.2f) : col::textDim);
            g.drawText (op.enabled ? (op.pitchMode == PitchMode::Ratio ? juce::String (op.ratio, 2) : juce::String (op.fixedHz, 0) + "Hz") : "off",
                        b.withTrimmedTop (l.box * 0.56f), juce::Justification::centred);
        }
        else
        {
            g.setColour (col::onAccent);
            g.setFont (font (l.box * 0.78f, true));
            g.drawText (juce::String (k + 1), b, juce::Justification::centred);
        }
    }
}
} // namespace ui
