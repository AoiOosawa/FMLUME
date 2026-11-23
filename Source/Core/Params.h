// Parameter code table: one integer code per continuous patch parameter.
// Used by macros, mod matrix, Explorer, Compatible Morph, JSON target ids and the UI.
#pragma once
#include "Core/Model.h"
#include <cmath>
#include <string>

namespace fmlume
{
enum class Scale : uint8_t { Linear, Log };

enum ParamFlags : uint32_t
{
    kFlagNone = 0,
    kFlagDiscrete = 1u << 0,   // stepped value: never a macro / mod target
    kFlagModTarget = 1u << 1,  // allowed as Mod Matrix target (SPEC §7)
    kFlagVirtual = 1u << 2,    // no stored base value (pitch / level / pan offsets)
    kFlagWet = 1u << 3,
    // Explorer groups
    kGroupIndex = 1u << 8, kGroupLevel = 1u << 9, kGroupEnv = 1u << 10, kGroupFilter = 1u << 11,
    kGroupMod = 1u << 12, kGroupFx = 1u << 13, kGroupRatio = 1u << 14, kGroupMisc = 1u << 15
};

struct ParamDesc
{
    const char* name;
    float min, max, def;
    Scale scale;
    uint32_t flags;
    const char* unit;
};

enum OpField
{
    kOpRatio, kOpFixedHz, kOpDetune, kOpLevel, kOpVelToLevel, kOpVelToIndex, kOpKeyTracking,
    kOpEnvA, kOpEnvD, kOpEnvS, kOpEnvR, kOpPhaseOffset, kOpFeedback, kOpCarrierGain, kOpFieldCount
};

namespace pc
{
constexpr int opBase = 0;
constexpr int edgeBase = kNumOps * kOpFieldCount;          // 84
constexpr int filterCutoff = edgeBase + kNumOps * kNumOps; // 120
constexpr int filterRes = filterCutoff + 1;
constexpr int filterDrive = filterCutoff + 2;
constexpr int lfo1Rate = filterCutoff + 3;
constexpr int lfo2Rate = filterCutoff + 4;
constexpr int modEnvA = filterCutoff + 5;
constexpr int modEnvD = modEnvA + 1;
constexpr int modEnvS = modEnvA + 2;
constexpr int modEnvR = modEnvA + 3;
constexpr int pitch = modEnvA + 4;  // 129
constexpr int level = pitch + 1;
constexpr int pan = pitch + 2;
constexpr int matrixDepthBase = pitch + 3; // 132
constexpr int engineCount = matrixDepthBase + kNumModSlots; // 140
constexpr int fxBase = engineCount;
constexpr int fxEnd = fxBase + kNumFx * kFxParams; // 212
constexpr int portamento = fxEnd;                  // shared performance parameter
constexpr int total = portamento + 1;

constexpr int op (int opIndex, int field) { return opBase + opIndex * kOpFieldCount + field; }
constexpr int edge (int from, int to) { return edgeBase + from * kNumOps + to; }
constexpr int fx (int type, int p) { return fxBase + type * kFxParams + p; }
constexpr int fx (FxType type, int p) { return fx ((int) type, p); }
constexpr bool isEngine (int code) { return code >= 0 && code < engineCount; }
constexpr bool isFx (int code) { return code >= fxBase && code < fxEnd; }
constexpr bool isShared (int code) { return code >= fxBase && code < total; }
constexpr bool isEdge (int code) { return code >= edgeBase && code < filterCutoff; }
constexpr bool isOp (int code) { return code >= 0 && code < edgeBase; }
} // namespace pc

const ParamDesc& paramDesc (int code);
const ParamDesc& fxParamDesc (int type, int p);
int fxNumParams (int type);

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline float toNorm (const ParamDesc& d, float v)
{
    v = clampf (v, d.min, d.max);
    if (d.scale == Scale::Log) return std::log (v / d.min) / std::log (d.max / d.min);
    return (v - d.min) / (d.max - d.min);
}
inline float fromNorm (const ParamDesc& d, float n)
{
    n = clampf (n, 0.0f, 1.0f);
    if (d.scale == Scale::Log) return d.min * std::pow (d.max / d.min, n);
    return d.min + (d.max - d.min) * n;
}

int findEdge (const EnginePatch& e, int from, int to); // -1 if absent

// Base (stored) value access. Edge codes exist only when the edge is present.
bool paramExists (const PatchData& d, bool sideB, int code);
float getParam (const PatchData& d, bool sideB, int code); // physical units; virtual -> neutral
bool setParam (PatchData& d, bool sideB, int code, float value); // clamps; false if absent

bool isMacroTarget (int code);
bool isModTarget (int code);

std::string paramCodeToId (int code);        // "op1.level", "edge.1.2.index", "fx.reverb.wet"
int paramIdToCode (const std::string& id);   // -1 if unknown
std::string paramDisplayName (int code);     // "Op 1 Level"
std::string formatParamValue (int code, float value);

float applyCurve (Curve c, float x); // odd-symmetric shaping of x in [-1, 1]
// Normalised-domain delta contributed by one macro mapping at macro value v (0..1).
// Always 0 at v == 0.5 (neutral, SPEC §6.1).
float macroDelta (const MacroMapping& m, float v);

const char* modSourceName (ModSource s); // "lfo1", "macro3", ...
int modSourceFromName (const std::string& s);
inline bool modSourceIsMacro (ModSource s) { return s >= ModSource::Macro1 && s <= ModSource::Macro8; }
} // namespace fmlume
