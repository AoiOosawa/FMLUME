#include "DSP/FM/SynthEngine.h"
#include "DSP/Util/Tables.h"
#include <mutex>

namespace fmlume
{
namespace
{
constexpr int kMaxChunk = SynthEngine::kControlInterval * SynthEngine::kMaxOversample; // internal samples
constexpr int kRampLevel = 0, kRampCarrier = 6, kRampFeedback = 12, kRampEdge = 18, kRampNorm = 48, kNumRamps = 49;
constexpr float kVoiceGain = 0.35f;
constexpr float kTwoPiF = 6.2831853071795864f;
constexpr int kMailSlots = 4;
constexpr int kMaxSeqEvents = 1024;
constexpr int kDecimK2 = 11; // 2x -> 1x stage (47 taps)
constexpr int kDecimK1 = 4;  // 4x -> 2x stage (19 taps)

struct SideRt
{
    float baseNorm[pc::engineCount] {};
    float basePhys[pc::engineCount] {};
    bool exists[pc::engineCount] {};
    float eff[pc::engineCount] {};
    float cur[kNumRamps] {}, inc[kNumRamps] {}, tgt[kNumRamps] {};
    EnvCoefs env[kNumOps];
};

struct PatchSlot
{
    CompiledPatch cp;
    SideRt rt;
    float fxBaseNorm[kNumFx * kFxParams + 1] {}; // shared codes: fx params + portamento
};

struct OpRt
{
    double phase = 0.0, inc = 0.0, dInc = 0.0;
    float p1 = 0.0f, p2 = 0.0f;
    const float* tbl = nullptr;
    int mask = 0;
    float scale = 0.0f;
};

struct NoteVoice
{
    bool active = false, held = false, sustained = false, releasing = false, fresh = true;
    int note = -1;
    float vel = 0.0f;
    uint64_t age = 0;
    int slot = 0, unison = 1;
    float fade = 1.0f, fadeInc = 0.0f;
    bool hasPending = false, pendReleased = false;
    int pendNote = 0;
    float pendVel = 0.0f;
    float pitchNow = 0.0f, pitchTarget = 0.0f;
    double releaseSamples = 0.0;
    EnvState env[kNumOps];
    OpRt op[kMaxUnison][kNumOps];
};

struct MonoNote { int note; float vel; };

inline float hash01 (uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA6Bu;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
    return (float) (h & 0xFFFFFFu) / 16777216.0f;
}
} // namespace

struct SynthEngine::Impl
{
    SynthEngine& owner;
    explicit Impl (SynthEngine& o) : owner (o) {}

    // ---- configuration
    double sr = 48000.0, isr = 96000.0;
    int maxBlock = 512, os = 2;
    bool prepared = false;
    const Tables* tables = nullptr;

    // ---- patch mailbox (non-RT writer -> RT reader, latest wins)
    std::unique_ptr<CompiledPatch[]> mail;
    std::atomic<int> mailState[kMailSlots] {}; // 0 free, 1 writing, 2 queued, 3 reading
    std::atomic<uint64_t> latest { 0 };
    std::mutex writerMutex;
    CompiledPatch writerTmp;
    uint32_t writerGen = 0, lastForcedGen = 0;
    uint32_t seenGen = 0, incomingGen = 0;
    CompiledPatch incoming;
    bool hasIncoming = false;

    // ---- generations
    std::unique_ptr<PatchSlot[]> slots;
    int cur = 0;
    bool snapNext = true;

    // ---- voices
    std::unique_ptr<NoteVoice[]> voices;
    uint64_t ageCounter = 0;
    MonoNote monoStack[16];
    int monoCount = 0;
    bool pedal = false;
    float lastPitch = 0.0f;
    bool haveLastPitch = false;
    double freeRun[kNumOps] {};

    // ---- control state
    HostParams hostNow;
    float macroSm[kNumMacros] {}, quickSm[kNumQuick] {};
    float detuneSm = 0.0f, spreadSm = 0.0f;
    float bendTarget = 0.0f, bendSm = 0.0f, wheelTarget = 0.0f, wheelSm = 0.0f, atTarget = 0.0f, atSm = 0.0f;
    float lastVel = 0.0f, lastKey = 0.0f;
    Lfo lfo[2];
    float lfoVal[2] {};
    EnvState modEnv;
    float modEnvVal = 0.0f;
    float outGain[2] {}, outGainT[2] { 1.0f, 1.0f }; // preset level and pan, left / right
    float masterGain = 0.0f, masterGainT = 0.5f;
    float fxEff[kNumFx][kFxParams] {};
    float portaEff = 0.0f;
    uint64_t sampleCounter = 0;
    double bpmNow = 120.0;

    // ---- clock
    double freePpq = 0.0, expectedPpq = 0.0;
    bool wasPlaying = false;

    // ---- processing
    Halfband<kDecimK2> dec2[2];
    Halfband<kDecimK1> dec1[2];
    // DC blocker on the summed voices (before the filter and the effects), like the
    // AC-coupled output of a hardware FM synth: 1:1 phase modulation and operator feedback
    // produce a real DC component (docs/DECISIONS.md D-051).
    struct DcBlock
    {
        float x1 = 0.0f, y1 = 0.0f;
        void reset() { x1 = y1 = 0.0f; }
        void process (float* buf, int n, float r)
        {
            float x = x1, y = y1;
            for (int i = 0; i < n; ++i) { const float in = buf[i]; y = in - x + r * y; x = in; buf[i] = y; }
            x1 = x;
            y1 = std::fabs (y) < 1.0e-20f ? 0.0f : y;
        }
    };
    static constexpr double kDcBlockHz = 5.0;
    DcBlock dcBlock[2];
    StereoFilter filter;
    TranceGate gate;
    FxChain fx;
    Performer performer;
    std::vector<float> bus[2]; // [ch], internal rate
    float hostBuf[2][kControlInterval] {};
    float ampBuf[kNumOps][kMaxChunk] {}, outBuf[kNumOps][kMaxChunk] {};
    float modBuf[kMaxChunk] {}, mixBuf[kMaxChunk] {}, gainBuf[kMaxChunk] {};
    MidiEvent seqEvents[kMaxSeqEvents];
    MidiEvent inBuf[kMaxSeqEvents];
    MidiEvent carry[64];
    int numCarry = 0;
    int pendingQuality = -1;
    int silentBlocks = 0;
    bool tickStart = true;

