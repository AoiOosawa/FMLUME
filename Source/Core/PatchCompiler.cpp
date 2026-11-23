#include "Core/PatchCompiler.h"
#include "Core/Params.h"
#include <algorithm>
#include <cmath>

namespace fmlume
{
namespace
{
bool topoSort (const EnginePatch& e, int order[kNumOps])
{
    int indeg[kNumOps] {};
    for (int i = 0; i < e.numEdges; ++i) ++indeg[e.edges[(size_t) i].to];
    bool done[kNumOps] {};
    int n = 0;
    while (n < kNumOps)
    {
        int pick = -1;
        for (int k = 0; k < kNumOps; ++k)
            if (! done[k] && indeg[k] == 0) { pick = k; break; }
        if (pick < 0) return false; // cycle
        done[pick] = true;
        order[n++] = pick;
        for (int i = 0; i < e.numEdges; ++i)
            if (e.edges[(size_t) i].from == pick) --indeg[e.edges[(size_t) i].to];
    }
    return true;
}

uint64_t fnv (uint64_t h, uint64_t v)
{
    for (int i = 0; i < 8; ++i) { h ^= (v >> (i * 8)) & 0xffu; h *= 1099511628211ULL; }
    return h;
}

uint64_t engineHash (const EnginePatch& e)
{
    uint64_t h = 1469598103934665603ULL;
    for (auto& op : e.operators)
    {
        h = fnv (h, op.enabled ? 1u : 0u);
        h = fnv (h, (uint64_t) op.waveform);
    }
    bool adj[kNumOps][kNumOps] {};
    for (int i = 0; i < e.numEdges; ++i) adj[e.edges[(size_t) i].from][e.edges[(size_t) i].to] = true;
    for (int a = 0; a < kNumOps; ++a)
        for (int b = 0; b < kNumOps; ++b) h = fnv (h, adj[a][b] ? 1u : 0u);
    return h;
}

void compileEngine (const EnginePatch& e, CompiledEngine& c)
{
    c = CompiledEngine {};
    int order[kNumOps];
    topoSort (e, order);
    for (int k = 0; k < kNumOps; ++k)
        if (e.operators[(size_t) order[k]].enabled) c.order[c.numActive++] = order[k];
    for (int i = 0; i < e.numEdges; ++i)
    {
        const auto& ed = e.edges[(size_t) i];
        if (! e.operators[(size_t) ed.from].enabled || ! e.operators[(size_t) ed.to].enabled) continue;
        int& n = c.inCount[ed.to];
        c.inFrom[ed.to][n] = ed.from;
        c.inEdge[ed.to][n] = (int8_t) i;
        ++n;
    }
    c.structureHash = engineHash (e);
}

bool finite (float v) { return std::isfinite (v); }

bool inRange (float v, float lo, float hi) { return finite (v) && v >= lo - 1.0e-6f && v <= hi + 1.0e-6f; }

std::string validateEngine (const EnginePatch& e, const char* side)
{
    const std::string s = side;
    if (e.numEdges < 0 || e.numEdges > kMaxEdges) return s + ": edge count out of range";
    PatchData tmp;
    tmp.a.engine = e;
    for (int i = 0; i < kNumOps; ++i)
    {
        const auto& op = e.operators[(size_t) i];
        const std::string o = s + ".operators[" + std::to_string (i) + "]";
        if ((int) op.waveform >= (int) Waveform::Count) return o + ": bad waveform";
        if ((int) op.pitchMode >= (int) PitchMode::Count) return o + ": bad pitchMode";
        for (int f = 0; f < kOpFieldCount; ++f)
        {
            const auto& d = paramDesc (pc::op (i, f));
            const float v = getParam (tmp, false, pc::op (i, f));
            if (! inRange (v, d.min, d.max)) return o + "." + d.name + ": out of range";
        }
    }
    for (int i = 0; i < e.numEdges; ++i)
    {
        const auto& ed = e.edges[(size_t) i];
        const std::string o = s + ".edges[" + std::to_string (i) + "]";
        if (ed.from < 0 || ed.from >= kNumOps || ed.to < 0 || ed.to >= kNumOps) return o + ": operator index out of range";
        if (ed.from == ed.to) return o + ": self edge (use operator feedback instead)";
        if (! inRange (ed.index, 0.0f, 16.0f)) return o + ": index out of range";
        for (int j = 0; j < i; ++j)
            if (e.edges[(size_t) j].from == ed.from && e.edges[(size_t) j].to == ed.to) return o + ": duplicate edge";
    }
    if (hasCycle (e)) return s + ": operator graph contains a cycle";
    bool carrier = false;
    for (auto& op : e.operators) carrier = carrier || (op.enabled && op.carrierGain > 0.0f && op.level > 0.0f);
    if (! carrier) return s + ": no audible carrier (need an enabled operator with carrierGain > 0 and level > 0)";
    if ((int) e.filter.type >= (int) FilterType::Count) return s + ".filter: bad type";
    if (! inRange (e.filter.cutoffHz, 20.0f, 20000.0f)) return s + ".filter.cutoff: out of range";
    if (! inRange (e.filter.resonance, 0.0f, 0.95f)) return s + ".filter.resonance: out of range";
    if (! inRange (e.filter.drive, 0.0f, 1.0f)) return s + ".filter.drive: out of range";
    for (int i = 0; i < 2; ++i)
    {
        const auto& l = e.lfo[(size_t) i];
        if ((int) l.shape >= (int) LfoShape::Count) return s + ".lfo: bad shape";
        if (! inRange (l.rateHz, 0.02f, 20.0f)) return s + ".lfo.rate: out of range";
        if (l.syncDiv >= kNumSyncDivs) return s + ".lfo.division: out of range";
        if (! inRange (l.phase, 0.0f, 1.0f)) return s + ".lfo.phase: out of range";
    }
    if (! inRange (e.modEnv.a, 0.001f, 20.0f) || ! inRange (e.modEnv.d, 0.001f, 30.0f)
        || ! inRange (e.modEnv.s, 0.0f, 1.0f) || ! inRange (e.modEnv.r, 0.001f, 40.0f)) return s + ".modEnv: out of range";
    if (! inRange (e.pitchBendRange, 1.0f, 24.0f)) return s + ".pitchBendRange: out of range";
    return {};
}

std::string validateMacro (const Macro& m, const std::string& where)
{
    if (! inRange (m.defaultValue, 0.0f, 1.0f)) return where + ".defaultValue: out of range";
    if (m.numMappings < 0 || m.numMappings > kMaxMacroMappings) return where + ": too many mappings";
    for (int i = 0; i < m.numMappings; ++i)
    {
        const auto& mp = m.mappings[(size_t) i];
        if (! isMacroTarget (mp.target)) return where + ".mappings[" + std::to_string (i) + "]: target is not a continuous parameter";
        if (! inRange (mp.amount, -1.0f, 1.0f)) return where + ": amount out of range";
        if (! inRange (mp.min, 0.0f, 1.0f) || ! inRange (mp.max, 0.0f, 1.0f) || mp.min > mp.max) return where + ": min/max out of range";
        if ((int) mp.curve >= (int) Curve::Count || (int) mp.polarity >= (int) Polarity::Count) return where + ": bad curve/polarity";
    }
    return {};
}

std::string validateSide (const EngineSide& s, const char* name)
{
    if (auto e = validateEngine (s.engine, name); ! e.empty()) return e;
    for (int i = 0; i < kNumMacros; ++i)
        if (auto e = validateMacro (s.macros[(size_t) i], std::string (name) + ".macros[" + std::to_string (i) + "]"); ! e.empty()) return e;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const auto& m = s.matrix[(size_t) i];
        const std::string w = std::string (name) + ".matrix[" + std::to_string (i) + "]";
        if ((int) m.source >= (int) ModSource::Count) return w + ": bad source";
        if (! inRange (m.depth, -1.0f, 1.0f)) return w + ": depth out of range";
        if ((int) m.curve >= (int) Curve::Count) return w + ": bad curve";
        if (m.target >= 0 && ! isModTarget (m.target)) return w + ": target is not a modulation target";
        if (m.target >= pc::total) return w + ": bad target";
    }
    return {};
}
} // namespace

