// Sound Explorer v1 (SPEC §8): seeded, constrained, rule-based variation of the current patch.
// No machine learning, no network access; everything runs locally on a non-realtime thread.
#pragma once
#include "Core/Model.h"
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
};
} // namespace fmlume