    // =====================================================================================
    void cacheBase (PatchSlot& sl)
    {
        auto& rt = sl.rt;
        for (int c = 0; c < pc::engineCount; ++c)
        {
            const auto& d = paramDesc (c);
            rt.exists[c] = paramExists (sl.cp.data, c);
            rt.basePhys[c] = rt.exists[c] ? getParam (sl.cp.data, c) : d.def;
            rt.baseNorm[c] = toNorm (d, rt.basePhys[c]);
        }
        for (int t = 0; t < kNumFx; ++t)
            for (int p = 0; p < kFxParams; ++p)
            {
                const auto& d = fxParamDesc (t, p);
                sl.fxBaseNorm[t * kFxParams + p] = d.name != nullptr ? toNorm (d, sl.cp.data.fx.proc[(size_t) t].p[(size_t) p]) : 0.0f;
            }
        sl.fxBaseNorm[pc::portamento - pc::fxBase] = toNorm (paramDesc (pc::portamento), sl.cp.data.perf.portamento);
    }

    void resetState()
    {
        for (int i = 0; i < kMaxPolyphony; ++i) voices[(size_t) i] = NoteVoice {};
        ageCounter = 0;
        monoCount = 0;
        pedal = false;
        haveLastPitch = false;
        std::memset (freeRun, 0, sizeof (freeRun));
        bendTarget = bendSm = wheelTarget = wheelSm = atTarget = atSm = 0.0f;
        lastVel = lastKey = 0.0f;
        for (int i = 0; i < 2; ++i)
        {
            const auto& ls = slots[cur].cp.data.sound.engine.lfo[(size_t) i];
            lfo[i].reset (0x1234u + (uint64_t) i * 7919u, ls.phase);
            lfoVal[i] = 0.0f;
        }
        modEnv = EnvState {};
        modEnvVal = 0.0f;
        filter.reset();
        for (int ch = 0; ch < 2; ++ch) { dec2[ch].reset(); dec1[ch].reset(); dcBlock[ch].reset(); }
        gate.reset();
        fx.reset();
        performer.reset();
        sampleCounter = 0;
        freePpq = expectedPpq = 0.0;
        wasPlaying = false;
        numCarry = 0;
        snapNext = true;
        silentBlocks = 0;
    }

    void setOversampling (int quality)
    {
        os = quality <= 0 ? 1 : (quality == 1 ? 2 : 4);
        isr = sr * os;
        owner.activeQuality.store (quality <= 0 ? 0 : (quality == 1 ? 1 : 2));
        for (int ch = 0; ch < 2; ++ch) { dec2[ch].reset(); dec1[ch].reset(); dcBlock[ch].reset(); }
        snapNext = true;
    }

    // ---- mailbox reader -----------------------------------------------------------------
    void pollMailbox()
    {
        const uint64_t l = latest.load (std::memory_order_acquire);
        const uint32_t gen = (uint32_t) (l >> 8);
        if (gen == seenGen || gen == 0) return;
        const int slot = (int) (l & 0xffu);
        int expected = 2;
        if (! mailState[slot].compare_exchange_strong (expected, 3, std::memory_order_acq_rel)) return; // writer reclaimed it
        const bool keepForce = hasIncoming && incoming.forceNewGeneration;
        incoming = mail[(size_t) slot];
        incoming.forceNewGeneration = incoming.forceNewGeneration || keepForce;
        mailState[slot].store (0, std::memory_order_release);
        hasIncoming = true;
        incomingGen = gen;
        seenGen = gen;
    }

    void applyIncoming()
    {
        if (! hasIncoming) return;
        PatchSlot& c = slots[(size_t) cur];
        if (! incoming.forceNewGeneration && incoming.structureHash == c.cp.structureHash)
        {
            c.cp = incoming;
            cacheBase (c);
        }
        else
        {
            const int next = (cur + 1) % kPatchSlots;
            bool busy = false;
            for (int i = 0; i < kMaxPolyphony; ++i)
            {
                auto& v = voices[(size_t) i];
                if (v.active && v.slot == next)
                {
                    busy = true;
                    if (v.fadeInc == 0.0f) v.fadeInc = (float) (-1.0 / (0.010 * isr)); // generation limit: 10 ms fade
                    v.held = false;
                }
            }
            if (busy) return; // retry on the next block
            std::memset (c.rt.inc, 0, sizeof (c.rt.inc));
            PatchSlot& n = slots[(size_t) next];
            n.cp = incoming;
            cacheBase (n);
            cur = next;
            snapNext = true;
        }
        hasIncoming = false;
        owner.consumedGen.store (incomingGen, std::memory_order_release);
    }

    // ---- modulation / parameter evaluation -------------------------------------------------
    float sourceValue (ModSource src) const
    {
        switch (src)
        {
            case ModSource::LFO1: return lfoVal[0];
            case ModSource::LFO2: return lfoVal[1];
            case ModSource::ModEnv: return modEnvVal;
            case ModSource::Velocity: return lastVel;
            case ModSource::Keytrack: return lastKey;
            case ModSource::ModWheel: return wheelSm;
            case ModSource::Aftertouch: return atSm;
            default: break;
        }
        if (modSourceIsMacro (src)) return macroSm[(int) src - (int) ModSource::Macro1];
        return 0.0f;
    }

    static void addMacro (const Macro& mac, float value, const float* baseNorm, const bool* exists, int codeOffset, int codeEnd, float* delta)
    {
        for (int k = 0; k < mac.numMappings; ++k)
        {
            const auto& m = mac.mappings[(size_t) k];
            const int c = m.target;
            if (c < codeOffset || c >= codeEnd) continue;
            if (exists != nullptr && ! exists[c]) continue;
            const float b = baseNorm[c - codeOffset];
            const float d = macroDelta (m, value);
            const float r = clampf (b + d, std::min (b, m.min), std::max (b, m.max));
            delta[c] += r - b;
        }
    }