bool hasCycle (const EnginePatch& e)
{
    int order[kNumOps];
    return ! topoSort (e, order);
}

bool edgeWouldBeInvalid (const EnginePatch& e, int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= kNumOps || to >= kNumOps) return true;
    if (findEdge (e, from, to) >= 0 || e.numEdges >= kMaxEdges) return true;
    EnginePatch t = e;
    t.edges[(size_t) t.numEdges++] = { (int8_t) from, (int8_t) to, 0.0f };
    return hasCycle (t);
}

ValidationResult validatePatch (const PatchData& d)
{
    ValidationResult r;
    auto fail = [&r] (std::string msg) { r.ok = false; r.error = std::move (msg); return r; };
    if (auto e = validateSide (d.a, "engineA"); ! e.empty()) return fail (e);
    if (d.hasB)
        if (auto e = validateSide (d.b, "engineB"); ! e.empty()) return fail (e);
    if ((int) d.morph.mode >= (int) MorphMode::Count) return fail ("morph.mode: bad value");

    const auto& p = d.perf;
    if (! inRange (p.portamento, 0.0f, 2.0f)) return fail ("performance.portamento: out of range");
    if ((int) p.arp.mode >= (int) ArpMode::Count) return fail ("performance.arp.mode: bad value");
    if ((int) p.arp.octaveMode >= (int) ArpOctaveMode::Count) return fail ("performance.arp.octaveMode: bad value");
    if (p.arp.rate < kRateFirst || p.arp.rate > kArpRateLast) return fail ("performance.arp.rate: must be 1/4..1/32");
    if (! inRange (p.arp.gate, 0.05f, 1.0f) || p.arp.octaves < 1 || p.arp.octaves > 4 || ! inRange (p.arp.swing, 0.0f, 0.6f))
        return fail ("performance.arp: out of range");
    if ((int) p.chord.type >= (int) ChordType::Count || (int) p.chord.voicing >= (int) Voicing::Count
        || p.chord.octave < -2 || p.chord.octave > 2 || ! inRange (p.chord.strumMs, 0.0f, 100.0f))
        return fail ("performance.chord: out of range");
    for (float s : p.gate.steps) if (! inRange (s, 0.0f, 1.0f)) return fail ("performance.tranceGate.steps: out of range");
    for (float s : p.gate.stepsRight) if (! inRange (s, 0.0f, 1.0f)) return fail ("performance.tranceGate.stepsRight: out of range");
    if (! inRange (p.gate.width, 0.0f, 1.0f)) return fail ("performance.tranceGate.width: out of range");
    if (p.gate.rate < kRateFirst || p.gate.rate > kGateRateLast || ! inRange (p.gate.swing, 0.0f, 0.6f)
        || ! inRange (p.gate.smoothMs, 0.0f, 30.0f) || ! inRange (p.gate.mix, 0.0f, 1.0f))
        return fail ("performance.tranceGate: out of range");

    bool seen[kNumFx] {};
    for (uint8_t t : d.fx.order)
    {
        if (t >= kNumFx || seen[t]) return fail ("fx.order: must be a permutation of all effect types");
        seen[t] = true;
    }
    for (int t = 0; t < kNumFx; ++t)
        for (int i = 0; i < fxNumParams (t); ++i)
        {
            const auto& desc = fxParamDesc (t, i);
            if (! inRange (d.fx.proc[(size_t) t].p[(size_t) i], desc.min, desc.max))
                return fail (std::string ("fx.") + fxTypeName ((FxType) t) + "." + desc.name + ": out of range");
        }
    for (int i = 0; i < kNumQuick; ++i)
    {
        const auto& q = d.fx.quick[(size_t) i];
        if (auto e = validateMacro (q, "fx.quick[" + std::to_string (i) + "]"); ! e.empty()) return fail (e);
        for (int k = 0; k < q.numMappings; ++k)
            if (! pc::isFx (q.mappings[(size_t) k].target)) return fail ("fx.quick: target must be an fx parameter");
    }
    if (! inRange (d.out.levelDb, -24.0f, 12.0f)) return fail ("output.levelDb: out of range");

    const auto& h = d.host;
    if (h.polyphony < 1 || h.polyphony > kMaxPolyphony || h.unison < 1 || h.unison > kMaxUnison
        || ! inRange (h.detune, 0.0f, 50.0f) || ! inRange (h.spread, 0.0f, 100.0f) || ! inRange (h.morphAmount, 0.0f, 1.0f))
        return fail ("performance: polyphony/unison/morph out of range");
    for (float q : h.quick) if (! inRange (q, 0.0f, 1.0f)) return fail ("fx.quick value out of range");

    // Soft problems: targets that do not exist in this patch.
    PatchData copy = d;
    sanitizeTargets (copy, &r.warnings);
    return r;
}

