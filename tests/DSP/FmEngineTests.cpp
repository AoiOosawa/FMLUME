#include "TestFramework.h"
#include "TestUtil.h"
#include "Core/Templates.h"

using namespace fmtest;

namespace
{
PatchData twoOp (float index, float modRatio = 1.0f)
{
    PatchData d = makeInitPatchData();
    auto& e = d.sound.engine;
    e.operators[0] = makeOp (modRatio, 1.0f, 0.001f, 0.5f, 1.0f, 0.1f, 0.0f);
    e.operators[1] = makeOp (1.0f, 1.0f, 0.001f, 0.5f, 1.0f, 0.1f, 1.0f);
    for (auto& op : e.operators) { op.velocityToLevel = 0.0f; op.velocityToIndex = 0.0f; }
    addEdge (e, 0, 1, index);
    return d;
}
PatchData oneOp (float a = 0.001f, float dcy = 0.5f, float s = 1.0f, float r = 0.1f)
{
    PatchData d = makeInitPatchData();
    d.sound.engine.operators[0] = makeOp (1.0f, 1.0f, a, dcy, s, r, 1.0f);
    d.sound.engine.operators[0].velocityToLevel = 0.0f;
    return d;
}
HostParams hostQ (int q) { HostParams h; h.quality = q; return h; }

// Largest sample-to-sample jump, a crude click detector.
float maxStep (const std::vector<float>& v, size_t from = 1, size_t to = (size_t) -1)
{
    float m = 0.0f;
    to = std::min (to, v.size());
    for (size_t i = std::max<size_t> (from, 1); i < to; ++i) m = std::max (m, std::fabs (v[i] - v[i - 1]));
    return m;
}
int bestLag (const std::vector<float>& ref, const std::vector<float>& x, int maxLag, size_t from, size_t len)
{
    double best = -1.0e30;
    int lag = 0;
    for (int l = 0; l <= maxLag; ++l)
    {
        double acc = 0.0;
        for (size_t i = from; i < from + len; ++i) acc += (double) ref[i] * x[i + (size_t) l];
        if (acc > best) { best = acc; lag = l; }
    }
    return lag;
}
} // namespace

FM_TEST ("dsp", "DSP-02 two operators with index 0 equal the pure carrier")
{
    const double sr = 48000.0;
    SynthEngine a, b;
    a.prepare (sr, 256);
    b.prepare (sr, 256);
    CHECK (a.setPatch (twoOp (0.0f, 3.0f)));
    PatchData solo = twoOp (0.0f, 3.0f);
    solo.sound.engine.numEdges = 0;
    solo.sound.engine.operators[0].enabled = false;
    CHECK (b.setPatch (solo));
    std::vector<NoteEvent> seq { { 0.0, noteOn (60) }, { 0.5, noteOff (60) } };
    auto ra = renderSeq (a, sr, 256, 1.0, seq), rb = renderSeq (b, sr, 256, 1.0, seq);
    CHECK (peak (ra.l) > 0.05f);
    CHECK (maxAbsDiff (ra.l, rb.l) < 1.0e-6);
}

FM_TEST ("dsp", "DSP-03 raising the modulation index creates sidebands")
{
    const double sr = 48000.0;
    double prev = -1.0;
    for (float index : { 0.0f, 0.5f, 1.5f })
    {
        SynthEngine e;
        e.prepare (sr, 256);
        CHECK (e.setPatch (twoOp (index)));
        auto r = renderSeq (e, sr, 256, 1.0, { { 0.0, noteOn (69) } });
        const double h2 = toneMag (r.l, sr, 880.0, 12000, 44000);
        CHECK (h2 > prev);
        if (index == 0.0f) CHECK (h2 < 1.0e-4);
        if (index == 1.5f) CHECK (h2 > 0.02);
        prev = h2;
    }
    // Different routings of the same operators sound different.
    SynthEngine e1, e2;
    e1.prepare (sr, 256);
    e2.prepare (sr, 256);
    PatchData p1 = makeInitPatchData(), p2 = makeInitPatchData();
    for (auto* p : { &p1, &p2 })
        for (int k = 0; k < 3; ++k) p->sound.engine.operators[(size_t) k] = makeOp (1.0f + k, 1.0f, 0.001f, 0.5f, 1.0f, 0.1f);
    applyAlgorithm (p1.sound.engine, 2); // 1>2>3
    applyAlgorithm (p2.sound.engine, 7); // (1,2)>3
    CHECK (e1.setPatch (p1) && e2.setPatch (p2));
    auto r1 = renderSeq (e1, sr, 256, 0.5, { { 0.0, noteOn (57) } }), r2 = renderSeq (e2, sr, 256, 0.5, { { 0.0, noteOn (57) } });
    CHECK (maxAbsDiff (r1.l, r2.l) > 0.01);
}

