#include "TestFramework.h"
#include "TestUtil.h"
#include "Core/Templates.h"

using namespace fmtest;

namespace
{
PatchData shortBlip()
{
    PatchData d = makeInitPatchData();
    d.sound.engine.operators[0] = makeOp (1.0f, 1.0f, 0.001f, 0.5f, 1.0f, 0.02f, 1.0f);
    d.sound.engine.operators[0].velocityToLevel = 0.0f;
    return d;
}
void enable (PatchData& d, FxType t) { d.fx.proc[(size_t) t].enabled = true; }
float& fxp (PatchData& d, FxType t, int p) { return d.fx.proc[(size_t) t].p[(size_t) p]; }
float maxStepIn (const std::vector<float>& v, size_t from, size_t to)
{
    float m = 0.0f;
    for (size_t i = std::max<size_t> (from, 1); i < std::min (to, v.size()); ++i) m = std::max (m, std::fabs (v[i] - v[i - 1]));
    return m;
}
MidiEvent at (MidiEvent e, int offset) { e.offset = offset; return e; }
} // namespace

FM_TEST ("dsp", "FX-01 reverb and delay tails outlive the note; bypassed effects do no work")
{
    const double sr = 48000.0;
    std::vector<NoteEvent> seq { { 0.0, noteOn (69, 127) }, { 0.2, noteOff (69) } };
    SynthEngine dry;
    dry.prepare (sr, 256);
    CHECK (dry.setPatch (shortBlip()));
    auto rd = renderSeq (dry, sr, 256, 3.0, seq);
    CHECK (peak (rd.l, (size_t) (0.5 * sr)) < 1.0e-4f);

    PatchData rev = shortBlip();
    enable (rev, FxType::Reverb);
    fxp (rev, FxType::Reverb, 1) = 3.0f;
    fxp (rev, FxType::Reverb, 4) = 0.5f;
    SynthEngine e1;
    e1.prepare (sr, 256);
    CHECK (e1.setPatch (rev));
    auto rr = renderSeq (e1, sr, 256, 3.0, seq);
    CHECK (allFinite (rr.l));
    const double early = rms (rr.l, (size_t) (0.5 * sr), (size_t) (0.7 * sr)), late = rms (rr.l, (size_t) (2.0 * sr), (size_t) (2.2 * sr));
    CHECK (early > 1.0e-3);
    CHECK (late > 1.0e-5 && late < early * 0.5); // decaying naturally, still audible
    CHECK (maxAbsDiff (rr.l, rr.r) > 1.0e-3);   // decorrelated stereo

    PatchData del = shortBlip();
    enable (del, FxType::Delay);
    fxp (del, FxType::Delay, 0) = 250.0f;
    fxp (del, FxType::Delay, 3) = 0.5f;
    fxp (del, FxType::Delay, 4) = 0.0f;
    fxp (del, FxType::Delay, 5) = 0.6f;
    for (double rate : { 44100.0, 96000.0 })
    {
        SynthEngine e2;
        e2.prepare (rate, 256);
        CHECK (e2.setPatch (del));
        auto r2 = renderSeq (e2, rate, 256, 2.0, { { 0.0, noteOn (69, 127) }, { 0.05, noteOff (69) } });
        // Echoes at 250 ms multiples, each quieter (delay time is sample-rate independent).
        const float e0 = peak (r2.l, 0, (size_t) (0.1 * rate));
        const float e1p = peak (r2.l, (size_t) (0.25 * rate), (size_t) (0.35 * rate));
        const float gap = peak (r2.l, (size_t) (0.15 * rate), (size_t) (0.24 * rate));
        const float e2p = peak (r2.l, (size_t) (0.50 * rate), (size_t) (0.60 * rate));
        CHECK_NEAR (e1p / e0, 0.6, 0.08);
        CHECK (gap < 0.02f * e0);
        CHECK (e2p < e1p && e2p > 0.2f * e1p);
    }

    // Bypass: processors that are disabled or globally bypassed never run.
    FxChain chain;
    chain.prepare (sr);
    FxState st = rev.fx;
    float eff[kNumFx][kFxParams];
    for (int t = 0; t < kNumFx; ++t) for (int p = 0; p < kFxParams; ++p) eff[t][p] = st.proc[(size_t) t].p[(size_t) p];
    std::vector<float> l (512, 0.1f), r (512, 0.1f);
    chain.setParams (st, eff, true, 120.0);
    for (int i = 0; i < 20; ++i) chain.process (l.data(), r.data(), 512);
    for (int t = 0; t < kNumFx; ++t) CHECK (chain.getProcessCount ((FxType) t) == 0);
    chain.setParams (st, eff, false, 120.0);
    for (int i = 0; i < 20; ++i) chain.process (l.data(), r.data(), 512);
    CHECK (chain.getProcessCount (FxType::Reverb) > 0);
    CHECK (chain.getProcessCount (FxType::Delay) == 0 && chain.getProcessCount (FxType::Chorus) == 0);
}