    // Fills delta[] (normalised offsets of every parameter) from the macros, quick controls and mod matrix.
    void evalModulation (PatchSlot& sl, float* delta)
    {
        const SoundPatch& es = sl.cp.data.sound;
        const SideRt& rt = sl.rt;
        std::memset (delta, 0, sizeof (float) * (size_t) pc::total);
        for (int i = 0; i < kNumMacros; ++i)
        {
            addMacro (es.macros[(size_t) i], macroSm[i], rt.baseNorm, rt.exists, 0, pc::engineCount, delta);
            addMacro (es.macros[(size_t) i], macroSm[i], sl.fxBaseNorm, nullptr, pc::fxBase, pc::total, delta);
        }
        for (int i = 0; i < kNumQuick; ++i)
            addMacro (sl.cp.data.fx.quick[(size_t) i], quickSm[i], sl.fxBaseNorm, nullptr, pc::fxBase, pc::total, delta);
        for (int j = 0; j < kNumModSlots; ++j)
        {
            const auto& m = es.matrix[(size_t) j];
            if (! m.enabled || m.target < 0) continue;
            const int c = m.target;
            if (pc::isEngine (c) && ! rt.exists[c]) continue;
            const int dc = pc::matrixDepthBase + j;
            const float depth = clampf (rt.baseNorm[dc] + delta[dc], 0.0f, 1.0f) * 2.0f - 1.0f;
            delta[c] += depth * applyCurve (m.curve, sourceValue (m.source));
        }
    }

    void advanceModSources (PatchSlot& sl, float dt)
    {
        const SoundPatch& es = sl.cp.data.sound;
        const SideRt& rt = sl.rt;
        for (int i = 0; i < 2; ++i)
        {
            const auto& ls = es.engine.lfo[(size_t) i];
            float rate = rt.eff[i == 0 ? pc::lfo1Rate : pc::lfo2Rate];
            if (ls.sync) rate = (float) (bpmNow / 60.0 / syncDivBeats (ls.syncDiv));
            lfoVal[i] = lfo[i].tick (ls.shape, rate, dt);
        }
        EnvCoefs c;
        c.set (rt.eff[pc::modEnvA], rt.eff[pc::modEnvD], rt.eff[pc::modEnvS], rt.eff[pc::modEnvR], sr / kControlInterval);
        modEnvVal = modEnv.tick (c);
    }

    void controlTick()
    {
        const bool snap = snapNext;
        snapNext = false;
        tickStart = true;
        const float dt = (float) (kControlInterval / sr);
        const float cH = snap ? 1.0f : (float) (1.0 - std::exp (-dt / 0.015));
        const float cF = snap ? 1.0f : (float) (1.0 - std::exp (-dt / 0.004));
        for (int i = 0; i < kNumMacros; ++i) macroSm[i] += cH * (clampf (hostNow.macro[(size_t) i], 0.0f, 1.0f) - macroSm[i]);
        for (int i = 0; i < kNumQuick; ++i) quickSm[i] += cH * (clampf (hostNow.quick[(size_t) i], 0.0f, 1.0f) - quickSm[i]);
        detuneSm += cH * (clampf (hostNow.detune, 0.0f, 50.0f) - detuneSm);
        spreadSm += cH * (clampf (hostNow.spread, 0.0f, 100.0f) - spreadSm);
        bendSm += cF * (bendTarget - bendSm);
        wheelSm += cH * (wheelTarget - wheelSm);
        atSm += cH * (atTarget - atSm);

        PatchSlot& sl = slots[(size_t) cur];
        SideRt& rt = sl.rt;
        const EnginePatch& ep = sl.cp.data.sound.engine;

        // Global mod envelope gate.
        bool gateOn = false;
        for (int i = 0; i < kMaxPolyphony; ++i)
            gateOn = gateOn || (voices[(size_t) i].active && ! voices[(size_t) i].releasing && voices[(size_t) i].fadeInc == 0.0f);
        if (! gateOn) modEnv.gateOff();

        float delta[pc::total];
        if (snap) std::memcpy (rt.eff, rt.basePhys, sizeof (rt.eff));
        advanceModSources (sl, dt);
        evalModulation (sl, delta);

        for (int c = 0; c < pc::engineCount; ++c)
        {
            if (! rt.exists[c]) continue;
            rt.eff[c] = delta[c] == 0.0f ? rt.basePhys[c] : fromNorm (paramDesc (c), rt.baseNorm[c] + delta[c]);
        }
        {
            float target[kNumRamps] {};
            float carrierSum = 0.0f;
            for (int k = 0; k < kNumOps; ++k)
            {
                target[kRampLevel + k] = rt.eff[pc::op (k, kOpLevel)];
                target[kRampCarrier + k] = rt.eff[pc::op (k, kOpCarrierGain)];
                target[kRampFeedback + k] = rt.eff[pc::op (k, kOpFeedback)];
                if (ep.operators[(size_t) k].enabled) carrierSum += target[kRampCarrier + k];
                rt.env[k].set (rt.eff[pc::op (k, kOpEnvA)], rt.eff[pc::op (k, kOpEnvD)], rt.eff[pc::op (k, kOpEnvS)],
                               rt.eff[pc::op (k, kOpEnvR)], isr);
            }
            for (int e = 0; e < ep.numEdges; ++e)
                target[kRampEdge + e] = rt.eff[pc::edge (ep.edges[(size_t) e].from, ep.edges[(size_t) e].to)];
            target[kRampNorm] = 1.0f / std::sqrt (std::max (1.0f, carrierSum));
            const float invLen = 1.0f / (float) (kControlInterval * os);
            for (int r = 0; r < kNumRamps; ++r)
            {
                if (snap) { rt.cur[r] = rt.tgt[r] = target[r]; rt.inc[r] = 0.0f; }
                else
                {
                    rt.tgt[r] += cF * (target[r] - rt.tgt[r]);
                    rt.inc[r] = (rt.tgt[r] - rt.cur[r]) * invLen;
                }
            }
        }

        // Filter and output gains (preset level and pan).
        {
            filter.setTarget (ep.filter.type, rt.eff[pc::filterCutoff], rt.eff[pc::filterRes], rt.eff[pc::filterDrive]);
            const float lvl = clampf (rt.eff[pc::level], 0.0f, 2.0f);
            const float pan = clampf (rt.eff[pc::pan], -1.0f, 1.0f);
            outGainT[0] = lvl * (pan > 0.0f ? 1.0f - pan : 1.0f);
            outGainT[1] = lvl * (pan < 0.0f ? 1.0f + pan : 1.0f);
            if (snap) { outGain[0] = outGainT[0]; outGain[1] = outGainT[1]; }
        }

        // FX parameters (macros, quick controls and mod matrix).
        for (int t = 0; t < kNumFx; ++t)
            for (int p = 0; p < kFxParams; ++p)
            {
                const auto& d = fxParamDesc (t, p);
                if (d.name == nullptr) continue;
                const float dl = delta[pc::fx (t, p)];
                fxEff[t][p] = (dl == 0.0f || (d.flags & kFlagDiscrete)) ? sl.cp.data.fx.proc[(size_t) t].p[(size_t) p]
                                                                         : fromNorm (d, sl.fxBaseNorm[t * kFxParams + p] + dl);
            }
        fx.setParams (sl.cp.data.fx, fxEff, hostNow.fxBypassAll, bpmNow);
        {
            const float dl = delta[pc::portamento];
            portaEff = dl == 0.0f ? sl.cp.data.perf.portamento
                                  : fromNorm (paramDesc (pc::portamento), sl.fxBaseNorm[pc::portamento - pc::fxBase] + dl);
        }

        masterGainT = dbToGain (clampf (hostNow.gainDb, -60.0f, 12.0f) + sl.cp.data.out.levelDb);
        if (hostNow.gainDb <= -59.95f) masterGainT = 0.0f;
        if (snap) masterGain = masterGainT;
    }

