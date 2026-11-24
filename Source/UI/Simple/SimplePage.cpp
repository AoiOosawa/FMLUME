#include "UI/Simple/SimplePage.h"
#include "Core/PatchCompiler.h"

namespace ui
{
using namespace fmlume;

// ======================================================================================= Macros
MacroPanel::MacroPanel (Ctx& c, int firstMacro, const juce::String& t, bool large) : ctx (c), first (firstMacro), title (t)
{
    for (int i = 0; i < 4; ++i)
    {
        auto* k = knobs.add (new HostKnob (ctx.proc, FmlumeAudioProcessor::kMacroIds[first + i], "Macro " + juce::String (first + i + 1)));
        k->large = large;
        addAndMakeVisible (k);
    }
}

void MacroPanel::paint (juce::Graphics& g) { paintPanel (g, getLocalBounds(), title); }

void MacroPanel::resized()
{
    auto r = panelContent (getLocalBounds(), 10, 8);
    const bool big = first == 0 && r.getHeight() >= 130;
    for (auto* k : knobs) k->large = big;
    layoutKnobRow (r, { knobs.begin(), knobs.end() }, big ? 170 : 124, 8);
}

void MacroPanel::refresh (const Preset& p)
{
    const auto& names = p.meta.macroNames;
    const auto& side = p.data.sound;
    for (int i = 0; i < 4; ++i)
    {
        const int m = first + i;
        juce::String name = utf8 (names[(size_t) m]);
        if (name.isEmpty()) name = "Macro " + juce::String (m + 1);
        knobs[i]->setCaption (name);
        juce::String tip = name + "  (host parameter: Macro " + juce::String (m + 1) + ")";
        const auto& macro = side.macros[(size_t) m];
        if (macro.numMappings == 0) tip << "\nNot assigned. Assign targets in Advanced > Filter & Mod.";
        for (int k = 0; k < macro.numMappings; ++k)
            tip << "\n" << (macro.mappings[(size_t) k].amount >= 0 ? "+ " : "- ") << paramDisplayName (macro.mappings[(size_t) k].target);
        knobs[i]->slider.setTooltip (tip);
        knobs[i]->setAlpha (macro.numMappings == 0 ? 0.45f : 1.0f);
    }
}

// ======================================================================================= Quick FX
QuickFxPanel::QuickFxPanel (Ctx& c) : bypass (c.proc, "fx.bypassAll", "FX Bypass")
{
    static const char* const names[4] = { "Warmth", "Width", "Space", "Echo" };
    static const char* const tips[4] = { "Warmth: drive and low/high EQ tilt", "Width: chorus amount and stereo width", "Space: reverb amount and length", "Echo: delay level and feedback" };
    for (int i = 0; i < 4; ++i)
    {
        auto* k = knobs.add (new HostKnob (c.proc, FmlumeAudioProcessor::kQuickIds[i], names[i]));
        k->slider.setTooltip (juce::String (tips[i]) + ". 50 % = the preset's own setting. (host automatable)");
        addAndMakeVisible (k);
    }
    bypass.setColour (juce::ToggleButton::tickColourId, col::accentB);
    addAndMakeVisible (bypass);
}

void QuickFxPanel::paint (juce::Graphics& g) { paintPanel (g, getLocalBounds(), "FX"); }

void QuickFxPanel::resized()
{
    auto r = panelContent (getLocalBounds(), 10, 8);
    bypass.setBounds (r.removeFromRight (74).withSizeKeepingCentre (74, juce::jmin (r.getHeight(), 92)));
    layoutKnobRow (r, { knobs.begin(), knobs.end() }, 104, 6);
}

// ======================================================================================= Rhythm
RhythmPanel::RhythmPanel (Ctx& c, bool arpPanel)
    : isArp (arpPanel), power (c.proc, arpPanel ? "arp.enabled" : "tranceGate.enabled", arpPanel ? "Arp" : "Gate"), pattern (c),
      mode (c, "Arp mode", labels::list (labels::arpMode),
            [] (const Preset& p) { return (int) p.data.perf.arp.mode; }, [] (Preset& p, int v) { p.data.perf.arp.mode = (ArpMode) v; }),
      rate (c, arpPanel ? "Arp rate" : "Gate rate", arpPanel ? rateNames() : gateRateNames(),
            [arpPanel] (const Preset& p) { return (int) (arpPanel ? p.data.perf.arp.rate : p.data.perf.gate.rate) - kRateFirst; },
            [arpPanel] (Preset& p, int v) { (arpPanel ? p.data.perf.arp.rate : p.data.perf.gate.rate) = (uint8_t) (v + kRateFirst); }),
      stereo (c, "Stereo", "Separate gate patterns for the left and right channel. Edit the two rows in Advanced > Rhythm, or pick a pattern from the Stereo group.",
              [] (const Preset& p) { return p.data.perf.gate.stereo; },
              [] (Preset& p, bool v) {
                  auto& g = p.data.perf.gate;
                  if (v && ! g.stereo) g.stepsRight = g.steps; // start from the mono pattern
                  g.stereo = v;
              }),
      width (c, "Width", 0.0f, 100.0f, 1.0f, 100.0f, " %",
             [] (const Preset& p) { return p.data.perf.gate.width * 100.0f; }, [] (Preset& p, float v) { p.data.perf.gate.width = v * 0.01f; })
{
    width.captionWidth = 44;
    width.slider.setTooltip ("Stereo width of the gate: 100 % = left and right fully separate, 0 % = both channels open together.");
    addChildComponent (stereo);
    addChildComponent (width);
    addAndMakeVisible (power);
    power.onStateChange = [this] {
        const bool on = power.getToggleState();
        for (auto* comp : std::initializer_list<juce::Component*> { &pattern, &mode, &rate, &caption1, &caption2, &stereo }) comp->setAlpha (on ? 1.0f : 0.45f);
        width.setAlpha (on && stereo.getToggleState() ? 1.0f : 0.4f); // width only matters for a stereo gate
    };
    styleLabel (caption1, 12.0f, col::textDim, juce::Justification::centred);
    styleLabel (caption2, 12.0f, col::textDim, juce::Justification::centred);
    caption1.setText (isArp ? "Mode" : "Pattern", juce::dontSendNotification);
    caption2.setText ("Rate", juce::dontSendNotification);
    addAndMakeVisible (caption1);
    addAndMakeVisible (caption2);
    addAndMakeVisible (isArp ? static_cast<juce::Component&> (mode) : static_cast<juce::Component&> (pattern));
    addAndMakeVisible (rate);
}

void RhythmPanel::paint (juce::Graphics& g) { paintPanel (g, getLocalBounds(), isArp ? "Arpeggiator" : "Trance Gate"); }

void RhythmPanel::resized()
{
    auto r = panelContent (getLocalBounds(), 10, 8);
    power.setBounds (r.removeFromLeft (58).withSizeKeepingCentre (58, juce::jmin (r.getHeight(), 92)));
    r.removeFromLeft (6);
    // The gate panel has a second line (Stereo, Width) when there is room for it.
    const bool extra = ! isArp && r.getHeight() >= 88;
    stereo.setVisible (extra);
    width.setVisible (extra);
    r = r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), extra ? 88 : 60));
    if (extra)
    {
        auto line = r.removeFromBottom (22);
        r.removeFromBottom (6);
        stereo.setBounds (line.removeFromLeft (86));
        width.setBounds (line);
    }
    auto right = r.removeFromRight (juce::jmax (70, r.getWidth() * 36 / 100));
    r.removeFromRight (6);
    caption1.setBounds (r.removeFromTop (16));
    caption2.setBounds (right.removeFromTop (16));
    r.removeFromTop (6);
    right.removeFromTop (6);
    (isArp ? static_cast<juce::Component&> (mode) : static_cast<juce::Component&> (pattern)).setBounds (r.removeFromTop (28));
    rate.setBounds (right.removeFromTop (28));
}

