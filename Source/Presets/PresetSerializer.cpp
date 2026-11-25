#include "Presets/PresetSerializer.h"
#include <iterator>
#include "Core/Params.h"
#include "Core/PatchCompiler.h"
#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <ctime>
#include <random>

namespace fmlume
{
namespace
{
const char* const kWaveforms[] = { "sine", "triangle", "saw", "pulse" };
const char* const kPitchModes[] = { "ratio", "fixed" };
const char* const kFilterTypes[] = { "off", "LP12", "LP24", "HP12", "BP12" };
const char* const kLfoShapes[] = { "sine", "triangle", "saw", "square", "randomSmooth" };
const char* const kCurves[] = { "linear", "exp", "sCurve" };
const char* const kPolarities[] = { "bipolar", "unipolar" };
const char* const kArpModes[] = { "up", "down", "upDown", "random", "asPlayed" };
const char* const kArpOctaveModes[] = { "range", "up", "down", "upDown" };
const char* const kChordTypes[] = { "off", "major", "minor", "sus2", "sus4", "dim", "aug", "7", "m7", "maj7" };
const char* const kVoicings[] = { "root", "1st", "2nd" };
const char* const kQuickIds[] = { "fx.quick.warmth", "fx.quick.width", "fx.quick.space", "fx.quick.echo" };

// The tables are indexed by enum value when writing: a new enum value without a name must not compile.
#define FMLUME_NAMES_MATCH(table, Enum) static_assert (std::size (table) == (size_t) Enum::Count, #table " must name every " #Enum)
FMLUME_NAMES_MATCH (kWaveforms, Waveform);
FMLUME_NAMES_MATCH (kPitchModes, PitchMode);
FMLUME_NAMES_MATCH (kFilterTypes, FilterType);
FMLUME_NAMES_MATCH (kLfoShapes, LfoShape);
FMLUME_NAMES_MATCH (kCurves, Curve);
FMLUME_NAMES_MATCH (kPolarities, Polarity);
FMLUME_NAMES_MATCH (kArpModes, ArpMode);
FMLUME_NAMES_MATCH (kArpOctaveModes, ArpOctaveMode);
FMLUME_NAMES_MATCH (kChordTypes, ChordType);
FMLUME_NAMES_MATCH (kVoicings, Voicing);
#undef FMLUME_NAMES_MATCH
static_assert (std::size (kQuickIds) == (size_t) kNumQuick);

template <size_t N>
int enumIndex (const char* const (&names)[N], const std::string& s)
{
    for (size_t i = 0; i < N; ++i) if (s == names[i]) return (int) i;
    return -1;
}

// ------------------------------------------------------------------------------- writing
Json adsrJson (const ADSR& e)
{
    Json j = Json::object();
    j.set ("a", e.a); j.set ("d", e.d); j.set ("s", e.s); j.set ("r", e.r);
    return j;
}

Json engineJson (const EnginePatch& e)
{
    Json j = Json::object();
    Json ops = Json::array();
    for (auto& op : e.operators)
    {
        Json o = Json::object();
        o.set ("enabled", op.enabled);
        o.set ("waveform", kWaveforms[(int) op.waveform]);
        o.set ("pitchMode", kPitchModes[(int) op.pitchMode]);
        o.set ("ratio", op.ratio);
        o.set ("fixedHz", op.fixedHz);
        o.set ("detuneCents", op.detuneCents);
        o.set ("level", op.level);
        o.set ("velocityToLevel", op.velocityToLevel);
        o.set ("velocityToIndex", op.velocityToIndex);
        o.set ("keyTracking", op.keyTracking);
        o.set ("env", adsrJson (op.env));
        o.set ("phaseOffset", op.phaseOffset);
        o.set ("keySync", op.keySync);
        o.set ("feedback", op.feedback);
        o.set ("carrierGain", op.carrierGain);
        ops.push (std::move (o));
    }
    j.set ("operators", std::move (ops));
    Json edges = Json::array();
    for (int i = 0; i < e.numEdges; ++i)
    {
        Json ed = Json::object();
        ed.set ("from", (int) e.edges[(size_t) i].from);
        ed.set ("to", (int) e.edges[(size_t) i].to);
        ed.set ("index", e.edges[(size_t) i].index);
        edges.push (std::move (ed));
    }
    j.set ("edges", std::move (edges));
    Json f = Json::object();
    f.set ("type", kFilterTypes[(int) e.filter.type]);
    f.set ("cutoff", e.filter.cutoffHz);
    f.set ("resonance", e.filter.resonance);
    f.set ("drive", e.filter.drive);
    j.set ("filter", std::move (f));
    Json lfos = Json::array();
    for (auto& l : e.lfo)
    {
        Json o = Json::object();
        o.set ("shape", kLfoShapes[(int) l.shape]);
        o.set ("sync", l.sync);
        o.set ("rateHz", l.rateHz);
        o.set ("division", syncDivName (l.syncDiv));
        o.set ("phase", l.phase);
        o.set ("retrig", l.retrig);
        lfos.push (std::move (o));
    }
    j.set ("lfo", std::move (lfos));
    j.set ("modEnv", adsrJson (e.modEnv));
    j.set ("pitchBendRange", e.pitchBendRange);
    return j;
}

Json mappingsJson (const Macro& m)
{
    Json arr = Json::array();
    for (int k = 0; k < m.numMappings; ++k)
    {
        const auto& mp = m.mappings[(size_t) k];
        Json o = Json::object();
        o.set ("targetParamId", paramCodeToId (mp.target));
        o.set ("amount", mp.amount);
        o.set ("curve", kCurves[(int) mp.curve]);
        o.set ("polarity", kPolarities[(int) mp.polarity]);
        o.set ("min", mp.min);
        o.set ("max", mp.max);
        arr.push (std::move (o));
    }
    return arr;
}

Json macrosJson (const std::array<Macro, kNumMacros>& macros, const std::array<std::string, kNumMacros>& names)
{
    Json arr = Json::array();
    for (int i = 0; i < kNumMacros; ++i)
    {
        Json o = Json::object();
        o.set ("id", "macro." + std::to_string (i + 1));
        o.set ("name", names[(size_t) i]);
        o.set ("defaultValue", macros[(size_t) i].defaultValue);
        o.set ("mappings", mappingsJson (macros[(size_t) i]));
        arr.push (std::move (o));
    }
    return arr;
}

Json matrixJson (const std::array<ModSlot, kNumModSlots>& matrix)
{
    Json arr = Json::array();
    for (auto& m : matrix)
    {
        Json o = Json::object();
        o.set ("source", modSourceName (m.source));
        o.set ("target", m.target >= 0 ? paramCodeToId (m.target) : std::string());
        o.set ("depth", m.depth);
        o.set ("curve", kCurves[(int) m.curve]);
        o.set ("enabled", m.enabled);
        arr.push (std::move (o));
    }
    return arr;
}

// ------------------------------------------------------------------------------- reading
// "1.10.2" > "1.9"? Missing parts count as 0; anything after the numbers (e.g. "-beta") is ignored.
static bool isNewerVersion (const std::string& a, const std::string& b)
{
    auto parts = [] (const std::string& v) {
        std::array<long, 3> n {};
        size_t i = 0;
        for (int k = 0; k < 3 && i < v.size() && std::isdigit ((unsigned char) v[i]); ++k)
        {
            long x = 0;
            while (i < v.size() && std::isdigit ((unsigned char) v[i]) && x < 1000000) x = x * 10 + (v[i++] - '0');
            n[(size_t) k] = x;
            if (i < v.size() && v[i] == '.') ++i; else break;
        }
        return n;
    };
    return parts (a) > parts (b);
}

struct Reader
{
    std::string error;
    // Names this version does not know (an effect type, waveform, mod source, ... added by a newer
    // FMLUME). They are not errors: the setting keeps its default and the user is told (COMPATIBILITY.md).
    std::vector<std::string> unknown;
    bool ok() const { return error.empty(); }
    void unknownValue (const std::string& path, const std::string& value)
    {
        if (unknown.size() < 32) unknown.push_back (path + " = '" + value + "'");
    }
    bool fail (const std::string& path, const std::string& msg)
    {
        if (error.empty()) error = path + ": " + msg;
        return false;
    }
    const Json* get (const Json& obj, const char* key, const std::string& path, Json::Type type)
    {
        if (! ok()) return nullptr;
        if (! obj.isObject()) { fail (path, "expected an object"); return nullptr; }
        const Json* v = obj.find (key);
        if (v == nullptr) { fail (path + "." + key, "missing required field"); return nullptr; }
        if (v->type != type) { fail (path + "." + key, "wrong type"); return nullptr; }
        return v;
    }
    bool num (const Json& obj, const char* key, const std::string& path, float& out)
    {
        const Json* v = get (obj, key, path, Json::Type::Number);
        if (v == nullptr) return false;
        // range-check as double first: converting an out-of-range double to float is undefined
        if (! (std::fabs (v->number) <= 1.0e30)) return fail (path + "." + key, "number out of range");
        out = (float) v->number;
        return true;
    }
    bool integer (const Json& obj, const char* key, const std::string& path, int& out, int lo, int hi)
    {
        const Json* v = get (obj, key, path, Json::Type::Number);
        if (v == nullptr) return false;
        if (v->number != std::floor (v->number) || v->number < lo || v->number > hi)
            return fail (path + "." + key, "expected an integer in [" + std::to_string (lo) + ", " + std::to_string (hi) + "]");
        out = (int) v->number;
        return true;
    }
    bool boolean (const Json& obj, const char* key, const std::string& path, bool& out)
    {
        if (const Json* v = get (obj, key, path, Json::Type::Bool)) { out = v->boolean; return true; }
        return false;
    }
    bool str (const Json& obj, const char* key, const std::string& path, std::string& out)
    {
        if (const Json* v = get (obj, key, path, Json::Type::String)) { out = v->string; return true; }
        return false;
    }
    template <size_t N, typename E>
    bool enumeration (const Json& obj, const char* key, const std::string& path, const char* const (&names)[N], E& out)
    {
        std::string s;
        if (! str (obj, key, path, s)) return false;
        const int i = enumIndex (names, s);
        if (i < 0) { unknownValue (path + "." + key, s); return false; }
        out = (E) i;
        return true;
    }
    const Json* array (const Json& obj, const char* key, const std::string& path, int exactSize = -1, int maxSize = -1)
    {
        const Json* v = get (obj, key, path, Json::Type::Array);
        if (v == nullptr) return nullptr;
        if (exactSize >= 0 && (int) v->items.size() != exactSize) { fail (path + "." + key, "expected exactly " + std::to_string (exactSize) + " items"); return nullptr; }
        if (maxSize >= 0 && (int) v->items.size() > maxSize) { fail (path + "." + key, "too many items"); return nullptr; }
        return v;
    }
    const Json* object (const Json& obj, const char* key, const std::string& path) { return get (obj, key, path, Json::Type::Object); }
};

void readAdsr (Reader& r, const Json& parent, const char* key, const std::string& path, ADSR& e)
{
    if (const Json* j = r.object (parent, key, path))
    {
        const std::string p = path + "." + key;
        r.num (*j, "a", p, e.a); r.num (*j, "d", p, e.d); r.num (*j, "s", p, e.s); r.num (*j, "r", p, e.r);
    }
}

void readEngine (Reader& r, const Json& j, const std::string& path, EnginePatch& e)
{
    e = EnginePatch {};
    if (const Json* ops = r.array (j, "operators", path, kNumOps))
        for (int i = 0; i < kNumOps && r.ok(); ++i)
        {
            const Json& o = ops->items[(size_t) i];
            const std::string p = path + ".operators[" + std::to_string (i) + "]";
            auto& op = e.operators[(size_t) i];
            r.boolean (o, "enabled", p, op.enabled);
            r.enumeration (o, "waveform", p, kWaveforms, op.waveform);
            r.enumeration (o, "pitchMode", p, kPitchModes, op.pitchMode);
            r.num (o, "ratio", p, op.ratio);
            r.num (o, "fixedHz", p, op.fixedHz);
            r.num (o, "detuneCents", p, op.detuneCents);
            r.num (o, "level", p, op.level);
            r.num (o, "velocityToLevel", p, op.velocityToLevel);
            r.num (o, "velocityToIndex", p, op.velocityToIndex);
            r.num (o, "keyTracking", p, op.keyTracking);
            readAdsr (r, o, "env", p, op.env);
            r.num (o, "phaseOffset", p, op.phaseOffset);
            r.boolean (o, "keySync", p, op.keySync);
            r.num (o, "feedback", p, op.feedback);
            r.num (o, "carrierGain", p, op.carrierGain);
        }
    if (const Json* edges = r.array (j, "edges", path, -1, kMaxEdges))
        for (size_t i = 0; i < edges->items.size() && r.ok(); ++i)
        {
            const std::string p = path + ".edges[" + std::to_string (i) + "]";
            int from = 0, to = 0;
            float index = 0.0f;
            r.integer (edges->items[i], "from", p, from, 0, kNumOps - 1);
            r.integer (edges->items[i], "to", p, to, 0, kNumOps - 1);
            r.num (edges->items[i], "index", p, index);
            e.edges[(size_t) e.numEdges++] = { (int8_t) from, (int8_t) to, index };
        }
    if (const Json* f = r.object (j, "filter", path))
    {
        const std::string p = path + ".filter";
        r.enumeration (*f, "type", p, kFilterTypes, e.filter.type);
        r.num (*f, "cutoff", p, e.filter.cutoffHz);
        r.num (*f, "resonance", p, e.filter.resonance);
        r.num (*f, "drive", p, e.filter.drive);
    }
    if (const Json* lfos = r.array (j, "lfo", path, 2))
        for (int i = 0; i < 2 && r.ok(); ++i)
        {
            const std::string p = path + ".lfo[" + std::to_string (i) + "]";
            const Json& o = lfos->items[(size_t) i];
            auto& l = e.lfo[(size_t) i];
            r.enumeration (o, "shape", p, kLfoShapes, l.shape);
            r.boolean (o, "sync", p, l.sync);
            r.num (o, "rateHz", p, l.rateHz);
            std::string div;
            if (r.str (o, "division", p, div))
            {
                const int d = syncDivFromName (div);
                if (d < 0) r.unknownValue (p + ".division", div);
                else l.syncDiv = (uint8_t) d;
            }
            r.num (o, "phase", p, l.phase);
            r.boolean (o, "retrig", p, l.retrig);
        }
    readAdsr (r, j, "modEnv", path, e.modEnv);
    r.num (j, "pitchBendRange", path, e.pitchBendRange);
}

void readMappings (Reader& r, const Json& parent, const std::string& path, Macro& m)
{
    m.numMappings = 0;
    if (const Json* maps = r.array (parent, "mappings", path, -1, kMaxMacroMappings))
        for (size_t k = 0; k < maps->items.size() && r.ok(); ++k)
        {
            const std::string p = path + ".mappings[" + std::to_string (k) + "]";
            const Json& o = maps->items[k];
            auto& mp = m.mappings[(size_t) m.numMappings];
            mp = MacroMapping {};
            std::string id;
            if (r.str (o, "targetParamId", p, id))
            {
                const int code = paramIdToCode (id);
                if (code < 0) { r.unknownValue (p + ".targetParamId", id); continue; } // the mapping is left out
                mp.target = (int16_t) code;
            }
            r.num (o, "amount", p, mp.amount);
            r.enumeration (o, "curve", p, kCurves, mp.curve);
            r.enumeration (o, "polarity", p, kPolarities, mp.polarity);
            // min / max are optional (SPEC §12.2): default 0 / 1.
            if (o.isObject() && o.find ("min") != nullptr) r.num (o, "min", p, mp.min);
            if (o.isObject() && o.find ("max") != nullptr) r.num (o, "max", p, mp.max);
            ++m.numMappings;
        }
}

void readMacros (Reader& r, const Json& arr, const std::string& path, std::array<Macro, kNumMacros>& macros, std::array<std::string, kNumMacros>& names)
{
    if (! arr.isArray() || (int) arr.items.size() != kNumMacros) { r.fail (path, "expected exactly 8 macros"); return; }
    for (int i = 0; i < kNumMacros && r.ok(); ++i)
    {
        const std::string p = path + "[" + std::to_string (i) + "]";
        const Json& o = arr.items[(size_t) i];
        std::string id;
        if (r.str (o, "id", p, id) && id != "macro." + std::to_string (i + 1)) r.fail (p + ".id", "expected macro." + std::to_string (i + 1));
        r.str (o, "name", p, names[(size_t) i]);
        r.num (o, "defaultValue", p, macros[(size_t) i].defaultValue);
        readMappings (r, o, p, macros[(size_t) i]);
    }
}

void readMatrix (Reader& r, const Json& arr, const std::string& path, std::array<ModSlot, kNumModSlots>& matrix)
{
    if (! arr.isArray() || (int) arr.items.size() != kNumModSlots) { r.fail (path, "expected exactly 8 slots"); return; }
    for (int i = 0; i < kNumModSlots && r.ok(); ++i)
    {
        const std::string p = path + "[" + std::to_string (i) + "]";
        const Json& o = arr.items[(size_t) i];
        auto& m = matrix[(size_t) i];
        m = ModSlot {};
        std::string src, tgt;
        if (r.str (o, "source", p, src))
        {
            const int s = modSourceFromName (src);
            if (s < 0) { r.unknownValue (p + ".source", src); m.target = -1; continue; } // the slot stays empty
            m.source = (ModSource) s;
        }
        if (r.str (o, "target", p, tgt))
        {
            // Unknown targets are not fatal: the slot is disabled with a warning (SPEC §7).
            m.target = (int16_t) (tgt.empty() ? -1 : paramIdToCode (tgt));
        }
        r.num (o, "depth", p, m.depth);
        r.enumeration (o, "curve", p, kCurves, m.curve);
        r.boolean (o, "enabled", p, m.enabled);
        if (m.target >= 0 && ! isModTarget (m.target)) m.target = -1;
    }
}

// Schema 0 was the first development layout: a single "engine" object and no "output" block.
bool migrate0to1 (Json& root, std::string& error)
{
    const Json* engine = root.find ("engine");
    if (engine == nullptr || ! engine->isObject()) { error = "schema 0 preset has no 'engine' object"; return false; }
    Json copy = *engine;
    root.remove ("engine");
    root.set ("engineA", std::move (copy));
    if (root.find ("output") == nullptr) { Json o = Json::object(); o.set ("levelDb", 0.0f); root.set ("output", std::move (o)); }
    if (root.find ("metadata") == nullptr) root.set ("metadata", Json::object());
    root.set ("schemaVersion", 1);
    return true;
}

// Schema 1 had a second sound for Morph ("engineB", "macrosB", "matrixB", "morph", "slotNames").
// Morph was removed before the first release (docs/DECISIONS.md D-055): the first sound becomes
// "engine", everything belonging to the second one is dropped.
bool migrate1to2 (Json& root, std::string& error, bool& droppedSecondSound)
{
    const Json* engine = root.find ("engineA");
    if (engine == nullptr || ! engine->isObject()) { error = "preset.engineA: missing required field"; return false; }
    const Json* b = root.find ("engineB");
    droppedSecondSound = b != nullptr && b->isObject();
    Json copy = *engine;
    root.remove ("engineA");
    root.set ("engine", std::move (copy));
    for (const char* key : { "engineB", "macrosB", "matrixB", "morph", "slotNames" }) root.remove (key);
    root.set ("schemaVersion", 2);
    return true;
}
} // namespace

// =================================================================================== write
Json presetToJsonValue (const Preset& p)
{
    const PatchData& d = p.data;
    Json j = Json::object();
    j.set ("schemaVersion", kSchemaVersion);
    j.set ("pluginVersion", p.meta.pluginVersion.empty() ? std::string (FMLUME_VERSION_STRING) : p.meta.pluginVersion);
    j.set ("patchId", p.meta.patchId);
    j.set ("displayName", p.meta.displayName);
    j.set ("author", p.meta.author);
    j.set ("category", categoryName (p.meta.category));
    Json tags = Json::array();
    for (auto& t : p.meta.tags) tags.push (t);
    j.set ("tags", std::move (tags));
    j.set ("description", p.meta.description);
    j.set ("createdAt", p.meta.createdAt);
    j.set ("modifiedAt", p.meta.modifiedAt);
    j.set ("favorite", p.meta.favorite);
    j.set ("engine", engineJson (d.sound.engine));
    j.set ("macros", macrosJson (d.sound.macros, p.meta.macroNames));
    j.set ("matrix", matrixJson (d.sound.matrix));

    Json perf = Json::object();
    perf.set ("polyphony", d.host.polyphony);
    Json uni = Json::object();
    uni.set ("count", d.host.unison);
    uni.set ("detuneCents", d.host.detune);
    uni.set ("spread", d.host.spread);
    perf.set ("unison", std::move (uni));
    perf.set ("mono", d.perf.mono);
    perf.set ("legato", d.perf.legato);
    perf.set ("portamento", d.perf.portamento);
    Json arp = Json::object();
    arp.set ("enabled", d.host.arpEnabled);
    arp.set ("mode", kArpModes[(int) d.perf.arp.mode]);
    arp.set ("octaveMode", kArpOctaveModes[(int) d.perf.arp.octaveMode]);
    arp.set ("rate", syncDivName (d.perf.arp.rate));
    arp.set ("gate", d.perf.arp.gate);
    arp.set ("octaves", d.perf.arp.octaves);
    arp.set ("swing", d.perf.arp.swing);
    arp.set ("resetOnStop", d.perf.arp.resetOnStop);
    perf.set ("arp", std::move (arp));
    Json chord = Json::object();
    chord.set ("type", kChordTypes[(int) d.perf.chord.type]);
    chord.set ("voicing", kVoicings[(int) d.perf.chord.voicing]);
    chord.set ("octave", d.perf.chord.octave);
    chord.set ("strumMs", d.perf.chord.strumMs);
    perf.set ("chord", std::move (chord));
    Json gate = Json::object();
    gate.set ("enabled", d.host.gateEnabled);
    Json steps = Json::array();
    for (float s : d.perf.gate.steps) steps.push (s);
    gate.set ("steps", std::move (steps));
    gate.set ("rate", syncDivName (d.perf.gate.rate));
    gate.set ("swing", d.perf.gate.swing);
    gate.set ("smoothMs", d.perf.gate.smoothMs);
    gate.set ("mix", d.perf.gate.mix);
    // Optional stereo extension: only written when it carries information, so mono presets
    // keep their original form.
    if (d.perf.gate.stereo || d.perf.gate.stepsRight != d.perf.gate.steps || d.perf.gate.width != 1.0f)
    {
        gate.set ("stereo", d.perf.gate.stereo);
        Json right = Json::array();
        for (float s : d.perf.gate.stepsRight) right.push (s);
        gate.set ("stepsRight", std::move (right));
        gate.set ("width", d.perf.gate.width);
    }
    perf.set ("tranceGate", std::move (gate));
    j.set ("performance", std::move (perf));

    Json fx = Json::object();
    Json order = Json::array();
    for (uint8_t t : d.fx.order) order.push (fxTypeName ((FxType) t));
    fx.set ("order", std::move (order));
    fx.set ("bypassAll", d.host.fxBypassAll);
    Json procs = Json::array();
    for (int t = 0; t < kNumFx; ++t)
    {
        Json o = Json::object();
        o.set ("type", fxTypeName ((FxType) t));
        o.set ("enabled", d.fx.proc[(size_t) t].enabled);
        Json params = Json::object();
        for (int k = 0; k < fxNumParams (t); ++k) params.set (fxParamDesc (t, k).name, d.fx.proc[(size_t) t].p[(size_t) k]);
        o.set ("params", std::move (params));
        procs.push (std::move (o));
    }
    fx.set ("processors", std::move (procs));
    Json quick = Json::array();
    for (int i = 0; i < kNumQuick; ++i)
    {
        Json o = Json::object();
        o.set ("id", kQuickIds[i]);
        o.set ("value", d.host.quick[(size_t) i]);
        o.set ("mappings", mappingsJson (d.fx.quick[(size_t) i]));
        quick.push (std::move (o));
    }
    fx.set ("quick", std::move (quick));
    j.set ("fx", std::move (fx));

    Json out = Json::object();
    out.set ("levelDb", d.out.levelDb);
    j.set ("output", std::move (out));

    Json meta;
    if (! Json::parse (p.meta.metadataJson, meta) || ! meta.isObject()) meta = Json::object();
    j.set ("metadata", std::move (meta));
    return j;
}

bool presetToJson (const Preset& p, std::string& out, std::string* error)
{
    const auto v = validatePatch (p.data);
    if (! v.ok)
    {
        if (error) *error = "cannot export an invalid patch: " + v.error;
        return false;
    }
    out = presetToJsonValue (p).dump (2);
    out.push_back ('\n');
    return true;
}

namespace
{
constexpr size_t kMaxMetadataBytes = 64u * 1024u;
constexpr int kMaxMetadataDepth = 8;

int jsonDepth (const Json& j)
{
    int deepest = 0;
    for (auto& v : j.items) deepest = std::max (deepest, jsonDepth (v));
    for (auto& m : j.members) deepest = std::max (deepest, jsonDepth (m.second));
    return deepest + 1;
}
} // namespace

// =================================================================================== read
bool presetFromJsonValue (const Json& rootIn, Preset& out, std::string* error, std::vector<std::string>* warnings)
{
    auto failWith = [error] (const std::string& msg) { if (error) *error = msg; return false; };
    if (! rootIn.isObject()) return failWith ("preset root must be a JSON object");
    Json root = rootIn;

    const Json* sv = root.find ("schemaVersion");
    if (sv == nullptr || ! sv->isNumber() || sv->number != std::floor (sv->number)) return failWith ("schemaVersion: missing or not an integer");
    if (sv->number < 0.0 || sv->number > 1.0e6) return failWith ("schemaVersion: invalid"); // before the cast: out-of-range double -> int is undefined
    int version = (int) sv->number;
    if (version > kSchemaVersion) return failWith ("schemaVersion " + std::to_string (version) + " is newer than this plug-in supports (" + std::to_string (kSchemaVersion) + ")");
    if (version < 0) return failWith ("schemaVersion: invalid");
    if (version == 0)
    {
        std::string e;
        if (! migrate0to1 (root, e)) return failWith (e);
        if (warnings) warnings->push_back ("migrated from schema 0");
        version = 1;
    }
    if (version == 1)
    {
        std::string e;
        bool dropped = false;
        if (! migrate1to2 (root, e, dropped)) return failWith (e);
        if (warnings && dropped) warnings->push_back ("the second sound (Morph) of this preset was dropped: Morph is no longer part of FMLUME");
        version = 2;
    }

    Reader r;
    Preset p;
    p.data = PatchData {};
    PatchData& d = p.data;
    const std::string top = "preset";
    r.str (root, "pluginVersion", top, p.meta.pluginVersion);
    r.str (root, "patchId", top, p.meta.patchId);
    r.str (root, "displayName", top, p.meta.displayName);
    r.str (root, "author", top, p.meta.author);
    std::string cat;
    if (r.str (root, "category", top, cat))
    {
        const int c = categoryFromName (cat);
        if (c < 0) r.unknownValue ("preset.category", cat);
        else p.meta.category = (Category) c;
    }
    if (const Json* tags = r.array (root, "tags", top, -1, 64))
        for (auto& t : tags->items)
        {
            if (! t.isString()) { r.fail ("preset.tags", "expected strings"); break; }
            p.meta.tags.push_back (t.string);
        }
    r.str (root, "description", top, p.meta.description);
    // Optional bookkeeping fields.
    if (const Json* v = root.find ("createdAt"); v && v->isString()) p.meta.createdAt = v->string;
    if (const Json* v = root.find ("modifiedAt"); v && v->isString()) p.meta.modifiedAt = v->string;
    if (const Json* v = root.find ("favorite"); v && v->isBool()) p.meta.favorite = v->boolean;

    if (const Json* e = r.object (root, "engine", top)) readEngine (r, *e, "engine", d.sound.engine);
    if (const Json* m = r.array (root, "macros", top, kNumMacros)) readMacros (r, *m, "macros", d.sound.macros, p.meta.macroNames);
    if (const Json* m = r.array (root, "matrix", top, kNumModSlots)) readMatrix (r, *m, "matrix", d.sound.matrix);
    for (int i = 0; i < kNumMacros; ++i) d.host.macro[(size_t) i] = d.sound.macros[(size_t) i].defaultValue;

    if (const Json* perf = r.object (root, "performance", top))
    {
        const std::string pp = "performance";
        r.integer (*perf, "polyphony", pp, d.host.polyphony, 1, kMaxPolyphony);
        if (const Json* u = r.object (*perf, "unison", pp))
        {
            r.integer (*u, "count", pp + ".unison", d.host.unison, 1, kMaxUnison);
            r.num (*u, "detuneCents", pp + ".unison", d.host.detune);
            r.num (*u, "spread", pp + ".unison", d.host.spread);
        }
        r.boolean (*perf, "mono", pp, d.perf.mono);
        r.boolean (*perf, "legato", pp, d.perf.legato);
        r.num (*perf, "portamento", pp, d.perf.portamento);
        auto division = [&r] (const Json& o, const char* key, const std::string& path, uint8_t& out) {
            std::string s;
            if (! r.str (o, key, path, s)) return;
            const int dv = syncDivFromName (s);
            if (dv < 0) r.unknownValue (path + "." + key, s);
            else out = (uint8_t) dv;
        };
        if (const Json* a = r.object (*perf, "arp", pp))
        {
            const std::string ap = pp + ".arp";
            r.boolean (*a, "enabled", ap, d.host.arpEnabled);
            r.enumeration (*a, "mode", ap, kArpModes, d.perf.arp.mode);
            d.perf.arp.octaveMode = ArpOctaveMode::Range;
            if (a->find ("octaveMode") != nullptr) r.enumeration (*a, "octaveMode", ap, kArpOctaveModes, d.perf.arp.octaveMode);
            division (*a, "rate", ap, d.perf.arp.rate);
            r.num (*a, "gate", ap, d.perf.arp.gate);
            r.integer (*a, "octaves", ap, d.perf.arp.octaves, 1, 4);
            r.num (*a, "swing", ap, d.perf.arp.swing);
            r.boolean (*a, "resetOnStop", ap, d.perf.arp.resetOnStop);
        }
        if (const Json* c = r.object (*perf, "chord", pp))
        {
            const std::string cp = pp + ".chord";
            r.enumeration (*c, "type", cp, kChordTypes, d.perf.chord.type);
            r.enumeration (*c, "voicing", cp, kVoicings, d.perf.chord.voicing);
            r.integer (*c, "octave", cp, d.perf.chord.octave, -2, 2);
            r.num (*c, "strumMs", cp, d.perf.chord.strumMs);
        }
        if (const Json* g = r.object (*perf, "tranceGate", pp))
        {
            const std::string gp = pp + ".tranceGate";
            r.boolean (*g, "enabled", gp, d.host.gateEnabled);
            if (const Json* steps = r.array (*g, "steps", gp, kGateSteps))
                for (int i = 0; i < kGateSteps; ++i)
                {
                    if (! steps->items[(size_t) i].isNumber()) { r.fail (gp + ".steps", "expected numbers"); break; }
                    d.perf.gate.steps[(size_t) i] = (float) steps->items[(size_t) i].number;
                }
            division (*g, "rate", gp, d.perf.gate.rate);
            r.num (*g, "swing", gp, d.perf.gate.swing);
            r.num (*g, "smoothMs", gp, d.perf.gate.smoothMs);
            r.num (*g, "mix", gp, d.perf.gate.mix);
            // Optional stereo extension; absent = mono (right follows left).
            d.perf.gate.stereo = false;
            d.perf.gate.width = 1.0f;
            d.perf.gate.stepsRight = d.perf.gate.steps;
            if (g->find ("stereo") != nullptr) r.boolean (*g, "stereo", gp, d.perf.gate.stereo);
            if (g->find ("width") != nullptr) r.num (*g, "width", gp, d.perf.gate.width);
            if (g->find ("stepsRight") != nullptr)
                if (const Json* right = r.array (*g, "stepsRight", gp, kGateSteps))
                    for (int i = 0; i < kGateSteps; ++i)
                    {
                        if (! right->items[(size_t) i].isNumber()) { r.fail (gp + ".stepsRight", "expected numbers"); break; }
                        d.perf.gate.stepsRight[(size_t) i] = (float) right->items[(size_t) i].number;
                    }
        }
    }

    if (const Json* fx = r.object (root, "fx", top))
    {
        // The order lists every effect once. Effects this version does not know are skipped; any
        // it knows but the file does not mention (impossible today) go to the end in default order.
        if (const Json* order = r.array (*fx, "order", "fx", -1, 64))
        {
            bool placed[kNumFx] {};
            int n = 0;
            for (auto& o : order->items)
            {
                if (! o.isString()) { r.fail ("fx.order", "expected effect names"); break; }
                const int t = fxTypeFromName (o.string);
                if (t < 0) { r.unknownValue ("fx.order", o.string); continue; }
                if (placed[t]) { r.fail ("fx.order", "effect '" + o.string + "' listed twice"); break; }
                placed[t] = true;
                d.fx.order[(size_t) n++] = (uint8_t) t;
            }
            for (int t = 0; t < kNumFx && r.ok(); ++t) if (! placed[t]) d.fx.order[(size_t) n++] = (uint8_t) t;
        }
        r.boolean (*fx, "bypassAll", "fx", d.host.fxBypassAll);
        if (const Json* procs = r.array (*fx, "processors", "fx", -1, 64))
        {
            bool seen[kNumFx] {};
            for (auto& o : procs->items)
            {
                if (! r.ok()) break;
                std::string type;
                if (! r.str (o, "type", "fx.processors[]", type)) break;
                const int t = fxTypeFromName (type);
                if (t < 0) { r.unknownValue ("fx.processors[]", type); continue; }
                if (seen[t]) { r.fail ("fx.processors[]", "duplicate effect type '" + type + "'"); break; }
                seen[t] = true;
                const std::string pp = std::string ("fx.") + type;
                r.boolean (o, "enabled", pp, d.fx.proc[(size_t) t].enabled);
                if (const Json* params = r.object (o, "params", pp))
                    for (int k = 0; k < fxNumParams (t); ++k)
                        r.num (*params, fxParamDesc (t, k).name, pp + ".params", d.fx.proc[(size_t) t].p[(size_t) k]);
            }
        }
        if (const Json* quick = r.array (*fx, "quick", "fx", kNumQuick))
            for (int i = 0; i < kNumQuick && r.ok(); ++i)
            {
                const std::string qp = "fx.quick[" + std::to_string (i) + "]";
                const Json& o = quick->items[(size_t) i];
                std::string id;
                if (r.str (o, "id", qp, id) && id != kQuickIds[i]) r.fail (qp + ".id", std::string ("expected ") + kQuickIds[i]);
                r.num (o, "value", qp, d.host.quick[(size_t) i]);
                readMappings (r, o, qp, d.fx.quick[(size_t) i]);
            }
    }
    if (const Json* o = r.object (root, "output", top)) r.num (*o, "levelDb", "output", d.out.levelDb);
    // Unknown metadata is carried along verbatim, but only while it stays small and shallow: the
    // preset is nested one level deeper inside project state and is re-parsed on every save.
    if (const Json* meta = r.object (root, "metadata", top))
    {
        std::string text = meta->dump (0);
        if (text.size() > kMaxMetadataBytes || jsonDepth (*meta) > kMaxMetadataDepth)
        {
            text = "{}";
            if (warnings) warnings->push_back ("metadata was too large or too deeply nested and has been dropped");
        }
        p.meta.metadataJson = std::move (text);
    }

    if (! r.ok()) return failWith (r.error);
    if (warnings && ! r.unknown.empty())
    {
        std::string w = "this preset uses settings this version of FMLUME does not know (made with a newer version?); they were left at their defaults: ";
        for (size_t i = 0; i < r.unknown.size(); ++i) w += (i ? ", " : "") + r.unknown[i];
        warnings->push_back (w);
    }
    else if (warnings && isNewerVersion (p.meta.pluginVersion, FMLUME_VERSION_STRING))
        warnings->push_back ("this preset was saved by a newer FMLUME (" + p.meta.pluginVersion + "); settings added since are ignored");

    const auto v = validatePatch (d);
    if (! v.ok) return failWith (v.error);
    if (warnings) for (auto& w : v.warnings) warnings->push_back (w);
    sanitizeTargets (d);
    for (float m : d.host.macro) if (! (m >= 0.0f && m <= 1.0f)) return failWith ("macros: defaultValue out of range");
    out = std::move (p);
    return true;
}

bool presetFromJson (const std::string& text, Preset& out, std::string* error, std::vector<std::string>* warnings)
{
    Json root;
    std::string e;
    if (! Json::parse (text, root, &e))
    {
        if (error) *error = "not valid JSON (" + e + ")";
        return false;
    }
    return presetFromJsonValue (root, out, error, warnings);
}

// =================================================================================== state
bool stateToJson (const Preset& p, const HostParams& host, std::string& out, std::string* error, bool edited)
{
    const auto v = validatePatch (p.data);
    if (! v.ok)
    {
        if (error) *error = v.error;
        return false;
    }
    Json j = Json::object();
    j.set ("fmlumeState", 1);
    j.set ("preset", presetToJsonValue (p));
    Json h = Json::object();
    h.set ("gainDb", host.gainDb);
    h.set ("pan", host.pan);
    h.set ("polyphony", host.polyphony);
    h.set ("unison", host.unison);
    h.set ("detune", host.detune);
    h.set ("spread", host.spread);
    h.set ("quality", host.quality);
    Json macros = Json::array();
    for (float m : host.macro) macros.push (m);
    h.set ("macros", std::move (macros));
    h.set ("fxBypassAll", host.fxBypassAll);
    Json quick = Json::array();
    for (float q : host.quick) quick.push (q);
    h.set ("quick", std::move (quick));
    h.set ("arpEnabled", host.arpEnabled);
    h.set ("gateEnabled", host.gateEnabled);
    j.set ("host", std::move (h));
    j.set ("edited", edited);
    out = j.dump (0);
    return true;
}

bool stateFromJson (const std::string& text, Preset& out, HostParams& host, std::string* error, std::vector<std::string>* warnings, bool* edited)
{
    auto failWith = [error] (const std::string& msg) { if (error) *error = msg; return false; };
    Json root;
    std::string e;
    if (! Json::parse (text, root, &e)) return failWith ("state is not valid JSON (" + e + ")");
    const Json* ver = root.isObject() ? root.find ("fmlumeState") : nullptr;
    if (ver == nullptr || ! ver->isNumber() || ver->number != 1.0) return failWith ("unsupported state version");
    const Json* preset = root.find ("preset");
    const Json* h = root.find ("host");
    if (preset == nullptr || h == nullptr || ! h->isObject()) return failWith ("state is missing 'preset' or 'host'");
    Preset p;
    if (! presetFromJsonValue (*preset, p, error, warnings)) return false;
    if (edited != nullptr)
    {
        const Json* ed = root.find ("edited"); // optional: older states have no such key
        *edited = ed != nullptr && ed->isBool() && ed->boolean;
    }

    Reader r;
    HostParams hp;
    r.num (*h, "gainDb", "host", hp.gainDb);
    r.num (*h, "pan", "host", hp.pan);
    r.integer (*h, "polyphony", "host", hp.polyphony, 1, kMaxPolyphony);
    r.integer (*h, "unison", "host", hp.unison, 1, kMaxUnison);
    r.num (*h, "detune", "host", hp.detune);
    r.num (*h, "spread", "host", hp.spread);
    r.integer (*h, "quality", "host", hp.quality, 0, 2);
    if (const Json* m = r.array (*h, "macros", "host", kNumMacros))
        for (int i = 0; i < kNumMacros; ++i) hp.macro[(size_t) i] = m->items[(size_t) i].isNumber() ? (float) m->items[(size_t) i].number : 0.5f;
    r.boolean (*h, "fxBypassAll", "host", hp.fxBypassAll);
    if (const Json* q = r.array (*h, "quick", "host", kNumQuick))
        for (int i = 0; i < kNumQuick; ++i) hp.quick[(size_t) i] = q->items[(size_t) i].isNumber() ? (float) q->items[(size_t) i].number : 0.5f;
    r.boolean (*h, "arpEnabled", "host", hp.arpEnabled);
    r.boolean (*h, "gateEnabled", "host", hp.gateEnabled);
    if (! r.ok()) return failWith (r.error);
    auto in = [] (float v, float lo, float hi) { return std::isfinite (v) && v >= lo && v <= hi; };
    if (! in (hp.gainDb, -60.0f, 12.0f) || ! in (hp.pan, -1.0f, 1.0f) || ! in (hp.detune, 0.0f, 50.0f) || ! in (hp.spread, 0.0f, 100.0f))
        return failWith ("host: value out of range");
    for (float m : hp.macro) if (! in (m, 0.0f, 1.0f)) return failWith ("host.macros: value out of range");
    for (float q : hp.quick) if (! in (q, 0.0f, 1.0f)) return failWith ("host.quick: value out of range");
    out = std::move (p);
    host = hp;
    return true;
}

// =================================================================================== misc
bool patchDataEqual (const PatchData& a, const PatchData& b)
{
    Preset pa, pb;
    pa.data = a;
    pb.data = b;
    return presetToJsonValue (pa).dump (0) == presetToJsonValue (pb).dump (0);
}

uint64_t patchChecksum (const PatchData& d)
{
    Preset p;
    p.meta = PresetMeta {};
    p.data = d;
    const std::string s = presetToJsonValue (p).dump (0);
    uint64_t h = 1469598103934665603ULL;
    for (const char c : s) { h ^= (uint8_t) c; h *= 1099511628211ULL; }
    return h;
}

std::string makePatchId()
{
    static std::mt19937_64 rng { std::random_device {}() ^ (uint64_t) std::chrono::steady_clock::now().time_since_epoch().count() };
    const uint64_t a = rng(), b = rng();
    char buf[40];
    std::snprintf (buf, sizeof (buf), "%08x-%04x-4%03x-%04x-%012llx", (unsigned) (a >> 32), (unsigned) ((a >> 16) & 0xffff),
                   (unsigned) (a & 0xfff), (unsigned) (0x8000 | ((b >> 48) & 0x3fff)), (unsigned long long) (b & 0xffffffffffffULL));
    return buf;
}

std::string nowIso8601()
{
    const std::time_t t = std::time (nullptr);
    std::tm tm {};

#if defined(_WIN32)
    gmtime_s (&tm, &t);
#else
    gmtime_r (&t, &tm);
#endif

    char buf[32];
    std::strftime (buf, sizeof (buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}
} // namespace fmlume