    // ---- voice management -------------------------------------------------------------------
    const PerformanceState& perf() const { return slots[(size_t) cur].cp.data.perf; }

    void triggerMod()
    {
        modEnv.gateOn();
        const auto& e = slots[(size_t) cur].cp.data.sound.engine;
        for (int i = 0; i < 2; ++i)
            if (e.lfo[(size_t) i].retrig) lfo[i].retrigger (e.lfo[(size_t) i].phase);
    }

    void startVoice (NoteVoice& v, int note, float vel)
    {
        const PatchSlot& sl = slots[(size_t) cur];
        v.active = true;
        v.held = true;
        v.sustained = v.releasing = false;
        v.fresh = true;
        v.note = note;
        v.vel = vel;
        v.age = ++ageCounter;
        v.slot = cur;
        v.unison = std::min (std::max (hostNow.unison, 1), kMaxUnison);
        v.fade = 1.0f;
        v.fadeInc = 0.0f;
        v.hasPending = v.pendReleased = false;
        v.releaseSamples = 0.0;
        v.pitchTarget = (float) (note - 69);
        v.pitchNow = (portaEff > 0.0f && haveLastPitch) ? lastPitch : v.pitchTarget;
        lastPitch = v.pitchTarget;
        haveLastPitch = true;
        const EnginePatch& ep = sl.cp.data.sound.engine;
        for (int k = 0; k < kNumOps; ++k)
        {
            v.env[k].value = 0.0f;
            v.env[k].gateOn();
            const auto& op = ep.operators[(size_t) k];
            for (int u = 0; u < kMaxUnison; ++u)
            {
                double ph = op.keySync ? (double) op.phaseOffset * kTwoPi : freeRun[k];
                if (u > 0) ph += (double) hash01 ((uint32_t) u, (uint32_t) k) * kTwoPi;
                ph = std::fmod (ph, kTwoPi);
                v.op[u][k] = OpRt {};
                v.op[u][k].phase = ph;
            }
        }
        lastVel = vel;
        lastKey = clampf ((float) (note - 60) / 60.0f, -1.0f, 1.0f);
        triggerMod();
    }

    void releaseVoice (NoteVoice& v)
    {
        v.held = v.sustained = false;
        if (v.releasing) return;
        v.releasing = true;
        for (auto& e : v.env) e.gateOff();
    }

    void fadeVoice (NoteVoice& v, double seconds)
    {
        if (v.fadeInc == 0.0f) v.fadeInc = (float) (-1.0 / (seconds * isr));
        v.held = v.sustained = false;
    }

    void retriggerVoice (NoteVoice& v, int note, float vel, bool legato)
    {
        v.note = note;
        v.pitchTarget = (float) (note - 69);
        if (portaEff <= 0.0f) v.pitchNow = v.pitchTarget;
        lastPitch = v.pitchTarget;
        v.held = true;
        v.sustained = false;
        lastKey = clampf ((float) (note - 60) / 60.0f, -1.0f, 1.0f);
        if (legato && ! v.releasing) return;
        v.vel = vel;
        v.releasing = false;
        v.releaseSamples = 0.0;
        for (auto& e : v.env) e.gateOn();
        lastVel = vel;
        triggerMod();
    }

    NoteVoice* findMonoVoice()
    {
        NoteVoice* best = nullptr;
        for (int i = 0; i < kMaxPolyphony; ++i)
        {
            auto& v = voices[(size_t) i];
            if (v.active && v.fadeInc == 0.0f && (best == nullptr || v.age > best->age)) best = &v;
        }
        return best;
    }

    void noteOn (int note, float vel)
    {
        if (perf().mono)
        {
            for (int i = 0; i < monoCount; ++i)
                if (monoStack[i].note == note) { for (int j = i; j + 1 < monoCount; ++j) monoStack[j] = monoStack[j + 1]; --monoCount; break; }
            if (monoCount == 16) { for (int j = 0; j + 1 < monoCount; ++j) monoStack[j] = monoStack[j + 1]; --monoCount; }
            monoStack[monoCount++] = { note, vel };
            if (NoteVoice* v = findMonoVoice())
            {
                // Any additional voices left over from poly mode are released.
                for (int i = 0; i < kMaxPolyphony; ++i)
                    if (&voices[(size_t) i] != v && voices[(size_t) i].active) releaseVoice (voices[(size_t) i]);
                if (v->slot != cur) { fadeVoice (*v, 0.005); v->hasPending = true; v->pendNote = note; v->pendVel = vel; v->pendReleased = false; return; }
                retriggerVoice (*v, note, vel, perf().legato);
                return;
            }
            // No playable mono voice, but one is fading out with a note queued behind it (the
            // patch was swapped a moment ago): replace the queued note instead of adding a voice.
            for (int i = 0; i < kMaxPolyphony; ++i)
            {
                auto& fading = voices[(size_t) i];
                if (fading.active && fading.hasPending) { fading.pendNote = note; fading.pendVel = vel; fading.pendReleased = false; return; }
            }
        }
        else
        {
            for (int i = 0; i < kMaxPolyphony; ++i)
            {
                auto& v = voices[(size_t) i];
                if (v.active && v.note == note && ! v.releasing && v.fadeInc == 0.0f) releaseVoice (v);
            }
        }

        const int unison = std::min (std::max (hostNow.unison, 1), kMaxUnison);
        const int maxNotes = perf().mono ? 1 : std::min (std::max (hostNow.polyphony, 1), kMaxPolyphony);
        int notes = 0, subs = 0;
        for (int i = 0; i < kMaxPolyphony; ++i)
        {
            const auto& v = voices[(size_t) i];
            if (v.active && v.fadeInc == 0.0f) { ++notes; subs += v.unison; }
        }
        NoteVoice* target = nullptr;
        while (notes >= maxNotes || subs + unison > kMaxSubVoices) // SPEC §5.1 budget
        {
            NoteVoice* victim = nullptr;
            for (int pass = 0; pass < 2 && victim == nullptr; ++pass)
                for (int i = 0; i < kMaxPolyphony; ++i)
                {
                    auto& v = voices[(size_t) i];
                    if (! v.active || v.fadeInc != 0.0f) continue;
                    if (pass == 0 && ! v.releasing) continue;
                    if (victim == nullptr || v.age < victim->age) victim = &v;
                }
            if (victim == nullptr) break;
            fadeVoice (*victim, 0.005);
            --notes;
            subs -= victim->unison;
            if (target == nullptr) target = victim;
        }
        if (target == nullptr)
        {
            for (int i = 0; i < kMaxPolyphony && target == nullptr; ++i)
                if (! voices[(size_t) i].active) { startVoice (voices[(size_t) i], note, vel); return; }
            for (int i = 0; i < kMaxPolyphony && target == nullptr; ++i)
                if (voices[(size_t) i].fadeInc != 0.0f && ! voices[(size_t) i].hasPending) target = &voices[(size_t) i];
            if (target == nullptr) return; // every voice is fading with a queued note: drop
        }
        target->hasPending = true;
        target->pendNote = note;
        target->pendVel = vel;
        target->pendReleased = false;
    }

