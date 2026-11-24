#include "UI/Advanced/RoutingGraph.h"
#include "Core/PatchCompiler.h"
#include "Core/Templates.h"

namespace ui
{
using namespace fmlume;

// ================================================================================= gallery
AlgoGallery::AlgoGallery()
{
    for (int i = 0; i < numAlgorithmTemplates(); ++i)
    {
        EnginePatch e = makeInitEngine();
        applyAlgorithm (e, i);
        engines.push_back (e);
    }
    setTooltip ("Routing templates. Click one to rewire the operators; their tone settings are kept.");
}

juce::Rectangle<int> AlgoGallery::cell (int i) const
{
    const int n = (int) engines.size();
    const int cols = columns > 0 ? columns : n, rows = (n + cols - 1) / cols;
    const int w = getWidth() / juce::jmax (1, cols), h = getHeight() / juce::jmax (1, rows);
    return { (i % cols) * w, (i / cols) * h, w - 4, h - (rows > 1 ? 4 : 0) };
}

void AlgoGallery::paint (juce::Graphics& g)
{
    for (int i = 0; i < (int) engines.size(); ++i)
    {
        const auto c = cell (i);
        const bool cur = i == current;
        g.setColour (cur ? col::accent.withAlpha (0.2f) : (i == hover ? col::panel.brighter (0.08f) : col::panel));
        g.fillRoundedRectangle (c.toFloat(), 6.0f);
        g.setColour (cur ? col::accent : col::outline.withAlpha (0.8f));
        g.drawRoundedRectangle (c.toFloat().reduced (cur ? 1.0f : 0.5f), 6.0f, cur ? 2.5f : 1.0f);
        if (cur)
        {
            // number badge so the current algorithm is readable at a glance
            const auto badge = c.toFloat().reduced (3.0f).removeFromTop (14.0f).removeFromLeft (i >= 9 ? 22.0f : 16.0f);
            g.setColour (col::accent);
            g.fillRoundedRectangle (badge, 3.0f);
            g.setColour (col::onAccent);
            g.setFont (font (11.0f, true));
            g.drawText (juce::String (i + 1), badge, juce::Justification::centred);
        }
        else
        {
            g.setColour (col::textDim);
            g.setFont (font (11.0f, true));
            g.drawText (juce::String (i + 1), c.reduced (5, 3).removeFromTop (12), juce::Justification::topLeft);
        }
        const auto area = c.toFloat().reduced (5.0f, 4.0f).withTrimmedTop (11.0f);
        AlgoStyle st;
        st.mini = true;
        drawAlgorithm (g, engines[(size_t) i], layoutAlgorithm (engines[(size_t) i], area, false, 16.0f), st);
    }
}

void AlgoGallery::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < (int) engines.size(); ++i)
        if (cell (i).contains (e.getPosition())) h = i;
    if (h == hover) return;
    hover = h;
    if (h >= 0) setTooltip (juce::String (h + 1) + ". " + algorithmTemplate (h).name + "   " + algorithmTemplate (h).diagram + "\nClick to use this routing. Operator tone settings are kept.");
    repaint();
}

void AlgoGallery::mouseUp (const juce::MouseEvent& e)
{
    for (int i = 0; i < (int) engines.size(); ++i)
        if (cell (i).contains (e.getPosition()) && onChoose) { onChoose (i); return; }
}

// ================================================================================= graph
GraphView::GraphView (Ctx& c) : ctx (c) { engine = makeInitEngine(); } // no tooltip: it would cover the diagram

void GraphView::relayout()
{
    layout = layoutAlgorithm (engine, getLocalBounds().toFloat().reduced (14.0f, 8.0f), true, maxBox);
}

void GraphView::refresh (const Preset& p)
{
    engine = p.data.sound.engine;
    if (selectedEdgeFrom >= 0 && findEdge (engine, selectedEdgeFrom, selectedEdgeTo) < 0) selectedEdgeFrom = selectedEdgeTo = -1;
    relayout();
    repaint();
}

int GraphView::nodeAt (juce::Point<float> p) const
{
    for (int k = 0; k < kNumOps; ++k)
        if (layout.shown[k] && layout.boxOf (k).expanded (3.0f).contains (p)) return k;
    return -1;
}

bool GraphView::edgeAt (juce::Point<float> p, int& from, int& to) const
{
    for (int i = 0; i < engine.numEdges; ++i)
    {
        const auto& e = engine.edges[(size_t) i];
        if (! engine.operators[(size_t) e.from].enabled || ! engine.operators[(size_t) e.to].enabled) continue; // not drawn either
        juce::Point<float> nearest;
        layout.edgePath (e.from, e.to).getNearestPoint (p, nearest);
        if (nearest.getDistanceFrom (p) < 7.0f) { from = e.from; to = e.to; return true; }
    }
    return false;
}

void GraphView::paint (juce::Graphics& g)
{
    g.setColour (col::bg);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    AlgoStyle st;
    st.selectedOp = ctx.selectedOp;
    st.selectedFrom = selectedEdgeFrom;
    st.selectedTo = selectedEdgeTo;
    drawAlgorithm (g, engine, layout, st);

    if (dragging && dragFrom >= 0)
    {
        const int over = nodeAt (dragPos);
        const bool valid = over >= 0 && over != dragFrom && ! edgeWouldBeInvalid (engine, dragFrom, over);
        g.setColour (over < 0 || over == dragFrom ? col::textDim : (valid ? col::good : col::bad));
        g.drawArrow ({ layout.pos[dragFrom], over >= 0 && over != dragFrom ? layout.pos[over] : dragPos }, 2.0f, 10.0f, 10.0f);
        if (over >= 0 && over != dragFrom)
        {
            g.setFont (font (12.0f, true));
            g.drawText (valid ? "OP " + juce::String (dragFrom + 1) + " will modulate OP " + juce::String (over + 1) : juce::String ("not allowed (loop or duplicate)"),
                        juce::Rectangle<float> (dragPos.x - 120.0f, dragPos.y - 30.0f, 240.0f, 16.0f), juce::Justification::centred);
        }
    }
}

void GraphView::mouseDown (const juce::MouseEvent& e)
{
    dragFrom = nodeAt (e.position);
    dragPos = e.position;
    dragging = false;
}

void GraphView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragFrom < 0) return;
    dragPos = e.position;
    dragging = e.getDistanceFromDragStart() > 6;
    repaint();
}

void GraphView::mouseUp (const juce::MouseEvent& e)
{
    const int over = nodeAt (e.position);
    if (dragging && dragFrom >= 0 && over >= 0 && over != dragFrom)
    {
        const int from = dragFrom, to = over;
        if (ctx.run ([=] (PatchController& pc) { return pc.addEdge (from, to); }, {}))
        {
            selectedEdgeFrom = from;
            selectedEdgeTo = to;
            ctx.say ("OP " + juce::String (from + 1) + " now modulates OP " + juce::String (to + 1) + ".");
        }
    }
    else if (! dragging)
    {
        int from = -1, to = -1;
        if (over >= 0) { ctx.selectedOp = over; selectedEdgeFrom = selectedEdgeTo = -1; }
        else if (edgeAt (e.position, from, to)) { selectedEdgeFrom = from; selectedEdgeTo = to; }
        else selectedEdgeFrom = selectedEdgeTo = -1;
    }
    dragFrom = -1;
    dragging = false;
    if (onSelection) onSelection();
    repaint();
}

} // namespace ui