void RhythmPanel::refresh (const Preset& p)
{
    if (isArp) mode.refreshFrom (p); else pattern.refreshFrom (p);
    rate.refreshFrom (p);
    if (! isArp) { stereo.refreshFrom (p); width.refreshFrom (p); }
    power.onStateChange();
}

// ======================================================================================= Explorer
void ExplorerPanel::Tile::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.5f);
    const bool over = isMouseOver() && isEnabled();
    const juce::Colour hero = original ? col::textDim : col::accent;
    g.setColour (selected ? hero.withAlpha (0.2f) : (over ? col::panelHi.brighter (0.1f) : col::panelHi));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (selected ? (original ? col::text : col::accent) : col::outline);
    g.drawRoundedRectangle (r, 7.0f, selected ? 2.0f : 1.0f);
    auto t = getLocalBounds().reduced (9, 6);
    auto head = t.removeFromTop (22);
    if (! original)
    {
        g.setColour (selected ? col::accent : col::textDim);
        g.setFont (font (19.0f, true));
        g.drawText (letter, head.removeFromLeft (22), juce::Justification::centredLeft);
    }
    if (selected)
    {
        g.setColour (original ? col::text : col::accent);
        g.setFont (font (10.5f, true));
        g.drawText ("PLAYING", head.removeFromRight (56), juce::Justification::centredRight);
    }
    g.setColour (isEnabled() ? col::text : col::textDim);
    g.setFont (font (13.5f, true));
    g.drawFittedText (name, original ? head : t.removeFromTop (18), juce::Justification::centredLeft, 1, 0.75f);
    g.setColour (col::textDim);
    g.setFont (font (11.5f));
    g.drawFittedText (detail, t, juce::Justification::topLeft, 3, 0.8f);
}