    void noteOff (int note)
    {
        for (int i = 0; i < kMaxPolyphony; ++i)
            if (voices[(size_t) i].hasPending && voices[(size_t) i].pendNote == note) voices[(size_t) i].pendReleased = true;

        if (perf().mono || monoCount > 0)
        {
            bool wasTop = monoCount > 0 && monoStack[monoCount - 1].note == note;
            for (int i = 0; i < monoCount; ++i)
                if (monoStack[i].note == note) { for (int j = i; j + 1 < monoCount; ++j) monoStack[j] = monoStack[j + 1]; --monoCount; break; }
            if (perf().mono)
            {
                NoteVoice* v = findMonoVoice();
                if (v != nullptr && v->note == note)
                {
                    if (wasTop && monoCount > 0) { retriggerVoice (*v, monoStack[monoCount - 1].note, monoStack[monoCount - 1].vel, perf().legato); return; }
                    if (monoCount > 0) return;
                }
                if (monoCount == 0)
                {
                    // Last key up: nothing may keep sounding. This also catches voices left over
                    // from poly mode (the patch became mono while several keys were down).
                    for (int i = 0; i < kMaxPolyphony; ++i)
                    {
                        auto& held = voices[(size_t) i];
                        if (! held.active || ! held.held || held.fadeInc != 0.0f) continue;
                        if (pedal) { held.held = false; held.sustained = true; }
                        else releaseVoice (held);
                    }
                    return;
                }
                // Other keys are still down and the mono voice plays another note: fall through
                // and release any leftover voice that still holds this one.
            }
        }
        for (int i = 0; i < kMaxPolyphony; ++i)
        {
            auto& v = voices[(size_t) i];
            if (! v.active || v.note != note || ! v.held || v.fadeInc != 0.0f) continue;
            if (pedal) { v.held = false; v.sustained = true; }
            else releaseVoice (v);
        }
    }

    void handleEvent (const MidiEvent& e)
    {
        switch (e.type)
        {
            case MidiEvent::NoteOn:
                if (e.data2 > 0 && e.data1 < 128) { noteOn (e.data1, (float) std::min<int> (e.data2, 127) / 127.0f); break; }
                [[fallthrough]];
            case MidiEvent::NoteOff:
                if (e.data1 < 128) noteOff (e.data1);
                break;
            case MidiEvent::PitchBend: bendTarget = clampf (e.bend, -1.0f, 1.0f); break;
            case MidiEvent::ChannelPressure: atTarget = (float) std::min<int> (e.data1 ? e.data1 : e.data2, 127) / 127.0f; break;
            case MidiEvent::Controller:
                if (e.data1 == 1) wheelTarget = (float) std::min<int> (e.data2, 127) / 127.0f;
                else if (e.data1 == 64)
                {
                    const bool down = e.data2 >= 64;
                    if (pedal && ! down)
                        for (int i = 0; i < kMaxPolyphony; ++i)
                            if (voices[(size_t) i].active && voices[(size_t) i].sustained) releaseVoice (voices[(size_t) i]);
                    pedal = down;
                }
                break;
            case MidiEvent::AllNotesOff:
                monoCount = 0;
                pedal = false;
                for (int i = 0; i < kMaxPolyphony; ++i)
                {
                    voices[(size_t) i].hasPending = false;
                    if (voices[(size_t) i].active) releaseVoice (voices[(size_t) i]);
                }
                break;
            case MidiEvent::AllSoundOff:
                monoCount = 0;
                pedal = false;
                for (int i = 0; i < kMaxPolyphony; ++i)
                {
                    voices[(size_t) i].hasPending = false;
                    if (voices[(size_t) i].active) fadeVoice (voices[(size_t) i], 0.005);
                }
                break;
            default: break;
        }
    }

