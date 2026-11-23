#include "UI/Advanced/ModulationTab.h"

namespace ui
{
using namespace fmlume;

namespace
{
// Target combos: item id = parameter code + 1, and a dedicated id for "(none)" (code -1).
constexpr int kNoneId = 10000;
int idToCode (int id) { return id == kNoneId ? -1 : id - 1; }
int codeToId (int code) { return code >= 0 ? code + 1 : kNoneId; }

// Fills a combo with every parameter the predicate accepts, grouped by section. Item id = code + 1.
void fillTargets (juce::ComboBox& box, const Preset& p, bool sideB, bool modTargetsOnly)
{
    box.clear (juce::dontSendNotification);
    box.addItem ("(none)", kNoneId);
    const auto& e = sideOf (p, sideB).engine;
    auto accept = [&] (int c) {
        if (modTargetsOnly ? ! isModTarget (c) : ! isMacroTarget (c)) return false;
        if (! paramExists (p.data, sideB, c)) return false;
        if (pc::isOp (c) && ! e.operators[(size_t) (c / kOpFieldCount)].enabled) return false;
        if (pc::isFx (c) && ! p.data.fx.proc[(size_t) ((c - pc::fxBase) / kFxParams)].enabled) return false;
        if (c >= pc::matrixDepthBase && c < pc::engineCount && ! sideOf (p, sideB).matrix[(size_t) (c - pc::matrixDepthBase)].enabled) return false;
        return true;
    };
    auto section = [&] (const juce::String& name, int from, int to) {
        bool header = false;
        for (int c = from; c < to; ++c)
        {
            if (! accept (c)) continue;
            if (! header) { box.addSectionHeading (name); header = true; }
            box.addItem (paramDisplayName (c), c + 1);
        }
    };
    section ("FM amounts", pc::edgeBase, pc::filterCutoff);
    for (int k = 0; k < kNumOps; ++k) section ("Operator " + juce::String (k + 1), pc::op (k, 0), pc::op (k + 1, 0));
    section ("Filter / global", pc::filterCutoff, pc::matrixDepthBase);
    section ("Mod matrix depths", pc::matrixDepthBase, pc::engineCount);
    section ("Effects", pc::fxBase, pc::fxEnd);
    section ("Performance", pc::portamento, pc::total);
}
} // namespace

struct ModulationTab::LfoBox final : public juce::Component
{
    LfoBox (Ctx& c, int i)
        : index (i),
          shape (c, "LFO shape", labels::list (labels::lfoShape),
                 [&c, i] (const Preset& p) { return (int) sideOf (p, c.sideB()).engine.lfo[(size_t) i].shape; },
                 [&c, i] (Preset& p, int v) { sideOf (p, c.sideB()).engine.lfo[(size_t) i].shape = (LfoShape) v; }),
          division (c, "Tempo division", divisionNames (0, kNumSyncDivs - 1),
                    [&c, i] (const Preset& p) { return (int) sideOf (p, c.sideB()).engine.lfo[(size_t) i].syncDiv; },
                    [&c, i] (Preset& p, int v) { sideOf (p, c.sideB()).engine.lfo[(size_t) i].syncDiv = (uint8_t) v; }),
          sync (c, "Tempo sync", "Lock the LFO rate to the host tempo",
                [&c, i] (const Preset& p) { return sideOf (p, c.sideB()).engine.lfo[(size_t) i].sync; },
                [&c, i] (Preset& p, bool v) { sideOf (p, c.sideB()).engine.lfo[(size_t) i].sync = v; }),
          retrig (c, "Retrigger", "Restart the LFO at its start phase on every note",
                  [&c, i] (const Preset& p) { return sideOf (p, c.sideB()).engine.lfo[(size_t) i].retrig; },
                  [&c, i] (Preset& p, bool v) { sideOf (p, c.sideB()).engine.lfo[(size_t) i].retrig = v; }),
          rate (c, i == 0 ? pc::lfo1Rate : pc::lfo2Rate, "Rate"),
          phase (c, "Start phase", 0.0f, 1.0f, 0.01f, 0.0f, "",
                 [&c, i] (const Preset& p) { return sideOf (p, c.sideB()).engine.lfo[(size_t) i].phase; },
                 [&c, i] (Preset& p, float v) { sideOf (p, c.sideB()).engine.lfo[(size_t) i].phase = v; })
    {
        for (auto* comp : std::initializer_list<juce::Component*> { &shape, &division, &sync, &retrig, &rate, &phase }) addAndMakeVisible (comp);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (col::text);
        g.setFont (font (13.0f, true));
        g.drawText ("LFO " + juce::String (index + 1), getLocalBounds().removeFromTop (16).removeFromLeft (76), juce::Justification::centred);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        auto knobArea = r.removeFromLeft (76);
        knobArea.removeFromTop (16);
        rate.setBounds (knobArea);
        r.removeFromLeft (6);
        auto row = r.removeFromTop (24);
        shape.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (1, 0));
        division.setBounds (row.reduced (1, 0));
        r.removeFromTop (3);
        row = r.removeFromTop (22);
        sync.setBounds (row.removeFromLeft (row.getWidth() / 2));
        retrig.setBounds (row);
        r.removeFromTop (3);
        phase.setBounds (r.removeFromTop (22));
    }
    void refresh (const Preset& p)
    {
        shape.refreshFrom (p); division.refreshFrom (p); sync.refreshFrom (p); retrig.refreshFrom (p); phase.refreshFrom (p);
        rate.refresh();
        const bool synced = sync.getToggleState();
        division.setEnabled (synced);
        rate.setAlpha (synced ? 0.4f : 1.0f);
    }
    int index;
    BoundCombo shape, division;
    BoundToggle sync, retrig;
    ModelKnob rate;
    BoundSlider phase;
};

