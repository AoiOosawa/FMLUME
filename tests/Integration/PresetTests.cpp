#include "TestFramework.h"
#include "TestUtil.h"
#include "Core/PatchController.h"
#include "Core/Templates.h"
#include "Presets/PresetManager.h"
#include "Presets/PresetSerializer.h"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace fmtest;
namespace fs = std::filesystem;

namespace
{

// A preset that exercises every part of the model.
Preset richPreset()
{
    Preset p = makeCategoryPreset (Category::Bell);
    p.meta.displayName = "Rich \"Test\" \xE9\x9F\xB3";
    p.meta.patchId = "test-rich";
    p.meta.author = "unit test";
    p.meta.tags = { "Bright", "Metallic" };
    p.meta.description = "line1\nline2\ttab";
    p.meta.metadataJson = "{\"custom\":{\"a\":[1,2,3]},\"note\":\"keep me\"}";
    PatchData& d = p.data;
    d.host.unison = 3;
    d.host.detune = 17.5f;
    d.host.arpEnabled = true;
    d.perf.arp.mode = ArpMode::UpDown;
    d.perf.arp.rate = 10;
    d.perf.arp.swing = 0.2f;
    d.perf.chord.type = ChordType::Min7;
    d.perf.chord.voicing = Voicing::Second;
    d.perf.chord.strumMs = 12.5f;
    for (int i = 0; i < kGateSteps; ++i) d.perf.gate.steps[(size_t) i] = (float) (i % 5) * 0.25f;
    d.fx.order = { 8, 0, 2, 1, 3, 4, 5, 6, 7 };
    d.fx.proc[(size_t) FxType::Phaser].enabled = true;
    d.sound.engine.operators[1].waveform = Waveform::Saw;
    d.sound.engine.operators[0].pitchMode = PitchMode::Fixed;
    d.sound.engine.operators[0].fixedHz = 123.456f;
    d.sound.engine.lfo[1].sync = true;
    d.sound.engine.lfo[1].syncDiv = 14;
    d.sound.engine.lfo[1].shape = LfoShape::RandomSmooth;
    d.sound.macros[2].defaultValue = 0.123456789f;
    d.out.levelDb = -3.25f;
    return p;
}
} // namespace

FM_TEST ("integration", "PRS-01 JSON round trip is lossless")
{
    std::vector<Preset> presets { makeInitPreset(), richPreset() };
    for (int c = 0; c < (int) Category::Count; ++c) presets.push_back (makeCategoryPreset ((Category) c));
    for (auto& p : presets)
    {
        std::string json, err;
        CHECK (presetToJson (p, json, &err));
        Preset back;
        std::vector<std::string> warnings;
        CHECK (presetFromJson (json, back, &err, &warnings));
        if (! err.empty()) std::printf ("    %s\n", err.c_str());
        CHECK (warnings.empty());
        std::string json2;
        CHECK (presetToJson (back, json2));
        CHECK (json == json2); // canonical text identical
        CHECK (patchDataEqual (p.data, back.data));
        CHECK (back.meta.displayName == p.meta.displayName && back.meta.tags == p.meta.tags && back.meta.description == p.meta.description);
        CHECK (back.meta.macroNames == p.meta.macroNames);
        // float fields survive bit-exactly
        CHECK (back.data.sound.macros[2].defaultValue == p.data.sound.macros[2].defaultValue);
        CHECK (back.data.sound.engine.operators[0].fixedHz == p.data.sound.engine.operators[0].fixedHz);
        CHECK (patchChecksum (p.data) == patchChecksum (back.data));
    }
    // Extra metadata is preserved.
    std::string json;
    CHECK (presetToJson (richPreset(), json));
    CHECK (json.find ("keep me") != std::string::npos);
    Preset back;
    CHECK (presetFromJson (json, back));
    CHECK (back.meta.metadataJson.find ("\"custom\"") != std::string::npos);
    // Export refuses invalid data instead of clamping it.
    Preset bad = makeInitPreset();
    bad.data.sound.engine.operators[0].level = 3.0f;
    std::string out, err;
    CHECK (! presetToJson (bad, out, &err));
    CHECK (! err.empty());
}

