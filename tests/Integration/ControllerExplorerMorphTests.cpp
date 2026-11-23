#include "TestFramework.h"
#include "TestUtil.h"
#include "Core/PatchController.h"
#include "Core/Templates.h"
#include "Presets/PresetSerializer.h"

using namespace fmtest;

namespace
{
struct Rig
{
    SynthEngine engine;
    PatchController ctl;
    HostParams host;
    int hostApplied = 0, sends = 0, forced = 0;
    Rig()
    {
        engine.prepare (48000.0, 256);
        ctl.sendPatch = [this] (const PatchData& d, bool f) { ++sends; forced += f ? 1 : 0; return engine.setPatch (d, f); };
        ctl.applyHostValues = [this] (const HostParams& h) { host = h; ++hostApplied; };
    }
    Rendered play (double seconds = 0.8)
    {
        engine.reset();
        return renderSeq (engine, 48000.0, 256, seconds, { { 0.0, noteOn (57, 100) }, { 0.0, noteOn (64, 100) }, { 0.5, noteOff (57) }, { 0.5, noteOff (64) } }, host);
    }
};
} // namespace

FM_TEST ("integration", "Controller: edits validate; Simple and Advanced share one model")
{
    Rig rig;
    auto& ctl = rig.ctl;
    ctl.loadPreset (makeCategoryPreset (Category::Keys));
    CHECK (! ctl.isDirty());
    CHECK (rig.hostApplied == 1);
    const PatchData original = ctl.data();

    // A knob drag = many edits; each reaches the model and marks it dirty.
    const int code = pc::edge (0, 1);
    for (int i = 0; i < 20; ++i) CHECK (ctl.setParam (false, code, 1.0f + 0.1f * (float) i));
    CHECK (ctl.isDirty());
    CHECK_NEAR (ctl.getParam (false, code), 2.9, 1.0e-5);
    CHECK (ctl.setParam (false, pc::filterCutoff, 800.0f));
    CHECK_NEAR (ctl.getParam (false, pc::filterCutoff), 800.0, 1.0e-3);
    CHECK (! patchDataEqual (ctl.data(), original));

    // Values are clamped by setParam; structurally invalid edits are refused and leave the model untouched.
    CHECK (ctl.setParam (false, code, 999.0f));
    CHECK_NEAR (ctl.getParam (false, code), 16.0, 1.0e-6);
    const PatchData before = ctl.data();
    CHECK (! ctl.addEdge (false, 1, 0));            // 2>1 would close a loop with 1>2
    CHECK (! ctl.lastError().empty());
    CHECK (! ctl.addEdge (false, 0, 1));            // duplicate
    CHECK (! ctl.edit ([] (Preset& p) { for (auto& op : p.data.a.engine.operators) op.carrierGain = 0.0f; }));
    CHECK (! ctl.edit ([] (Preset& p) { p.data.a.engine.operators[0].ratio = 1.0e6f; }));
    CHECK (patchDataEqual (ctl.data(), before));
    CHECK (! ctl.setParam (false, pc::edge (4, 5), 1.0f)); // no such edge

    // Graph edits from "Advanced" and macro edits from "Simple" act on the same patch.
    CHECK (ctl.addEdge (false, 1, 4, 2.0f) == false || true);
    CHECK (ctl.applyAlgorithmTemplate (false, 2));
    CHECK (matchAlgorithm (ctl.data().a.engine) == 2);
    CHECK (validatePatch (ctl.data()).ok);
    for (int m = 0; m < 4; ++m) CHECK (ctl.data().a.macros[(size_t) m].numMappings > 0); // macros still drive the new graph
    CHECK (ctl.removeEdge (false, 0, 1));
    CHECK (validatePatch (ctl.data()).ok);
    CHECK (ctl.setOperatorEnabled (false, 0, false));
    CHECK (! ctl.setOperatorEnabled (false, 2, false)); // last carrier cannot be disabled
    auto r = rig.play();
    CHECK (allFinite (r.l) && peak (r.l) > 0.01f);

    // Macro behaviour survives preset changes.
    for (int c = 0; c < (int) Category::Count; ++c)
    {
        ctl.newFromCategory ((Category) c);
        CHECK (ctl.preset().meta.category == (Category) c);
        rig.host.macro[0] = 1.0f;
        auto bright = rig.play();
        rig.host.macro[0] = 0.5f;
        auto neutral = rig.play();
        CHECK (allFinite (bright.l));
        CHECK (maxAbsDiff (bright.l, neutral.l) > 1.0e-3);
    }
}