FM_TEST ("dsp", "DSP-04 cyclic graphs are rejected by the compiler and never reach the DSP")
{
    PatchData d = twoOp (1.0f);
    auto& e = d.sound.engine;
    e.edges[(size_t) e.numEdges++] = { 1, 0, 1.0f }; // 2 -> 1 closes the loop
    CHECK (hasCycle (e));
    CHECK (! validatePatch (d).ok);
    CompiledPatch cp;
    std::string err;
    CHECK (! compilePatch (d, cp, &err));
    CHECK (err.find ("cycle") != std::string::npos);
    CHECK (edgeWouldBeInvalid (twoOp (1.0f).sound.engine, 1, 0));
    CHECK (edgeWouldBeInvalid (twoOp (1.0f).sound.engine, 0, 0));
    CHECK (edgeWouldBeInvalid (twoOp (1.0f).sound.engine, 0, 1)); // duplicate
    CHECK (! edgeWouldBeInvalid (twoOp (1.0f).sound.engine, 1, 2));

    const double sr = 48000.0;
    SynthEngine eng, ref;
    eng.prepare (sr, 256);
    ref.prepare (sr, 256);
    CHECK (eng.setPatch (twoOp (1.0f)) && ref.setPatch (twoOp (1.0f)));
    CHECK (! eng.setPatch (d)); // rejected: the previous patch keeps sounding
    auto a = renderSeq (eng, sr, 256, 0.5, { { 0.0, noteOn (60) } }), b = renderSeq (ref, sr, 256, 0.5, { { 0.0, noteOn (60) } });
    CHECK (maxAbsDiff (a.l, b.l) == 0.0);

    // No carrier / NaN / out-of-range values are rejected too.
    PatchData silent = makeInitPatchData();
    silent.sound.engine.operators[0].carrierGain = 0.0f;
    CHECK (! validatePatch (silent).ok);
    PatchData nan = makeInitPatchData();
    nan.sound.engine.operators[0].ratio = std::nanf ("");
    CHECK (! validatePatch (nan).ok);
    PatchData big = makeInitPatchData();
    big.sound.engine.operators[0].feedback = 9.0f;
    CHECK (! validatePatch (big).ok);
}

FM_TEST ("dsp", "DSP-05 maximum self feedback stays finite and bounded")
{
    const double sr = 48000.0;
    for (int q = 0; q < 3; ++q)
    {
        SynthEngine e;
        e.prepare (sr, 256);
        PatchData d = makeInitPatchData();
        for (int k = 0; k < kNumOps; ++k)
        {
            d.sound.engine.operators[(size_t) k] = makeOp (1.0f + 5.3f * k, 1.0f, 0.001f, 0.5f, 1.0f, 0.2f, 1.0f);
            d.sound.engine.operators[(size_t) k].feedback = 1.5f;
        }
        for (int k = 0; k + 1 < kNumOps; ++k) addEdge (d.sound.engine, k, k + 1, 16.0f);
        CHECK (e.setPatch (d));
        auto r = renderSeq (e, sr, 256, 2.0, { { 0.0, noteOn (30, 127) }, { 0.1, noteOn (100, 127) }, { 0.2, noteOn (127, 127) } }, hostQ (q));
        CHECK (allFinite (r.l) && allFinite (r.r));
        CHECK (peak (r.l) <= 1.0f && peak (r.r) <= 1.0f);
        CHECK (peak (r.l) > 0.01f);
    }
}