FM_TEST ("dsp", "Every effect processes audio safely and changes the sound")
{
    const double sr = 48000.0;
    PatchData base = makeCategoryPreset (Category::Keys).data;
    for (auto& pr : base.fx.proc) pr.enabled = false;
    std::vector<NoteEvent> seq { { 0.0, noteOn (48, 120) }, { 0.0, noteOn (60, 120) }, { 0.6, noteOff (48) }, { 0.6, noteOff (60) } };
    SynthEngine e0;
    e0.prepare (sr, 256);
    CHECK (e0.setPatch (base));
    auto dry = renderSeq (e0, sr, 256, 1.5, seq);
    for (int t = 0; t < kNumFx; ++t)
    {
        PatchData d = base;
        enable (d, (FxType) t);
        for (int p = 0; p < fxNumParams (t); ++p)
            if (fxParamDesc (t, p).flags & kFlagWet) fxp (d, (FxType) t, p) = 0.6f;
        if (t == (int) FxType::EQ) { fxp (d, FxType::EQ, 0) = 12.0f; fxp (d, FxType::EQ, 2) = -9.0f; fxp (d, FxType::EQ, 5) = 6.0f; }
        if (t == (int) FxType::Drive) fxp (d, FxType::Drive, 0) = 0.8f;
        if (t == (int) FxType::Bitcrusher) { fxp (d, FxType::Bitcrusher, 0) = 5.0f; fxp (d, FxType::Bitcrusher, 1) = 8.0f; }
        if (t == (int) FxType::Compressor) { fxp (d, FxType::Compressor, 0) = -40.0f; fxp (d, FxType::Compressor, 1) = 10.0f; }
        // Push every remaining parameter to both extremes as well.
        for (int variant = 0; variant < 3; ++variant)
        {
            PatchData v = d;
            if (variant > 0)
                for (int p = 0; p < fxNumParams (t); ++p)
                    fxp (v, (FxType) t, p) = variant == 1 ? fxParamDesc (t, p).min : fxParamDesc (t, p).max;
            CHECK (validatePatch (v).ok);
            SynthEngine e;
            e.prepare (sr, 256);
            CHECK (e.setPatch (v));
            auto r = renderSeq (e, sr, 256, 1.5, seq);
            CHECK (allFinite (r.l) && allFinite (r.r));
            CHECK (peak (r.l) <= 1.0f && peak (r.r) <= 1.0f);
            if (variant == 0)
            {
                const double diff = std::max (maxAbsDiff (dry.l, r.l), maxAbsDiff (dry.r, r.r));
                CHECK (diff > 1.0e-3);
                if (diff <= 1.0e-3) std::printf ("    effect %s had no effect\n", fxTypeName ((FxType) t));
            }
        }
    }
}

FM_TEST ("dsp", "FX-02 reordering, toggling and bypassing the chain is click free; latency is constant")
{
    const double sr = 48000.0;
    PatchData d = shortBlip();
    d.sound.engine.operators[0].env.r = 0.3f;
    enable (d, FxType::Drive);
    enable (d, FxType::Chorus);
    enable (d, FxType::Reverb);
    fxp (d, FxType::Drive, 0) = 0.5f;
    SynthEngine e;
    e.prepare (sr, 128);
    CHECK (e.setPatch (d));
    HostParams h;
    std::vector<float> l (96000), r (96000);
    MidiEvent on = noteOn (57, 110);
    int step = 0;
    for (int pos = 0; pos + 128 <= 96000; pos += 128)
    {
        if (pos > 0 && pos % 9600 == 0)
        {
            PatchData x = d;
            switch (step++ % 4)
            {
                case 0: std::swap (x.fx.order[1], x.fx.order[8]); break;       // Drive <-> Reverb
                case 1: x.fx.proc[(size_t) FxType::Chorus].enabled = false; break;
                case 2: h.fxBypassAll = true; break;
                default: h.fxBypassAll = false; x.fx.order = { 8, 7, 6, 5, 4, 3, 2, 1, 0 }; break;
            }
            CHECK (e.setPatch (x, false));
        }
        e.process (l.data() + pos, r.data() + pos, 128, pos == 0 ? &on : nullptr, pos == 0 ? 1 : 0, h, {});
    }
    CHECK (allFinite (l) && allFinite (r));
    // 220 Hz at <= 1.0 amplitude: natural slope <= 0.029/sample. A hard chain switch would exceed this by far.
    CHECK (maxStepIn (l, 2000, 96000) < 0.09f);
    CHECK (maxStepIn (r, 2000, 96000) < 0.09f);

    // Same onset position whether Drive is enabled or not -> the reported latency is always valid.
    auto onset = [&] (bool driveOn, bool bypassAll) {
        PatchData p = shortBlip();
        p.fx.proc[(size_t) FxType::Drive].enabled = driveOn;
        SynthEngine eng;
        eng.prepare (sr, 256);
        CHECK (eng.setPatch (p));
        HostParams hp;
        hp.quality = 0;
        hp.fxBypassAll = bypassAll;
        auto out = renderSeq (eng, sr, 256, 0.2, { { 0.0, noteOn (81, 127) } }, hp);
        size_t first = 0;
        while (first < out.l.size() && std::fabs (out.l[first]) < 1.0e-6f) ++first;
        return (int) first;
    };
    const int off = onset (false, false);
    CHECK (off == SynthEngine::latencyForQuality (0) || off == SynthEngine::latencyForQuality (0) + 1);
    CHECK (std::abs (onset (true, false) - off) <= 6);  // FIR pre-ringing may start a few samples early
    CHECK (onset (true, false) <= off);
    CHECK (onset (false, true) == off);
}