struct ModulationTab::MatrixRow final : public juce::Component
{
    MatrixRow (Ctx& c, int i)
        : ctx (c), slot (i),
          enabled (c, juce::String (i + 1), "Enable this modulation slot",
                   [&c, i] (const Preset& p) { return sideOf (p, c.sideB()).matrix[(size_t) i].enabled; },
                   [&c, i] (Preset& p, bool v) { sideOf (p, c.sideB()).matrix[(size_t) i].enabled = v; }),
          source (c, "Modulation source", labels::list (labels::modSource),
                  [&c, i] (const Preset& p) { return (int) sideOf (p, c.sideB()).matrix[(size_t) i].source; },
                  [&c, i] (Preset& p, int v) { sideOf (p, c.sideB()).matrix[(size_t) i].source = (ModSource) v; }),
          curve (c, "Response curve", labels::list (labels::curve),
                 [&c, i] (const Preset& p) { return (int) sideOf (p, c.sideB()).matrix[(size_t) i].curve; },
                 [&c, i] (Preset& p, int v) { sideOf (p, c.sideB()).matrix[(size_t) i].curve = (Curve) v; }),
          depth (c, "", -1.0f, 1.0f, 0.001f, 0.0f, "",
                 [&c, i] (const Preset& p) { return sideOf (p, c.sideB()).matrix[(size_t) i].depth; },
                 [&c, i] (Preset& p, float v) { sideOf (p, c.sideB()).matrix[(size_t) i].depth = v; })
    {
        depth.captionWidth = 0;
        depth.slider.setTooltip ("Depth: 1.0 moves the target across its whole range. Small values go a long way for pitch (0.01 = about a quarter tone).");
        target.setTooltip ("Modulation target. Only existing connections can be modulated; the matrix never creates new FM connections.");
        target.onChange = [this] {
            const int id = target.getSelectedId();
            if (id == 0) return;
            const int code = idToCode (id);
            const bool b = ctx.sideB();
            ctx.edit ([this, code, b] (Preset& p) {
                auto& m = sideOf (p, b).matrix[(size_t) slot];
                m.target = (int16_t) code;
                if (code < 0) m.enabled = false;
            });
        };
        for (auto* comp : std::initializer_list<juce::Component*> { &enabled, &source, &target, &curve, &depth }) addAndMakeVisible (comp);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        enabled.setBounds (r.removeFromLeft (50));
        source.setBounds (r.removeFromLeft (r.getWidth() * 22 / 100).reduced (2, 1));
        target.setBounds (r.removeFromLeft (r.getWidth() * 36 / 100).reduced (2, 1));
        curve.setBounds (r.removeFromRight (84).reduced (2, 1));
        depth.setBounds (r.reduced (2, 0));
    }
    void refresh (const Preset& p)
    {
        enabled.refreshFrom (p); source.refreshFrom (p); curve.refreshFrom (p); depth.refreshFrom (p);
        const int t = sideOf (p, ctx.sideB()).matrix[(size_t) slot].target;
        target.setSelectedId (codeToId (t), juce::dontSendNotification);
    }
    Ctx& ctx;
    int slot;
    BoundToggle enabled;
    BoundCombo source, curve;
    juce::ComboBox target;
    BoundSlider depth;
};