    // ---- rendering ----------------------------------------------------------------------------
    // Renders the operators of a voice into bus[]. Returns true while any carrier is audible.
    bool renderVoiceOps (NoteVoice& v, PatchSlot& sl, int ilen)
    {
        const CompiledEngine& ce = sl.cp.engine;
        const EnginePatch& ep = sl.cp.data.sound.engine;
        SideRt& rt = sl.rt;
        const float velInv = 1.0f - v.vel;

        bool alive = false;
        for (int a = 0; a < ce.numActive; ++a)
        {
            const int k = ce.order[a];
            EnvState& e = v.env[k];
            const EnvCoefs& c = rt.env[k];
            const float velScale = 1.0f - rt.eff[pc::op (k, kOpVelToLevel)] * velInv;
            float lvl = rt.cur[kRampLevel + k] * velScale;
            const float lvlInc = rt.inc[kRampLevel + k] * velScale;
            float* amp = ampBuf[k];
            for (int i = 0; i < ilen; ++i) { amp[i] = e.tick (c) * lvl; lvl += lvlInc; }
            const bool carrier = rt.cur[kRampCarrier + k] > 1.0e-6f || rt.inc[kRampCarrier + k] != 0.0f;
            if (carrier && e.stage != EnvState::Idle && ! (e.stage == EnvState::Decay && e.value < 2.0e-5f && c.sustain < 2.0e-5f))
                alive = true;
        }

        // Per-sample output gain: steal fade * normalisation.
        {
            float f = v.fade, nrm = rt.cur[kRampNorm];
            const float fInc = v.fadeInc, nInc = rt.inc[kRampNorm];
            const float base = kVoiceGain / std::sqrt ((float) v.unison);
            for (int i = 0; i < ilen; ++i)
            {
                gainBuf[i] = (f > 0.0f ? f : 0.0f) * nrm * base;
                f += fInc; nrm += nInc;
            }
        }

        const bool retarget = tickStart || v.fresh;
        const float bendSemis = bendSm * ep.pitchBendRange;
        const float noteSemis = v.pitchNow + bendSemis;
        const float pitchMod = rt.eff[pc::pitch];
        // Frequency glides are scheduled once per control tick so the result is block-size independent.
        const double tickSmooth = 1.0 - std::exp (-(double) kControlInterval / (0.004 * sr));
        const double maxInc = kTwoPi * 0.45;
        float* bl = bus[0].data();
        float* br = bus[1].data();

        for (int u = 0; u < v.unison; ++u)
        {
            const float pos = v.unison > 1 ? 2.0f * (float) u / (float) (v.unison - 1) - 1.0f : 0.0f;
            const float uniCents = detuneSm * pos;
            const float ang = (pos * spreadSm * 0.01f + 1.0f) * 0.78539816f;
            const float panL = std::cos (ang) * 1.41421356f, panR = std::sin (ang) * 1.41421356f;
            bool mixStarted = false;

            for (int a = 0; a < ce.numActive; ++a)
            {
                const int k = ce.order[a];
                const auto& opS = ep.operators[(size_t) k];
                OpRt& o = v.op[u][k];

                if (retarget)
                {
                    const float kt = rt.eff[pc::op (k, kOpKeyTracking)];
                    const float semis = noteSemis * kt + (rt.eff[pc::op (k, kOpDetune)] + uniCents) * 0.01f + pitchMod;
                    const double baseHz = opS.pitchMode == PitchMode::Ratio ? 440.0 * rt.eff[pc::op (k, kOpRatio)] : (double) rt.eff[pc::op (k, kOpFixedHz)];
                    const double hz = baseHz * std::exp2 ((double) semis / 12.0);
                    const double targetInc = std::min (kTwoPi * hz / isr, maxInc);
                    if (v.fresh) { o.inc = targetInc; o.dInc = 0.0; }
                    else o.dInc = tickSmooth * (targetInc - o.inc) / (double) (kControlInterval * os);
                    int size = 0;
                    o.tbl = tables->select (opS.waveform, hz, isr, size);
                    o.mask = size - 1;
                    o.scale = (float) size / kTwoPiF;
                }
                const double dInc = o.dInc;
                const float* tbl = o.tbl;
                const int mask = o.mask;
                const float scale = o.scale;

                // Phase-modulation input from earlier operators (already rendered: DAG order).
                const int nIn = ce.inCount[k];
                for (int j = 0; j < nIn; ++j)
                {
                    const int from = ce.inFrom[k][j], edge = ce.inEdge[k][j];
                    const float vs = 1.0f - rt.eff[pc::op (from, kOpVelToIndex)] * velInv;
                    float idx = rt.cur[kRampEdge + edge] * vs;
                    const float idxInc = rt.inc[kRampEdge + edge] * vs;
                    const float* src = outBuf[from];
                    if (j == 0) for (int i = 0; i < ilen; ++i) { modBuf[i] = idx * src[i]; idx += idxInc; }
                    else for (int i = 0; i < ilen; ++i) { modBuf[i] += idx * src[i]; idx += idxInc; }
                }

                float fb = rt.cur[kRampFeedback + k];
                const float fbInc = rt.inc[kRampFeedback + k];
                const bool hasFb = fb > 0.0f || fbInc != 0.0f;
                double ph = o.phase, inc = o.inc;
                float p1 = o.p1, p2 = o.p2;
                const float* amp = ampBuf[k];
                float* out = outBuf[k];
                if (hasFb)
                {
                    for (int i = 0; i < ilen; ++i)
                    {
                        // Self feedback uses previous samples only (mean of n-1, n-2 suppresses hunting).
                        float mod = fb * 0.5f * (p1 + p2);
                        if (nIn > 0) mod += modBuf[i];
                        const float y = amp[i] * tableLookup (tbl, mask, ((float) ph + mod) * scale);
                        p2 = p1; p1 = y; out[i] = y;
                        fb += fbInc; ph += inc; inc += dInc;
                        if (ph >= kTwoPi) ph -= kTwoPi;
                    }
                }
                else if (nIn > 0)
                {
                    for (int i = 0; i < ilen; ++i)
                    {
                        out[i] = amp[i] * tableLookup (tbl, mask, ((float) ph + modBuf[i]) * scale);
                        ph += inc; inc += dInc;
                        if (ph >= kTwoPi) ph -= kTwoPi;
                    }
                    p1 = p2 = out[ilen - 1];
                }
                else
                {
                    for (int i = 0; i < ilen; ++i)
                    {
                        out[i] = amp[i] * tableLookup (tbl, mask, (float) ph * scale);
                        ph += inc; inc += dInc;
                        if (ph >= kTwoPi) ph -= kTwoPi;
                    }
                    p1 = p2 = out[ilen - 1];
                }
                o.phase = ph;
                o.inc = inc;
                o.p1 = sanitize (p1);
                o.p2 = sanitize (p2);

                float cg = rt.cur[kRampCarrier + k];
                const float cgInc = rt.inc[kRampCarrier + k];
                if (cg > 0.0f || cgInc != 0.0f)
                {
                    if (! mixStarted) { for (int i = 0; i < ilen; ++i) { mixBuf[i] = cg * out[i]; cg += cgInc; } mixStarted = true; }
                    else for (int i = 0; i < ilen; ++i) { mixBuf[i] += cg * out[i]; cg += cgInc; }
                }
            }
            if (mixStarted)
                for (int i = 0; i < ilen; ++i)
                {
                    const float x = mixBuf[i] * gainBuf[i];
                    bl[i] += x * panL;
                    br[i] += x * panR;
                }
        }
        return alive;
    }