FM_TEST ("dsp", "Chord mode expands notes, releases them all and keeps shared pitches alive")
{
    Performer p;
    p.prepare (48000.0);
    PerformanceState perf;
    perf.chord.type = ChordType::Major;
    Performer::Clock clock { 0.0, 120.0 / 60.0 / 48000.0, false, false, false };
    MidiEvent out[64];

    MidiEvent in1[] = { at (noteOn (60, 90), 10) };
    int n = p.process (in1, 1, 256, perf, false, clock, out, 64);
    CHECK (n == 3);
    CHECK (out[0].data1 == 60 && out[1].data1 == 64 && out[2].data1 == 67);
    for (int i = 0; i < n; ++i) CHECK (out[i].type == MidiEvent::NoteOn && out[i].offset == 10 && out[i].data2 == 90);

    // G major shares G (67) with C major.
    MidiEvent in2[] = { at (noteOn (67, 80), 0) };
    n = p.process (in2, 1, 256, perf, false, clock, out, 64);
    CHECK (n == 3);
    // Releasing C must not cut the G that the second key still holds.
    MidiEvent in3[] = { at (noteOff (60), 5) };
    n = p.process (in3, 1, 256, perf, false, clock, out, 64);
    CHECK (n == 2);
    for (int i = 0; i < n; ++i) CHECK (out[i].type == MidiEvent::NoteOff && (out[i].data1 == 60 || out[i].data1 == 64));
    MidiEvent in4[] = { at (noteOff (67), 5) };
    n = p.process (in4, 1, 256, perf, false, clock, out, 64);
    CHECK (n == 3);

    // Voicing, octave and strum.
    perf.chord.type = ChordType::Min7;
    perf.chord.voicing = Voicing::First;
    perf.chord.octave = 1;
    perf.chord.strumMs = 10.0f;
    MidiEvent in5[] = { at (noteOn (48, 100), 0) };
    n = p.process (in5, 1, 4096, perf, false, clock, out, 64);
    CHECK (n == 4);
    const int expect[4] = { 48 + 12 + 3, 48 + 12 + 7, 48 + 12 + 10, 48 + 12 + 12 };
    for (int i = 0; i < 4; ++i) { CHECK (out[i].data1 == expect[i]); CHECK (out[i].offset == i * 480); }
    // A note-off before a strummed note has sounded cancels it instead of leaving it hanging.
    perf.chord.strumMs = 100.0f;
    MidiEvent in6[] = { at (noteOff (48), 0), at (noteOn (50, 100), 0), at (noteOff (50), 100) };
    n = p.process (in6, 3, 256, perf, false, clock, out, 64);
    int ons = 0, offs = 0;
    for (int i = 0; i < n; ++i) { ons += out[i].type == MidiEvent::NoteOn; offs += out[i].type == MidiEvent::NoteOff; }
    // 4 offs for the first chord, 1 on + 1 off for the only sounded note of the second, and one
    // (redundant, harmless) off for each of the 3 cancelled notes: a cancelled pitch may already
    // be sounding from another key, so its off is always sent.
    CHECK (ons == 1 && offs == 8);
    n = p.process (nullptr, 0, 48000, perf, false, clock, out, 64);
    CHECK (n == 0);
    CHECK (p.getNumScheduled() == 0);

    // All notes off clears the bookkeeping.
    MidiEvent in7[] = { at (noteOn (60, 100), 0) };
    p.process (in7, 1, 256, perf, false, clock, out, 64);
    MidiEvent ano;
    ano.type = MidiEvent::AllNotesOff;
    n = p.process (&ano, 1, 256, perf, false, clock, out, 64);
    CHECK (out[n - 1].type == MidiEvent::AllNotesOff);
    CHECK (p.getNumScheduled() == 0);
}

