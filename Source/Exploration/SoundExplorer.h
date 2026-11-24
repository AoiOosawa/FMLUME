// Sound Explorer v1 (SPEC §8): seeded, constrained, rule-based variation of the current patch.
// No machine learning, no network access; everything runs locally on a non-realtime thread.
#pragma once
#include "Core/Model.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace fmlume
{
enum class ExploreDirection : uint8_t { Surprise, Brighter, Darker, Warmer, MoreMetallic, Softer, Punchier, MoreMotion, Count };
enum class ExploreStrength : uint8_t { Subtle, Medium, Wild, Count };

struct ExploreLocks
{
    bool graph = false;      // operator routing / enabled set / waveforms
    bool ratios = false;     // pitch ratios, fixed frequencies, key tracking
    bool envelopes = false;  // operator and mod envelopes
    bool fx = false;         // every FX parameter
    std::array<bool, kNumMacros> macros {}; // a locked macro keeps its mappings and its target parameters
};

struct ExploreSettings
{
    ExploreDirection direction = ExploreDirection::Surprise;
    ExploreStrength strength = ExploreStrength::Medium;
    ExploreLocks locks;
};

struct ExploreScores
{
    float brightness = 0, metallic = 0, softness = 0, punch = 0, motion = 0, warmth = 0;
};

struct ExploreCandidate
{
    PatchData data;
    std::string name;       // short label, e.g. "Brighter / Punchier"
    std::string tendency;   // one-line description of how it differs from the parent
    uint64_t seed = 0;
    uint64_t parentChecksum = 0;
    bool fallback = false;  // produced by the safe small-change fallback
    ExploreScores scores;
};

constexpr int kNumCandidates = 8;

const char* exploreDirectionName (ExploreDirection d);
const char* exploreStrengthName (ExploreStrength s);

// The same generation as SoundExplorer::generate, one candidate per next() call, so a worker
// thread can hand each candidate over as soon as it is ready. Deterministic like generate().
class ExploreRun
{
public:
    ExploreRun (const PatchData& parent, const ExploreSettings& settings, uint64_t seed);
    ~ExploreRun();
    bool done() const;
    // Adds the next candidate. Returns false (and adds nothing) if `cancel` was raised meanwhile.
    bool next (const std::atomic<bool>* cancel = nullptr);
    const std::vector<ExploreCandidate>& candidates() const;

private:
    struct State;
    std::unique_ptr<State> s;
};

class SoundExplorer
{
public:
    // Deterministic: the same parent, settings and seed always give the same eight candidates.
    static std::vector<ExploreCandidate> generate (const PatchData& parent, const ExploreSettings& settings, uint64_t seed);

    // Heuristic descriptors (not a perceptual classifier; see UI help text).
    static ExploreScores score (const PatchData& d);
    // Mean absolute difference of the explorable parameters in the normalised domain.
    static float distance (const PatchData& a, const PatchData& b);
    // True if every locked aspect of `candidate` is identical to `parent`.
    static bool locksRespected (const PatchData& parent, const PatchData& candidate, const ExploreLocks& locks);
    // Offline safety check: valid, finite, audible, not excessively loud. Returns the RMS level.
    static bool isSafe (const PatchData& d, float* rmsOut = nullptr, std::string* why = nullptr);
    // Perceived loudness (A-weighted, loudest 400 ms of a held chord) in dB: what candidates are levelled by.
    static float loudnessDb (const PatchData& d, double holdSeconds = 0.8);
    // How long the measuring chord is held for this sound: longer for slow swells.
    static double measuringHold (const PatchData& d);
};
} // namespace fmlume