FM_TEST ("integration", "PRS-03 broken, unknown-version and extreme presets are rejected without crashing")
{
    std::string good;
    CHECK (presetToJson (richPreset(), good));
    Preset tmp;
    CHECK (presetFromJson (good, tmp));

    auto mutate = [&] (const std::string& from, const std::string& to) {
        std::string s = good;
        const auto pos = s.find (from);
        CHECK (pos != std::string::npos);
        if (pos != std::string::npos) s.replace (pos, from.size(), to);
        return s;
    };
    struct Case { const char* what; std::string text; };
    std::vector<Case> cases = {
        { "empty file", "" },
        { "not json", "this is not json" },
        { "truncated", good.substr (0, good.size() / 2) },
        { "array root", "[1,2,3]" },
        { "future schema", mutate ("\"schemaVersion\": 2", "\"schemaVersion\": 3") },
        { "negative schema", mutate ("\"schemaVersion\": 2", "\"schemaVersion\": -1") },
        { "schema as string", mutate ("\"schemaVersion\": 2", "\"schemaVersion\": \"2\"") },
        { "missing engine", mutate ("\"engine\"", "\"engineX\"") },
        { "bad category", mutate ("\"category\": \"Bell\"", "\"category\": \"Kazoo\"") },
        { "bad waveform", mutate ("\"waveform\": \"saw\"", "\"waveform\": \"noise\"") },
        { "ratio out of range", mutate ("\"ratio\": 3.5", "\"ratio\": 1e9") },
        { "negative level", mutate ("\"level\": 0.5", "\"level\": -0.5") },
        { "NaN token", mutate ("\"ratio\": 3.5", "\"ratio\": NaN") },
        { "Infinity token", mutate ("\"ratio\": 3.5", "\"ratio\": Infinity") },
        { "wrong type", mutate ("\"enabled\": true", "\"enabled\": 1") },
        { "self edge", mutate ("\"from\": 0", "\"from\": 2") },
        { "edge op out of range", mutate ("\"to\": 2", "\"to\": 9") },
        { "unknown macro target", mutate ("\"targetParamId\": \"edge.1.3.index\"", "\"targetParamId\": \"op9.level\"") },
        { "too few macros", mutate ("\"id\": \"macro.8\"", "\"id\": \"macro.9\"") },
        { "fx order not a permutation", mutate ("\"order\": [\"reverb\", \"eq\"", "\"order\": [\"reverb\", \"reverb\"") },
        { "bad gate division", mutate ("\"tranceGate\": {\n      \"enabled\": false,", "\"tranceGate\": {\n      \"enabled\": 5,") },
        { "polyphony 0", mutate ("\"polyphony\": 16", "\"polyphony\": 0") },
        { "deep nesting", std::string (5000, '[') },
        { "huge number", mutate ("\"ratio\": 3.5", "\"ratio\": 1e999") },
        { "duplicate keys", mutate ("\"author\": \"unit test\"", "\"author\": \"a\", \"author\": \"b\"") },
        { "binary garbage", std::string ("\x00\x01\x02\xff\xfe{", 6) },
    };
    int rejected = 0;
    for (auto& c : cases)
    {
        Preset p;
        std::string err;
        const bool ok = presetFromJson (c.text, p, &err);
        CHECK (! ok);
        CHECK (! err.empty());
        if (ok) std::printf ("    accepted: %s\n", c.what);
        rejected += ok ? 0 : 1;
    }
    CHECK (rejected >= 10);

    // A cyclic graph in a file is rejected by validation.
    Preset cyc = makeCategoryPreset (Category::Bass);
    Json j = presetToJsonValue (cyc);
    Json* edges = nullptr;
    for (auto& m : j.members) if (m.first == "engine") for (auto& m2 : m.second.members) if (m2.first == "edges") edges = &m2.second;
    CHECK (edges != nullptr);
    Json back = Json::object();
    back.set ("from", 2); back.set ("to", 0); back.set ("index", 1.0f);
    edges->push (back);
    Preset p;
    std::string err;
    CHECK (! presetFromJsonValue (j, p, &err));
    CHECK (err.find ("cycle") != std::string::npos);

    // Old schema (0) is migrated; a mod slot with an unknown target is disabled with a warning, not fatal.
    Json old = presetToJsonValue (makeCategoryPreset (Category::Keys));
    old.remove ("output");
    old.set ("schemaVersion", 0);
    std::vector<std::string> warnings;
    Preset migrated;
    CHECK (presetFromJsonValue (old, migrated, &err, &warnings));
    CHECK (! warnings.empty());
    PatchData expected = makeCategoryPreset (Category::Keys).data;
    expected.out.levelDb = 0.0f; // schema 0 had no "output" block: the migrated level is the default
    CHECK (patchDataEqual (migrated.data, expected));

    // Schema 1 had a second sound for Morph. Such a preset still loads: the first sound is kept,
    // everything of the second one is dropped and reported (docs/DECISIONS.md D-055).
    {
        const Preset keys = makeCategoryPreset (Category::Keys);
        Json v1 = presetToJsonValue (keys);
        Json first = *v1.find ("engine");
        v1.remove ("engine");
        v1.set ("engineA", first);
        v1.set ("engineB", presetToJsonValue (makeCategoryPreset (Category::Bell)).find ("engine") != nullptr ? *presetToJsonValue (makeCategoryPreset (Category::Bell)).find ("engine") : Json());
        v1.set ("macrosB", *presetToJsonValue (makeCategoryPreset (Category::Bell)).find ("macros"));
        v1.set ("matrixB", *presetToJsonValue (makeCategoryPreset (Category::Bell)).find ("matrix"));
        Json morph = Json::object();
        morph.set ("enabled", true); morph.set ("mode", "dual"); morph.set ("amount", 0.5f);
        v1.set ("morph", morph);
        v1.set ("schemaVersion", 1);
        warnings.clear();
        Preset from1;
        CHECK (presetFromJsonValue (v1, from1, &err, &warnings));
        CHECK (warnings.size() == 1 && warnings[0].find ("Morph") != std::string::npos);
        CHECK (patchDataEqual (from1.data, keys.data));
        // without a second sound there is nothing to report
        v1.set ("engineB", Json());
        v1.set ("macrosB", Json());
        v1.set ("matrixB", Json());
        warnings.clear();
        CHECK (presetFromJsonValue (v1, from1, &err, &warnings));
        CHECK (warnings.empty());
        CHECK (patchDataEqual (from1.data, keys.data));
        // what is written is schema 2, with a single "engine"
        const Json written = presetToJsonValue (from1);
        CHECK (written.find ("engine") != nullptr && written.find ("engineA") == nullptr && written.find ("engineB") == nullptr && written.find ("morph") == nullptr);
    }

    Preset withSlot = makeCategoryPreset (Category::Pad);
    std::string text;
    CHECK (presetToJson (withSlot, text));
    const auto pos = text.find ("\"target\": \"edge.1.2.index\"");
    CHECK (pos != std::string::npos);
    text.replace (pos, 26, "\"target\": \"edge.5.6.index\"");
    warnings.clear();
    Preset loaded;
    CHECK (presetFromJson (text, loaded, &err, &warnings));
    CHECK (! warnings.empty());
    CHECK (! loaded.data.sound.matrix[0].enabled);

    // Fixture files on disk: everything under valid/ loads, everything under broken/ is refused.
    const std::string fixtures = std::string (FMLUME_SOURCE_DIR) + "/tests/Fixtures";
    int validCount = 0, brokenCount = 0;
    std::error_code ec;
    for (auto& it : fs::directory_iterator (fixtures + "/valid", ec))
        if (it.path().extension() == ".fmlume") { Preset q; std::string e; CHECK (PresetManager::importFrom (it.path().string(), q, &e)); if (! e.empty()) std::printf ("    %s: %s\n", it.path().filename().c_str(), e.c_str()); ++validCount; }
    for (auto& it : fs::directory_iterator (fixtures + "/broken", ec))
        if (it.path().extension() == ".fmlume") { Preset q; std::string e; const bool ok = PresetManager::importFrom (it.path().string(), q, &e); CHECK (! ok); if (ok) std::printf ("    accepted broken fixture %s\n", it.path().filename().c_str()); ++brokenCount; }
    CHECK (validCount >= 1);
    CHECK (brokenCount >= 10);
}