struct ModulationTab::MappingRow final : public juce::Component
{
    MappingRow (Ctx& c, ModulationTab& o, int i)
        : ctx (c), owner (o), row (i),
          curve (c, "Macro curve", labels::list (labels::curve),
                 [this] (const Preset& p) { const auto* m = mapping (p); return m ? (int) m->curve : 0; },
                 [this] (Preset& p, int v) { if (auto* m = mapping (p)) m->curve = (Curve) v; }),
          polarity (c, "Polarity", labels::list (labels::polarity),
                    [this] (const Preset& p) { const auto* m = mapping (p); return m ? (int) m->polarity : 0; },
                    [this] (Preset& p, int v) { if (auto* m = mapping (p)) m->polarity = (Polarity) v; }),
          amount (c, "", -1.0f, 1.0f, 0.001f, 0.0f, "",
                  [this] (const Preset& p) { const auto* m = mapping (p); return m ? m->amount : 0.0f; },
                  [this] (Preset& p, float v) { if (auto* m = mapping (p)) m->amount = v; })
    {
        amount.captionWidth = 0;
        amount.slider.setTooltip ("Amount: how far the macro moves this parameter at full throw (normalised; 1.0 = the whole range). 50 % macro is always neutral.");
        target.setTooltip ("Parameter controlled by this macro. Choose (none) to remove the mapping. Stepped parameters (waveform, routing, FX type) cannot be macro targets.");
        target.onChange = [this] {
            const int id = target.getSelectedId();
            if (id == 0) return;
            const int code = idToCode (id);
            const bool b = ctx.sideB();
            const int macro = owner.selectedMacro;
            ctx.edit ([this, code, b, macro] (Preset& p) {
                auto& m = sideOf (p, b).macros[(size_t) macro];
                if (code < 0)
                {
                    if (row < m.numMappings)
                    {
                        for (int j = row; j + 1 < m.numMappings; ++j) m.mappings[(size_t) j] = m.mappings[(size_t) j + 1];
                        --m.numMappings;
                    }
                }
                else if (row < m.numMappings) m.mappings[(size_t) row].target = (int16_t) code;
                else if (m.numMappings < kMaxMacroMappings)
                {
                    auto& mp = m.mappings[(size_t) m.numMappings++];
                    mp = MacroMapping {};
                    mp.target = (int16_t) code;
                    mp.amount = 0.2f;
                }
            });
        };
        // Range: the window (in % of the parameter's full range) the macro may move this target in.
        range.setSliderStyle (juce::Slider::TwoValueHorizontal);
        range.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        range.setRange (0.0, 100.0, 1.0);
        range.setMinAndMaxValues (0.0, 100.0, juce::dontSendNotification);
        range.setColour (juce::Slider::trackColourId, col::violet);
        range.onValueChange = [this] {
            if (updating) return;
            const float lo = (float) range.getMinValue() * 0.01f, hi = (float) range.getMaxValue() * 0.01f;
            ctx.edit ([this, lo, hi] (Preset& p) { if (auto* m = mapping (p)) { m->min = lo; m->max = juce::jmax (lo, hi); } });
            showRange();
        };
        for (auto* comp : std::initializer_list<juce::Component*> { &target, &curve, &polarity, &amount, &range }) addAndMakeVisible (comp);
    }
    void showRange()
    {
        range.setTooltip ("Range: this macro keeps the target between " + juce::String (juce::roundToInt (range.getMinValue())) + " % and "
                          + juce::String (juce::roundToInt (range.getMaxValue())) + " % of the parameter's full range. Drag either end.");
    }
    // Column layout shared with the header labels painted by the tab.
    struct Columns { juce::Rectangle<int> target, amount, range, curve, polarity; };
    static Columns columns (juce::Rectangle<int> r)
    {
        Columns c;
        c.target = r.removeFromLeft (r.getWidth() * 32 / 100);
        c.polarity = r.removeFromRight (86);
        c.curve = r.removeFromRight (84);
        c.range = r.removeFromRight (juce::jlimit (60, 120, r.getWidth() * 38 / 100));
        c.amount = r;
        return c;
    }
    const MacroMapping* mapping (const Preset& p) const
    {
        const auto& m = sideOf (p, ctx.sideB()).macros[(size_t) owner.selectedMacro];
        return row < m.numMappings ? &m.mappings[(size_t) row] : nullptr;
    }
    MacroMapping* mapping (Preset& p) const { return const_cast<MacroMapping*> (mapping (const_cast<const Preset&> (p))); }
    void resized() override
    {
        const auto c = columns (getLocalBounds());
        target.setBounds (c.target.reduced (2, 1));
        polarity.setBounds (c.polarity.reduced (2, 1));
        curve.setBounds (c.curve.reduced (2, 1));
        range.setBounds (c.range.reduced (6, 0));
        amount.setBounds (c.amount.reduced (2, 0));
    }
    void refresh (const Preset& p)
    {
        const auto* m = mapping (p);
        const int count = sideOf (p, ctx.sideB()).macros[(size_t) owner.selectedMacro].numMappings;
        setVisible (row <= count); // existing rows plus one empty row to add a mapping
        target.setSelectedId (codeToId (m ? m->target : -1), juce::dontSendNotification);
        for (auto* comp : std::initializer_list<juce::Component*> { &curve, &polarity, &amount, &range }) comp->setEnabled (m != nullptr);
        curve.refreshFrom (p); polarity.refreshFrom (p); amount.refreshFrom (p);
        if (! range.isMouseButtonDown())
        {
            updating = true;
            range.setMinAndMaxValues (m ? m->min * 100.0 : 0.0, m ? m->max * 100.0 : 100.0, juce::dontSendNotification);
            updating = false;
            showRange();
        }
    }
    Ctx& ctx;
    ModulationTab& owner;
    int row;
    juce::ComboBox target;
    BoundCombo curve, polarity;
    BoundSlider amount;
    juce::Slider range;
    bool updating = false;
};