    void renderVoice (NoteVoice& v, int ilen)
    {
        PatchSlot& sl = slots[(size_t) v.slot];
        const float porta = portaEff;
        if (porta <= 0.0f) v.pitchNow = v.pitchTarget;
        else if (tickStart && ! v.fresh)
            v.pitchNow += (float) (1.0 - std::exp (-(double) kControlInterval / (porta * 0.25 * sr))) * (v.pitchTarget - v.pitchNow);

        const bool alive = renderVoiceOps (v, sl, ilen);
        v.fresh = false;
        v.fade += v.fadeInc * (float) ilen;
        if (v.releasing) v.releaseSamples += ilen;

        bool finished = v.fade <= 0.0f || ! alive || (v.releasing && v.releaseSamples > 41.0 * isr);
        if (v.fadeInc != 0.0f && v.fade > 0.0f) finished = false; // let fades complete
        if (! finished) return;

        const bool pending = v.hasPending, pendRel = v.pendReleased;
        const int pn = v.pendNote;
        const float pv = v.pendVel;
        v.active = false;
        v.hasPending = false;
        v.fadeInc = 0.0f;
        if (pending)
        {
            startVoice (v, pn, pv);
            if (pendRel) { if (pedal) { v.held = false; v.sustained = true; } else releaseVoice (v); }
        }
    }

    void renderChunk (float* outL, float* outR, int len, double ppq, double beatsPerSample)
    {
        const int ilen = len * os;
        int activeNotes = 0;
        for (int i = 0; i < kMaxPolyphony; ++i) activeNotes += voices[(size_t) i].active ? 1 : 0;

        float* host[2] = { hostBuf[0], hostBuf[1] };
        const bool voiceWork = activeNotes > 0 || silentBlocks < 4;
        if (voiceWork)
        {
            for (int ch = 0; ch < 2; ++ch) std::memset (bus[ch].data(), 0, sizeof (float) * (size_t) ilen);
            for (int i = 0; i < kMaxPolyphony; ++i)
                if (voices[(size_t) i].active) renderVoice (voices[(size_t) i], ilen);
            silentBlocks = activeNotes > 0 ? 0 : silentBlocks + 1;
            tickStart = false;

            for (int ch = 0; ch < 2; ++ch)
            {
                const float* in = bus[ch].data();
                float* out = host[ch];
                if (os == 1) std::memcpy (out, in, sizeof (float) * (size_t) len);
                else if (os == 2) for (int i = 0; i < len; ++i) out[i] = dec2[ch].decimate (in[2 * i], in[2 * i + 1]);
                else
                    for (int i = 0; i < len; ++i)
                    {
                        const float x0 = dec1[ch].decimate (in[4 * i], in[4 * i + 1]);
                        const float x1 = dec1[ch].decimate (in[4 * i + 2], in[4 * i + 3]);
                        out[i] = dec2[ch].decimate (x0, x1);
                    }
            }
            const float dcR = (float) (1.0 - kTwoPi * kDcBlockHz / sr);
            for (int ch = 0; ch < 2; ++ch) dcBlock[ch].process (host[ch], len, dcR);
            filter.process (host[0], host[1], len);
        }
        else
        {
            std::memset (host[0], 0, sizeof (float) * (size_t) len);
            std::memset (host[1], 0, sizeof (float) * (size_t) len);
        }

        // Advance shared ramps and free-running phases.
        {
            PatchSlot& sl = slots[(size_t) cur];
            for (int r = 0; r < kNumRamps; ++r) sl.rt.cur[r] += sl.rt.inc[r] * (float) ilen;
            const EnginePatch& ep = sl.cp.data.sound.engine;
            for (int k = 0; k < kNumOps; ++k)
            {
                const auto& op = ep.operators[(size_t) k];
                const double hz = op.pitchMode == PitchMode::Ratio ? 440.0 * op.ratio : (double) op.fixedHz;
                freeRun[k] = std::fmod (freeRun[k] + kTwoPi * hz * (double) len / sr, kTwoPi);
            }
        }

        // Preset level / pan (both can be modulated).
        const float cg = onePoleCoef (0.005, sr);
        for (int i = 0; i < len; ++i)
        {
            outGain[0] += cg * (outGainT[0] - outGain[0]);
            outGain[1] += cg * (outGainT[1] - outGain[1]);
            outL[i] = host[0][i] * outGain[0];
            outR[i] = host[1][i] * outGain[1];
        }

        gate.process (outL, outR, len, perf().gate, hostNow.gateEnabled, ppq, beatsPerSample);
        fx.process (outL, outR, len);

        const float hp = clampf (hostNow.pan, -1.0f, 1.0f);
        const float pl = hp > 0.0f ? 1.0f - hp : 1.0f, pr = hp < 0.0f ? 1.0f + hp : 1.0f;
        for (int i = 0; i < len; ++i)
        {
            masterGain += cg * (masterGainT - masterGain);
            outL[i] = softLimit (sanitize (outL[i] * masterGain * pl));
            outR[i] = softLimit (sanitize (outR[i] * masterGain * pr));
        }
    }