FM_TEST ("dsp", "PERF-01 arpeggiator: tempo-locked steps, gate, octaves, modes, transport jumps, no stuck notes")
{
    const double sr = 48000.0, bpm = 120.0;
    const double bps = bpm / 60.0 / sr;
    PerformanceState perf;
    perf.arp.rate = 12; // 1/16 -> 6000 samples at 120 bpm
    perf.arp.gate = 0.5f;
    perf.arp.octaves = 2;
    MidiEvent out[256];

    auto collect = [&] (Performer& p, double& ppq, int blocks, int block, std::vector<std::pair<int64_t, MidiEvent>>& all, int64_t& t,
                        const MidiEvent* in = nullptr, int numIn = 0, bool playing = true, bool jumped = false, bool stopped = false) {
        for (int b = 0; b < blocks; ++b)
        {
            Performer::Clock clock { ppq, bps, playing, jumped && b == 0, stopped && b == 0 };
            const int n = p.process (b == 0 ? in : nullptr, b == 0 ? numIn : 0, block, perf, true, clock, out, 256);
            for (int i = 0; i < n; ++i) all.push_back ({ t + out[i].offset, out[i] });
            t += block;
            ppq += block * bps;
        }
    };

    Performer p;
    p.prepare (sr);
    std::vector<std::pair<int64_t, MidiEvent>> ev;
    double ppq = 0.0;
    int64_t t = 0;
    MidiEvent keys[] = { at (noteOn (64, 100), 0), at (noteOn (60, 90), 0), at (noteOn (67, 80), 0) };
    collect (p, ppq, 200, 480, ev, t, keys, 3, true, true); // 2 seconds
    std::vector<int> notes;
    std::vector<int64_t> onTimes;
    int sounding = 0, maxSounding = 0;
    for (auto& e : ev)
    {
        if (e.second.type == MidiEvent::NoteOn) { notes.push_back (e.second.data1); onTimes.push_back (e.first); ++sounding; }
        if (e.second.type == MidiEvent::NoteOff) --sounding;
        maxSounding = std::max (maxSounding, sounding);
    }
    // The step on the final block boundary may land one sample early (ppq accumulation rounding): ignore it.
    if (notes.size() == 17 && onTimes.back() >= 95999) { notes.pop_back(); onTimes.pop_back(); ev.pop_back(); --sounding; }
    CHECK (notes.size() == 16);
    const int upPattern[6] = { 60, 64, 67, 72, 76, 79 };
    for (size_t i = 0; i < notes.size(); ++i)
    {
        CHECK (notes[i] == upPattern[i % 6]);
        CHECK (std::llabs (onTimes[i] - (int64_t) i * 6000) <= 1);
    }
    CHECK (maxSounding == 1); // gate 50 %: never overlapping
    for (size_t i = 0; i + 1 < ev.size(); i += 2)
        CHECK (ev[i + 1].second.type == MidiEvent::NoteOff && ev[i + 1].first - ev[i].first == 3000);

    // Transport jump (loop back): pattern restarts on the grid, hanging step is released first.
    ev.clear();
    ppq = 4.0 + 0.1; // not on a step boundary
    collect (p, ppq, 30, 480, ev, t, nullptr, 0, true, true);
    CHECK (ev.size() >= 2);
    int first = -1;
    for (auto& e : ev) if (e.second.type == MidiEvent::NoteOn) { first = e.second.data1; break; }
    CHECK (first == 60);
    // Release keys: everything ends, nothing stays scheduled.
    MidiEvent offs[] = { at (noteOff (60), 0), at (noteOff (64), 0), at (noteOff (67), 0) };
    ev.clear();
    collect (p, ppq, 40, 480, ev, t, offs, 3);
    int bal = 0;
    for (auto& e : ev) bal += e.second.type == MidiEvent::NoteOn ? 1 : (e.second.type == MidiEvent::NoteOff ? -1 : 0);
    CHECK (bal <= 0);
    CHECK (p.getNumScheduled() == 0 && p.getNumHeld() == 0);

    // Modes: Down and UpDown orderings; swing delays odd steps.
    for (ArpMode mode : { ArpMode::Down, ArpMode::UpDown, ArpMode::AsPlayed, ArpMode::Random })
    {
        Performer q;
        q.prepare (sr);
        perf.arp.mode = mode;
        perf.arp.octaves = 1;
        perf.arp.swing = mode == ArpMode::Down ? 0.5f : 0.0f;
        std::vector<std::pair<int64_t, MidiEvent>> e2;
        double pq = 0.0;
        int64_t tt = 0;
        collect (q, pq, 100, 480, e2, tt, keys, 3, true, true);
        std::vector<int> ns;
        std::vector<int64_t> ts;
        for (auto& e : e2) if (e.second.type == MidiEvent::NoteOn) { ns.push_back (e.second.data1); ts.push_back (e.first); }
        CHECK (ns.size() >= 7);
        if (mode == ArpMode::Down) { CHECK (ns[0] == 67 && ns[1] == 64 && ns[2] == 60 && ns[3] == 67); CHECK (std::llabs (ts[1] - (6000 + 1500)) <= 1); CHECK (std::llabs (ts[2] - 12000) <= 1); }
        if (mode == ArpMode::UpDown) CHECK (ns[0] == 60 && ns[1] == 64 && ns[2] == 67 && ns[3] == 64 && ns[4] == 60 && ns[5] == 64);
        if (mode == ArpMode::AsPlayed) CHECK (ns[0] == 64 && ns[1] == 60 && ns[2] == 67 && ns[3] == 64);
        if (mode == ArpMode::Random) for (int nn : ns) CHECK (nn == 60 || nn == 64 || nn == 67);
    }
    perf.arp = ArpState {};

    // Through the engine: free-running (no host transport) arp starts immediately and never hangs.
    PatchData d = shortBlip();
    d.perf.arp.rate = 12;
    d.perf.arp.gate = 0.4f;
    SynthEngine eng;
    eng.prepare (sr, 256);
    CHECK (eng.setPatch (d));
    HostParams h;
    h.arpEnabled = true;
    auto r = renderSeq (eng, sr, 256, 3.0, { { 0.1, noteOn (60) }, { 0.1, noteOn (72) }, { 1.6, noteOff (60) }, { 1.6, noteOff (72) } }, h);
    CHECK (peak (r.l, (size_t) (0.1 * sr), (size_t) (0.15 * sr)) > 0.1f);  // immediate first step
    // Rhythmic gaps between steps (gate 40 %, 20 ms release): quiet just before the next step.
    CHECK (peak (r.l, (size_t) ((0.1 + 0.118) * sr), (size_t) ((0.1 + 0.124) * sr)) < 0.02f);
    CHECK (peak (r.l, (size_t) ((0.1 + 0.125) * sr), (size_t) ((0.1 + 0.17) * sr)) > 0.1f);
    CHECK (peak (r.l, (size_t) (2.2 * sr)) < 1.0e-4f);
    CHECK (eng.getActiveVoiceCount() == 0);

    // Toggling the arp while keys are held releases everything.
    SynthEngine eng2;
    eng2.prepare (sr, 256);
    CHECK (eng2.setPatch (d));
    std::vector<float> l (256), rr (256);
    MidiEvent on = noteOn (60);
    eng2.process (l.data(), rr.data(), 256, &on, 1, h, {});
    for (int i = 0; i < 100; ++i) eng2.process (l.data(), rr.data(), 256, nullptr, 0, h, {});
    h.arpEnabled = false;
    for (int i = 0; i < 400; ++i) eng2.process (l.data(), rr.data(), 256, nullptr, 0, h, {});
    CHECK (eng2.getActiveVoiceCount() == 0);
}