ModulationTab::ModulationTab (Ctx& c) : ctx (c)
{
    for (int i = 0; i < 2; ++i) { lfos[i] = std::make_unique<LfoBox> (ctx, i); addAndMakeVisible (*lfos[i]); }
    static const char* const envNames[4] = { "Attack", "Decay", "Sustain", "Release" };
    for (int i = 0; i < 4; ++i) addAndMakeVisible (envKnobs.add (new ModelKnob (ctx, pc::modEnvA + i, envNames[i])));
    for (int i = 0; i < kNumModSlots; ++i) addAndMakeVisible (matrix.add (new MatrixRow (ctx, i)));
    filterType = std::make_unique<BoundCombo> (ctx, "Filter type", labels::list (labels::filterType),
                                               [this] (const Preset& p) { return (int) sideOf (p, ctx.sideB()).engine.filter.type; },
                                               [this] (Preset& p, int v) { sideOf (p, ctx.sideB()).engine.filter.type = (FilterType) v; });
    addAndMakeVisible (*filterType);
    static const char* const filterNames[3] = { "Cutoff", "Resonance", "Drive" };
    for (int i = 0; i < 3; ++i) addAndMakeVisible (filterKnobs.add (new ModelKnob (ctx, pc::filterCutoff + i, filterNames[i])));

    juce::StringArray macroNames;
    for (int i = 0; i < kNumMacros; ++i) macroNames.add ("Macro " + juce::String (i + 1));
    styleCombo (macroSelect, macroNames, "Choose the macro to edit");
    macroSelect.setSelectedItemIndex (0, juce::dontSendNotification);
    macroSelect.onChange = [this] { selectedMacro = juce::jmax (0, macroSelect.getSelectedItemIndex()); refresh (ctx.snapshot()); };
    addAndMakeVisible (macroSelect);
    macroName.setFont (font (14.0f));
    macroName.setTooltip ("Display name of this macro in the Simple view. The host automation name stays \"Macro N\".");
    macroName.setInputRestrictions (24);
    auto commitName = [this] {
        const std::string name = macroName.getText().trim().toStdString();
        const bool b = ctx.sideB();
        const int m = selectedMacro;
        if (name == (b ? ctx.snapshot().meta.macroNamesB : ctx.snapshot().meta.macroNames)[(size_t) m]) return;
        ctx.edit ([=] (Preset& p) { (b ? p.meta.macroNamesB : p.meta.macroNames)[(size_t) m] = name; });
    };
    macroName.onReturnKey = commitName;
    macroName.onFocusLost = commitName;
    addAndMakeVisible (macroName);
    for (int i = 0; i < kMaxMacroMappings; ++i) addAndMakeVisible (mappings.add (new MappingRow (ctx, *this, i)));
    styleLabel (macroHint, 11.5f);
    releaseKeysAfterTextEntry (*this); // this tab is not in the editor's hierarchy until it is shown
    macroHint.setText ("Up to 8 targets per macro. Pick a target in the empty row to add one. Range limits how far the macro can move a target.", juce::dontSendNotification);
    addAndMakeVisible (macroHint);
}

