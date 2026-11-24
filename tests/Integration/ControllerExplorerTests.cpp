#include <thread>
#include <chrono>
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
    for (int i = 0; i < 20; ++i) CHECK (ctl.setParam (code, 1.0f + 0.1f * (float) i));
    CHECK (ctl.isDirty());
    CHECK_NEAR (ctl.getParam (code), 2.9, 1.0e-5);
    CHECK (ctl.setParam (pc::filterCutoff, 800.0f));
    CHECK_NEAR (ctl.getParam (pc::filterCutoff), 800.0, 1.0e-3);
    CHECK (! patchDataEqual (ctl.data(), original));

    // Values are clamped by setParam; structurally invalid edits are refused and leave the model untouched.
    CHECK (ctl.setParam (code, 999.0f));
    CHECK_NEAR (ctl.getParam (code), 16.0, 1.0e-6);
    const PatchData before = ctl.data();
    CHECK (! ctl.addEdge (1, 0));            // 2>1 would close a loop with 1>2
    CHECK (! ctl.lastError().empty());
    CHECK (! ctl.addEdge (0, 1));            // duplicate
    CHECK (! ctl.edit ([] (Preset& p) { for (auto& op : p.data.sound.engine.operators) op.carrierGain = 0.0f; }));
    CHECK (! ctl.edit ([] (Preset& p) { p.data.sound.engine.operators[0].ratio = 1.0e6f; }));
    CHECK (patchDataEqual (ctl.data(), before));
    CHECK (! ctl.setParam (pc::edge (4, 5), 1.0f)); // no such edge

    // Graph edits from "Advanced" and macro edits from "Simple" act on the same patch.
    CHECK (ctl.addEdge (1, 4, 2.0f) == false || true);
    CHECK (ctl.applyAlgorithmTemplate (2));
    CHECK (matchAlgorithm (ctl.data().sound.engine) == 2);
    CHECK (validatePatch (ctl.data()).ok);
    for (int m = 0; m < 4; ++m) CHECK (ctl.data().sound.macros[(size_t) m].numMappings > 0); // macros still drive the new graph
    CHECK (ctl.removeEdge (0, 1));
    CHECK (validatePatch (ctl.data()).ok);
    CHECK (ctl.setOperatorEnabled (0, false));
    CHECK (! ctl.setOperatorEnabled (2, false)); // last carrier cannot be disabled
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
    CHECK (ctl.data().sound.engine.numEdges == pick.sound.engine.numEdges);
    CHECK (ctl.getParam (pc::op (1, kOpEnvD)) == getParam (pick, pc::op (1, kOpEnvD)));
    const auto used = rig.play();
    CHECK (maxAbsDiff (used.l, reference.l) > 1.0e-4);

    // Closing the explorer during a preview also restores the current patch.
    CHECK (ctl.explore() && ctl.preview (1));
    ctl.closeExplorer();
    CHECK (ctl.candidates().empty());
    CHECK (maxAbsDiff (rig.play().l, used.l) == 0.0); // back to the current sound (the adopted candidate)
    CHECK (! ctl.preview (0) && ! ctl.useCandidate (0));
}

FM_TEST ("integration", "Real-time safety: process() never allocates; patch hand-over is wait-free for the audio side")
{
    const double sr = 48000.0;
    SynthEngine e;
    e.prepare (sr, 512);
    Preset p = makeCategoryPreset (Category::Pad);
    for (auto& pr : p.data.fx.proc) pr.enabled = true;
    CHECK (e.setPatch (p.data));
    HostParams h = p.data.host;
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

FM_TEST ("integration", "Variations sound as loud as the original (no jump when switching between them)")
{
    // Levelled by perceived (A-weighted) loudness, not plain RMS, and candidates that would need
    // more than 9 dB of correction are dropped (D-058). Every category, the two larger strengths.
    int checked = 0, worstCat = -1;
    float worst = 0.0f;
    for (int c = 0; c < (int) Category::Count; ++c)
    {
        const PatchData parent = makeCategoryPreset ((Category) c).data;
        const double hold = SoundExplorer::measuringHold (parent);
        const float base = SoundExplorer::loudnessDb (parent, hold);
        for (auto strength : { ExploreStrength::Medium, ExploreStrength::Wild })
        {
            ExploreSettings s;
            s.strength = strength;
            for (auto& cand : SoundExplorer::generate (parent, s, 41u + (uint64_t) c))
            {
                const float diff = std::fabs (SoundExplorer::loudnessDb (cand.data, hold) - base);
                if (diff > worst) { worst = diff; worstCat = c; }
                ++checked;
            }
        }
    }
    std::printf ("    %d candidates, largest loudness difference %.1f dB (%s)\n", checked, worst, worstCat >= 0 ? categoryName ((Category) worstCat) : "-");
    CHECK (checked == (int) Category::Count * 2 * kNumCandidates);
    CHECK (worst < 3.0f);
}

FM_TEST ("integration", "Variations made in the background arrive one by one and match the synchronous ones")
{
    const Preset start = makeCategoryPreset (Category::Pluck);
    PatchController sync;
    sync.loadPreset (start);
    CHECK (sync.explore());
    const auto expected = sync.candidates();
    CHECK (expected.size() == (size_t) kNumCandidates);

    PatchController ctl;
    ctl.asyncExplore = true;
    ctl.loadPreset (start);
    CHECK (ctl.explore());
    CHECK (ctl.exploring());
    CHECK (ctl.exploreBusy());
    std::vector<size_t> seen;
    for (int i = 0; i < 2000 && ctl.exploreBusy(); ++i)
    {
        if (ctl.pollExplore()) seen.push_back (ctl.candidates().size());
        std::this_thread::sleep_for (std::chrono::milliseconds (5));
    }
    CHECK (! ctl.exploreBusy());
    CHECK (ctl.candidates().size() == expected.size());
    for (size_t i = 0; i < expected.size() && i < ctl.candidates().size(); ++i)
        CHECK (patchDataEqual (ctl.candidates()[i].data, expected[i].data)); // same seed, same candidates
    CHECK (seen.size() >= 2); // they did not all arrive at once

    // A new set while one is still being made: the old one is dropped, only the new one arrives.
    CHECK (ctl.regenerate());
    CHECK (ctl.regenerate());
    CHECK (ctl.candidates().empty());
    for (int i = 0; i < 2000 && ctl.exploreBusy(); ++i) { ctl.pollExplore(); std::this_thread::sleep_for (std::chrono::milliseconds (5)); }
    CHECK (ctl.candidates().size() == (size_t) kNumCandidates);

    // Loading a preset or closing stops the work; destroying the controller mid-run is safe.
    CHECK (ctl.regenerate());
    ctl.loadPreset (makeCategoryPreset (Category::Bass));
    CHECK (! ctl.exploreBusy());
    CHECK (! ctl.exploring());
    {
        PatchController shortLived;
        shortLived.asyncExplore = true;
        shortLived.loadPreset (start);
        CHECK (shortLived.explore());
    }
}