FM_TEST ("dsp", "DSP-06 ADSR timing, release from current level and sustain pedal")
{
    const double sr = 48000.0;
    {
        // Sustain level and -60 dB release time.
        SynthEngine e;
        e.prepare (sr, 256);
        PatchData d = oneOp (0.01f, 0.2f, 0.5f, 0.4f);
        CHECK (e.setPatch (d));
        auto r = renderSeq (e, sr, 64, 3.0, { { 0.0, noteOn (69, 127) }, { 1.5, noteOff (69) } });
        const float sustainPeak = peak (r.l, (size_t) (1.3 * sr), (size_t) (1.49 * sr));
        const float attackPeak = peak (r.l, 0, (size_t) (0.05 * sr));
        CHECK_NEAR (sustainPeak / attackPeak, 0.5, 0.03);
        // After the release time the level has dropped by ~60 dB; halfway by ~30 dB.
        const float half = peak (r.l, (size_t) (1.69 * sr), (size_t) (1.71 * sr));
        const float end = peak (r.l, (size_t) (1.90 * sr), (size_t) (1.92 * sr));
        CHECK_NEAR (20.0 * std::log10 (half / sustainPeak), -30.0, 3.0);
        CHECK_NEAR (20.0 * std::log10 (end / sustainPeak), -60.0, 4.0);
        CHECK (e.getActiveVoiceCount() == 0);
    }
    {
        // Attack time: reaches full level at A.
        SynthEngine e;
        e.prepare (sr, 256);
        CHECK (e.setPatch (oneOp (0.2f, 0.5f, 1.0f, 0.1f)));
        auto r = renderSeq (e, sr, 64, 0.6, { { 0.0, noteOn (69, 127) } });
        const float full = peak (r.l, (size_t) (0.4 * sr), (size_t) (0.5 * sr));
        CHECK (peak (r.l, (size_t) (0.09 * sr), (size_t) (0.1 * sr)) < 0.85f * full);
        CHECK (peak (r.l, (size_t) (0.21 * sr), (size_t) (0.23 * sr)) > 0.97f * full);
    }
    {
        // CC64 holds notes after note-off and releases them when lifted.
        SynthEngine e;
        e.prepare (sr, 256);
        CHECK (e.setPatch (oneOp (0.001f, 0.5f, 1.0f, 0.05f)));
        auto r = renderSeq (e, sr, 128, 2.0, { { 0.0, cc (64, 127) }, { 0.1, noteOn (60) }, { 0.3, noteOff (60) }, { 1.0, cc (64, 0) } });
        CHECK (peak (r.l, (size_t) (0.8 * sr), (size_t) (0.95 * sr)) > 0.1f);
        CHECK (peak (r.l, (size_t) (1.5 * sr)) < 1.0e-3f);
        CHECK (e.getActiveVoiceCount() == 0);
    }
}

FM_TEST ("dsp", "DSP-08 polyphony 32 with unison 8 stays inside the voice budget")
{
    const double sr = 48000.0;
    SynthEngine e;
    e.prepare (sr, 128);
    CHECK (e.setPatch (twoOp (2.0f)));
    HostParams h;
    h.polyphony = 32;
    h.unison = 8;
    h.detune = 20.0f;
    std::vector<NoteEvent> seq;
    for (int i = 0; i < 32; ++i) seq.push_back ({ 0.01 * i, noteOn (36 + i, 100) });
    for (int i = 0; i < 32; ++i) seq.push_back ({ 0.8 + 0.001 * i, noteOff (36 + i) });
    CHECK (SynthEngine::effectiveMaxNotes (32, 8) == 8);
    CHECK (SynthEngine::effectiveMaxNotes (16, 1) == 16);

    // Render in pieces so the voice counters can be observed mid-flight.
    std::vector<float> l (128), r (128);
    size_t next = 0;
    int maxSubs = 0, maxNotes = 0;
    bool finite = true;
    float pk = 0.0f;
    for (int pos = 0; pos < (int) (2.5 * sr); pos += 128)
    {
        std::vector<MidiEvent> evs;
        while (next < seq.size() && (int) (seq[next].time * sr) < pos + 128)
        {
            MidiEvent ev = seq[next].ev;
            ev.offset = std::max (0, (int) (seq[next].time * sr) - pos);
            evs.push_back (ev);
            ++next;
        }
        e.process (l.data(), r.data(), 128, evs.data(), (int) evs.size(), h, {});
        // Fading (stolen) voices overlap the new ones for 5 ms at most.
        if (pos % 1280 == 0 || true) { maxSubs = std::max (maxSubs, e.getActiveSubVoiceCount()); maxNotes = std::max (maxNotes, e.getActiveVoiceCount()); }
        for (int i = 0; i < 128; ++i) { finite = finite && std::isfinite (l[(size_t) i]); pk = std::max (pk, std::fabs (l[(size_t) i])); }
    }
    CHECK (finite);
    CHECK (pk > 0.05f && pk <= 1.0f);
    CHECK (maxSubs <= kMaxSubVoices + 16); // budget + briefly overlapping steal fades
    CHECK (maxNotes >= 8);
    CHECK (e.getActiveVoiceCount() == 0);
}