FM_TEST ("integration", "PRS-02 saved state restores an identical render (every category, arp, gate)")
{
    const double sr = 48000.0;
    std::vector<NoteEvent> seq { { 0.0, noteOn (52, 100) }, { 0.05, noteOn (59, 80) }, { 0.3, cc (1, 90) }, { 0.5, bend (-0.4f) },
                                 { 0.8, noteOff (52) }, { 1.0, noteOn (64, 120) }, { 1.3, noteOff (59) }, { 1.6, noteOff (64) } };
    std::vector<std::pair<Preset, HostParams>> cases;
    for (int c = 0; c < (int) Category::Count; ++c)
    {
        Preset p = makeCategoryPreset ((Category) c);
        HostParams h = p.data.host;
        h.macro = { 0.8f, 0.3f, 0.6f, 0.9f, 0.1f, 0.7f, 0.55f, 0.2f };
        h.quick = { 0.7f, 0.9f, 0.6f, 0.8f };
        cases.push_back ({ p, h });
    }
    {
        Preset p = richPreset();
        HostParams h = p.data.host;
        h.gateEnabled = true;
        h.gainDb = -9.5f;
        h.pan = 0.25f;
        h.quality = 2;
        h.macro[4] = 0.9f;
        cases.push_back ({ p, h });
    }
    TransportInfo tr;
    tr.valid = tr.playing = true;
    tr.bpm = 133.0;
    tr.ppq = 7.25;
    for (auto& c : cases)
    {
        PatchController ctl;
        SynthEngine original;
        original.prepare (sr, 256);
        ctl.sendPatch = [&] (const PatchData& d, bool f) { return original.setPatch (d, f); };
        ctl.loadPreset (c.first);
        const Preset toSave = ctl.presetForSave (c.second);
        std::string state, err;
        CHECK (stateToJson (toSave, c.second, state, &err));
        auto a = renderSeq (original, sr, 256, 2.5, seq, c.second, tr);

        // "Restart the plug-in": brand new engine + controller fed only by the state blob.
        Preset restored;
        HostParams host2;
        CHECK (stateFromJson (state, restored, host2, &err));
        if (! err.empty()) std::printf ("    %s\n", err.c_str());
        PatchController ctl2;
        SynthEngine fresh;
        fresh.prepare (sr, 256);
        ctl2.sendPatch = [&] (const PatchData& d, bool f) { return fresh.setPatch (d, f); };
        CHECK (ctl2.setState (restored));
        CHECK (! ctl2.isDirty());
        auto b = renderSeq (fresh, sr, 256, 2.5, seq, host2, tr);
        CHECK (peak (a.l) > 0.01f);
        CHECK (maxAbsDiff (a.l, b.l) < 1.0e-5);
        CHECK (maxAbsDiff (a.r, b.r) < 1.0e-5);
    }
    Preset p;
    HostParams h;
    std::string err;
    // A project saved before Morph was removed (D-055) still opens: the two host values are ignored.
    {
        const Preset keys = makeCategoryPreset (Category::Keys);
        std::string state;
        CHECK (stateToJson (keys, keys.data.host, state, &err));
        const std::string marker = "\"fxBypassAll\"";
        const size_t at = state.find (marker);
        CHECK (at != std::string::npos);
        if (at != std::string::npos) state.insert (at, "\"morphAmount\":0.5,\"morphEnabled\":true,");
        CHECK (stateFromJson (state, p, h, &err));
        CHECK (patchDataEqual (p.data, keys.data));
    }
    CHECK (! stateFromJson ("{}", p, h, &err));
    CHECK (! stateFromJson ("garbage", p, h, &err));
    CHECK (! stateFromJson ("{\"fmlumeState\":7,\"preset\":{},\"host\":{}}", p, h, &err));
}