int sanitizeTargets (PatchData& d, std::vector<std::string>* warnings)
{
    int count = 0;
    auto doSide = [&] (EngineSide& s, bool sideB) {
        for (int i = 0; i < kNumModSlots; ++i)
        {
            auto& m = s.matrix[(size_t) i];
            if (! m.enabled) continue;
            const bool bad = m.source == ModSource::None || m.target < 0 || ! isModTarget (m.target) || ! paramExists (d, sideB, m.target);
            if (bad)
            {
                m.enabled = false;
                ++count;
                if (warnings) warnings->push_back (std::string (sideB ? "matrixB[" : "matrix[") + std::to_string (i) + "]: target/source undefined, slot disabled");
            }
        }
        for (int i = 0; i < kNumMacros; ++i)
        {
            auto& mac = s.macros[(size_t) i];
            for (int k = 0; k < mac.numMappings;)
            {
                if (! paramExists (d, sideB, mac.mappings[(size_t) k].target))
                {
                    for (int j = k; j + 1 < mac.numMappings; ++j) mac.mappings[(size_t) j] = mac.mappings[(size_t) j + 1];
                    --mac.numMappings;
                    ++count;
                    if (warnings) warnings->push_back (std::string (sideB ? "macrosB[" : "macros[") + std::to_string (i) + "]: mapping to missing parameter removed");
                }
                else ++k;
            }
        }
    };
    doSide (d.a, false);
    if (d.hasB) doSide (d.b, true);
    return count;
}

