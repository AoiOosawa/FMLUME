#include "Core/Model.h"
#include <iterator>
#include "Core/Params.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace fmlume
{
static const char* const kDivBase[7] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" };

double syncDivBeats (int div)
{
    if (div < 0 || div >= kNumSyncDivs) div = 12;
    const int base = div / 3, mod = div % 3;
    double beats = 4.0 / double (1 << base);
    if (mod == 1) beats *= 1.5;
    if (mod == 2) beats *= 2.0 / 3.0;
    return beats;
}

const char* syncDivName (int div)
{
    static char names[kNumSyncDivs][8];
    static bool init = false;
    if (! init)
    {
        for (int i = 0; i < kNumSyncDivs; ++i)
            std::snprintf (names[i], sizeof (names[i]), "%s%s", kDivBase[i / 3], i % 3 == 1 ? "." : (i % 3 == 2 ? "T" : ""));
        init = true;
    }
    if (div < 0 || div >= kNumSyncDivs) div = 12;
    return names[div];
}

int syncDivFromName (const std::string& s)
{
    for (int i = 0; i < kNumSyncDivs; ++i)
        if (s == syncDivName (i)) return i;
    return -1;
}

static const char* const kCategoryNames[] = { "Arp", "E.Piano", "Bell", "Bass", "Lead", "Pad", "Poly", "Pluck", "Brass", "Keys", "SFX" };
static_assert (std::size (kCategoryNames) == (size_t) Category::Count, "every Category needs a name");
const char* categoryName (Category c) { return kCategoryNames[std::min<int> ((int) c, (int) Category::Count - 1)]; }
int categoryFromName (const std::string& s)
{
    for (int i = 0; i < (int) Category::Count; ++i) if (s == kCategoryNames[i]) return i;
    return -1;
}

static const char* const kFxNames[] = { "eq", "drive", "chorus", "phaser", "flanger", "compressor", "bitcrusher", "delay", "reverb" };
static_assert (std::size (kFxNames) == (size_t) FxType::Count && (int) FxType::Count == kNumFx, "every FxType needs a name");
const char* fxTypeName (FxType t) { return kFxNames[std::min<int> ((int) t, kNumFx - 1)]; }
int fxTypeFromName (const std::string& s)
{
    for (int i = 0; i < kNumFx; ++i) if (s == kFxNames[i]) return i;
    return -1;
}

EnginePatch makeInitEngine()
{
    EnginePatch e;
    for (auto& op : e.operators) op = OperatorState {};
    auto& op1 = e.operators[0];
    op1.enabled = true;
    op1.carrierGain = 1.0f;
    op1.env = { 0.01f, 0.5f, 0.6f, 0.3f };
    e.modEnv = { 0.01f, 0.5f, 0.6f, 0.3f };
    return e;
}

PatchData makeInitPatchData()
{
    PatchData d;
    d.a.engine = makeInitEngine();
    d.b.engine = makeInitEngine();
    for (int t = 0; t < kNumFx; ++t)
        for (int p = 0; p < kFxParams; ++p)
            d.fx.proc[(size_t) t].p[(size_t) p] = fxParamDesc (t, p).def;
    return d;
}

Preset makeInitPreset()
{
    Preset p;
    p.meta.pluginVersion = FMLUME_VERSION_STRING;
    p.meta.patchId = "init";
    p.meta.displayName = "Init";
    p.meta.author = "FMLUME";
    static const char* const common[4] = { "Brightness", "Attack", "Release", "Character" };
    for (int i = 0; i < kNumMacros; ++i)
    {
        p.meta.macroNames[(size_t) i] = i < 4 ? common[i] : ("Macro " + std::to_string (i + 1));
        p.meta.macroNamesB[(size_t) i] = p.meta.macroNames[(size_t) i];
    }
    p.data = makeInitPatchData();
    return p;
}
} // namespace fmlume
