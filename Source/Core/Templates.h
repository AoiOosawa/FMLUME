// Algorithm (routing) templates and per-category starting patches (SPEC §4.4, §6.3, Phase 2).
#pragma once
#include "Core/Model.h"
#include <string>

namespace fmlume
{
struct AlgorithmTemplate
{
    const char* name;
    const char* diagram;       // human readable, 1-based operator numbers
    int numEdges;
    int8_t edges[8][2];        // from, to (0-based)
    uint8_t carrierMask;       // bit k = operator k is a carrier
    uint8_t usedMask;          // bit k = operator k enabled
    int8_t feedbackOp;         // -1 = none
};

int numAlgorithmTemplates();
const AlgorithmTemplate& algorithmTemplate (int index);
// Rewires an engine to a template. Operator tone settings (ratio, envelopes, waveform) are kept;
// indices of edges that already existed are kept, new edges start at `defaultIndex` radians.
void applyAlgorithm (EnginePatch& e, int templateIndex, float defaultIndex = 2.0f);
// Index of the template matching the engine's current routing, or -1 for a custom graph.
int matchAlgorithm (const EnginePatch& e);

// Looser match for display: operators that cannot be heard (switched off, or not connected to
// any carrier) are ignored, and the operators may be numbered differently from the template.
// Returns the template index or -1; `renumbered` tells whether the numbering differs.
int matchAlgorithmShape (const EnginePatch& e, bool* renumbered = nullptr);

// ---- patch building helpers (also used by the factory preset generator) ----
OperatorState makeOp (float ratio, float level, float a, float d, float s, float r, float carrierGain = 0.0f);
bool addEdge (EnginePatch& e, int from, int to, float index);
bool removeEdge (EnginePatch& e, int from, int to);
bool addMapping (Macro& m, int target, float amount, Curve curve = Curve::Linear, Polarity pol = Polarity::Bipolar,
                 float min = 0.0f, float max = 1.0f);
void setModSlot (SoundPatch& s, int slot, ModSource src, int target, float depth);

// Rebuilds macros 1-4 (Brightness / Attack / Release / Character) from the engine's current graph.
void assignCommonMacros (PatchData& d);
// Default FX quick controls: Warmth (Drive/EQ), Width (Chorus), Space (Reverb), Echo (Delay).
void assignQuickControls (PatchData& d);

const char* categoryMacroName (Category c, int macroIndex); // macroIndex 0..7

// A complete, playable starting patch for a category with all 8 macros assigned.
Preset makeCategoryPreset (Category c);
} // namespace fmlume