bool morphCompatible (const PatchData& d)
{
    if (! d.hasB) return false;
    const auto& a = d.a.engine;
    const auto& b = d.b.engine;
    if (engineHash (a) != engineHash (b)) return false; // graph, waveforms, enabled set
    for (int i = 0; i < kNumOps; ++i)
    {
        const auto& oa = a.operators[(size_t) i];
        const auto& ob = b.operators[(size_t) i];
        if (! oa.enabled) continue;
        if (oa.pitchMode != ob.pitchMode || oa.keySync != ob.keySync) return false;
        if ((oa.carrierGain > 0.0f) != (ob.carrierGain > 0.0f)) return false; // carrier layout
    }
    if (a.filter.type != b.filter.type) return false;
    return true; // unison count is a shared host parameter in v1
}

MorphKind resolveMorphKind (const PatchData& d, bool morphEnabled)
{
    if (! morphEnabled || ! d.hasB) return MorphKind::Off;
    if (d.morph.mode == MorphMode::Dual) return MorphKind::Dual;
    return morphCompatible (d) ? MorphKind::Compatible : MorphKind::Dual;
}

bool compilePatch (const PatchData& d, CompiledPatch& out, std::string* error)
{
    const auto v = validatePatch (d);
    if (! v.ok)
    {
        if (error) *error = v.error;
        return false;
    }
    out.data = d;
    sanitizeTargets (out.data);
    compileEngine (out.data.a.engine, out.ea);
    if (d.hasB) compileEngine (out.data.b.engine, out.eb); else out.eb = CompiledEngine {};
    out.compatible = morphCompatible (out.data);
    out.structureHash = fnv (fnv (out.ea.structureHash, d.hasB ? out.eb.structureHash : 0u), (uint64_t) d.hasB * 2u + (uint64_t) d.morph.mode * 4u);
    out.forceNewGeneration = false;
    return true;
}
} // namespace fmlume
