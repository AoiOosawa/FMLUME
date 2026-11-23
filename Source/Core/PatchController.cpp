#include "Core/PatchController.h"
#include "Core/Params.h"
#include "Core/PatchCompiler.h"
#include "Core/Templates.h"
#include "Presets/PresetSerializer.h"

namespace fmlume
{
PatchController::PatchController() { current = makeCategoryPreset (Category::EPiano); }

bool PatchController::push (bool forceNew)
{
    if (! sendPatch) return true;
    return sendPatch (current.data, forceNew);
}

bool PatchController::edit (const std::function<void (Preset&)>& fn, bool forceNew)
{
    Preset next = current;
    fn (next);
    sanitizeTargets (next.data);
    const auto v = validatePatch (next.data);
    if (! v.ok)
    {
        error = v.error;
        return false; // the engine keeps the previous valid patch
    }
    error.clear();
    if (previewing >= 0) previewing = -1; // editing ends an audition
    current = std::move (next);
    dirty = true;
    push (forceNew);
    notify();
    return true;
}

bool PatchController::setParam (bool sideB, int code, float value)
{
    if (! paramExists (current.data, sideB, code)) return false;
    return edit ([=] (Preset& p) { fmlume::setParam (p.data, sideB, code, value); });
}

float PatchController::getParam (bool sideB, int code) const { return fmlume::getParam (current.data, sideB, code); }

bool PatchController::addEdge (bool sideB, int from, int to, float index)
{
    const auto& e = (sideB ? current.data.b : current.data.a).engine;
    if (edgeWouldBeInvalid (e, from, to)) { error = "That connection would create a loop or already exists."; return false; }
    return edit ([=] (Preset& p) { fmlume::addEdge ((sideB ? p.data.b : p.data.a).engine, from, to, index); });
}

bool PatchController::removeEdge (bool sideB, int from, int to)
{
    return edit ([=] (Preset& p) { fmlume::removeEdge ((sideB ? p.data.b : p.data.a).engine, from, to); });
}

bool PatchController::setOperatorEnabled (bool sideB, int op, bool enabled)
{
    return edit ([=] (Preset& p) {
        auto& o = (sideB ? p.data.b : p.data.a).engine.operators[(size_t) op];
        o.enabled = enabled;
        if (enabled && o.level <= 0.0f) o.level = 1.0f;
    }, enabled ? "Enable Operator" : "Disable Operator");
}

bool PatchController::applyAlgorithmTemplate (bool sideB, int templateIndex)
{
    return edit ([=] (Preset& p) {
        applyAlgorithm ((sideB ? p.data.b : p.data.a).engine, templateIndex);
        sanitizeTargets (p.data);
        // Keep the four common macros meaningful for the new routing.
        assignCommonMacros (p.data, sideB);
    });
}

void PatchController::loadPreset (const Preset& p)
{
    if (! validatePatch (p.data).ok) { error = validatePatch (p.data).error; return; }
    closeExplorer();
    current = p;
    sanitizeTargets (current.data);
    if (! current.data.hasB) { current.data.b = current.data.a; current.meta.macroNamesB = current.meta.macroNames; }
    dirty = false;
    editB = false;
    push (true);
    if (applyHostValues) applyHostValues (current.data.host);
    notify();
}

void PatchController::newFromCategory (Category c)
{
    Preset p = makeCategoryPreset (c);
    p.meta.patchId = makePatchId();
    loadPreset (p);
    dirty = true;
    notify();
}

void PatchController::markSaved (const Preset& saved)
{
    current.meta = saved.meta;
    dirty = false;
    notify();
}

Preset PatchController::presetForSave (const HostParams& live) const
{
    Preset p = current;
    auto& h = p.data.host;
    h = live;
    for (int i = 0; i < kNumMacros; ++i)
    {
        p.data.a.macros[(size_t) i].defaultValue = clampf (live.macro[(size_t) i], 0.0f, 1.0f);
        p.data.b.macros[(size_t) i].defaultValue = p.data.a.macros[(size_t) i].defaultValue;
    }
    p.meta.pluginVersion = FMLUME_VERSION_STRING;
    return p;
}

bool PatchController::setState (const Preset& p)
{
    if (! validatePatch (p.data).ok) { error = validatePatch (p.data).error; return false; }
    closeExplorer();
    current = p;
    sanitizeTargets (current.data);
    if (! current.data.hasB) { current.data.b = current.data.a; current.meta.macroNamesB = current.meta.macroNames; }
    dirty = false;
    editB = false;
    push (true);
    notify();
    return true;
}

// ------------------------------------------------------------------------------------- morph
bool PatchController::loadIntoSlot (bool sideB, const Preset& source)
{
    return edit ([&] (Preset& p) {
        EngineSide& dst = sideB ? p.data.b : p.data.a;
        dst = source.data.a; // sound engine + macro assignments + mod matrix; FX / performance stay shared
        (sideB ? p.meta.macroNamesB : p.meta.macroNames) = source.meta.macroNames;
        (sideB ? p.meta.slotNameB : p.meta.slotNameA) = source.meta.displayName;
        // FX-targeting mappings of slot B would double up with the shared chain: they are inert by design.
        if (sideB) p.data.hasB = true;
    }, true);
}

bool PatchController::removeSlotB()
{
    if (! current.data.hasB) return false;
    editB = false;
    return edit ([] (Preset& p) {
        p.data.hasB = false;
        p.data.host.morphEnabled = false;
        p.meta.slotNameA.clear();
        p.meta.slotNameB.clear();
    }, true);
}

bool PatchController::swapSlots()
{
    if (! current.data.hasB) return false;
    return edit ([] (Preset& p) {
        std::swap (p.data.a, p.data.b);
        std::swap (p.meta.macroNames, p.meta.macroNamesB);
        // the names travel with the sounds (an unnamed A is the preset itself)
        if (p.meta.slotNameA.empty()) p.meta.slotNameA = p.meta.displayName;
        std::swap (p.meta.slotNameA, p.meta.slotNameB);
    }, true);
}

bool PatchController::copySlot (bool fromB)
{
    if (fromB && ! current.data.hasB) return false;
    return edit ([=] (Preset& p) {
        if (fromB) { p.data.a = p.data.b; p.meta.macroNames = p.meta.macroNamesB; }
        else { p.data.b = p.data.a; p.meta.macroNamesB = p.meta.macroNames; p.data.hasB = true; }
    }, true);
}

void PatchController::setEditingB (bool b)
{
    editB = b && current.data.hasB;
    notify();
}

// ------------------------------------------------------------------------------------- explorer
bool PatchController::runExplore (const PatchData& parent, bool keepHistory)
{
    if (exploreActive && keepHistory)
    {
        exploreHistory.push_back ({ exploreParent, cands, seed });
        if ((int) exploreHistory.size() > kMaxExploreHistory) exploreHistory.erase (exploreHistory.begin());
    }
    exploreParent = parent;
    cands = SoundExplorer::generate (parent, exploreSettings, seed);
    exploreActive = true;
    if (previewing >= 0) { previewing = -1; push (true); }
    notify();
    return ! cands.empty();
}

bool PatchController::explore()
{
    if (! exploreActive) exploreHistory.clear();
    ++seed;
    return runExplore (current.data, exploreActive);
}

bool PatchController::regenerate()
{
    if (! exploreActive) return explore();
    seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
    return runExplore (exploreParent, true);
}

bool PatchController::exploreFromThis (int candidate)
{
    if (candidate < 0 || candidate >= (int) cands.size()) return false;
    const PatchData parent = cands[(size_t) candidate].data;
    seed = cands[(size_t) candidate].seed ^ 0x5bd1e995u;
    return runExplore (parent, true);
}

bool PatchController::exploreBack()
{
    if (exploreHistory.empty()) return false;
    ExploreStep s = std::move (exploreHistory.back());
    exploreHistory.pop_back();
    exploreParent = s.parent;
    cands = std::move (s.cands);
    seed = s.seed;
    if (previewing >= 0) { previewing = -1; push (true); }
    notify();
    return true;
}

bool PatchController::preview (int candidate)
{
    if (candidate < 0 || candidate >= (int) cands.size()) return false;
    previewing = candidate;
    if (sendPatch) sendPatch (cands[(size_t) candidate].data, true);
    notify();
    return true;
}

void PatchController::cancelPreview()
{
    if (previewing < 0) return;
    previewing = -1;
    push (true); // the untouched current patch
    notify();
}

bool PatchController::useCandidate (int candidate)
{
    if (candidate < 0 || candidate >= (int) cands.size()) return false;
    const PatchData chosen = cands[(size_t) candidate].data;
    previewing = -1;
    return edit ([&] (Preset& p) {
        // Only the sound changes: slot B, shared performance settings and host values are kept.
        const PatchData keep = p.data;
        p.data = chosen;
        p.data.b = keep.b;
        p.data.hasB = keep.hasB;
        p.data.host = keep.host;
        p.data.perf = keep.perf;   // arp / chord / gate edits made while exploring are kept
        p.data.morph = keep.morph;
    }, true);
}

void PatchController::closeExplorer()
{
    const bool wasPreviewing = previewing >= 0;
    exploreActive = false;
    cands.clear();
    exploreHistory.clear();
    previewing = -1;
    if (wasPreviewing) push (true);
}
} // namespace fmlume