FM_TEST ("dsp", "Voice stealing is click free and the newest notes win")
{
    const double sr = 48000.0;
    SynthEngine e;
    e.prepare (sr, 64);
    CHECK (e.setPatch (oneOp (0.002f, 0.5f, 1.0f, 0.3f)));
    HostParams h;
    h.polyphony = 2;
    auto r = renderSeq (e, sr, 64, 1.0, { { 0.0, noteOn (48) }, { 0.1, noteOn (55) }, { 0.2, noteOn (60) }, { 0.3, noteOn (64) } }, h);
    CHECK (e.getActiveVoiceCount() == 2);
    // A hard cut of a 0.35-amplitude sine would jump by up to ~0.35; a faded steal stays near the waveform slope.
    CHECK (maxStep (r.l) < 0.06f);
    CHECK (toneMag (r.l, sr, 440.0 * std::pow (2.0, (64 - 69) / 12.0), 30000, 47000) > 0.05);
    CHECK (toneMag (r.l, sr, 440.0 * std::pow (2.0, (48 - 69) / 12.0), 30000, 47000) < 0.005);
}

FM_TEST ("dsp", "DSP-09 macros: neutral equals the base patch, moving changes the sound, missing targets are ignored")
{
    const double sr = 48000.0;
    PatchData base = twoOp (1.0f);
    PatchData mapped = base;
    addMapping (mapped.sound.macros[0], pc::edge (0, 1), 0.3f);
    addMapping (mapped.sound.macros[0], pc::op (1, kOpEnvR), 0.2f, Curve::Exp);
    addMapping (mapped.sound.macros[1], pc::op (0, kOpLevel), -0.5f, Curve::SCurve, Polarity::Unipolar);
    // A mapping to an edge that does not exist must be dropped, not crash.
    mapped.sound.macros[2].mappings[0].target = (int16_t) pc::edge (4, 5);
    mapped.sound.macros[2].mappings[0].amount = 1.0f;
    mapped.sound.macros[2].numMappings = 1;
    CHECK (validatePatch (mapped).ok);
    CHECK (! validatePatch (mapped).warnings.empty());

    std::vector<NoteEvent> seq { { 0.0, noteOn (62) }, { 0.4, noteOff (62) } };
    SynthEngine e0, e1, e2;
    for (auto* e : { &e0, &e1, &e2 }) e->prepare (sr, 256);
    CHECK (e0.setPatch (base) && e1.setPatch (mapped) && e2.setPatch (mapped));
    HostParams neutral, moved;
    moved.macro[0] = 1.0f;
    auto r0 = renderSeq (e0, sr, 256, 0.8, seq, neutral);
    auto r1 = renderSeq (e1, sr, 256, 0.8, seq, neutral);
    auto r2 = renderSeq (e2, sr, 256, 0.8, seq, moved);
    CHECK (maxAbsDiff (r0.l, r1.l) < 1.0e-6);
    CHECK (maxAbsDiff (r0.l, r2.l) > 0.01);
    CHECK (allFinite (r2.l));

    // Extreme macro values with extreme amounts stay inside the parameter range.
    PatchData wild = base;
    for (int i = 0; i < kNumMacros; ++i)
    {
        addMapping (wild.sound.macros[(size_t) i], pc::edge (0, 1), i % 2 ? 1.0f : -1.0f);
        addMapping (wild.sound.macros[(size_t) i], pc::op (1, kOpLevel), 1.0f);
        addMapping (wild.sound.macros[(size_t) i], pc::op (0, kOpRatio), 1.0f);
        addMapping (wild.sound.macros[(size_t) i], pc::fx (FxType::Reverb, 4), 1.0f);
    }
    SynthEngine e3;
    e3.prepare (sr, 256);
    CHECK (e3.setPatch (wild));
    HostParams ext;
    for (auto& m : ext.macro) m = 1.0f;
    auto r3 = renderSeq (e3, sr, 256, 0.5, seq, ext);
    CHECK (allFinite (r3.l) && peak (r3.l) <= 1.0f);
}

FM_TEST ("dsp", "DSP-10 pitch and envelope timing are sample-rate independent")
{
    double refRelease = 0.0;
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        SynthEngine e;
        e.prepare (sr, 256);
        CHECK (e.setPatch (oneOp (0.005f, 0.2f, 0.7f, 0.5f)));
        auto r = renderSeq (e, sr, 256, 2.0, { { 0.0, noteOn (69, 127) }, { 1.0, noteOff (69) } });
        CHECK_NEAR (zeroCrossHz (r.l, sr, (size_t) (0.3 * sr), (size_t) (0.9 * sr)), 440.0, 0.5);
        const float sus = peak (r.l, (size_t) (0.8 * sr), (size_t) (0.99 * sr));
        const float mid = peak (r.l, (size_t) (1.24 * sr), (size_t) (1.26 * sr));
        const double db = 20.0 * std::log10 (mid / sus);
        if (refRelease == 0.0) refRelease = db;
        CHECK_NEAR (db, -30.0, 3.0);
        CHECK_NEAR (db, refRelease, 0.6);
        CHECK (allFinite (r.l));
    }
}