FM_TEST ("integration", "PresetManager: save, rescan, search, favourites, import/export, broken files")
{
    const std::string dir = (fs::temp_directory_path() / ("fmlume-test-" + makePatchId())).string();
    {
        PresetManager pm;
        for (int c = 0; c < (int) Category::Count; ++c)
        {
            Preset p = makeCategoryPreset ((Category) c);
            p.meta.tags = { c % 2 ? "Warm" : "Bright" };
            std::string json;
            CHECK (presetToJson (p, json));
            CHECK (pm.addFactoryPreset (json));
        }
        CHECK (! pm.addFactoryPreset ("{ nope"));
        pm.setUserDirectory (dir);
        CHECK (pm.entries().size() == (size_t) Category::Count);
        CHECK (pm.search ("", (int) Category::Bass).size() == 1);
        CHECK (pm.search ("init pad").size() == 1);
        CHECK (pm.search ("", -1, "warm").size() == (size_t) Category::Count / 2); // every odd category
        CHECK (pm.search ("zzzz").empty());

        Preset mine = makeCategoryPreset (Category::Lead);
        mine.meta.tags = { "Retro" };
        std::string err;
        CHECK (pm.saveUser (mine, "My Lead: v1/2", false, &err));
        CHECK (pm.entries().size() == (size_t) Category::Count + 1);
        CHECK (! pm.saveUser (mine, "My Lead: v1/2", false, &err)); // exists
        CHECK (pm.saveUser (mine, "My Lead: v1/2", true, &err));    // overwrite keeps one file
        CHECK (pm.entries().size() == (size_t) Category::Count + 1);
        const auto hits = pm.search ("my lead");
        CHECK (hits.size() == 1);
        Preset loaded;
        CHECK (pm.load (hits[0], loaded, &err));
        CHECK (patchDataEqual (loaded.data, mine.data));
        CHECK (loaded.meta.patchId != mine.meta.patchId); // user copies get their own id
        CHECK (! pm.entries()[(size_t) hits[0]].factory);

        pm.setFavourite (loaded.meta.patchId, true);
        CHECK (pm.search ("", -1, {}, true).size() == 1);

        // Broken file in the user folder is skipped, never fatal.
        { std::ofstream f (dir + "/broken.fmlume"); f << "{ \"schemaVersion\": 1, "; }
        std::vector<std::string> errors;
        pm.rescanUser (&errors);
        CHECK (errors.size() == 1);
        CHECK (pm.entries().size() == (size_t) Category::Count + 1);

        // Export / import through arbitrary paths.
        const std::string ext = dir + "/exported.fmlume";
        CHECK (PresetManager::exportTo (ext, makeCategoryPreset (Category::SFX), &err));
        Preset imp;
        CHECK (PresetManager::importFrom (ext, imp, &err));
        CHECK (imp.meta.category == Category::SFX);
        CHECK (! PresetManager::importFrom (dir + "/does-not-exist.fmlume", imp, &err));
        CHECK (pm.deleteUser (pm.findById (loaded.meta.patchId), &err));
        CHECK (! pm.deleteUser (0, &err)); // factory presets are read-only
    }
    {
        // Favourites persist across instances.
        PresetManager pm2;
        pm2.setUserDirectory (dir);
        CHECK (pm2.search ("", -1, {}, true).empty()); // preset deleted, but the favourite id is remembered harmlessly
    }
    std::error_code ec;
    fs::remove_all (dir, ec);
    CHECK (PresetManager::sanitiseFileName ("a/b:c*?") == "a_b_c__");
    CHECK (PresetManager::sanitiseFileName ("  ..  ") == "Untitled");
}

