#include "TestFramework.h"
#include "TestUtil.h"
#include "Core/PatchCompiler.h"
#include "Presets/PresetManager.h"
#include "Presets/PresetSerializer.h"
#include <map>
#include <set>

using namespace fmtest;

FM_TEST ("integration", "Factory bank: 108 presets, 12 per category, all valid, tagged, macro-complete and audible")
{
    PresetManager pm;
    std::vector<std::string> errors;
    const int count = pm.loadFactoryDirectory (std::string (FMLUME_SOURCE_DIR) + "/Resources/Presets", &errors);
    for (auto& e : errors) std::printf ("    %s\n", e.c_str());
    CHECK (errors.empty());
    CHECK (count >= 108);
    std::map<int, int> perCategory;
    std::set<std::string> ids, names;
    const double sr = 48000.0;
    for (int i = 0; i < (int) pm.entries().size(); ++i)
    {
        Preset p;
        std::string err;
        CHECK (pm.load (i, p, &err));
        ++perCategory[(int) p.meta.category];
        CHECK (ids.insert (p.meta.patchId).second);       // stable, unique ids
        CHECK (names.insert (p.meta.displayName).second);
        CHECK (p.meta.patchId.rfind ("factory.", 0) == 0);
        CHECK (! p.meta.tags.empty());
        CHECK (p.meta.description.find ("Velocity:") != std::string::npos);
        const auto v = validatePatch (p.data);
        CHECK (v.ok && v.warnings.empty());
        for (int m = 0; m < kNumMacros; ++m)
        {
            CHECK (p.data.a.macros[(size_t) m].numMappings > 0);
            CHECK (! p.meta.macroNames[(size_t) m].empty());
            CHECK (p.data.a.macros[(size_t) m].defaultValue == 0.5f);
        }
        // Common macro names keep their meaning across the whole bank.
        CHECK (p.meta.macroNames[0] == "Brightness" && p.meta.macroNames[1] == "Attack" && p.meta.macroNames[2] == "Release" && p.meta.macroNames[3] == "Character");
        for (int q = 0; q < kNumQuick; ++q) CHECK (p.data.fx.quick[(size_t) q].numMappings > 0);

        SynthEngine e;
        e.prepare (sr, 256);
        CHECK (e.setPatch (p.data));
        const int note = p.meta.category == Category::Bass ? 36 : 60;
        TransportInfo tr;
        tr.valid = tr.playing = true;
        auto r = renderSeq (e, sr, 256, 0.8, { { 0.0, noteOn (note, 100) }, { 0.0, noteOn (note + 7, 100) }, { 0.5, noteOff (note) }, { 0.5, noteOff (note + 7) } }, p.data.host, tr);
        CHECK (allFinite (r.l) && allFinite (r.r));
        const float pk = std::max (peak (r.l), peak (r.r));
        CHECK (pk > 0.01f);  // audible at the default master gain
        CHECK (pk < 0.9f);   // and not riding the limiter
        if (pk <= 0.01f || pk >= 0.9f) std::printf ("    %s peak %.3f\n", p.meta.displayName.c_str(), pk);
        // All FX off still gives a usable FM tone (SPEC §10.1).
        HostParams dry = p.data.host;
        dry.fxBypassAll = true;
        SynthEngine e2;
        e2.prepare (sr, 256);
        CHECK (e2.setPatch (p.data));
        auto d = renderSeq (e2, sr, 256, 0.5, { { 0.0, noteOn (note, 100) } }, dry, tr);
        CHECK (peak (d.l) > 0.01f);
    }
    CHECK (perCategory.size() == (size_t) Category::Count);
    for (auto& kv : perCategory) CHECK (kv.second >= 12);
    CHECK (pm.allTags().size() >= 8);
    CHECK (pm.search ("", -1, "Cinematic").size() >= 8);
    CHECK (pm.search ("bell").size() >= 5);
}