ModulationTab::~ModulationTab() = default;

void ModulationTab::paint (juce::Graphics& g)
{
    paintPanel (g, filterArea, "Filter");
    paintPanel (g, lfoArea, "LFOs");
    paintPanel (g, envArea, "Mod Envelope");
    paintPanel (g, matrixArea, "Mod Matrix");
    paintPanel (g, macroArea, "Macro Editor");
    if (! mappings.isEmpty())
    {
        // column headers above the mapping rows
        const auto first = mappings[0]->getBounds();
        const auto c = MappingRow::columns (first.withY (first.getY() - 16).withHeight (14));
        g.setColour (col::textDim);
        g.setFont (font (11.0f));
        g.drawText ("Target", c.target.reduced (4, 0), juce::Justification::centredLeft);
        g.drawText ("Amount", c.amount.reduced (4, 0), juce::Justification::centredLeft);
        g.drawText ("Range", c.range.reduced (6, 0), juce::Justification::centredLeft);
        g.drawText ("Curve", c.curve.reduced (4, 0), juce::Justification::centredLeft);
        g.drawText ("Polarity", c.polarity.reduced (4, 0), juce::Justification::centredLeft);
    }
}

void ModulationTab::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto left = r.removeFromLeft (r.getWidth() * 46 / 100);
    r.removeFromLeft (8);
    auto top = left.removeFromTop (juce::jlimit (200, 244, left.getHeight() * 42 / 100));
    left.removeFromTop (8);
    envArea = top.removeFromRight (juce::jmin (250, top.getWidth() * 40 / 100));
    top.removeFromRight (8);
    lfoArea = top;
    matrixArea = left;
    filterArea = r.removeFromTop (juce::jlimit (120, 150, r.getHeight() * 26 / 100));
    r.removeFromTop (8);
    macroArea = r;

    auto f = panelContent (filterArea, 12, 8);
    filterType->setBounds (f.removeFromLeft (juce::jmin (170, f.getWidth() / 3)).withSizeKeepingCentre (juce::jmin (170, f.getWidth() / 3), 26));
    f.removeFromLeft (10);
    layoutKnobRow (f, { filterKnobs.begin(), filterKnobs.end() }, 100);

    auto l = lfoArea.reduced (10).withTrimmedTop (22);
    lfos[0]->setBounds (l.removeFromTop (l.getHeight() / 2).reduced (0, 2));
    lfos[1]->setBounds (l.reduced (0, 2));
    auto e = envArea.reduced (8).withTrimmedTop (24);
    const int kw = e.getWidth() / 4;
    for (auto* k : envKnobs) k->setBounds (e.removeFromLeft (kw).withHeight (juce::jmin (e.getHeight(), 96)));
    auto m = matrixArea.reduced (8).withTrimmedTop (26);
    const int rh = juce::jmin (30, juce::jmax (22, m.getHeight() / kNumModSlots));
    for (auto* row : matrix) row->setBounds (m.removeFromTop (rh).reduced (0, 2));
    auto mc = macroArea.reduced (10).withTrimmedTop (26);
    auto head = mc.removeFromTop (26);
    macroSelect.setBounds (head.removeFromLeft (110));
    head.removeFromLeft (8);
    macroName.setBounds (head.removeFromLeft (juce::jmin (220, head.getWidth())));
    mc.removeFromTop (22); // room for the column headers
    macroHint.setBounds (mc.removeFromBottom (18));
    const int mh = juce::jmin (30, mc.getHeight() / kMaxMacroMappings);
    for (auto* row : mappings) row->setBounds (mc.removeFromTop (mh).reduced (0, 2));
}

