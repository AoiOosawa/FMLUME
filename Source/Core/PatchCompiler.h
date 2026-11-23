// Validation and compilation of the canonical model into immutable audio data (SPEC §3, §4.1).
#pragma once
#include "Core/Model.h"
#include <string>
#include <vector>

namespace fmlume
{
struct CompiledEngine
{
    int order[kNumOps] {};      // topological order (modulators first) of enabled operators
    int numActive = 0;
    int inCount[kNumOps] {};    // incoming edges per operator (from enabled operators only)
    int8_t inFrom[kNumOps][kNumOps] {};
    int8_t inEdge[kNumOps][kNumOps] {}; // index into EnginePatch::edges
    uint64_t structureHash = 0;
};

enum class MorphKind : uint8_t { Off, Compatible, Dual };

struct CompiledPatch
{
    PatchData data;
    CompiledEngine ea, eb;
    bool compatible = false;   // A/B structurally compatible (SPEC §9.2)
    uint64_t structureHash = 0;
    bool forceNewGeneration = false;
};

struct ValidationResult
{
    bool ok = true;
    std::string error;
    std::vector<std::string> warnings;
};

// True if adding from->to would create a cycle (or is a self / duplicate edge).
bool edgeWouldBeInvalid (const EnginePatch& e, int from, int to);
bool hasCycle (const EnginePatch& e);

// Strict validation: rejects out-of-range / non-finite values, cyclic or duplicate edges,
// patches without an audible carrier, malformed macro mappings. Never clamps.
ValidationResult validatePatch (const PatchData& d);
// Repairs soft problems that validation only warns about (mod slots / macro mappings whose target
// does not exist, e.g. a removed edge). Returns the number of items disabled.
int sanitizeTargets (PatchData& d, std::vector<std::string>* warnings = nullptr);

bool morphCompatible (const PatchData& d);
MorphKind resolveMorphKind (const PatchData& d, bool morphEnabled);

// Compiles a validated patch. Returns false (and leaves `out` untouched) if invalid.
bool compilePatch (const PatchData& d, CompiledPatch& out, std::string* error = nullptr);
} // namespace fmlume