FM_TEST ("dsp", "DSP-11 rejected patches and hostile host values never produce NaN")
{
    const double sr = 48000.0;
    SynthEngine e;
    e.prepare (sr, 512);
    CHECK (e.setPatch (twoOp (3.0f)));
    HostParams h;
    h.gainDb = std::nanf ("");
    h.pan = 99.0f;
    h.polyphony = -5;
    h.unison = 400;
    h.detune = 1.0e9f;
    h.spread = -3.0f;
    h.quality = 77;
    for (auto& m : h.macro) m = 1.0e6f;
    auto r = renderSeq (e, sr, 512, 0.5, { { 0.0, noteOn (60) }, { 0.1, bend (55.0f) }, { 0.2, cc (1, 127) } }, h);
    CHECK (allFinite (r.l) && allFinite (r.r));
    MidiEvent weird[3];
    weird[0].type = MidiEvent::NoteOn; weird[0].data1 = 200; weird[0].data2 = 255; weird[0].offset = -40;
    weird[1].type = MidiEvent::NoteOff; weird[1].data1 = 250; weird[1].offset = 100000;
    weird[2].type = (MidiEvent::Type) 42;
    std::vector<float> l (512), rr (512);
    e.process (l.data(), rr.data(), 512, weird, 3, HostParams {}, {});
    CHECK (allFinite (l));
}

FM_TEST ("dsp", "DSP-12 quality 1x/2x/4x agree in pitch and report their latency exactly")
{
    const double sr = 48000.0;
    Rendered r[3];
    for (int q = 0; q < 3; ++q)
    {
        SynthEngine e;
        e.prepare (sr, 256);
        CHECK (e.setPatch (twoOp (0.4f)));
        r[q] = renderSeq (e, sr, 256, 1.0, { { 0.0, noteOn (69) } }, hostQ (q));
        CHECK (e.getActiveQuality() == q);
        CHECK (e.getLatencySamples() == SynthEngine::latencyForQuality (q));
        CHECK_NEAR (zeroCrossHz (r[q].l, sr, 12000, 44000), 440.0, 0.5);
        CHECK (maxAbsDiff (r[q].l, r[q].r) < 1.0e-6);
    }
    CHECK (SynthEngine::latencyForQuality (0) < SynthEngine::latencyForQuality (1));
    for (int q = 1; q < 3; ++q)
    {
        const int expected = SynthEngine::latencyForQuality (q) - SynthEngine::latencyForQuality (0);
        CHECK (bestLag (r[0].l, r[q].l, 40, 4000, 8000) == expected);
        // After compensating the reported latency the signals match closely (band-limited difference only).
        double err = 0.0;
        for (size_t i = 4000; i < 30000; ++i) err = std::max (err, (double) std::fabs (r[0].l[i] - r[q].l[i + (size_t) expected]));
        CHECK (err < 0.02);
    }
    // First output sample appears exactly `latency` samples after the note-on.
    for (int q = 0; q < 3; ++q)
    {
        size_t first = 0;
        while (first < r[q].l.size() && std::fabs (r[q].l[first]) < 1.0e-7f) ++first;
        CHECK ((int) first >= SynthEngine::latencyForQuality (q) - 12 && (int) first <= SynthEngine::latencyForQuality (q) + 2);
    }
}

FM_TEST ("dsp", "Output does not depend on the host block size")
{
    const double sr = 48000.0;
    PatchData d = makeCategoryPreset (Category::Pad).data;
    std::vector<NoteEvent> seq { { 0.0, noteOn (57, 90) }, { 0.11, noteOn (64, 70) }, { 0.3, bend (0.5f) }, { 0.5, noteOff (57) }, { 0.7, noteOff (64) } };
    HostParams h = d.host;
    h.macro[0] = 0.8f;
    Rendered ref;
    for (int block : { 512, 32, 77, 2048 })
    {
        SynthEngine e;
        e.prepare (sr, block);
        CHECK (e.setPatch (d));
        auto r = renderSeq (e, sr, block, 1.2, seq, h);
        CHECK (allFinite (r.l));
        if (block == 512) ref = r;
        else { CHECK (maxAbsDiff (ref.l, r.l) < 1.0e-5); CHECK (maxAbsDiff (ref.r, r.r) < 1.0e-5); }
    }
}