FM_TEST ("dsp", "Trance gate follows the host position and leaves FX tails alone")
{
    const double sr = 48000.0;
    PatchData d = shortBlip();
    d.sound.engine.operators[0].env.r = 0.05f;
    for (int i = 0; i < kGateSteps; ++i) d.perf.gate.steps[(size_t) i] = i % 2 == 0 ? 1.0f : 0.0f;
    d.perf.gate.rate = 9;      // 1/8 -> 0.25 s at 120 bpm
    d.perf.gate.smoothMs = 2.0f;
    CHECK (TranceGate::stepAt (0.0, 0.5, 0.0f) == 0);
    CHECK (TranceGate::stepAt (0.5, 0.5, 0.0f) == 1);
    CHECK (TranceGate::stepAt (0.5, 0.5, 0.5f) == 0); // swing delays the odd step
    CHECK (TranceGate::stepAt (0.7, 0.5, 0.5f) == 1);
    CHECK (TranceGate::stepAt (8.0, 0.5, 0.0f) == 0); // wraps after 16 steps

    HostParams h;
    h.gateEnabled = true;
    TransportInfo tr;
    tr.valid = tr.playing = true;
    tr.bpm = 120.0;
    SynthEngine e;
    e.prepare (sr, 256);
    CHECK (e.setPatch (d));
    auto r = renderSeq (e, sr, 256, 1.2, { { 0.0, noteOn (69, 127) } }, h, tr);
    CHECK (peak (r.l, (size_t) (0.05 * sr), (size_t) (0.2 * sr)) > 0.1f);
    CHECK (peak (r.l, (size_t) (0.30 * sr), (size_t) (0.48 * sr)) < 0.005f);
    CHECK (peak (r.l, (size_t) (0.55 * sr), (size_t) (0.7 * sr)) > 0.1f);
    float step = 0.0f;
    for (size_t i = 1; i < r.l.size(); ++i) step = std::max (step, std::fabs (r.l[i] - r.l[i - 1]));
    CHECK (step < 0.05f);

    // Starting from a different song position selects a different step.
    SynthEngine e2;
    e2.prepare (sr, 256);
    CHECK (e2.setPatch (d));
    tr.ppq = 0.5;
    auto r2 = renderSeq (e2, sr, 256, 0.5, { { 0.0, noteOn (69, 127) } }, h, tr);
    CHECK (peak (r2.l, (size_t) (0.06 * sr), (size_t) (0.2 * sr)) < 0.005f);
    CHECK (peak (r2.l, (size_t) (0.30 * sr), (size_t) (0.45 * sr)) > 0.1f);

    // Gate closed + reverb: the tail keeps ringing because the gate sits before the FX chain.
    PatchData rev = d;
    rev.fx.proc[(size_t) FxType::Reverb].enabled = true;
    rev.fx.proc[(size_t) FxType::Reverb].p[4] = 0.6f;
    SynthEngine e3;
    e3.prepare (sr, 256);
    CHECK (e3.setPatch (rev));
    tr.ppq = 0.0;
    auto r3 = renderSeq (e3, sr, 256, 0.5, { { 0.0, noteOn (69, 127) } }, h, tr);
    CHECK (rms (r3.l, (size_t) (0.32 * sr), (size_t) (0.48 * sr)) > 2.0e-3);
}