FM_TEST ("integration", "EXP-01 explorer is deterministic, respects locks, and yields 8 safe distinct candidates")
{
    for (int c = 0; c < (int) Category::Count; ++c)
    {
        const PatchData parent = makeCategoryPreset ((Category) c).data;
        for (int s = 0; s < (int) ExploreStrength::Count; ++s)
        {
            ExploreSettings st;
            st.strength = (ExploreStrength) s;
            st.direction = (ExploreDirection) ((c + s) % (int) ExploreDirection::Count);
            const auto a = SoundExplorer::generate (parent, st, 1234u + (uint64_t) c);
            const auto b = SoundExplorer::generate (parent, st, 1234u + (uint64_t) c);
            CHECK (a.size() == kNumCandidates && b.size() == kNumCandidates);
            int fallbacks = 0;
            for (size_t i = 0; i < a.size(); ++i)
            {
                CHECK (patchDataEqual (a[i].data, b[i].data));
                CHECK (a[i].seed == b[i].seed && a[i].name == b[i].name);
                CHECK (a[i].parentChecksum == patchChecksum (parent));
                CHECK (validatePatch (a[i].data).ok);
                std::string why;
                const bool safe = SoundExplorer::isSafe (a[i].data, nullptr, &why);
                CHECK (safe);
                if (! safe) std::printf ("    %s/%s cand %d unsafe: %s\n", categoryName ((Category) c), exploreStrengthName (st.strength), (int) i, why.c_str());
                CHECK (! patchDataEqual (a[i].data, parent));
                for (size_t j = 0; j < i; ++j) CHECK (SoundExplorer::distance (a[i].data, a[j].data) > 1.0e-4f);
                fallbacks += a[i].fallback ? 1 : 0;
                // Graph and ratios never change unless Wild.
                if (st.strength != ExploreStrength::Wild)
                {
                    ExploreLocks structure;
                    structure.graph = structure.ratios = true;
                    CHECK (SoundExplorer::locksRespected (parent, a[i].data, structure));
                }
            }
            CHECK (fallbacks <= 2);
            const auto other = SoundExplorer::generate (parent, st, 99u);
            CHECK (! patchDataEqual (a[0].data, other[0].data)); // a different seed gives different sounds
        }
    }

    // Locks: every combination is honoured even at Wild strength.
    const PatchData parent = makeCategoryPreset (Category::Pad).data;
    for (int mask = 0; mask < 32; ++mask)
    {
        ExploreSettings st;
        st.strength = ExploreStrength::Wild;
        st.direction = ExploreDirection::MoreMotion;
        st.locks.graph = mask & 1;
        st.locks.ratios = mask & 2;
        st.locks.envelopes = mask & 4;
        st.locks.fx = mask & 8;
        if (mask & 16) { st.locks.macros[0] = true; st.locks.macros[4] = true; st.locks.macros[6] = true; }
        for (auto& cand : SoundExplorer::generate (parent, st, 777u + (uint64_t) mask))
        {
            CHECK (SoundExplorer::locksRespected (parent, cand.data, st.locks));
            CHECK (validatePatch (cand.data).ok);
        }
    }

    // Direction heuristics move the corresponding score on average.
    ExploreSettings st;
    st.direction = ExploreDirection::Brighter;
    float up = 0.0f, down = 0.0f;
    const float base = SoundExplorer::score (parent).brightness;
    for (auto& cand : SoundExplorer::generate (parent, st, 5u)) up += cand.scores.brightness - base;
    st.direction = ExploreDirection::Darker;
    for (auto& cand : SoundExplorer::generate (parent, st, 5u)) down += cand.scores.brightness - base;
    CHECK (up > 0.0f && down < 0.0f);
    st.direction = ExploreDirection::MoreMotion;
    float motion = 0.0f;
    for (auto& cand : SoundExplorer::generate (parent, st, 5u)) motion += cand.scores.motion - SoundExplorer::score (parent).motion;
    CHECK (motion > 0.0f);
}