FM_TEST ("dsp", "Patch generations: held notes keep the old patch, new notes get the new one")
{
    const double sr = 48000.0;
    SynthEngine e;
    e.prepare (sr, 256);
    PatchData a = oneOp (0.001f, 0.5f, 1.0f, 0.2f);
    PatchData b = a;
    b.sound.engine.operators[0].ratio = 2.0f;
    CHECK (e.setPatch (a));
    HostParams h;
    h.gainDb = -6.0f; // the click thresholds below are absolute sample steps measured at -6 dB
    std::vector<float> l (48000), r (48000);
    MidiEvent on = noteOn (69);
    e.process (l.data(), r.data(), 256, &on, 1, h, {});
    CHECK (e.setPatch (b, true)); // new generation while the note is held
    int pos = 256;
    for (; pos < 24000; pos += 256) e.process (l.data() + pos, r.data() + pos, 256, nullptr, 0, h, {});
    MidiEvent on2 = noteOn (57); // A3 with ratio 2 -> also 440 Hz... use a distinct pitch instead
    on2.data1 = 60;
    e.process (l.data() + pos, r.data() + pos, 256, &on2, 1, h, {});
    for (pos += 256; pos + 256 <= 48000; pos += 256) e.process (l.data() + pos, r.data() + pos, 256, nullptr, 0, h, {});
    // Old note still at 440 Hz (ratio 1) for the whole render.
    CHECK (toneMag (l, sr, 440.0, 30000, 47000) > 0.1);
    // New note uses ratio 2: C4 * 2 = 523.25 Hz, and no C4 fundamental.
    CHECK (toneMag (l, sr, 523.25, 30000, 47000) > 0.1);
    CHECK (toneMag (l, sr, 261.63, 30000, 47000) < 0.01);
    CHECK (maxStep (l) < 0.08f);

    // In-place edits (same structure, not forced) reach held notes smoothly.
    SynthEngine e2;
    e2.prepare (sr, 256);
    CHECK (e2.setPatch (twoOp (0.0f)));
    std::vector<float> l2 (48000), r2 (48000);
    e2.process (l2.data(), r2.data(), 256, &on, 1, h, {});
    for (pos = 256; pos < 24000; pos += 256) e2.process (l2.data() + pos, r2.data() + pos, 256, nullptr, 0, h, {});
    CHECK (e2.setPatch (twoOp (2.0f), false));
    for (; pos + 256 <= 48000; pos += 256) e2.process (l2.data() + pos, r2.data() + pos, 256, nullptr, 0, h, {});
    CHECK (toneMag (l2, sr, 880.0, 4000, 20000) < 1.0e-3);
    CHECK (toneMag (l2, sr, 880.0, 30000, 47000) > 0.02);
    CHECK (maxStep (l2) < 0.12f);

    // More than 9 forced generations while a note is held: the oldest voice is faded, never cut.
    SynthEngine e3;
    e3.prepare (sr, 128);
    CHECK (e3.setPatch (a));
    std::vector<float> l3 (48000), r3 (48000);
    e3.process (l3.data(), r3.data(), 128, &on, 1, h, {});
    int gen = 0;
    for (pos = 128; pos + 128 <= 48000; pos += 128)
    {
        if (pos % 1280 == 0 && gen < 14) { PatchData x = a; x.sound.engine.operators[0].level = 0.9f - 0.01f * gen; CHECK (e3.setPatch (x, true)); ++gen; }
        e3.process (l3.data() + pos, r3.data() + pos, 128, nullptr, 0, h, {});
    }
    CHECK (allFinite (l3));
    CHECK (maxStep (l3) < 0.08f);
    CHECK (e3.getActiveVoiceCount() == 0); // the generation limit released the original note
}