ExplorerPanel::ExplorerPanel (Ctx& c) : ctx (c)
{
    direction.addItem ("Any direction", 1);
    for (int i = 1; i < (int) ExploreDirection::Count; ++i) direction.addItem (exploreDirectionName ((ExploreDirection) i), i + 1);
    direction.setSelectedId (1, juce::dontSendNotification);
    direction.setTooltip ("Nudge the variations in one direction. A hint for the generator, not a guarantee of how each one sounds.");
    strength.addItemList ({ "Subtle changes", "Medium changes", "Wild changes" }, 1);
    strength.setSelectedId (2, juce::dontSendNotification);
    strength.setTooltip ("How far the variations may move from the current sound. Wild may also change routing and modulator ratios.");
    for (auto* cb : { &direction, &strength }) addAndMakeVisible (cb);

    generate.setTooltip ("Create eight variations of the current sound. Nothing changes until you press Keep this sound.");
    generate.setColour (juce::TextButton::buttonColourId, col::accent);
    generate.setColour (juce::TextButton::textColourOffId, col::onAccent);
    keepUnchanged.setTooltip ("Choose parts of the sound the variations must not touch");
    use.setTooltip ("Make the tile you are hearing your current sound. This replaces the sound you started from; save it first if you want to keep it.");
    use.setColour (juce::TextButton::buttonColourId, col::accent);
    use.setColour (juce::TextButton::textColourOffId, col::onAccent);
    more.setTooltip ("Eight new variations based on the tile you are hearing");
    back.setTooltip ("Go back to the previous set of variations (up to 20 steps)");
    done.setTooltip ("Close the variations and return to your current sound. Nothing is kept unless you pressed Keep this sound.");

    generate.onClick = [this] { pushSettings(); run ([] (PatchController& pc) { return pc.candidates().empty() ? pc.explore() : pc.regenerate(); }); };
    keepUnchanged.onClick = [this] { keepMenu(); };
    use.onClick = [this] {
        const int i = previewing;
        if (i < 0) return;
        if (run ([i] (PatchController& pc) { return pc.useCandidate (i); }))
            ctx.say ("Variation kept as the current sound. Save stores it as a preset.");
        else
            ctx.say ("The variation could not be applied.", true);
    };
    more.onClick = [this] {
        const int i = previewing;
        if (i < 0) return;
        pushSettings();
        run ([i] (PatchController& pc) { return pc.exploreFromThis (i); });
    };
    back.onClick = [this] { run ([] (PatchController& pc) { return pc.exploreBack(); }); };
    done.onClick = [this] { run ([] (PatchController& pc) { pc.closeExplorer(); return true; }); if (ctx.refreshAll) ctx.refreshAll(); };
    for (auto* b : { &generate, &keepUnchanged, &use, &more, &back, &done }) addAndMakeVisible (b);

    styleLabel (guide, 13.0f, col::text);
    addAndMakeVisible (guide);

    originalTile.original = true;
    originalTile.name = "Original";
    originalTile.detail = "Your current sound";
    originalTile.setTooltip ("Back to your current sound (plays a test chord)");
    originalTile.onClick = [this] {
        run ([] (PatchController& pc) { pc.cancelPreview(); return true; });
        ctx.proc.auditionChord();
    };
    addAndMakeVisible (originalTile);
    for (int i = 0; i < kNumCandidates; ++i)
    {
        auto* tile = tiles.add (new Tile());
        tile->letter = juce::String::charToString ((juce::juce_wchar) ('A' + i));
        tile->setTooltip ("Click to hear this variation. A test chord plays; your keyboard also plays it until you pick another tile.");
        tile->onClick = [this, i] {
            run ([i] (PatchController& pc) { return pc.previewIndex() == i ? true : pc.preview (i); });
            ctx.proc.auditionChord();
        };
        addAndMakeVisible (tile);
    }
}