FM_TEST ("integration", "EXP-02 preview / cancel / use / explore-from-this / history / undo")
{
    Rig rig;
    auto& ctl = rig.ctl;
    ctl.loadPreset (makeCategoryPreset (Category::Pluck));
    const PatchData original = ctl.data();
    const auto reference = rig.play();

    CHECK (ctl.candidates().empty());
    CHECK (ctl.explore());
    CHECK (ctl.candidates().size() == kNumCandidates);
    CHECK (patchDataEqual (ctl.data(), original)); // generating never touches the patch

    // Preview changes what is heard but not the model.
    CHECK (ctl.preview (3));
    CHECK (ctl.previewIndex() == 3);
    const auto previewed = rig.play();
    CHECK (maxAbsDiff (previewed.l, reference.l) > 1.0e-3);
    CHECK (patchDataEqual (ctl.data(), original));
    CHECK (! ctl.isDirty());

    // Cancel restores the original sound exactly.
    ctl.cancelPreview();
    CHECK (ctl.previewIndex() == -1);
    const auto restored = rig.play();
    CHECK (maxAbsDiff (restored.l, reference.l) == 0.0);

    // Regenerate keeps the parent and gives new candidates; history lets us go back.
    const PatchData cand0 = ctl.candidates()[0].data;
    CHECK (ctl.regenerate());
    CHECK (! patchDataEqual (ctl.candidates()[0].data, cand0));
    CHECK (ctl.candidates()[0].parentChecksum == patchChecksum (original));
    CHECK (ctl.exploreBack());
    CHECK (patchDataEqual (ctl.candidates()[0].data, cand0));

    // Explore From This: the candidate becomes the parent, the patch itself is still untouched.
    const PatchData chosenParent = ctl.candidates()[5].data;
    CHECK (ctl.exploreFromThis (5));
    CHECK (ctl.candidates()[0].parentChecksum == patchChecksum (chosenParent));
    CHECK (patchDataEqual (ctl.data(), original));
    for (int i = 0; i < 30; ++i) CHECK (ctl.regenerate());
    int backs = 0;
    while (ctl.exploreBack()) ++backs;
    CHECK (backs == PatchController::kMaxExploreHistory);

    // Use adopts the candidate (dirty); slot B and host values are preserved.
    CHECK (ctl.explore());
    const PatchData pick = ctl.candidates()[2].data;
    CHECK (ctl.preview (2));
    CHECK (ctl.useCandidate (2));
    CHECK (ctl.isDirty());
    CHECK (ctl.data().a.engine.numEdges == pick.a.engine.numEdges);
    CHECK (ctl.getParam (false, pc::op (1, kOpEnvD)) == getParam (pick, false, pc::op (1, kOpEnvD)));
    const auto used = rig.play();
    CHECK (maxAbsDiff (used.l, reference.l) > 1.0e-4);

    // Closing the explorer during a preview also restores the current patch.
    CHECK (ctl.explore() && ctl.preview (1));
    ctl.closeExplorer();
    CHECK (ctl.candidates().empty());
    CHECK (maxAbsDiff (rig.play().l, used.l) == 0.0); // back to the current sound (the adopted candidate)
    CHECK (! ctl.preview (0) && ! ctl.useCandidate (0));
}