FM_TEST ("integration", "Stereo gate fields are optional in the file and round-trip when used")
{
    // A mono preset is written without the extension and reads back with right == left.
    Preset mono = makeInitPreset();
    for (int i = 0; i < kGateSteps; ++i) mono.data.perf.gate.steps[(size_t) i] = mono.data.perf.gate.stepsRight[(size_t) i] = i % 3 == 0 ? 1.0f : 0.25f;
    std::string json, err;
    CHECK (presetToJson (mono, json, &err));
    CHECK (json.find ("stepsRight") == std::string::npos);
    Preset back;
    CHECK (presetFromJson (json, back, &err, nullptr));
    CHECK (! back.data.perf.gate.stereo);
    CHECK (back.data.perf.gate.stepsRight == mono.data.perf.gate.steps);
    CHECK (back.data.perf.gate.width == 1.0f);

    Preset stereo = mono;
    stereo.data.perf.gate.stereo = true;
    stereo.data.perf.gate.width = 0.6f;
    stereo.data.perf.gate.rate = 18; // 1/64, gate only
    for (int i = 0; i < kGateSteps; ++i) stereo.data.perf.gate.stepsRight[(size_t) i] = i % 2 == 0 ? 0.0f : 0.75f;
    CHECK (presetToJson (stereo, json, &err));
    Preset back2;
    CHECK (presetFromJson (json, back2, &err, nullptr));
    CHECK (back2.data.perf.gate.stereo);
    CHECK (back2.data.perf.gate.stepsRight == stereo.data.perf.gate.stepsRight);
    CHECK (back2.data.perf.gate.steps == stereo.data.perf.gate.steps);
    CHECK_NEAR (back2.data.perf.gate.width, 0.6f, 1.0e-6f);
    CHECK (back2.data.perf.gate.rate == 18);

    // Out-of-range values are refused.
    stereo.data.perf.gate.width = 1.5f;
    CHECK (! validatePatch (stereo.data).ok);
}