FM_TEST ("dsp", "Stereo trance gate drives the two channels from separate patterns")
{
    const double sr = 48000.0;
    PatchData d = shortBlip();
    d.sound.engine.operators[0].env.s = 1.0f;
    d.perf.gate.rate = 9; // 1/8 -> 0.25 s per step at 120 bpm
    d.perf.gate.smoothMs = 2.0f;
    d.perf.gate.stereo = true;
    for (int i = 0; i < kGateSteps; ++i)
    {
        d.perf.gate.steps[(size_t) i] = i % 2 == 0 ? 1.0f : 0.0f;      // left: even steps
        d.perf.gate.stepsRight[(size_t) i] = i % 2 == 0 ? 0.0f : 1.0f; // right: odd steps
    }
    HostParams h;
    h.gateEnabled = true;
    TransportInfo tr;
    tr.valid = tr.playing = true;
    tr.bpm = 120.0;
    const auto render = [&] (const PatchData& patch) {
        SynthEngine e;
        e.prepare (sr, 256);
        CHECK (e.setPatch (patch));
        return renderSeq (e, sr, 256, 0.5, { { 0.0, noteOn (69, 127) } }, h, tr);
    };
    const size_t a0 = (size_t) (0.08 * sr), a1 = (size_t) (0.2 * sr), b0 = (size_t) (0.33 * sr), b1 = (size_t) (0.45 * sr);
    auto r = render (d);
    CHECK (peak (r.l, a0, a1) > 0.05f);
    CHECK (peak (r.r, a0, a1) < 0.005f);
    CHECK (peak (r.l, b0, b1) < 0.005f);
    CHECK (peak (r.r, b0, b1) > 0.05f);

    // Width 0: both channels open whenever either pattern is open (rhythm kept, panning gone).
    d.perf.gate.width = 0.0f;
    r = render (d);
    CHECK (peak (r.l, a0, a1) > 0.05f);
    CHECK (peak (r.r, a0, a1) > 0.05f);
    CHECK (peak (r.l, b0, b1) > 0.05f);

    // Stereo off: the right pattern is ignored.
    d.perf.gate.width = 1.0f;
    d.perf.gate.stereo = false;
    r = render (d);
    CHECK (peak (r.r, a0, a1) > 0.05f);
    CHECK (peak (r.r, b0, b1) < 0.005f);
}