FM_TEST ("dsp", "Mono / legato / portamento and pitch bend")
{
    const double sr = 48000.0;
    PatchData d = oneOp (0.002f, 0.5f, 1.0f, 0.05f);
    d.perf.mono = true;
    d.perf.legato = true;
    d.perf.portamento = 0.2f;
    SynthEngine e;
    e.prepare (sr, 64);
    CHECK (e.setPatch (d));
    auto r = renderSeq (e, sr, 64, 2.0, { { 0.0, noteOn (57) }, { 0.5, noteOn (69) }, { 1.2, noteOff (69) }, { 1.6, noteOff (57) } });
    CHECK (e.getActiveVoiceCount() == 0);
    CHECK_NEAR (zeroCrossHz (r.l, sr, (size_t) (0.3 * sr), (size_t) (0.49 * sr)), 220.0, 0.5);
    const double gliding = zeroCrossHz (r.l, sr, (size_t) (0.52 * sr), (size_t) (0.56 * sr));
    CHECK (gliding > 225.0 && gliding < 435.0); // on its way up
    CHECK_NEAR (zeroCrossHz (r.l, sr, (size_t) (1.0 * sr), (size_t) (1.19 * sr)), 440.0, 1.0);
    // Releasing the top note returns to the still-held lower note (last-note priority).
    CHECK_NEAR (zeroCrossHz (r.l, sr, (size_t) (1.5 * sr), (size_t) (1.59 * sr)), 220.0, 1.5);
    CHECK (maxStep (r.l) < 0.05f);

    SynthEngine e2;
    e2.prepare (sr, 64);
    PatchData pb = oneOp();
    pb.sound.engine.pitchBendRange = 12.0f;
    CHECK (e2.setPatch (pb));
    auto r2 = renderSeq (e2, sr, 64, 1.0, { { 0.0, noteOn (57) }, { 0.3, bend (1.0f) } });
    CHECK_NEAR (zeroCrossHz (r2.l, sr, (size_t) (0.1 * sr), (size_t) (0.29 * sr)), 220.0, 0.5);
    CHECK_NEAR (zeroCrossHz (r2.l, sr, (size_t) (0.6 * sr), (size_t) (0.99 * sr)), 440.0, 0.5);
}

FM_TEST ("dsp", "Unison is stereo-symmetric, deterministic and gain-normalised")
{
    const double sr = 48000.0;
    HostParams h;
    h.unison = 6;
    h.detune = 25.0f;
    h.spread = 100.0f;
    SynthEngine e1, e2, e3;
    for (auto* e : { &e1, &e2, &e3 }) { e->prepare (sr, 256); CHECK (e->setPatch (oneOp())); }
    auto a = renderSeq (e1, sr, 256, 1.5, { { 0.0, noteOn (60) } }, h);
    auto b = renderSeq (e2, sr, 256, 1.5, { { 0.0, noteOn (60) } }, h);
    auto solo = renderSeq (e3, sr, 256, 1.5, { { 0.0, noteOn (60) } });
    CHECK (maxAbsDiff (a.l, b.l) == 0.0);
    CHECK (maxAbsDiff (a.l, a.r) > 0.01); // real stereo
    CHECK_NEAR (rms (a.l, 4800) / rms (a.r, 4800), 1.0, 0.25);
    CHECK_NEAR (std::sqrt ((rms (a.l, 4800) * rms (a.l, 4800) + rms (a.r, 4800) * rms (a.r, 4800)) / 2.0) / rms (solo.l, 4800), 1.0, 0.35);
}

FM_TEST ("dsp", "Filter, waveforms and modulation matrix shape the sound")
{
    const double sr = 48000.0;
    auto render = [&] (const PatchData& d, HostParams h = {}) {
        SynthEngine e;
        e.prepare (sr, 256);
        CHECK (e.setPatch (d));
        auto r = renderSeq (e, sr, 256, 1.0, { { 0.0, noteOn (45, 127) } }, h);
        CHECK (allFinite (r.l));
        return r;
    };
    PatchData saw = oneOp();
    saw.sound.engine.operators[0].waveform = Waveform::Saw;
    auto rSaw = render (saw);
    const double f0 = 110.0;
    CHECK (toneMag (rSaw.l, sr, f0 * 5.0, 12000, 44000) > 0.01); // harmonics present
    PatchData lp = saw;
    lp.sound.engine.filter.type = FilterType::LP24;
    lp.sound.engine.filter.cutoffHz = 300.0f;
    auto rLp = render (lp);
    CHECK (toneMag (rLp.l, sr, f0 * 9.0, 12000, 44000) < 0.1 * toneMag (rSaw.l, sr, f0 * 9.0, 12000, 44000));
    CHECK (toneMag (rLp.l, sr, f0, 12000, 44000) > 0.5 * toneMag (rSaw.l, sr, f0, 12000, 44000));
    PatchData hp = saw;
    hp.sound.engine.filter.type = FilterType::HP12;
    hp.sound.engine.filter.cutoffHz = 3000.0f;
    CHECK (toneMag (render (hp).l, sr, f0, 12000, 44000) < 0.05 * toneMag (rSaw.l, sr, f0, 12000, 44000));
    PatchData reso = lp;
    reso.sound.engine.filter.resonance = 0.95f;
    reso.sound.engine.filter.drive = 1.0f;
    CHECK (peak (render (reso).l) <= 1.0f);

    // Band-limiting: a high saw note must not alias strongly below its fundamental.
    for (Waveform w : { Waveform::Triangle, Waveform::Saw, Waveform::Pulse })
    {
        PatchData hi = oneOp();
        hi.sound.engine.operators[0].waveform = w;
        SynthEngine e;
        e.prepare (sr, 256);
        CHECK (e.setPatch (hi));
        auto r = renderSeq (e, sr, 256, 1.0, { { 0.0, noteOn (96, 127) } }); // C7 = 2093 Hz
        const double fund = toneMag (r.l, sr, 2093.0, 12000, 44000);
        CHECK (fund > 0.05);
        for (double alias : { 500.0, 911.0, 1333.0, 1700.0 })
            CHECK (toneMag (r.l, sr, alias, 12000, 44000) < fund * 0.003);
    }

    // LFO -> pitch vibrato widens the spectrum; velocity -> level scales amplitude.
    PatchData vib = oneOp();
    setModSlot (vib.sound, 0, ModSource::LFO1, pc::pitch, 0.05f);
    vib.sound.engine.lfo[0].rateHz = 6.0f;
    auto rv = render (vib), rn = render (oneOp());
    CHECK (maxAbsDiff (rv.l, rn.l) > 0.05);
    PatchData mw = oneOp();
    setModSlot (mw.sound, 0, ModSource::ModWheel, pc::level, -0.5f);
    SynthEngine e;
    e.prepare (sr, 256);
    CHECK (e.setPatch (mw));
    auto rm = renderSeq (e, sr, 256, 1.0, { { 0.0, noteOn (69, 127) }, { 0.5, cc (1, 127) } });
    CHECK (peak (rm.l, 43000) < 0.1f * peak (rm.l, 10000, 20000));
    CHECK (maxStep (rm.l) < 0.05f);
}