    // Transparent below +/-0.9, smooth knee to a hard ceiling of 1.0.
    static inline float softLimit (float x)
    {
        const float ax = std::fabs (x);
        if (ax <= 0.9f) return x;
        const float over = (ax - 0.9f) * 10.0f;
        const float y = 0.9f + 0.1f * (over / (1.0f + over));
        return x < 0.0f ? -y : y;
    }
};

// =========================================================================================
SynthEngine::SynthEngine() : impl (std::make_unique<Impl> (*this))
{
    impl->mail.reset (new CompiledPatch[kMailSlots]);
    impl->slots.reset (new PatchSlot[kPatchSlots]);
    impl->voices.reset (new NoteVoice[kMaxPolyphony]);
    CompiledPatch init;
    compilePatch (makeInitPatchData(), init);
    for (int i = 0; i < kPatchSlots; ++i) impl->slots[(size_t) i].cp = init;
    impl->cacheBase (impl->slots[0]);
    prepare (48000.0, 512);
}

SynthEngine::~SynthEngine() = default;

int SynthEngine::latencyForQuality (int quality)
{
    const int osLatency = quality <= 0 ? 0 : (quality == 1 ? kDecimK2 : (kDecimK1 + 2 * kDecimK2) / 2);
    return osLatency + FxChain::kLatencySamples;
}

int SynthEngine::effectiveMaxNotes (int polyphony, int unison)
{
    polyphony = std::min (std::max (polyphony, 1), kMaxPolyphony);
    unison = std::min (std::max (unison, 1), kMaxUnison);
    return std::max (1, std::min (polyphony, kMaxSubVoices / unison));
}

void SynthEngine::prepare (double sampleRate, int maxBlockSize)
{
    auto& d = *impl;
    d.tables = &Tables::get();
    d.sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    d.maxBlock = std::max (maxBlockSize, 1);
    for (auto& ch : d.bus) ch.assign ((size_t) kMaxChunk, 0.0f);
    d.filter.prepare (d.sr);
    d.gate.prepare (d.sr);
    d.fx.prepare (d.sr);
    d.performer.prepare (d.sr);
    d.setOversampling (activeQuality.load());
    d.prepared = true;
    d.resetState();
}

void SynthEngine::reset() { impl->resetState(); }

bool SynthEngine::setPatch (const PatchData& data, bool forceNewGeneration, std::string* error)
{
    auto& d = *impl;
    std::lock_guard<std::mutex> lock (d.writerMutex);
    if (! compilePatch (data, d.writerTmp, error)) return false;

    const uint32_t consumed = consumedGen.load (std::memory_order_acquire);
    const bool force = forceNewGeneration || d.lastForcedGen > consumed;
    d.writerTmp.forceNewGeneration = force;

    const int latestSlot = (int) (d.latest.load (std::memory_order_acquire) & 0xffu);
    int slot = -1;
    for (int pass = 0; pass < 3 && slot < 0; ++pass)
        for (int i = 0; i < kMailSlots && slot < 0; ++i)
        {
            if (pass == 1 && i == latestSlot) continue;
            int expected = pass == 0 ? 0 : 2;
            if (d.mailState[i].compare_exchange_strong (expected, 1, std::memory_order_acq_rel)) slot = i;
        }
    if (slot < 0) return false; // cannot happen: the reader holds at most one slot
    d.mail[(size_t) slot] = d.writerTmp;
    const uint32_t gen = ++d.writerGen;
    if (force) d.lastForcedGen = gen;
    d.mailState[slot].store (2, std::memory_order_release);
    d.latest.store (((uint64_t) gen << 8) | (uint64_t) slot, std::memory_order_release);
    return true;
}

void SynthEngine::process (float* left, float* right, int n, const MidiEvent* events, int numEvents,
                           const HostParams& host, const TransportInfo& transport)
{
    auto& d = *impl;
    if (n <= 0)
    {
        for (int i = 0; i < numEvents && d.numCarry < 64; ++i) { d.carry[d.numCarry] = events[i]; d.carry[d.numCarry++].offset = 0; }
        return;
    }

    d.pollMailbox();
    d.applyIncoming();
    d.hostNow = host;

    // Quality switch: fade everything, then rebuild at the next silent block boundary.
    const int wantQ = std::min (std::max (host.quality, 0), 2);
    if (wantQ != activeQuality.load (std::memory_order_relaxed))
    {
        bool any = false;
        for (int i = 0; i < kMaxPolyphony; ++i)
            if (d.voices[(size_t) i].active) { any = true; d.voices[(size_t) i].hasPending = false; d.fadeVoice (d.voices[(size_t) i], 0.005); }
        if (! any) d.setOversampling (wantQ);
    }

    // Musical clock: host position while playing, otherwise a free-running internal clock.
    const bool hostPlaying = transport.valid && transport.playing;
    d.bpmNow = transport.valid && transport.bpm > 1.0 ? std::min (transport.bpm, 999.0) : 120.0;
    const double bps = d.bpmNow / 60.0 / d.sr;
    double ppq = hostPlaying ? transport.ppq : d.freePpq;
    const bool jumped = hostPlaying && d.wasPlaying && std::fabs (ppq - d.expectedPpq) > 1.0e-3;
    const bool started = hostPlaying && ! d.wasPlaying;
    const bool stopped = ! hostPlaying && d.wasPlaying;

    Performer::Clock clock { ppq, bps, hostPlaying, jumped || started, stopped };
    const MidiEvent* in = events;
    int numIn = numEvents;
    if (d.numCarry > 0)
    {
        int k = 0;
        for (; k < d.numCarry; ++k) d.inBuf[k] = d.carry[k];
        for (int i = 0; i < numEvents && k < kMaxSeqEvents; ++i) d.inBuf[k++] = events[i];
        d.numCarry = 0;
        in = d.inBuf;
        numIn = k;
    }
    const int total = d.performer.process (in, numIn, n, d.perf(), host.arpEnabled, clock, d.seqEvents, kMaxSeqEvents);

    int pos = 0, ev = 0;
    float peakL = 0.0f, peakR = 0.0f;
    while (pos < n)
    {
        // Control tick first, so events at this position (e.g. a note-on in the very first block)
        // already see the current patch's effective parameters.
        const int phase = (int) (d.sampleCounter % (uint64_t) kControlInterval);
        if (phase == 0 || d.snapNext) d.controlTick();
        while (ev < total && d.seqEvents[ev].offset <= pos) d.handleEvent (d.seqEvents[ev++]);
        int end = n;
        if (ev < total) end = std::min (end, std::max (d.seqEvents[ev].offset, pos + 1));
        end = std::min (end, pos + kControlInterval - phase);
        const int len = end - pos;
        d.renderChunk (left + pos, right + pos, len, ppq + pos * bps, bps);
        for (int i = pos; i < end; ++i) { peakL = std::max (peakL, std::fabs (left[i])); peakR = std::max (peakR, std::fabs (right[i])); }
        pos = end;
        d.sampleCounter += (uint64_t) len;
    }
    while (ev < total) d.handleEvent (d.seqEvents[ev++]);

    d.expectedPpq = ppq + n * bps;
    d.freePpq = d.expectedPpq;
    d.wasPlaying = hostPlaying;

    int notes = 0, subs = 0;
    for (int i = 0; i < kMaxPolyphony; ++i)
        if (d.voices[(size_t) i].active) { ++notes; subs += d.voices[(size_t) i].unison; }
    meterNotes.store (notes, std::memory_order_relaxed);
    meterSubs.store (subs, std::memory_order_relaxed);
    meterPeak[0].store (peakL, std::memory_order_relaxed);
    meterPeak[1].store (peakR, std::memory_order_relaxed);
}
} // namespace fmlume