FM_TEST ("dsp", "Arp octave order: the figure is played inside one octave, then steps by an octave")
{
    const double sr = 48000.0;
    const auto run = [&] (ArpMode mode, ArpOctaveMode order, int octaves, int steps) {
        PerformanceState perf;
        perf.arp.mode = mode;
        perf.arp.octaveMode = order;
        perf.arp.octaves = octaves;
        perf.arp.rate = 12;
        perf.arp.gate = 0.5f;
        Performer p;
        p.prepare (sr);
        const double bps = 120.0 / 60.0 / sr;
        double ppq = 0.0;
        std::vector<int> notes;
        MidiEvent in[3] = { noteOn (60), noteOn (64), noteOn (67) };
        MidiEvent out[256];
        const int block = 240;
        for (int pos = 0; (int) notes.size() < steps && pos < (int) (sr * 20); pos += block)
        {
            Performer::Clock clock { ppq, bps, true, pos == 0, false };
            const int n = p.process (pos == 0 ? in : nullptr, pos == 0 ? 3 : 0, block, perf, true, clock, out, 256);
            for (int i = 0; i < n; ++i) if (out[i].type == MidiEvent::NoteOn) notes.push_back (out[i].data1);
            ppq += block * bps;
        }
        notes.resize ((size_t) steps);
        return notes;
    };
    using V = std::vector<int>;
    // up-down notes, octaves stepping up: C E G E | C' E' G' E' | C'' ... (the reference figure)
    CHECK (run (ArpMode::UpDown, ArpOctaveMode::Up, 3, 14) == (V { 60, 64, 67, 64, 72, 76, 79, 76, 84, 88, 91, 88, 60, 64 }));
    // octaves stepping down start at the top
    CHECK (run (ArpMode::UpDown, ArpOctaveMode::Down, 2, 9) == (V { 72, 76, 79, 76, 60, 64, 67, 64, 72 }));
    CHECK (run (ArpMode::Up, ArpOctaveMode::UpDown, 3, 13) == (V { 60, 64, 67, 72, 76, 79, 84, 88, 91, 72, 76, 79, 60 }));
    CHECK (run (ArpMode::Down, ArpOctaveMode::Up, 2, 7) == (V { 67, 64, 60, 79, 76, 72, 67 }));
    // unchanged default: the order runs across the whole range
    CHECK (run (ArpMode::UpDown, ArpOctaveMode::Range, 2, 11) == (V { 60, 64, 67, 72, 76, 79, 76, 72, 67, 64, 60 }));
}


// ---- regressions from the 2026-11 code review (docs/DECISIONS.md D-040) ----
FM_TEST ("dsp", "Review: a strummed note cancelled before it sounds still releases the pitch held by another key")
{
    const double sr = 48000.0;
    PerformanceState perf;
    perf.chord.type = ChordType::Major;
    perf.chord.strumMs = 100.0f;
    Performer p;
    p.prepare (sr);
    Performer::Clock clock { 0.0, 120.0 / 60.0 / sr, false, false, false };
    MidiEvent out[128];
    int onE = 0, offE = 0;
    const auto run = [&] (const MidiEvent* in, int numIn, int n) {
        const int count = p.process (in, numIn, n, perf, false, clock, out, 128);
        for (int i = 0; i < count; ++i)
            if (out[i].data1 == 64) { onE += out[i].type == MidiEvent::NoteOn; offE += out[i].type == MidiEvent::NoteOff; }
    };
    MidiEvent e4[] = { at (noteOn (64, 100), 0) };
    run (e4, 1, 480);                 // E4 key: its root (64) sounds at once
    MidiEvent c4[] = { at (noteOn (60, 100), 0) };
    run (c4, 1, 480);                 // C4 key: its third (64) is strummed 100 ms later
    MidiEvent offKeyE[] = { at (noteOff (64), 0) };
    run (offKeyE, 1, 480);
    MidiEvent offKeyC[] = { at (noteOff (60), 0) };
    run (offKeyC, 1, 480);            // released before the strummed 64 fires
    run (nullptr, 0, 48000);
    CHECK (onE == 1);
    CHECK (offE >= 1);                // the sounding E must be released
    CHECK (p.getNumScheduled() == 0);
}

FM_TEST ("dsp", "Review: arp swing with a long gate does not cut repeated pitches short")
{
    const double sr = 48000.0, bps = 120.0 / 60.0 / sr;
    PerformanceState perf;
    perf.arp.rate = 12;    // 6000 samples per step
    perf.arp.gate = 0.8f;
    perf.arp.swing = 0.6f; // odd steps 1800 samples late
    Performer p;
    p.prepare (sr);
    MidiEvent out[256];
    MidiEvent key[] = { at (noteOn (60, 100), 0) };
    double ppq = 0.0;
    int64_t t = 0, lastOn = -1, shortest = INT64_MAX;
    int notes = 0;
    for (int b = 0; b < 400; ++b)
    {
        Performer::Clock clock { ppq, bps, true, b == 0, false };
        const int n = p.process (b == 0 ? key : nullptr, b == 0 ? 1 : 0, 240, perf, true, clock, out, 256);
        for (int i = 0; i < n; ++i)
        {
            if (out[i].type == MidiEvent::NoteOn) { lastOn = t + out[i].offset; ++notes; }
            // an off that follows an on of the same pitch closely would mean the new note was cut
            else if (out[i].type == MidiEvent::NoteOff && lastOn >= 0 && t + out[i].offset > lastOn) shortest = std::min (shortest, t + out[i].offset - lastOn);
        }
        t += 240;
        ppq += 240 * bps;
    }
    CHECK (notes >= 14);
    CHECK (shortest >= 3000); // every note sounds for at least half a step (it was 600 samples)
}