FM_TEST ("integration", "Algorithm shape match ignores unheard operators and operator numbering")
{
    for (int i = 0; i < numAlgorithmTemplates(); ++i)
    {
        EnginePatch e = makeInitEngine();
        applyAlgorithm (e, i);
        bool renumbered = true;
        CHECK (matchAlgorithmShape (e, &renumbered) == matchAlgorithm (e));
        CHECK (! renumbered);
    }
    // Two 2-operator stacks built on other operator numbers, plus an unconnected operator.
    EnginePatch e = makeInitEngine();
    for (auto& op : e.operators) { op = makeOp (1.0f, 1.0f, 0.01f, 0.3f, 0.5f, 0.2f); op.enabled = false; }
    e.numEdges = 0;
    for (int k : { 0, 1, 2, 3, 4 }) e.operators[(size_t) k].enabled = true;
    e.operators[1].carrierGain = 1.0f;
    e.operators[3].carrierGain = 1.0f;
    CHECK (addEdge (e, 0, 1, 1.0f));
    CHECK (addEdge (e, 2, 3, 1.0f));
    EnginePatch ref = makeInitEngine();
    int twoStacks = -1;
    for (int i = 0; i < numAlgorithmTemplates() && twoStacks < 0; ++i)
    {
        const auto& t = algorithmTemplate (i);
        int used = 0, carriers = 0;
        for (int k = 0; k < kNumOps; ++k) { used += (t.usedMask >> k) & 1; carriers += (t.carrierMask >> k) & 1; }
        if (used == 4 && carriers == 2 && t.numEdges == 2 && t.feedbackOp < 0 && t.edges[0][1] != t.edges[1][1]) twoStacks = i;
    }
    CHECK (twoStacks >= 0);
    CHECK (matchAlgorithm (e) == -1); // operator 5 is on but unconnected: no exact match
    CHECK (matchAlgorithmShape (e) == twoStacks);
    // A graph no template has.
    CHECK (addEdge (e, 4, 1, 1.0f));
    CHECK (addEdge (e, 4, 3, 1.0f));
    CHECK (addEdge (e, 0, 3, 1.0f));
    CHECK (matchAlgorithmShape (e) == -1 || matchAlgorithmShape (e) != twoStacks);
}

// ---- hostile input (security review, docs/DECISIONS.md D-040) ----
FM_TEST ("integration", "JSON parser enforces resource limits and is locale independent")
{
    Json j;
    std::string err;
    // too many keys in one object (the duplicate-key check is linear per member)
    std::string many = "{";
    for (int i = 0; i < 3000; ++i) many += (i ? ",\"k" : "\"k") + std::to_string (i) + "\":0";
    many += "}";
    CHECK (! Json::parse (many, j, &err));
    // too many values overall
    std::string big = "[";
    for (int i = 0; i < 250000; ++i) big += i ? ",0" : "0";
    big += "]";
    CHECK (! Json::parse (big, j, &err));
    // too large
    CHECK (! Json::parse (std::string (Json::kMaxJsonBytes + 1, ' '), j, &err));
    // normal documents still parse, and numbers are written with '.'
    CHECK (Json::parse ("{\"a\":[0.5,1e3,-2.25]}", j, &err));
    CHECK (j.find ("a")->items[0].number == 0.5);
    CHECK (j.dump (0).find (',') != std::string::npos);
    CHECK (j.dump (0).find ("0.5") != std::string::npos);
}

