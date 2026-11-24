// Fluent builder used to describe the factory presets compactly. Operator numbers are 1-based.
#pragma once
#include "Core/Params.h"
#include "Core/PatchCompiler.h"
#include "Core/Templates.h"
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace gen
{
using namespace fmlume;
using Map = std::initializer_list<std::pair<int, float>>;

inline int E (int from, int to) { return pc::edge (from - 1, to - 1); }
inline int O (int op, int field) { return pc::op (op - 1, field); }
inline int FX (FxType t, int p) { return pc::fx (t, p); }
inline int DEPTH (int slot) { return pc::matrixDepthBase + slot; }

struct B
{
    Preset p;
    explicit B (Category c) : p (makeCategoryPreset (c)) {}
    PatchData& d() { return p.data; }
    EnginePatch& e() { return p.data.sound.engine; }
    OperatorState& o (int k) { return e().operators[(size_t) (k - 1)]; }

    B& name (const char* n) { p.meta.displayName = n; return *this; }
    B& desc (const char* aim, const char* velocity) { p.meta.description = std::string (aim) + " Velocity: " + velocity; return *this; }
    B& tags (std::initializer_list<const char*> t) { p.meta.tags.assign (t.begin(), t.end()); return *this; }

    // ---- operators / routing
    B& clear() { for (auto& op : e().operators) op = OperatorState {}; e().numEdges = 0; for (auto& m : d().sound.matrix) m = ModSlot {}; for (int i = 4; i < 8; ++i) d().sound.macros[(size_t) i] = Macro {}; return *this; }
    B& op (int k, float ratio, float level, float a, float dcy, float s, float r, float carrier = 0.0f) { o (k) = makeOp (ratio, level, a, dcy, s, r, carrier); return *this; }
    B& ratio (int k, float v) { o (k).ratio = v; return *this; }
    B& level (int k, float v) { o (k).level = v; return *this; }
    B& out (int k, float v) { o (k).carrierGain = v; return *this; }
    B& env (int k, float a, float dcy, float s, float r) { o (k).env = { a, dcy, s, r }; return *this; }
    B& wave (int k, Waveform w) { o (k).waveform = w; return *this; }
    B& fb (int k, float v) { o (k).feedback = v; return *this; }
    B& det (int k, float cents) { o (k).detuneCents = cents; return *this; }
    B& vel (int k, float toLevel, float toIndex) { o (k).velocityToLevel = toLevel; o (k).velocityToIndex = toIndex; return *this; }
    B& fixed (int k, float hz) { o (k).pitchMode = PitchMode::Fixed; o (k).fixedHz = hz; o (k).keyTracking = 0.0f; return *this; }
    B& off (int k) { o (k).enabled = false; o (k).carrierGain = 0.0f; return *this; }
    B& edge (int from, int to, float index)
    {
        const int i = findEdge (e(), from - 1, to - 1);
        if (i >= 0) e().edges[(size_t) i].index = index; else addEdge (e(), from - 1, to - 1, index);
        return *this;
    }
    B& cut (int from, int to) { removeEdge (e(), from - 1, to - 1); return *this; }

    // ---- filter / modulation
    B& filter (FilterType t, float cutoff, float res = 0.1f, float drive = 0.0f) { auto& f = e().filter; f.type = t; f.cutoffHz = cutoff; f.resonance = res; f.drive = drive; return *this; }
    B& lfo (int i, float rate, LfoShape shape = LfoShape::Sine, bool retrig = false) { auto& l = e().lfo[(size_t) (i - 1)]; l.rateHz = rate; l.shape = shape; l.retrig = retrig; l.sync = false; return *this; }
    B& lfoSync (int i, int div, LfoShape shape = LfoShape::Sine) { auto& l = e().lfo[(size_t) (i - 1)]; l.sync = true; l.syncDiv = (uint8_t) div; l.shape = shape; l.retrig = true; return *this; }
    B& modEnv (float a, float dcy, float s, float r) { e().modEnv = { a, dcy, s, r }; return *this; }
    B& mod (int slot, ModSource src, int target, float depth) { setModSlot (d().sound, slot, src, target, depth); return *this; }
    B& bend (float semis) { e().pitchBendRange = semis; return *this; }

    // ---- macros 5..8 (category macros). u = unipolar "add from zero" mappings.
    B& m (int macro, Map maps) { auto& mc = d().sound.macros[(size_t) (macro - 1)]; mc = Macro {}; for (auto& kv : maps) addMapping (mc, kv.first, kv.second); return *this; }
    B& mu (int macro, Map maps) { auto& mc = d().sound.macros[(size_t) (macro - 1)]; for (auto& kv : maps) addMapping (mc, kv.first, kv.second, Curve::Linear, Polarity::Unipolar, 0.5f, 1.0f); return *this; }
    B& mname (int macro, const char* n) { p.meta.macroNames[(size_t) (macro - 1)] = n; return *this; }

    // ---- performance
    B& uni (int count, float detune, float spread) { d().host.unison = count; d().host.detune = detune; d().host.spread = spread; return *this; }
    B& poly (int n) { d().host.polyphony = n; d().perf.mono = false; d().perf.legato = false; return *this; }
    B& mono (bool legato, float porta) { d().perf.mono = true; d().perf.legato = legato; d().perf.portamento = porta; return *this; }
    B& arp (ArpMode mode, int rate, float gate, int octaves) { d().host.arpEnabled = true; d().perf.arp.mode = mode; d().perf.arp.rate = (uint8_t) rate; d().perf.arp.gate = gate; d().perf.arp.octaves = octaves; return *this; }
    B& arpOct (ArpOctaveMode m) { d().perf.arp.octaveMode = m; return *this; }
    B& swing (float v) { d().perf.arp.swing = v; return *this; }
    // One key plays a chord; together with the arpeggiator one key plays a whole arpeggio.
    B& chord (ChordType t, Voicing v = Voicing::Root, int octave = 0, float strumMs = 0.0f) { auto& c = d().perf.chord; c.type = t; c.voicing = v; c.octave = octave; c.strumMs = strumMs; return *this; }
    B& gate (int rate, std::initializer_list<float> steps) { d().host.gateEnabled = true; d().perf.gate.rate = (uint8_t) rate; size_t i = 0; for (float s : steps) if (i < kGateSteps) d().perf.gate.steps[i++] = s; d().perf.gate.stepsRight = d().perf.gate.steps; return *this; }

    // ---- effects
    FxProcessorState& fx (FxType t) { return d().fx.proc[(size_t) t]; }
    B& fxOn (FxType t, std::initializer_list<std::pair<int, float>> params = {}) { fx (t).enabled = true; for (auto& kv : params) fx (t).p[(size_t) kv.first] = kv.second; return *this; }
    B& fxOff (FxType t) { fx (t).enabled = false; return *this; }
    B& chorus (float mix, float depth = 0.4f, float rate = 0.6f) { return fxOn (FxType::Chorus, { { 3, mix }, { 1, depth }, { 0, rate } }); }
    B& delay (float wet, float ms, float feedback = 0.35f, float stereo = 0.5f) { return fxOn (FxType::Delay, { { 5, wet }, { 0, ms }, { 1, 0.0f }, { 3, feedback }, { 4, stereo } }); }
    B& delaySync (float wet, int div, float feedback = 0.4f, float stereo = 0.8f) { return fxOn (FxType::Delay, { { 5, wet }, { 1, 1.0f }, { 2, (float) div }, { 3, feedback }, { 4, stereo } }); }
    B& reverb (float wet, float decay = 2.5f, float size = 0.6f, float damping = 0.5f) { return fxOn (FxType::Reverb, { { 4, wet }, { 1, decay }, { 0, size }, { 3, damping } }); }
    B& drive (float gain, float tone = 0.6f) { return fxOn (FxType::Drive, { { 0, gain }, { 1, tone }, { 2, 1.0f } }); }
    B& eq (float lowDb, float midDb, float highDb, float midHz = 1000.0f) { return fxOn (FxType::EQ, { { 0, lowDb }, { 2, midDb }, { 5, highDb }, { 3, midHz } }); }
    B& phaser (float wet, float rate = 0.4f, float depth = 0.6f, float feedback = 0.3f) { return fxOn (FxType::Phaser, { { 3, wet }, { 0, rate }, { 1, depth }, { 2, feedback } }); }
    B& flanger (float wet, float rate = 0.25f, float depth = 0.6f, float feedback = 0.4f) { return fxOn (FxType::Flanger, { { 3, wet }, { 0, rate }, { 1, depth }, { 2, feedback } }); }
    B& comp (float threshold, float ratio, float makeup) { return fxOn (FxType::Compressor, { { 0, threshold }, { 1, ratio }, { 4, makeup } }); }
    B& crush (float bits, float downsample, float wet) { return fxOn (FxType::Bitcrusher, { { 0, bits }, { 1, downsample }, { 2, wet } }); }
};
} // namespace gen
