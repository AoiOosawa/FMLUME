#include "Core/PatchController.h"
#include "Core/Params.h"
#include "Core/PatchCompiler.h"
#include "Core/Templates.h"
#include "Presets/PresetSerializer.h"
#include <algorithm>
#include <cmath>

namespace fmlume
{
PatchController::PatchController() { current = makeCategoryPreset (Category::EPiano); }

bool PatchController::presetHostValuesEqual (const HostParams& a, const HostParams& b)
{
    // tolerant: values pass through the host's normalised 0..1 range and back
    const auto same = [] (float x, float y) { return std::abs (x - y) <= 1.0e-4f * std::max (1.0f, std::abs (y)); };
    for (int i = 0; i < kNumMacros; ++i) if (! same (a.macro[(size_t) i], b.macro[(size_t) i])) return false;
    for (int i = 0; i < kNumQuick; ++i) if (! same (a.quick[(size_t) i], b.quick[(size_t) i])) return false;
    return a.polyphony == b.polyphony && a.unison == b.unison && same (a.detune, b.detune) && same (a.spread, b.spread)
        && a.arpEnabled == b.arpEnabled && a.gateEnabled == b.gateEnabled;
}

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

bool PatchController::setParam (int code, float value)
{
    if (! paramExists (current.data, code)) return false;
    return edit ([=] (Preset& p) { fmlume::setParam (p.data, code, value); });
}

float PatchController::getParam (int code) const { return fmlume::getParam (current.data, code); }

bool PatchController::addEdge (int from, int to, float index)
{
    const auto& e = current.data.sound.engine;
    if (edgeWouldBeInvalid (e, from, to)) { error = "That connection would create a loop or already exists."; return false; }
    return edit ([=] (Preset& p) { fmlume::addEdge (p.data.sound.engine, from, to, index); });
}

bool PatchController::removeEdge (int from, int to)
{
    return edit ([=] (Preset& p) { fmlume::removeEdge (p.data.sound.engine, from, to); });
}

bool PatchController::setOperatorEnabled (int op, bool enabled)
{
    return edit ([=] (Preset& p) {
        auto& o = p.data.sound.engine.operators[(size_t) op];
        o.enabled = enabled;
        if (enabled && o.level <= 0.0f) o.level = 1.0f;
    }, enabled ? "Enable Operator" : "Disable Operator");
}

bool PatchController::applyAlgorithmTemplate (int templateIndex)
{
    return edit ([=] (Preset& p) {
        applyAlgorithm (p.data.sound.engine, templateIndex);
        sanitizeTargets (p.data);
        // Keep the four common macros meaningful for the new routing.
        assignCommonMacros (p.data);
    });
}

void PatchController::loadPreset (const Preset& p)
{
    if (! validatePatch (p.data).ok) { error = validatePatch (p.data).error; return; }
    closeExplorer();
    current = p;
    sanitizeTargets (current.data);
    dirty = false;
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
        p.data.sound.macros[(size_t) i].defaultValue = clampf (live.macro[(size_t) i], 0.0f, 1.0f);
    p.meta.pluginVersion = FMLUME_VERSION_STRING;
    return p;
}

bool PatchController::setState (const Preset& p)
{
    if (! validatePatch (p.data).ok) { error = validatePatch (p.data).error; return false; }
    closeExplorer();
    current = p;
    sanitizeTargets (current.data);
    dirty = false;
    push (true);
    notify();
    return true;
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
        // Only the sound changes: performance settings and host values are kept.
        const PatchData keep = p.data;
        p.data = chosen;
        p.data.host = keep.host;
        p.data.perf = keep.perf;   // arp / chord / gate edits made while exploring are kept
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