FM_TEST ("integration", "Preset loader: oversized metadata is dropped, out-of-range numbers are refused")
{
    Preset p = makeInitPreset();
    std::string deep = "{\"a\":";
    for (int i = 0; i < 40; ++i) deep += "[";
    deep += "0";
    for (int i = 0; i < 40; ++i) deep += "]";
    deep += "}";
    p.meta.metadataJson = deep;
    std::string json, err;
    CHECK (presetToJson (p, json, &err));
    Preset back;
    std::vector<std::string> warnings;
    CHECK (presetFromJson (json, back, &err, &warnings));
    CHECK (back.meta.metadataJson == "{}");
    CHECK (! warnings.empty());
    // ... so the preset still fits when nested inside project state
    std::string state;
    HostParams host;
    CHECK (stateToJson (back, host, state));
    Preset restored;
    CHECK (stateFromJson (state, restored, host, &err));

    // small metadata survives untouched
    p.meta.metadataJson = "{\"vendor\":{\"note\":\"keep me\"}}";
    CHECK (presetToJson (p, json, &err));
    CHECK (presetFromJson (json, back, &err, nullptr));
    CHECK (back.meta.metadataJson.find ("keep me") != std::string::npos);

    // absurd numbers are rejected before any narrowing conversion
    CHECK (presetToJson (makeInitPreset(), json, &err));
    const std::string key = "\"schemaVersion\": 2";
    const size_t at = json.find (key);
    CHECK (at != std::string::npos);
    if (at != std::string::npos)
    {
        std::string bad = json;
        bad.replace (at, key.size(), "\"schemaVersion\": 1e300");
        CHECK (! presetFromJson (bad, back, &err, nullptr));
    }
}

FM_TEST ("integration", "PresetManager refuses non-regular and oversized files and never writes through a planted temp file")
{
    const fs::path dir = fs::temp_directory_path() / ("fmlume-sec-" + makePatchId());
    fs::create_directories (dir);
    const fs::path outside = dir / "outside.txt";
    { std::ofstream f (outside); f << "precious"; }
    const fs::path user = dir / "Presets";
    fs::create_directories (user);
    std::error_code ec;
    // a device node and an oversized file dressed up as presets, plus a real one
    fs::create_symlink ("/dev/zero", user / "zero.fmlume", ec);
    { std::ofstream f (user / "huge.fmlume"); f << std::string (5u * 1024u * 1024u, ' '); }
    // temp names an older build would have used, pointing outside the folder
    fs::create_symlink (outside, user / "favorites.json.tmp", ec);
    fs::create_symlink (outside, user / "Mine.fmlume.tmp", ec);

    PresetManager pm;
    pm.setUserDirectory (user.string());
    std::vector<std::string> errors;
    pm.rescanUser (&errors);          // must return (no hang, no huge allocation)
    CHECK (pm.entries().empty());
    std::string err;
    CHECK (pm.saveUser (makeInitPreset(), "Mine", false, &err));
    pm.setFavourite ("some.id", true);
    CHECK (pm.entries().size() == 1);
    std::ifstream check (outside);
    std::string content;
    std::getline (check, content);
    CHECK (content == "precious");    // nothing was written through the planted links

    Preset p;
    CHECK (! PresetManager::importFrom ((user / "zero.fmlume").string(), p, &err));
    CHECK (! PresetManager::importFrom ((user / "huge.fmlume").string(), p, &err));
    CHECK (! PresetManager::importFrom (dir.string(), p, &err)); // a directory
    fs::remove_all (dir, ec);
}


FM_TEST ("integration", "Init sounds of every category are level-matched to the factory presets")
{
    // A three-note chord at master 0 dB must stay clear of the limiter and still be clearly audible.
    for (int c = 0; c < (int) Category::Count; ++c)
    {
        const Preset p = makeCategoryPreset ((Category) c);
        SynthEngine e;
        e.prepare (48000.0, 256);
        CHECK (e.setPatch (p.data));
        HostParams h = p.data.host;
        h.gainDb = 0.0f;
        h.arpEnabled = false;
        const int base = (Category) c == Category::Bass ? 36 : 55;
        auto r = renderSeq (e, 48000.0, 256, 1.0, { { 0.0, noteOn (base, 100) }, { 0.0, noteOn (base + 4, 100) }, { 0.0, noteOn (base + 9, 100) } }, h);
        const float pk = std::max (peak (r.l), peak (r.r));
        CHECK (pk < 0.8f);
        CHECK (pk > 0.05f);
    }
}
