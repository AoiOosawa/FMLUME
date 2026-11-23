#include "UI/Advanced/FxTab.h"

namespace ui
{
using namespace fmlume;

namespace
{
const char* const kFxTitles[kNumFx] = { "EQ", "Drive", "Chorus / Ensemble", "Phaser", "Flanger", "Compressor", "Bitcrusher", "Delay", "Reverb" };
}

struct FxTab::Row final : public juce::Component
{
    Row (FxTab& o, int position) : owner (o), pos (position)
    {
        enabled.setTooltip ("Enable this effect. A disabled effect costs no CPU.");
        enabled.onClick = [this] {
            const int t = type;
            const bool v = enabled.getToggleState();
            owner.ctx.edit ([t, v] (Preset& p) { p.data.fx.proc[(size_t) t].enabled = v; }, v ? "Enable Effect" : "Disable Effect");
        };
        addAndMakeVisible (enabled);
        setInterceptsMouseClicks (false, true); // the rest of the row belongs to the tab: click = select, drag = reorder
    }
    void resized() override { enabled.setBounds (getLocalBounds().reduced (4, 3).removeFromLeft (36)); }
    FxTab& owner;
    int pos, type = 0;
    juce::ToggleButton enabled;
};

FxTab::FxTab (Ctx& c) : ctx (c), bypassAll (c.proc, "fx.bypassAll", "Bypass all")
{
    for (int i = 0; i < kNumFx; ++i) addAndMakeVisible (rows.add (new Row (*this, i)));
    for (int i = 0; i < kFxParams; ++i) addChildComponent (knobs.add (new ModelKnob (ctx, -1)));
    delaySync = std::make_unique<BoundToggle> (ctx, "Tempo sync", "Use a tempo division instead of milliseconds",
                                               [] (const Preset& p) { return p.data.fx.proc[(size_t) FxType::Delay].p[1] >= 0.5f; },
                                               [] (Preset& p, bool v) { p.data.fx.proc[(size_t) FxType::Delay].p[1] = v ? 1.0f : 0.0f; });
    juce::StringArray divs;
    for (int i = 0; i < kNumSyncDivs; ++i) divs.add (syncDivName (i));
    delayDivision = std::make_unique<BoundCombo> (ctx, "Delay division", divs,
                                                  [] (const Preset& p) { return (int) std::lround (p.data.fx.proc[(size_t) FxType::Delay].p[2]); },
                                                  [] (Preset& p, int v) { p.data.fx.proc[(size_t) FxType::Delay].p[2] = (float) v; });
    addChildComponent (*delaySync);
    addChildComponent (*delayDivision);
    addAndMakeVisible (bypassAll);
    styleLabel (title, 16.0f, col::text);
    styleLabel (hint, 12.0f);
    hint.setJustificationType (juce::Justification::topLeft);
    hint.setText ("Signal runs top to bottom. Drag a row up or down to change the order; reordering fades briefly to stay click free. "
                  "Delay and reverb tails keep ringing after notes end and after the trance gate closes.", juce::dontSendNotification);
    addAndMakeVisible (title);
    addAndMakeVisible (hint);
    select (0);
}

FxTab::~FxTab() = default;

juce::Rectangle<int> FxTab::rowBounds (int pos) const
{
    auto r = listArea.reduced (8).withTrimmedTop (26);
    const int h = juce::jmin (38, r.getHeight() / kNumFx);
    return { r.getX(), r.getY() + pos * h, r.getWidth(), h - 3 };
}

void FxTab::paint (juce::Graphics& g)
{
    paintPanel (g, listArea, "Effect chain");
    paintPanel (g, paramArea);
    const bool reordering = dragging && dragPos >= 0;
    const auto drawRow = [&] (juce::Rectangle<int> r, int pos, float alpha, bool lifted) {
        const int type = order[(size_t) pos];
        const bool sel = type == selected;
        const bool on = rows[pos]->enabled.getToggleState();
        g.setColour ((sel ? col::accent.withAlpha (0.2f) : col::panelHi).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour ((sel || lifted ? col::accent : col::outline).withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 5.0f, sel || lifted ? 1.6f : 1.0f);
        g.setColour ((on ? col::text : col::textDim).withMultipliedAlpha (alpha));
        g.setFont (font (14.0f, sel));
        g.drawText (juce::String (pos + 1) + ".  " + kFxTitles[type] + (on ? "" : "  (off)"), r.withTrimmedLeft (48).withTrimmedRight (34), juce::Justification::centredLeft);
        // grip: the row can be dragged
        g.setColour (col::textDim.withMultipliedAlpha (alpha * (lifted ? 1.0f : 0.7f)));
        const auto grip = r.withTrimmedLeft (r.getWidth() - 30).withSizeKeepingCentre (14, 12).toFloat();
        for (int i = 0; i < 3; ++i) g.fillRoundedRectangle (grip.getX(), grip.getY() + (float) i * 5.0f, grip.getWidth(), 2.0f, 1.0f);
    };
    for (int pos = 0; pos < kNumFx; ++pos)
        drawRow (rowBounds (pos), pos, reordering && pos == dragPos ? 0.35f : 1.0f, false);
    if (reordering)
    {
        // where the row will land, and the row itself following the pointer
        const auto target = rowBounds (dragTarget);
        if (dragTarget != dragPos)
        {
            g.setColour (col::accentB);
            g.fillRoundedRectangle ((float) target.getX(), (float) (dragTarget > dragPos ? target.getBottom() + 1 : target.getY() - 3), (float) target.getWidth(), 3.0f, 1.5f);
        }
        const auto home = rowBounds (dragPos);
        drawRow (home.withY (juce::jlimit (rowBounds (0).getY(), rowBounds (kNumFx - 1).getY(), dragY - home.getHeight() / 2)), dragPos, 0.92f, true);
    }
}

void FxTab::resized()
{
    auto r = getLocalBounds().reduced (8);
    listArea = r.removeFromLeft (juce::jlimit (260, 340, r.getWidth() * 30 / 100));
    r.removeFromLeft (8);
    paramArea = r;
    for (int pos = 0; pos < kNumFx; ++pos) rows[pos]->setBounds (rowBounds (pos));
    bypassAll.setBounds (listArea.reduced (10, 7).removeFromTop (20).removeFromRight (juce::jmin (160, listArea.getWidth() / 2))); // in the panel header
    auto p = paramArea.reduced (14);
    title.setBounds (p.removeFromTop (26));
    hint.setBounds (p.removeFromBottom (54));
    p.removeFromTop (6);
    const int cols = 4, cw = p.getWidth() / cols, ch = juce::jmin (120, p.getHeight() / 2);
    int slot = 0;
    for (auto* k : knobs)
        if (k->isVisible()) { k->setBounds (p.getX() + (slot % cols) * cw, p.getY() + (slot / cols) * ch, cw - 8, ch - 8); ++slot; }
    if (delaySync->isVisible())
    {
        juce::Rectangle<int> cell (p.getX() + (slot % cols) * cw, p.getY() + (slot / cols) * ch, cw - 8, ch - 8);
        delaySync->setBounds (cell.removeFromTop (24));
        cell.removeFromTop (6);
        delayDivision->setBounds (cell.removeFromTop (26));
    }
}

void FxTab::select (int type)
{
    selected = type;
    const bool isDelay = type == (int) FxType::Delay;
    for (int i = 0; i < kFxParams; ++i)
    {
        const auto& d = fxParamDesc (type, i);
        const bool show = d.name != nullptr && ! (isDelay && (i == 1 || i == 2)); // delay sync / division have dedicated controls
        knobs[i]->setVisible (show);
        if (show)
        {
            juce::String caption;
            for (const char* c = d.name; *c != 0; ++c)
            {
                if (c != d.name && std::isupper ((unsigned char) *c)) caption << ' ';
                caption << juce::String::charToString ((juce::juce_wchar) (c == d.name ? std::toupper ((unsigned char) *c) : *c));
            }
            knobs[i]->setCaption (caption);
            knobs[i]->setCode (pc::fx (type, i));
            knobs[i]->setAccent (d.flags & kFlagWet ? col::accentB : col::accent);
        }
        else knobs[i]->setCode (-1);
    }
    delaySync->setVisible (isDelay);
    delayDivision->setVisible (isDelay);
    title.setText (kFxTitles[type], juce::dontSendNotification);
    resized();
    repaint();
}

void FxTab::move (int fromPos, int toPos)
{
    if (fromPos == toPos || fromPos < 0 || toPos < 0 || fromPos >= kNumFx || toPos >= kNumFx) return;
    ctx.edit ([=] (Preset& p) {
        auto& o = p.data.fx.order;
        const uint8_t t = o[(size_t) fromPos];
        if (fromPos < toPos) for (int i = fromPos; i < toPos; ++i) o[(size_t) i] = o[(size_t) i + 1];
        else for (int i = fromPos; i > toPos; --i) o[(size_t) i] = o[(size_t) i - 1];
        o[(size_t) toPos] = t;
    });
}

int FxTab::rowAt (juce::Point<int> p) const
{
    for (int pos = 0; pos < kNumFx; ++pos)
        if (rowBounds (pos).contains (p)) return pos;
    return -1;
}

void FxTab::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (rowAt (e.getPosition()) >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void FxTab::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    dragPos = dragTarget = rowAt (e.getPosition());
    if (dragPos >= 0) { select (order[(size_t) dragPos]); refresh (ctx.snapshot()); }
}

void FxTab::mouseDrag (const juce::MouseEvent& e)
{
    if (dragPos < 0) return;
    if (! dragging && e.getDistanceFromDragStart() < 5) return; // a click, not a drag
    dragging = true;
    dragY = e.y;
    const auto first = rowBounds (0);
    const int h = rowBounds (1).getY() - first.getY();
    dragTarget = juce::jlimit (0, kNumFx - 1, (e.y - first.getY()) / juce::jmax (1, h));
    repaint (listArea);
}

void FxTab::mouseUp (const juce::MouseEvent&)
{
    if (dragging && dragPos >= 0 && dragTarget >= 0 && dragTarget != dragPos) move (dragPos, dragTarget);
    dragging = false;
    dragPos = dragTarget = -1;
    repaint (listArea);
}

void FxTab::refresh (const Preset& p)
{
    order = p.data.fx.order;
    for (int pos = 0; pos < kNumFx; ++pos)
    {
        auto* row = rows[pos];
        row->type = order[(size_t) pos];
        row->enabled.setToggleState (p.data.fx.proc[(size_t) row->type].enabled, juce::dontSendNotification);
    }
    const bool on = p.data.fx.proc[(size_t) selected].enabled;
    for (auto* k : knobs) if (k->isVisible()) { k->refresh(); k->setAlpha (on ? 1.0f : 0.5f); }
    delaySync->refreshFrom (p);
    delayDivision->refreshFrom (p);
    if (selected == (int) FxType::Delay)
    {
        const bool synced = delaySync->getToggleState();
        delayDivision->setEnabled (synced);
        knobs[0]->setAlpha (synced ? 0.35f : (on ? 1.0f : 0.5f));
    }
    repaint();
}
} // namespace ui