void ExplorerPanel::keepMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Variations must not change...");
    m.addItem (1, "Operator routing and waveforms", true, locks.graph);
    m.addItem (2, "Pitch ratios", true, locks.ratios);
    m.addItem (3, "Envelopes", true, locks.envelopes);
    m.addItem (4, "Effects", true, locks.fx);
    m.addSeparator();
    const Preset p = ctx.snapshot();
    for (int i = 0; i < kNumMacros; ++i)
        m.addItem (10 + i, "Macro: " + (p.meta.macroNames[(size_t) i].empty() ? "Macro " + juce::String (i + 1) : utf8 (p.meta.macroNames[(size_t) i])), true, locks.macros[(size_t) i]);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&keepUnchanged), [safe = juce::Component::SafePointer<ExplorerPanel> (this)] (int r) {
        if (safe == nullptr || r <= 0) return;
        auto& l = safe->locks;
        if (r == 1) l.graph = ! l.graph;
        else if (r == 2) l.ratios = ! l.ratios;
        else if (r == 3) l.envelopes = ! l.envelopes;
        else if (r == 4) l.fx = ! l.fx;
        else if (r >= 10) l.macros[(size_t) (r - 10)] = ! l.macros[(size_t) (r - 10)];
        int n = (l.graph ? 1 : 0) + (l.ratios ? 1 : 0) + (l.envelopes ? 1 : 0) + (l.fx ? 1 : 0);
        for (bool b : l.macros) n += b ? 1 : 0;
        safe->keepUnchanged.setButtonText (n > 0 ? "Keep unchanged (" + juce::String (n) + ")" : "Keep unchanged...");
    });
}

void ExplorerPanel::pushSettings()
{
    ExploreSettings s;
    s.direction = (ExploreDirection) juce::jmax (0, direction.getSelectedItemIndex());
    s.strength = (ExploreStrength) juce::jmax (0, strength.getSelectedItemIndex());
    s.locks = locks;
    ctx.proc.withModel ([&s] (PatchController& pc) { pc.exploreSettings = s; return 0; });
}

bool ExplorerPanel::run (const std::function<bool (PatchController&)>& fn)
{
    const bool ok = ctx.proc.withModel ([&fn] (PatchController& pc) { return fn (pc); });
    refresh (ctx.snapshot());
    return ok;
}

void ExplorerPanel::paint (juce::Graphics& g) { paintPanel (g, getLocalBounds(), "Variations"); }

void ExplorerPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto head = r.removeFromTop (24);
    head.removeFromLeft (110); // panel title
    generate.setBounds (head.removeFromLeft (96));
    head.removeFromLeft (6);
    direction.setBounds (head.removeFromLeft (juce::jmin (140, head.getWidth() / 3)));
    head.removeFromLeft (4);
    strength.setBounds (head.removeFromLeft (juce::jmin (140, head.getWidth() / 2)));
    head.removeFromLeft (4);
    keepUnchanged.setBounds (head.removeFromLeft (juce::jmin (150, head.getWidth())));
    r.removeFromTop (5);
    guide.setBounds (r.removeFromTop (18));
    r.removeFromTop (4);
    auto actions = r.removeFromBottom (26);
    r.removeFromBottom (5);
    use.setBounds (actions.removeFromLeft (130));
    actions.removeFromLeft (6);
    more.setBounds (actions.removeFromLeft (112));
    done.setBounds (actions.removeFromRight (64));
    actions.removeFromRight (6);
    back.setBounds (actions.removeFromRight (112));

    originalTile.setBounds (r.removeFromLeft (juce::jmax (92, r.getWidth() / 6)).reduced (0, 0));
    r.removeFromLeft (4);
    const int cw = r.getWidth() / 4, ch = r.getHeight() / 2;
    for (int i = 0; i < tiles.size(); ++i)
        tiles[i]->setBounds (r.getX() + (i % 4) * cw, r.getY() + (i / 4) * ch, cw - 2, ch - 2);
}