FM_TEST ("dsp", "Review: a first key in free-run arp never double-triggers or silences the pattern")
{
    const double sr = 48000.0, bps = 120.0 / 60.0 / sr;
    PerformanceState perf;
    perf.arp.rate = 12; // 6000 samples per step
    perf.arp.gate = 0.5f;
    for (int keyOffset : { 0, 100, 250, 400, 511 })
        for (int idleBlocks : { 0, 11, 23, 937 }) // different phases of the old grid
        {
            Performer p;
            p.prepare (sr);
            MidiEvent out[256];
            double ppq = 0.0;
            int64_t t = 0;
            std::vector<int64_t> ons;
            for (int b = 0; b < idleBlocks + 200; ++b)
            {
                Performer::Clock clock { ppq, bps, false, false, false };
                MidiEvent key[] = { at (noteOn (60, 100), keyOffset) };
                const bool press = b == idleBlocks;
                const int n = p.process (press ? key : nullptr, press ? 1 : 0, 512, perf, true, clock, out, 256);
                for (int i = 0; i < n; ++i) if (out[i].type == MidiEvent::NoteOn) ons.push_back (t + out[i].offset);
                t += 512;
                ppq += 512 * bps;
            }
            CHECK (ons.size() >= 15);
            bool even = ! ons.empty() && ons[0] == (int64_t) idleBlocks * 512 + keyOffset; // starts on the key
            for (size_t i = 1; i < ons.size(); ++i) even = even && std::llabs ((ons[i] - ons[i - 1]) - 6000) <= 2;
            CHECK (even);
        }
}

FM_TEST ("dsp", "Review: enabling Drive at an odd block size does not mute the synth")
{
    const double sr = 48000.0;
    PatchData d = shortBlip();
    d.sound.engine.operators[0].env.s = 1.0f;
    d.fx.proc[(size_t) FxType::Drive].enabled = false;
    for (int block : { 7, 33, 64 })
    {
        SynthEngine e;
        e.prepare (sr, 512);
        CHECK (e.setPatch (d));
        HostParams h;
        std::vector<float> l ((size_t) block), r ((size_t) block);
        MidiEvent on = noteOn (69, 127);
        e.process (l.data(), r.data(), block, &on, 1, h, {});
        for (int i = 0; i < 4000 / block; ++i) e.process (l.data(), r.data(), block, nullptr, 0, h, {});
        PatchData withDrive = d;
        withDrive.fx.proc[(size_t) FxType::Drive].enabled = true;
        withDrive.fx.proc[(size_t) FxType::Drive].p[0] = 0.5f;
        withDrive.fx.proc[(size_t) FxType::Drive].p[2] = 1.0f;
        CHECK (e.setPatch (withDrive));
        float peakLate = 0.0f;
        for (int i = 0; i < 48000 / block; ++i)
        {
            e.process (l.data(), r.data(), block, nullptr, 0, h, {});
            if (i > 24000 / block) for (float v : l) peakLate = std::max (peakLate, std::fabs (v));
        }
        CHECK (peakLate > 0.01f);
    }
}

FM_TEST ("dsp", "Review: switching to mono while keys are held leaves no voice stuck")
{
    const double sr = 48000.0;
    PatchData d = shortBlip();
    d.sound.engine.operators[0].env.s = 1.0f;
    d.sound.engine.operators[0].env.r = 0.05f;
    SynthEngine e;
    e.prepare (sr, 256);
    CHECK (e.setPatch (d));
    HostParams h;
    std::vector<float> l (256), r (256);
    MidiEvent on[] = { noteOn (60, 100), noteOn (64, 100) };
    e.process (l.data(), r.data(), 256, on, 2, h, {});
    for (int i = 0; i < 20; ++i) e.process (l.data(), r.data(), 256, nullptr, 0, h, {});
    d.perf.mono = true;
    CHECK (e.setPatch (d));
    for (int i = 0; i < 20; ++i) e.process (l.data(), r.data(), 256, nullptr, 0, h, {});
    MidiEvent off[] = { noteOff (60), noteOff (64) };
    e.process (l.data(), r.data(), 256, off, 2, h, {});
    for (int i = 0; i < 400; ++i) e.process (l.data(), r.data(), 256, nullptr, 0, h, {});
    CHECK (e.getActiveVoiceCount() == 0);
}
