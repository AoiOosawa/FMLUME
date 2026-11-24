#include "UI/Advanced/AlgorithmTab.h"
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
    const int w = getWidth() / juce::jmax (1, n);
    return { i * w, 0, w - 4, getHeight() };
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
GraphView::GraphView (Ctx& c) : ctx (c)
{
    engine = makeInitEngine();
    setTooltip ("Carriers (bottom row, joined by the OUT bar) are what you hear. Operators above them are modulators. "
                "Drag from one operator to another to connect them, click an operator or a line to edit it.");
}

void GraphView::relayout()
{
    layout = layoutAlgorithm (engine, getLocalBounds().toFloat().reduced (30.0f, 26.0f).withTrimmedTop (10.0f), true, 88.0f);
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
        juce::Point<float> nearest;
        layout.edgePath (e.from, e.to).getNearestPoint (p, nearest);
        if (nearest.getDistanceFrom (p) < 7.0f) { from = e.from; to = e.to; return true; }
    }
    return false;
}

void GraphView::paint (juce::Graphics& g)
{
    g.setColour (col::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (col::outline.withAlpha (0.8f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);

    // legend
    auto legend = getLocalBounds().reduced (12, 8).removeFromTop (16);
    const auto item = [&] (juce::Colour c, const juce::String& text, int width) {
        auto r = legend.removeFromLeft (width);
        g.setColour (c);
        g.fillRoundedRectangle (r.removeFromLeft (12).withSizeKeepingCentre (11, 11).toFloat(), 2.5f);
        g.setColour (col::textDim);
        g.setFont (font (11.5f));
        g.drawText (text, r.withTrimmedLeft (5), juce::Justification::centredLeft);
    };
    item (col::accent, "Carrier (heard)", 118);
    item (col::violet, "Modulator (shapes the tone)", 186);
    item (col::warm, "Feedback", 80);

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

// ================================================================================= tab
AlgorithmTab::AlgorithmTab (Ctx& c)
    : ctx (c), graph (c), level (c, pc::op (0, kOpLevel), "Level"), carrier (c, pc::op (0, kOpCarrierGain), "Output"),
      feedback (c, pc::op (0, kOpFeedback), "Feedback"), ratio (c, pc::op (0, kOpRatio), "Ratio"), index (c, -1, "FM Amount")
{
    addAndMakeVisible (gallery);
    addAndMakeVisible (graph);
    graph.onSelection = [this] { selectionChanged(); };
    gallery.onChoose = [this] (int i) {
        if (ctx.run ([=] (PatchController& pc) { return pc.applyAlgorithmTemplate (i); }))
            ctx.say (juce::String ("Routing: ") + algorithmTemplate (i).name + ". Macros 1-4 were re-assigned for the new routing.");
    };
    styleLabel (algoName, 14.0f, col::accent);
    algoName.setFont (font (14.0f, true));
    styleLabel (opTitle, 14.0f, col::text);
    opTitle.setFont (font (14.0f, true));
    styleLabel (edgeTitle, 14.0f, col::text);
    edgeTitle.setFont (font (14.0f, true));
    styleLabel (hint, 12.0f);
    hint.setJustificationType (juce::Justification::topLeft);
    hint.setText ("Tip: more operators in a tall stack = brighter, more complex tone. Several carriers side by side = "
                  "a fuller, layered tone. A change that would leave nothing audible or create a loop is refused.", juce::dontSendNotification);
    for (auto* l : { &algoName, &opTitle, &edgeTitle, &hint }) addAndMakeVisible (l);
    opEnabled.onClick = [this] {
        const bool v = opEnabled.getToggleState();
        const int op = ctx.selectedOp;
        if (! ctx.run ([=] (PatchController& pc) { return pc.setOperatorEnabled (op, v); })) refresh (ctx.snapshot());
    };
    addAndMakeVisible (opEnabled);
    for (auto* k : { &level, &carrier, &feedback, &ratio, &index }) addAndMakeVisible (k);
    index.setAccent (col::accentB);
    removeEdge.onClick = [this] {
        if (graph.selectedEdgeFrom < 0) return;
        const int from = graph.selectedEdgeFrom, to = graph.selectedEdgeTo;
        ctx.run ([=] (PatchController& pc) { return pc.removeEdge (from, to); });
    };
    addAndMakeVisible (removeEdge);
    resetMacros.setTooltip ("Re-create the Brightness / Attack / Release / Character macro assignments for the current routing");
    resetMacros.onClick = [this] {
        ctx.edit ([] (Preset& p) { assignCommonMacros (p.data); });
    };
    addAndMakeVisible (resetMacros);
}

void AlgorithmTab::paint (juce::Graphics& g) { paintPanel (g, sideArea, "Selected"); }

void AlgorithmTab::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    gallery.setBounds (r.removeFromTop (juce::jlimit (66, 96, r.getHeight() * 17 / 100)));
    r.removeFromTop (4);
    algoName.setBounds (r.removeFromTop (20).reduced (4, 0));
    r.removeFromTop (4);
    sideArea = r.removeFromRight (juce::jlimit (260, 320, r.getWidth() * 28 / 100));
    r.removeFromRight (8);
    graph.setBounds (r);

    auto side = panelContent (sideArea, 12, 10, 2);
    auto row = side.removeFromTop (22);
    opEnabled.setBounds (row.removeFromRight (64));
    opTitle.setBounds (row);
    side.removeFromTop (2);
    const int knobH = juce::jlimit (72, 92, side.getHeight() * 22 / 100);
    row = side.removeFromTop (knobH);
    const int w = row.getWidth() / 4;
    for (auto* k : { &ratio, &level, &carrier, &feedback }) k->setBounds (row.removeFromLeft (w));
    side.removeFromTop (8);
    edgeTitle.setBounds (side.removeFromTop (22));
    row = side.removeFromTop (knobH);
    index.setBounds (row.removeFromLeft (w + 12));
    removeEdge.setBounds (row.withSizeKeepingCentre (row.getWidth() - 16, 26));
    side.removeFromTop (8);
    resetMacros.setBounds (side.removeFromBottom (26));
    side.removeFromBottom (8);
    hint.setBounds (side);
}

void AlgorithmTab::selectionChanged()
{
    shownOp = -1;
    refresh (ctx.snapshot());
}

void AlgorithmTab::refresh (const Preset& p)
{
    graph.refresh (p);
    const auto& e = p.data.sound.engine;
    const int op = juce::jlimit (0, kNumOps - 1, ctx.selectedOp);
    const auto& o = e.operators[(size_t) op];
    bool renumbered = false;
    const int match = matchAlgorithmShape (e, &renumbered);
    if (gallery.current != match) { gallery.current = match; gallery.repaint(); }
    algoName.setText (match < 0 ? juce::String ("CUSTOM routing: this sound does not use one of the 18 templates")
                                : "ALGORITHM " + juce::String (match + 1) + "   " + algorithmTemplate (match).name
                                      + (renumbered ? "   (same shape, operators numbered differently)" : juce::String()),
                      juce::dontSendNotification);
    algoName.setColour (juce::Label::textColourId, match < 0 ? col::warm : col::accent);

    const int ratioCode = pc::op (op, o.pitchMode == PitchMode::Ratio ? kOpRatio : kOpFixedHz);
    if (shownOp != op || ratio.getCode() != ratioCode)
    {
        shownOp = op;
        level.setCode (pc::op (op, kOpLevel));
        carrier.setCode (pc::op (op, kOpCarrierGain));
        feedback.setCode (pc::op (op, kOpFeedback));
        ratio.setCode (ratioCode);
        ratio.setCaption (o.pitchMode == PitchMode::Ratio ? "Ratio" : "Fixed Hz");
    }
    opTitle.setText ("Operator " + juce::String (op + 1) + (o.enabled ? (o.carrierGain > 0.0f ? "  (carrier)" : "  (modulator)") : "  (off)"), juce::dontSendNotification);
    opEnabled.setToggleState (o.enabled, juce::dontSendNotification);

    const bool edge = graph.selectedEdgeFrom >= 0;
    const int edgeCode = edge ? pc::edge (graph.selectedEdgeFrom, graph.selectedEdgeTo) : -1;
    if (index.getCode() != edgeCode) index.setCode (edgeCode);
    index.setVisible (edge);
    removeEdge.setVisible (edge);
    edgeTitle.setText (edge ? "Connection " + juce::String (graph.selectedEdgeFrom + 1) + " > " + juce::String (graph.selectedEdgeTo + 1)
                            : juce::String ("Click a line to edit a connection"), juce::dontSendNotification);
    edgeTitle.setColour (juce::Label::textColourId, edge ? col::text : col::textDim);
    for (auto* k : { &level, &carrier, &feedback, &ratio, &index }) k->refresh();
}
} // namespace ui