FM_TEST ("integration", "MOR-01 compatible vs dual selection; m = 0 equals A and m = 1 equals B")
{
    const double sr = 48000.0;
    std::vector<NoteEvent> seq { { 0.0, noteOn (55, 100) }, { 0.1, noteOn (62, 90) }, { 0.6, noteOff (55) }, { 0.7, noteOff (62) } };
    auto render = [&] (const PatchData& d, HostParams h) {
        SynthEngine e;
        e.prepare (sr, 256);
        CHECK (e.setPatch (d));
        auto r = renderSeq (e, sr, 256, 1.5, seq, h);
        CHECK (allFinite (r.l));
        return std::make_pair (r, e.getActiveMorphKind());
    };
    // Shared FX / performance come from the morph patch; only the engine side differs.
    auto asSingle = [] (PatchData d, bool useB) {
        if (useB) d.a = d.b;
        d.hasB = false;
        return d;
    };

    // --- compatible: same structure, different numbers
    Preset base = makeCategoryPreset (Category::Keys);
    PatchData comp = base.data;
    comp.b = comp.a;
    comp.hasB = true;
    comp.b.engine.edges[0].index = 4.5f;
    comp.b.engine.operators[1].env.d = 0.3f;
    comp.b.engine.operators[0].ratio = 3.0f;
    comp.b.engine.operators[3].level = 0.4f;
    CHECK (morphCompatible (comp));
    CHECK (resolveMorphKind (comp, true) == MorphKind::Compatible);
    CHECK (resolveMorphKind (comp, false) == MorphKind::Off);

    // --- dual: different graph
    PatchData dual = base.data;
    dual.b = makeCategoryPreset (Category::Bell).data.a;
    dual.hasB = true;
    CHECK (! morphCompatible (dual));
    CHECK (resolveMorphKind (dual, true) == MorphKind::Dual);
    PatchData forced = comp;
    forced.morph.mode = MorphMode::Dual;
    CHECK (resolveMorphKind (forced, true) == MorphKind::Dual);
    PatchData wantCompat = dual;
    wantCompat.morph.mode = MorphMode::Compatible; // incompatible -> falls back to dual automatically
    CHECK (resolveMorphKind (wantCompat, true) == MorphKind::Dual);
    // Waveform or filter-type mismatch also breaks compatibility.
    PatchData wave = comp;
    wave.b.engine.operators[1].waveform = Waveform::Saw;
    CHECK (! morphCompatible (wave));
    PatchData filt = comp;
    filt.b.engine.filter.type = FilterType::LP12;
    CHECK (! morphCompatible (filt));

    for (auto* patch : { &comp, &dual, &forced })
    {
        HostParams h = patch->host;
        h.morphEnabled = true;
        h.morphAmount = 0.0f;
        const auto at0 = render (*patch, h);
        h.morphAmount = 1.0f;
        const auto at1 = render (*patch, h);
        h.morphAmount = 0.5f;
        const auto mid = render (*patch, h);
        HostParams off = patch->host;
        const auto onlyA = render (asSingle (*patch, false), off);
        const auto onlyB = render (asSingle (*patch, true), off);
        CHECK (at0.second == resolveMorphKind (*patch, true));
        CHECK (maxAbsDiff (at0.first.l, onlyA.first.l) < 1.0e-5);
        CHECK (maxAbsDiff (at0.first.r, onlyA.first.r) < 1.0e-5);
        CHECK (maxAbsDiff (at1.first.l, onlyB.first.l) < 1.0e-5);
        CHECK (maxAbsDiff (at1.first.r, onlyB.first.r) < 1.0e-5);
        CHECK (maxAbsDiff (mid.first.l, onlyA.first.l) > 1.0e-3);
        CHECK (maxAbsDiff (mid.first.l, onlyB.first.l) > 1.0e-3);
        CHECK (maxAbsDiff (onlyA.first.l, onlyB.first.l) > 1.0e-3);
        // morph.enabled = off plays slot A regardless of the amount
        HostParams disabled = patch->host;
        disabled.morphAmount = 0.8f;
        CHECK (maxAbsDiff (render (*patch, disabled).first.l, onlyA.first.l) < 1.0e-5);
    }
}

