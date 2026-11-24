// Single owner of the editable patch (Canonical Model) on the non-realtime side.
// Every UI edit, preset load and Explorer action goes through here, so
// Simple and Advanced always edit the same state (SPEC §6.4). Not thread-safe: the owner calls it
// under its own mutex (the plug-in: modelMutex; that includes host threads restoring a project).
#pragma once
#include "Core/Model.h"
#include "Exploration/SoundExplorer.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

namespace fmlume
{
class PatchController
{
public:
    PatchController();
    ~PatchController();
    PatchController (const PatchController&) = delete;
    PatchController& operator= (const PatchController&) = delete;

    // ---- wiring (set once by the owner) ------------------------------------------------
    // Sends a validated patch to the audio engine. forceNew = start a new patch generation.
    std::function<bool (const PatchData&, bool forceNew)> sendPatch;
    // Asks the owner to apply preset-stored host values (macro defaults, unison, ...) to the host parameters.
    std::function<void (const HostParams&)> applyHostValues;
    // Called after any change to the model / explorer state (UI refresh).
    std::function<void()> changed;

    // ---- model access -------------------------------------------------------------------
    const Preset& preset() const { return current; }
    const PatchData& data() const { return current.data; }
    bool isModelDirty() const { return dirty; } // the patch itself; see isDirty (HostParams) for the header's mark
    // The preset also stores host values (macros, quick FX, unison, ...) that live in
    // the host parameters, not in the model. The owner records them whenever they match the preset
    // (after a load / state restore / save); moving one of them afterwards makes the preset dirty too.
    void setHostBaseline (const HostParams& h) { hostBaseline = h; }
    bool isDirty (const HostParams& live) const { return dirty || ! presetHostValuesEqual (live, hostBaseline); }
    // Compares only the host values a preset stores (not master gain / pan / quality / FX bypass, D-010).
    static bool presetHostValuesEqual (const HostParams& a, const HostParams& b);
    const std::string& lastError() const { return error; }

    // Applies an edit. The mutated copy is validated first; invalid results are rejected and
    // never reach the engine. There is no undo history (docs/DECISIONS.md D-042).
    bool edit (const std::function<void (Preset&)>& fn, bool forceNew = false);
    bool setParam (int code, float value);
    float getParam (int code) const;

    // Graph editing helpers (Advanced > Algorithm). All keep the patch valid.
    bool addEdge (int from, int to, float index = 1.5f);
    bool removeEdge (int from, int to);
    bool setOperatorEnabled (int op, bool enabled);
    bool applyAlgorithmTemplate (int templateIndex);

    // ---- presets --------------------------------------------------------------------------
    void loadPreset (const Preset& p);
    void newFromCategory (Category c);
    void markSaved (const Preset& saved); // after a successful save: adopt id / name, clear dirty
    // The current preset with live host values folded in (what gets written to disk / state).
    Preset presetForSave (const HostParams& live) const;
    bool setState (const Preset& p, bool edited = false); // host project state; `edited` = it was edited when saved

    // ---- Sound Explorer (SPEC §8, §14.3) ----------------------------------------------------
    ExploreSettings exploreSettings;
    // false (tests, tools): explore() returns with all eight candidates. true (the plug-in): they
    // are made on a worker thread and arrive one by one through pollExplore().
    bool asyncExplore = false;
    bool pollExplore();                     // message thread: takes over finished candidates; true if anything changed
    bool exploring() const { return exploreActive; }
    bool exploreBusy() const { return busy; } // candidates still being made
    bool explore();                         // candidates from the current patch
    bool regenerate();                      // same parent, new seed
    bool exploreFromThis (int candidate);   // candidate becomes the new parent (not yet adopted)
    bool exploreBack();                     // previous parent (history, max 20)
    bool preview (int candidate);           // audition without touching the current patch
    void cancelPreview();                   // back to the current patch
    bool useCandidate (int candidate);      // adopt as the current sound
    void closeExplorer();
    const std::vector<ExploreCandidate>& candidates() const { return cands; }
    int previewIndex() const { return previewing; }
    bool hasExploreHistory() const { return ! exploreHistory.empty(); }
    uint64_t exploreSeed() const { return seed; }
    void setExploreSeed (uint64_t s) { seed = s; }

    static constexpr int kMaxExploreHistory = 20;

private:
    struct ExploreStep
    {
        PatchData parent;
        std::vector<ExploreCandidate> cands;
        uint64_t seed = 0;
    };
    bool push (bool forceNew);
    void notify() { if (changed) changed(); }
    bool runExplore (const PatchData& parent, bool keepHistory);
    bool adopt (const Preset&, bool edited); // shared by loadPreset / setState
    void endPreview();                       // back to the current patch if a candidate was being heard
    void stopWorker();

    Preset current;
    bool dirty = false;
    HostParams hostBaseline;
    std::string error;

    PatchData exploreParent;
    bool exploreActive = false;
    std::vector<ExploreCandidate> cands;
    std::vector<ExploreStep> exploreHistory;
    int previewing = -1;
    uint64_t seed = 1;

    // background generation (asyncExplore)
    std::thread worker;
    std::atomic<bool> cancelWorker { false };
    std::mutex workerMutex;
    std::vector<ExploreCandidate> workerOut; // guarded by workerMutex
    bool workerDone = false;                 // guarded by workerMutex
    bool busy = false;
};
} // namespace fmlume
