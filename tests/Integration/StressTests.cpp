#include "TestFramework.h"
#include "TestUtil.h"
#include "Core/PatchController.h"
#include "Core/Templates.h"
#include "Presets/PresetManager.h"

using namespace fmtest;

// SPEC §17.3: five minutes of automatic MIDI stress with extreme automation, preset swaps and
// transport jumps. Checks: no allocation in process(), finite bounded output, no hanging voices.
FM_TEST ("stress", "Five-minute stress: random MIDI, automation extremes, preset swaps, transport jumps")
{
    const double sr = 48000.0;
    PresetManager pm;
    pm.loadFactoryDirectory (std::string (FMLUME_SOURCE_DIR) + "/Resources/Presets");
    CHECK (pm.entries().size() >= 108);

    SynthEngine e;
    e.prepare (sr, 512);
    PatchController ctl;
    ctl.sendPatch = [&] (const PatchData& d, bool f) { return e.setPatch (d, f); };
    HostParams h;
    ctl.applyHostValues = [&] (const HostParams& v) { const HostParams keep = h; h = v; h.gainDb = keep.gainDb; h.quality = keep.quality; };

    std::vector<float> l (512), r (512);
    Pcg32 rng (2026);
    TransportInfo tr;
    tr.valid = true;
    tr.bpm = 128.0;
    const int64_t totalSamples = (int64_t) (300.0 * sr);
    int64_t done = 0;
    int allocs = 0, block = 0, presetLoads = 0, maxSubs = 0;
    bool finite = true, bounded = true;
    bool held[128] {};

    while (done < totalSamples)
    {
        // --- non-realtime side (never inside the allocation scope)
        if (block % 211 == 0)
        {
            Preset p;
            if (pm.load (rng.nextInt ((int) pm.entries().size()), p)) { ctl.loadPreset (p); ++presetLoads; }
            if (rng.nextFloat() < 0.3f) { Preset b; if (pm.load (rng.nextInt ((int) pm.entries().size()), b)) ctl.loadIntoSlot (true, b); }
        }
        if (block % 53 == 0) ctl.setParam (false, pc::filterCutoff, 20.0f + rng.nextFloat() * 19980.0f);
        if (block % 97 == 0) { ctl.exploreSettings.strength = ExploreStrength::Wild; }
        if (block % 1499 == 0) { ctl.explore(); ctl.preview (rng.nextInt (kNumCandidates)); }
        if (block % 1499 == 700) { if (rng.nextFloat() < 0.5f) ctl.useCandidate (rng.nextInt (kNumCandidates)); else ctl.cancelPreview(); }

        // --- host automation at the extremes
        for (auto& m : h.macro) if (rng.nextFloat() < 0.2f) m = rng.nextFloat() < 0.5f ? (rng.nextFloat() < 0.5f ? 0.0f : 1.0f) : rng.nextFloat();
        for (auto& q : h.quick) if (rng.nextFloat() < 0.1f) q = rng.nextFloat();
        if (rng.nextFloat() < 0.1f) h.morphAmount = rng.nextFloat() < 0.5f ? (float) rng.nextInt (2) : rng.nextFloat();
        if (block % 400 == 0) h.morphEnabled = rng.nextFloat() < 0.5f;
        if (block % 500 == 0) h.unison = 1 + rng.nextInt (8);
        if (block % 600 == 0) h.polyphony = 1 + rng.nextInt (32);
        if (block % 900 == 0) h.quality = rng.nextInt (3);
        if (block % 700 == 0) h.arpEnabled = rng.nextFloat() < 0.4f;
        if (block % 800 == 0) h.gateEnabled = rng.nextFloat() < 0.4f;
        if (block % 1000 == 0) h.fxBypassAll = rng.nextFloat() < 0.2f;
        h.detune = rng.nextFloat() * 50.0f;
        h.spread = rng.nextFloat() * 100.0f;
        h.gainDb = -12.0f + rng.nextFloat() * 12.0f;
        h.pan = rng.nextBipolar();

        // --- transport: play / stop / jump / loop / tempo changes
        if (block % 450 == 0) tr.playing = rng.nextFloat() < 0.8f;
        if (block % 777 == 0) tr.ppq = rng.nextFloat() * 400.0;
        if (block % 1300 == 0) tr.bpm = 40.0 + rng.nextFloat() * 220.0;
        if (block % 120 == 119 && tr.playing) tr.ppq = std::floor (tr.ppq / 4.0) * 4.0; // loop back to the bar

        // --- MIDI
        MidiEvent ev[24];
        int n = 0;
        const int len = block % 17 == 0 ? 0 : (block % 5 == 0 ? 32 + rng.nextInt (480) : 512);
        const int burst = rng.nextFloat() < 0.05f ? 12 : (rng.nextFloat() < 0.35f ? 1 + rng.nextInt (3) : 0);
        for (int i = 0; i < burst; ++i)
        {
            const int note = 24 + rng.nextInt (84);
            MidiEvent m = held[note] ? noteOff (note) : noteOn (note, 1 + rng.nextInt (127));
            held[note] = ! held[note];
            m.offset = len > 0 ? rng.nextInt (len) : 0;
            ev[n++] = m;
        }
        if (rng.nextFloat() < 0.05f) { ev[n] = bend (rng.nextBipolar()); ev[n++].offset = 0; }
        if (rng.nextFloat() < 0.05f) { ev[n] = cc (rng.nextFloat() < 0.5f ? 1 : 64, rng.nextInt (128)); ev[n++].offset = 0; }
        if (rng.nextFloat() < 0.01f) { MidiEvent m; m.type = rng.nextFloat() < 0.5f ? MidiEvent::AllNotesOff : MidiEvent::AllSoundOff; ev[n++] = m; std::fill (std::begin (held), std::end (held), false); }
        std::stable_sort (ev, ev + n, [] (const MidiEvent& a, const MidiEvent& b) { return a.offset < b.offset; });

        {
            AllocationScope scope;
            e.process (l.data(), r.data(), len, ev, n, h, tr);
            allocs += scope.count();
        }
        for (int i = 0; i < len; ++i)
        {
            finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
            bounded = bounded && std::fabs (l[(size_t) i]) <= 1.0f && std::fabs (r[(size_t) i]) <= 1.0f;
        }
        maxSubs = std::max (maxSubs, e.getActiveSubVoiceCount());
        if (tr.playing) tr.ppq += (double) len / sr * tr.bpm / 60.0;
        done += len;
        ++block;
    }
    CHECK (allocs == 0);
    CHECK (finite);
    CHECK (bounded);
    CHECK (presetLoads > 50);
    CHECK (maxSubs <= 2 * kMaxSubVoices + 64); // Dual budget (128 units) plus briefly overlapping steal fades

    // Everything released -> the engine falls silent and no voice is left behind.
    h.arpEnabled = false;
    h.gateEnabled = false;
    MidiEvent off;
    off.type = MidiEvent::AllNotesOff;
    MidiEvent pedalUp = cc (64, 0);
    MidiEvent both[2] = { pedalUp, off };
    e.process (l.data(), r.data(), 512, both, 2, h, tr);
    for (int i = 0; i < (int) (45.0 * sr / 512); ++i) e.process (l.data(), r.data(), 512, nullptr, 0, h, tr);
    CHECK (e.getActiveVoiceCount() == 0);
    std::printf ("    %d blocks, %d preset loads, max %d sub-voices\n", block, presetLoads, maxSubs);
}