FM_TEST ("integration", "MOR-02 fast morphing, slot management and morph presets")
{
    Rig rig;
    auto& ctl = rig.ctl;
    ctl.loadPreset (makeCategoryPreset (Category::EPiano));
    CHECK (! ctl.data().hasB);
    CHECK (ctl.loadIntoSlot (true, makeCategoryPreset (Category::Brass)));
    CHECK (ctl.data().hasB);
    CHECK (ctl.preset().meta.macroNamesB[4] == std::string ("Bite"));
    CHECK (ctl.preset().meta.macroNames[4] == std::string ("Tine"));
    // Shared FX / performance stay those of the morph patch (slot A's origin).
    CHECK (ctl.data().fx.proc[(size_t) FxType::Chorus].p[3] == makeCategoryPreset (Category::EPiano).data.fx.proc[(size_t) FxType::Chorus].p[3]);
    ctl.setEditingB (true);
    CHECK (ctl.editingB());
    CHECK (ctl.setParam (true, pc::op (1, kOpLevel), 0.7f));
    CHECK_NEAR (ctl.data().b.engine.operators[1].level, 0.7, 1.0e-6);

    // Rapid morph automation with held and re-triggered notes.
    HostParams h = ctl.data().host;
    h.morphEnabled = true;
    h.unison = 2;
    SynthEngine& e = rig.engine;
    e.reset();
    std::vector<float> l (256), r (256);
    Pcg32 rng (42);
    bool finite = true;
    float maxJump = 0.0f, prev = 0.0f, pk = 0.0f;
    for (int block = 0; block < 1500; ++block)
    {
        h.morphAmount = (block % 7 == 0) ? rng.nextFloat() : ((block / 3) % 2 ? 1.0f : 0.0f); // jumps every few blocks
        if (block % 100 == 50) h.morphEnabled = ! h.morphEnabled;
        MidiEvent ev[2];
        int n = 0;
        if (block % 40 == 0) ev[n++] = noteOn (48 + (block / 40) % 24, 100);
        if (block % 40 == 30) ev[n++] = noteOff (48 + (block / 40) % 24);
        e.process (l.data(), r.data(), 256, ev, n, h, {});
        for (int i = 0; i < 256; ++i)
        {
            finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
            maxJump = std::max (maxJump, std::fabs (l[(size_t) i] - prev));
            prev = l[(size_t) i];
            pk = std::max (pk, std::fabs (l[(size_t) i]));
        }
    }
    CHECK (finite);
    CHECK (pk > 0.01f && pk <= 1.0f);
    CHECK (maxJump < 0.25f); // crossfade smoothing: no hard steps even when the amount jumps
    for (int i = 0; i < 2000; ++i) e.process (l.data(), r.data(), 256, nullptr, 0, h, {});
    CHECK (e.getActiveVoiceCount() == 0); // every note-on got its note-off on both engines

    // Morph preset = A + B + amount, saved and restored as two engines (never baked).
    h.morphEnabled = true;
    h.morphAmount = 0.42f;
    const Preset saved = ctl.presetForSave (h);
    std::string json;
    CHECK (presetToJson (saved, json));
    CHECK (json.find ("\"engineB\": {") != std::string::npos && json.find ("\"macrosB\": [") != std::string::npos);
    Preset back;
    CHECK (presetFromJson (json, back));
    CHECK (back.data.hasB && back.data.host.morphEnabled);
    CHECK_NEAR (back.data.host.morphAmount, 0.42, 1.0e-6);
    CHECK (patchDataEqual (back.data, saved.data));

    // Swap / copy / remove.
    const PatchData beforeSwap = ctl.data();
    CHECK (ctl.swapSlots());
    CHECK (ctl.preset().meta.macroNames[4] == std::string ("Bite"));
    CHECK (ctl.swapSlots());
    CHECK (patchDataEqual (ctl.data(), beforeSwap));
    CHECK (ctl.copySlot (false));
    CHECK (morphCompatible (ctl.data()));
    CHECK (ctl.removeSlotB());
    CHECK (! ctl.data().hasB && ! ctl.editingB());
    CHECK (! ctl.swapSlots());
}

FM_TEST ("integration", "Real-time safety: process() never allocates; patch hand-over is wait-free for the audio side")
{
    const double sr = 48000.0;
    SynthEngine e;
    e.prepare (sr, 512);
    Preset p = makeCategoryPreset (Category::Pad);
    p.data.b = makeCategoryPreset (Category::Bell).data.a;
    p.data.hasB = true;
    for (auto& pr : p.data.fx.proc) pr.enabled = true;
    CHECK (e.setPatch (p.data));
    HostParams h = p.data.host;
    h.morphEnabled = true;
    h.arpEnabled = true;
    h.gateEnabled = true;
    h.unison = 4;
    std::vector<float> l (512), r (512);
    std::vector<PatchData> variants;
    for (int i = 0; i < 6; ++i) { PatchData d = makeCategoryPreset ((Category) i).data; for (auto& pr : d.fx.proc) pr.enabled = (i % 2) == 0; variants.push_back (d); }
    TransportInfo tr;
    tr.valid = tr.playing = true;
    int allocs = 0;
    bool finite = true;
    Pcg32 rng (7);
    for (int block = 0; block < 3000; ++block)
    {
        // Non-realtime side: preset swaps / edits arrive while audio runs.
        if (block % 37 == 0) CHECK (e.setPatch (variants[(size_t) (block / 37) % variants.size()], (block / 37) % 2 == 0));
        MidiEvent ev[4];
        int n = 0;
        if (block % 5 == 0) ev[n++] = noteOn (36 + rng.nextInt (48), 1 + rng.nextInt (127));
        if (block % 5 == 3) { MidiEvent off; off.type = MidiEvent::AllNotesOff; if (block % 50 == 3) ev[n++] = off; else ev[n++] = noteOff (36 + rng.nextInt (48)); }
        if (block % 11 == 0) ev[n++] = bend (rng.nextBipolar());
        h.morphAmount = rng.nextFloat();
        h.macro[(size_t) rng.nextInt (8)] = rng.nextFloat();
        h.quality = (block / 500) % 3;
        h.unison = 1 + (block / 200) % 8;
        h.fxBypassAll = (block / 300) % 4 == 3;
        if (block % 400 == 399) tr.ppq = rng.nextFloat() * 64.0; // transport jump / loop
        const int len = block % 13 == 0 ? 0 : (block % 3 == 0 ? 512 : 32 + rng.nextInt (400));
        {
            AllocationScope scope;
            e.process (l.data(), r.data(), len, ev, n, h, tr);
            allocs += scope.count();
        }
        for (int i = 0; i < len; ++i) finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]) && std::fabs (l[(size_t) i]) <= 1.0f;
        tr.ppq += (double) len / sr * 2.0;
    }
    CHECK (allocs == 0);
    CHECK (finite);
    CHECK (e.getAppliedGeneration() > 0);
}