void ModulationTab::rebuildTargets (const Preset& p)
{
    const bool b = ctx.sideB();
    const auto& s = sideOf (p, b);
    juce::String sig (b ? "B" : "A");
    for (auto& op : s.engine.operators) sig << (op.enabled ? '1' : '0');
    for (int i = 0; i < s.engine.numEdges; ++i) sig << (int) s.engine.edges[(size_t) i].from << (int) s.engine.edges[(size_t) i].to;
    for (auto& pr : p.data.fx.proc) sig << (pr.enabled ? 'e' : 'd');
    for (auto& m : s.matrix) sig << (m.enabled ? 'M' : 'm');
    if (sig == targetSignature) return;
    targetSignature = sig;
    for (auto* row : matrix) fillTargets (row->target, p, b, true);
    for (auto* row : mappings) fillTargets (row->target, p, b, false);
}

void ModulationTab::refresh (const Preset& p)
{
    rebuildTargets (p);
    for (auto& l : lfos) l->refresh (p);
    for (auto* k : envKnobs) k->refresh();
    filterType->refreshFrom (p);
    const bool filterOn = sideOf (p, ctx.sideB()).engine.filter.type != FilterType::Off;
    for (auto* k : filterKnobs) { k->refresh(); k->setAlpha (filterOn ? 1.0f : 0.45f); }
    for (auto* row : matrix) row->refresh (p);
    for (auto* row : mappings) row->refresh (p);
    // A different preset (or slot) replaces the text even while the field has focus, so a pending
    // edit can never be committed onto the wrong sound.
    const juce::String owner = juce::String (p.meta.patchId) + (ctx.sideB() ? ":B:" : ":A:") + juce::String (selectedMacro);
    const bool otherOwner = owner != macroNameOwner;
    macroNameOwner = owner;
    if (otherOwner || ! macroName.hasKeyboardFocus (true))
        macroName.setText ((ctx.sideB() ? p.meta.macroNamesB : p.meta.macroNames)[(size_t) selectedMacro], juce::dontSendNotification);
}
} // namespace ui