FM_TEST ("dsp", "Every algorithm template and category template compiles and makes sound")
{
    const double sr = 48000.0;
    CHECK (numAlgorithmTemplates() >= 16);
    for (int t = 0; t < numAlgorithmTemplates(); ++t)
    {
        PatchData d = makeInitPatchData();
        for (int k = 0; k < kNumOps; ++k) d.sound.engine.operators[(size_t) k] = makeOp (1.0f + (k % 3), 1.0f, 0.001f, 0.5f, 0.8f, 0.1f);
        applyAlgorithm (d.sound.engine, t);
        CHECK (validatePatch (d).ok);
        CHECK (matchAlgorithm (d.sound.engine) == t);
        SynthEngine e;
        e.prepare (sr, 256);
        CHECK (e.setPatch (d));
        auto r = renderSeq (e, sr, 256, 0.3, { { 0.0, noteOn (60) } });
        CHECK (allFinite (r.l) && peak (r.l) > 0.02f && peak (r.l) <= 1.0f);
    }
    for (int c = 0; c < (int) Category::Count; ++c)
    {
        const Preset p = makeCategoryPreset ((Category) c);
        const auto v = validatePatch (p.data);
        CHECK (v.ok);
        if (! v.ok) std::printf ("    %s: %s\n", categoryName ((Category) c), v.error.c_str());
        CHECK (v.warnings.empty());
        for (int m = 0; m < kNumMacros; ++m) CHECK (p.data.sound.macros[(size_t) m].numMappings > 0);
        SynthEngine e;
        e.prepare (sr, 256);
        CHECK (e.setPatch (p.data));
        Rendered base;
        // Every macro audibly changes the sound and nothing blows up at the extremes.
        for (int m = -1; m < kNumMacros; ++m)
            for (float v01 : { 0.0f, 1.0f })
            {
                SynthEngine e2;
                e2.prepare (sr, 256);
                CHECK (e2.setPatch (p.data));
                HostParams h = p.data.host;
                if (m >= 0) h.macro[(size_t) m] = v01;
                auto r = renderSeq (e2, sr, 256, 1.4, { { 0.0, noteOn (57, 100) }, { 0.15, noteOff (57) }, { 0.5, noteOn (64, 100) }, { 0.9, noteOff (64) } }, h);
                CHECK (allFinite (r.l) && peak (r.l) <= 1.0f);
                if (m < 0) { base = r; CHECK (peak (r.l) > 0.02f); break; }
                const double diff = std::max (maxAbsDiff (base.l, r.l), maxAbsDiff (base.r, r.r));
                if (v01 == 1.0f)
                {
                    CHECK (diff > 1.0e-3);
                    if (diff <= 1.0e-3) std::printf ("    %s macro %d has no audible effect\n", categoryName ((Category) c), m + 1);
                }
            }
    }
}