void ExplorerPanel::refresh (const Preset&)
{
    struct View { std::vector<std::pair<juce::String, juce::String>> items; std::vector<bool> fallback; int preview = -1; bool history = false; };
    const View v = ctx.proc.withModel ([] (PatchController& pc) {
        View out;
        for (auto& c : pc.candidates()) { out.items.push_back ({ c.name, c.tendency }); out.fallback.push_back (c.fallback); }
        out.preview = pc.previewIndex();
        out.history = pc.hasExploreHistory();
        return out;
    });
    const bool active = ! v.items.empty();
    previewing = v.preview;
    for (int i = 0; i < tiles.size(); ++i)
    {
        auto* tile = tiles[i];
        const bool present = active && i < (int) v.items.size();
        tile->setEnabled (present);
        tile->name = present ? v.items[(size_t) i].first : juce::String ("-");
        tile->detail = present ? v.items[(size_t) i].second + (v.fallback[(size_t) i] ? " (safe fallback)" : "") : juce::String();
        tile->selected = present && v.preview == i;
        tile->setAlpha (present ? 1.0f : 0.4f);
        tile->repaint();
    }
    originalTile.setEnabled (active);
    originalTile.selected = active && v.preview < 0;
    originalTile.setAlpha (active ? 1.0f : 0.4f);
    originalTile.repaint();
    generate.setButtonText (active ? "New set" : "Generate");
    use.setEnabled (v.preview >= 0);
    more.setEnabled (v.preview >= 0);
    done.setEnabled (active);
    back.setEnabled (v.history);
    guide.setText (! active ? "1.  Press Generate to get eight variations of this sound."
                            : (v.preview < 0 ? "2.  Click a tile (A-H) to hear it. Original takes you back."
                                             : "3.  Like it? Keep this sound.  Close but not quite? More like this."),
                   juce::dontSendNotification);
}

// ======================================================================================= page
SimplePage::SimplePage (Ctx& c)
    : macros (c, 0, "Macros", true), character (c, 4, "Character", false), quick (c), gate (c, false), arp (c, true), explorer (c)
{
    for (auto* comp : std::initializer_list<juce::Component*> { &macros, &character, &quick, &gate, &arp, &explorer, &about }) addAndMakeVisible (comp);
    styleLabel (about, 12.5f, col::textDim);
    about.setMinimumHorizontalScale (0.8f);
}

void SimplePage::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    about.setBounds (r.removeFromBottom (18).reduced (6, 0));
    r.removeFromBottom (4);
    const int h = r.getHeight();
    auto top = r.removeFromTop (h * 31 / 100);
    r.removeFromTop (8);
    auto mid = r.removeFromTop (h * 23 / 100);
    r.removeFromTop (8);

    macros.setBounds (top.removeFromLeft (top.getWidth() * 58 / 100));
    top.removeFromLeft (8);
    character.setBounds (top);

    quick.setBounds (mid.removeFromLeft (mid.getWidth() * 44 / 100));
    mid.removeFromLeft (8);
    gate.setBounds (mid.removeFromLeft (mid.getWidth() / 2 - 4));
    mid.removeFromLeft (8);
    arp.setBounds (mid);

    explorer.setBounds (r);
}

void SimplePage::refresh (const Preset& p)
{
    macros.refresh (p);
    character.refresh (p);
    gate.refresh (p);
    arp.refresh (p);
    explorer.refresh (p);
    juce::String t = categoryName (p.meta.category);
    if (! p.meta.tags.empty())
    {
        t << "  |  " << joinTags (p.meta.tags);
    }
    if (! p.meta.description.empty()) t << "  -  " << utf8 (p.meta.description);
    about.setText (t, juce::dontSendNotification);
}
} // namespace ui