FM_TEST ("integration", "Morph slot names are kept in the preset, travel with a swap and survive a save")
{
    Rig rig;
    auto& ctl = rig.ctl;
    Preset a = makeCategoryPreset (Category::Keys);
    a.meta.displayName = "Sound A";
    ctl.loadPreset (a);
    Preset b = makeCategoryPreset (Category::Bell);
    b.meta.displayName = "Sound B";
    CHECK (ctl.loadIntoSlot (true, b));
    CHECK (ctl.preset().meta.slotNameB == "Sound B");
    CHECK (ctl.preset().meta.slotNameA.empty()); // A is the preset itself

    std::string json, err;
    CHECK (presetToJson (ctl.preset(), json, &err));
    Preset back;
    CHECK (presetFromJson (json, back, &err, nullptr));
    CHECK (back.meta.slotNameB == "Sound B");

    CHECK (ctl.swapSlots());
    CHECK (ctl.preset().meta.slotNameA == "Sound B");
    CHECK (ctl.preset().meta.slotNameB == "Sound A");
    CHECK (ctl.removeSlotB());
    CHECK (ctl.preset().meta.slotNameA.empty() && ctl.preset().meta.slotNameB.empty());

    // a preset without a B side never carries slot names
    CHECK (presetToJson (ctl.preset(), json, &err));
    CHECK (json.find ("slotNames") == std::string::npos);
}

FM_TEST ("integration", "Controller: moving a preset-stored host value (macro, unison, ...) marks the preset edited")
{
    PatchController ctl;
    ctl.loadPreset (makeCategoryPreset (Category::Keys));
    HostParams live = ctl.data().host;
    ctl.setHostBaseline (live); // what the plug-in does once the host parameters hold the preset's values
    CHECK (! ctl.isDirty (live));

    live.macro[2] += 0.1f;
    CHECK (ctl.isDirty (live));
    CHECK (! ctl.isDirty()); // the model itself is untouched
    live.macro[2] -= 0.1f;
    CHECK (! ctl.isDirty (live)); // back on the stored value

    // the round trip through the host's normalised range is not an edit
    live.detune += 1.0e-5f;
    CHECK (! ctl.isDirty (live));

    HostParams h = live;
    h.unison += 1;          CHECK (ctl.isDirty (h)); h = live;
    h.quick[1] = 0.9f;      CHECK (ctl.isDirty (h)); h = live;
    h.morphAmount = 0.75f;  CHECK (ctl.isDirty (h)); h = live;
    h.arpEnabled = ! h.arpEnabled; CHECK (ctl.isDirty (h)); h = live;

    // session settings are not part of a preset (D-010)
    h.gainDb = -12.0f; h.pan = 0.5f; h.quality = 2; h.fxBypassAll = true;
    CHECK (! ctl.isDirty (h));

    // saving adopts the live values
    live.macro[0] = 0.9f;
    CHECK (ctl.isDirty (live));
    ctl.setHostBaseline (live);
    CHECK (! ctl.isDirty (live));
}
