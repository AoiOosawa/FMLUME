#include "Core/Params.h"
#include <iterator>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace fmlume
{
namespace
{
constexpr uint32_t M = kFlagModTarget;

const ParamDesc kOpDescs[kOpFieldCount] = {
    { "ratio", 0.125f, 32.0f, 1.0f, Scale::Log, kGroupRatio, "x" },
    { "fixedHz", 0.1f, 16000.0f, 440.0f, Scale::Log, kGroupRatio, "Hz" },
    { "detune", -100.0f, 100.0f, 0.0f, Scale::Linear, M | kGroupMisc, "ct" },
    { "level", 0.0f, 1.0f, 1.0f, Scale::Linear, M | kGroupLevel, "" },
    { "velocityToLevel", 0.0f, 1.0f, 0.5f, Scale::Linear, kGroupMisc, "" },
    { "velocityToIndex", 0.0f, 1.0f, 0.3f, Scale::Linear, kGroupMisc, "" },
    { "keyTracking", 0.0f, 1.0f, 1.0f, Scale::Linear, kGroupRatio, "" },
    { "attack", 0.001f, 20.0f, 0.01f, Scale::Log, kGroupEnv, "s" },
    { "decay", 0.001f, 30.0f, 0.5f, Scale::Log, kGroupEnv, "s" },
    { "sustain", 0.0f, 1.0f, 0.6f, Scale::Linear, kGroupEnv, "" },
    { "release", 0.001f, 40.0f, 0.3f, Scale::Log, kGroupEnv, "s" },
    { "phaseOffset", 0.0f, 1.0f, 0.0f, Scale::Linear, kGroupMisc, "" },
    { "feedback", 0.0f, 1.5f, 0.0f, Scale::Linear, kGroupIndex, "rad" },
    { "carrierGain", 0.0f, 1.0f, 0.0f, Scale::Linear, kGroupLevel, "" },
};
const ParamDesc kEdgeDesc = { "index", 0.0f, 16.0f, 0.0f, Scale::Linear, M | kGroupIndex, "rad" };
const ParamDesc kMiscDescs[] = {
    { "filter.cutoff", 20.0f, 20000.0f, 20000.0f, Scale::Log, M | kGroupFilter, "Hz" },
    { "filter.resonance", 0.0f, 0.95f, 0.0f, Scale::Linear, M | kGroupFilter, "" },
    { "filter.drive", 0.0f, 1.0f, 0.0f, Scale::Linear, kGroupFilter, "" },
    { "lfo1.rate", 0.02f, 20.0f, 2.0f, Scale::Log, kGroupMod, "Hz" },
    { "lfo2.rate", 0.02f, 20.0f, 2.0f, Scale::Log, kGroupMod, "Hz" },
    { "modEnv.attack", 0.001f, 20.0f, 0.01f, Scale::Log, kGroupMod, "s" },
    { "modEnv.decay", 0.001f, 30.0f, 0.5f, Scale::Log, kGroupMod, "s" },
    { "modEnv.sustain", 0.0f, 1.0f, 0.6f, Scale::Linear, kGroupMod, "" },
    { "modEnv.release", 0.001f, 40.0f, 0.3f, Scale::Log, kGroupMod, "s" },
    { "pitch", -12.0f, 12.0f, 0.0f, Scale::Linear, M | kFlagVirtual, "st" },
    { "master.level", 0.0f, 2.0f, 1.0f, Scale::Linear, M | kFlagVirtual, "" },
    { "master.pan", -1.0f, 1.0f, 0.0f, Scale::Linear, M | kFlagVirtual, "" },
};
const ParamDesc kPortamentoDesc = { "performance.portamento", 0.0f, 2.0f, 0.0f, Scale::Linear, kGroupMisc, "s" };
const ParamDesc kMatrixDepthDesc = { "depth", -1.0f, 1.0f, 0.0f, Scale::Linear, kGroupMod, "" };

constexpr uint32_t F = kGroupFx;
constexpr uint32_t W = kGroupFx | kFlagWet | kFlagModTarget;
constexpr uint32_t D = kFlagDiscrete;
const ParamDesc kNoParam = { nullptr, 0.0f, 1.0f, 0.0f, Scale::Linear, D, "" };

const ParamDesc kFxDescs[kNumFx][kFxParams] = {
    // EQ
    { { "lowGain", -18, 18, 0, Scale::Linear, F, "dB" }, { "lowFreq", 40, 600, 120, Scale::Log, F, "Hz" },
      { "midGain", -18, 18, 0, Scale::Linear, F, "dB" }, { "midFreq", 200, 8000, 1000, Scale::Log, F, "Hz" },
      { "midQ", 0.3f, 8, 0.9f, Scale::Log, F, "" }, { "highGain", -18, 18, 0, Scale::Linear, F, "dB" },
      { "highFreq", 1500, 16000, 6000, Scale::Log, F, "Hz" }, kNoParam },
    // Drive
    { { "gain", 0, 1, 0.3f, Scale::Linear, F, "" }, { "tone", 0, 1, 0.6f, Scale::Linear, F, "" },
      { "wet", 0, 1, 1, Scale::Linear, W, "" }, { "trim", -24, 12, 0, Scale::Linear, F, "dB" },
      kNoParam, kNoParam, kNoParam, kNoParam },
    // Chorus
    { { "rate", 0.05f, 8, 0.6f, Scale::Log, F, "Hz" }, { "depth", 0, 1, 0.4f, Scale::Linear, F, "" },
      { "width", 0, 1, 0.8f, Scale::Linear, F, "" }, { "wet", 0, 1, 0.35f, Scale::Linear, W, "" },
      { "voices", 1, 4, 2, Scale::Linear, D, "" }, kNoParam, kNoParam, kNoParam },
    // Phaser
    { { "rate", 0.02f, 10, 0.4f, Scale::Log, F, "Hz" }, { "depth", 0, 1, 0.6f, Scale::Linear, F, "" },
      { "feedback", 0, 0.9f, 0.3f, Scale::Linear, F, "" }, { "wet", 0, 1, 0.5f, Scale::Linear, W, "" },
      kNoParam, kNoParam, kNoParam, kNoParam },
    // Flanger
    { { "rate", 0.02f, 10, 0.25f, Scale::Log, F, "Hz" }, { "depth", 0, 1, 0.6f, Scale::Linear, F, "" },
      { "feedback", 0, 0.9f, 0.4f, Scale::Linear, F, "" }, { "wet", 0, 1, 0.5f, Scale::Linear, W, "" },
      kNoParam, kNoParam, kNoParam, kNoParam },
    // Compressor
    { { "threshold", -60, 0, -18, Scale::Linear, F, "dB" }, { "ratio", 1, 20, 3, Scale::Log, F, ":1" },
      { "attack", 0.1f, 200, 10, Scale::Log, F, "ms" }, { "release", 5, 2000, 150, Scale::Log, F, "ms" },
      { "makeup", 0, 24, 0, Scale::Linear, F, "dB" }, { "wet", 0, 1, 1, Scale::Linear, W, "" }, kNoParam, kNoParam },
    // Bitcrusher
    { { "bits", 4, 16, 12, Scale::Linear, F, "bit" }, { "downsample", 1, 32, 1, Scale::Log, F, "x" },
      { "wet", 0, 1, 1, Scale::Linear, W, "" }, kNoParam, kNoParam, kNoParam, kNoParam, kNoParam },
    // Delay
    { { "time", 1, 2000, 350, Scale::Log, F, "ms" }, { "sync", 0, 1, 0, Scale::Linear, D, "" },
      { "division", 0, 20, 9, Scale::Linear, D, "" }, { "feedback", 0, 0.9f, 0.35f, Scale::Linear, F, "" },
      { "stereo", 0, 1, 0.5f, Scale::Linear, F, "" }, { "wet", 0, 1, 0.25f, Scale::Linear, W, "" }, kNoParam, kNoParam },
    // Reverb
    { { "size", 0, 1, 0.6f, Scale::Linear, F, "" }, { "decay", 0.2f, 20, 2.5f, Scale::Log, F, "s" },
      { "preDelay", 0, 200, 15, Scale::Linear, F, "ms" }, { "damping", 0, 1, 0.5f, Scale::Linear, F, "" },
      { "wet", 0, 1, 0.25f, Scale::Linear, W, "" }, kNoParam, kNoParam, kNoParam },
};

float* enginePtr (SoundPatch& s, int code)
{
    auto& e = s.engine;
    if (pc::isOp (code))
    {
        auto& op = e.operators[(size_t) (code / kOpFieldCount)];
        switch (code % kOpFieldCount)
        {
            case kOpRatio: return &op.ratio;
            case kOpFixedHz: return &op.fixedHz;
            case kOpDetune: return &op.detuneCents;
            case kOpLevel: return &op.level;
            case kOpVelToLevel: return &op.velocityToLevel;
            case kOpVelToIndex: return &op.velocityToIndex;
            case kOpKeyTracking: return &op.keyTracking;
            case kOpEnvA: return &op.env.a;
            case kOpEnvD: return &op.env.d;
            case kOpEnvS: return &op.env.s;
            case kOpEnvR: return &op.env.r;
            case kOpPhaseOffset: return &op.phaseOffset;
            case kOpFeedback: return &op.feedback;
            case kOpCarrierGain: return &op.carrierGain;
            default: return nullptr;
        }
    }
    if (pc::isEdge (code))
    {
        const int rel = code - pc::edgeBase;
        const int idx = findEdge (e, rel / kNumOps, rel % kNumOps);
        return idx >= 0 ? &e.edges[(size_t) idx].index : nullptr;
    }
    switch (code)
    {
        case pc::filterCutoff: return &e.filter.cutoffHz;
        case pc::filterRes: return &e.filter.resonance;
        case pc::filterDrive: return &e.filter.drive;
        case pc::lfo1Rate: return &e.lfo[0].rateHz;
        case pc::lfo2Rate: return &e.lfo[1].rateHz;
        case pc::modEnvA: return &e.modEnv.a;
        case pc::modEnvD: return &e.modEnv.d;
        case pc::modEnvS: return &e.modEnv.s;
        case pc::modEnvR: return &e.modEnv.r;
        default: break;
    }
    if (code >= pc::matrixDepthBase && code < pc::engineCount)
        return &s.matrix[(size_t) (code - pc::matrixDepthBase)].depth;
    return nullptr;
}

float* dataPtr (PatchData& d, int code)
{
    if (pc::isEngine (code)) return enginePtr (d.sound, code);
    if (pc::isFx (code))
    {
        const int rel = code - pc::fxBase;
        const int t = rel / kFxParams, p = rel % kFxParams;
        if (kFxDescs[t][p].name == nullptr) return nullptr;
        return &d.fx.proc[(size_t) t].p[(size_t) p];
    }
    if (code == pc::portamento) return &d.perf.portamento;
    return nullptr;
}
} // namespace

const ParamDesc& fxParamDesc (int type, int p) { return kFxDescs[type][p]; }
int fxNumParams (int type)
{
    int n = 0;
    while (n < kFxParams && kFxDescs[type][n].name != nullptr) ++n;
    return n;
}

const ParamDesc& paramDesc (int code)
{
    if (pc::isOp (code)) return kOpDescs[code % kOpFieldCount];
    if (pc::isEdge (code)) return kEdgeDesc;
    if (code >= pc::filterCutoff && code < pc::matrixDepthBase) return kMiscDescs[code - pc::filterCutoff];
    if (code >= pc::matrixDepthBase && code < pc::engineCount) return kMatrixDepthDesc;
    if (pc::isFx (code)) return kFxDescs[(code - pc::fxBase) / kFxParams][(code - pc::fxBase) % kFxParams];
    if (code == pc::portamento) return kPortamentoDesc;
    return kNoParam;
}

int findEdge (const EnginePatch& e, int from, int to)
{
    for (int i = 0; i < e.numEdges; ++i)
        if (e.edges[(size_t) i].from == from && e.edges[(size_t) i].to == to) return i;
    return -1;
}

bool paramExists (const PatchData& d, int code)
{
    if (code < 0 || code >= pc::total) return false;
    if (paramDesc (code).flags & kFlagVirtual) return true;
    return dataPtr (const_cast<PatchData&> (d), code) != nullptr;
}

float getParam (const PatchData& d, int code)
{
    const auto& desc = paramDesc (code);
    if (const float* p = dataPtr (const_cast<PatchData&> (d), code)) return *p;
    return desc.def;
}

bool setParam (PatchData& d, int code, float value)
{
    float* p = dataPtr (d, code);
    if (p == nullptr) return false;
    const auto& desc = paramDesc (code);
    value = clampf (value, desc.min, desc.max);
    if (desc.flags & kFlagDiscrete) value = std::round (value);
    *p = value;
    return true;
}

bool isMacroTarget (int code)
{
    if (code < 0 || code >= pc::total) return false;
    const auto& d = paramDesc (code);
    if (d.name == nullptr || (d.flags & kFlagDiscrete)) return false;
    return true;
}

bool isModTarget (int code)
{
    if (code < 0 || code >= pc::total) return false;
    const auto& d = paramDesc (code);
    return d.name != nullptr && (d.flags & kFlagModTarget) != 0;
}

std::string paramCodeToId (int code)
{
    char buf[64] = "";
    if (pc::isOp (code))
        std::snprintf (buf, sizeof (buf), "op%d.%s", code / kOpFieldCount + 1, kOpDescs[code % kOpFieldCount].name);
    else if (pc::isEdge (code))
        std::snprintf (buf, sizeof (buf), "edge.%d.%d.index", (code - pc::edgeBase) / kNumOps + 1, (code - pc::edgeBase) % kNumOps + 1);
    else if (code >= pc::filterCutoff && code < pc::matrixDepthBase)
        return kMiscDescs[code - pc::filterCutoff].name;
    else if (code >= pc::matrixDepthBase && code < pc::engineCount)
        std::snprintf (buf, sizeof (buf), "matrix.%d.depth", code - pc::matrixDepthBase + 1);
    else if (pc::isFx (code))
    {
        const auto& d = paramDesc (code);
        if (d.name == nullptr) return {};
        std::snprintf (buf, sizeof (buf), "fx.%s.%s", fxTypeName ((FxType) ((code - pc::fxBase) / kFxParams)), d.name);
    }
    else if (code == pc::portamento)
        return kPortamentoDesc.name;
    return buf;
}

int paramIdToCode (const std::string& id)
{
    static std::vector<std::string> table;
    static std::once_flag once;
    std::call_once (once, [] {
        table.resize ((size_t) pc::total);
        for (int c = 0; c < pc::total; ++c) table[(size_t) c] = paramCodeToId (c);
    });
    if (id.empty()) return -1;
    for (int c = 0; c < pc::total; ++c)
        if (table[(size_t) c] == id) return c;
    return -1;
}

std::string paramDisplayName (int code)
{
    static const char* const opNames[kOpFieldCount] = { "Ratio", "Fixed Hz", "Detune", "Level", "Vel>Level", "Vel>Index", "Key Track",
                                                        "Attack", "Decay", "Sustain", "Release", "Phase", "Feedback", "Carrier" };
    static const char* const miscNames[] = { "Filter Cutoff", "Filter Reso", "Filter Drive", "LFO 1 Rate", "LFO 2 Rate", "Mod Env Attack",
                                             "Mod Env Decay", "Mod Env Sustain", "Mod Env Release", "Pitch", "Level", "Pan" };
    static const char* const fxNames[kNumFx] = { "EQ", "Drive", "Chorus", "Phaser", "Flanger", "Comp", "Crush", "Delay", "Reverb" };
    char buf[64] = "";
    if (pc::isOp (code))
        std::snprintf (buf, sizeof (buf), "Op %d %s", code / kOpFieldCount + 1, opNames[code % kOpFieldCount]);
    else if (pc::isEdge (code))
        std::snprintf (buf, sizeof (buf), "FM %d>%d", (code - pc::edgeBase) / kNumOps + 1, (code - pc::edgeBase) % kNumOps + 1);
    else if (code >= pc::filterCutoff && code < pc::matrixDepthBase)
        return miscNames[code - pc::filterCutoff];
    else if (code >= pc::matrixDepthBase && code < pc::engineCount)
        std::snprintf (buf, sizeof (buf), "Mod %d Depth", code - pc::matrixDepthBase + 1);
    else if (pc::isFx (code) && paramDesc (code).name != nullptr)
    {
        std::string n = paramDesc (code).name;
        n[0] = (char) std::toupper ((unsigned char) n[0]);
        std::snprintf (buf, sizeof (buf), "%s %s", fxNames[(code - pc::fxBase) / kFxParams], n.c_str());
    }
    else if (code == pc::portamento)
        return "Glide";
    return buf;
}

std::string formatParamValue (int code, float value)
{
    const auto& d = paramDesc (code);
    char buf[48];
    const float a = std::fabs (value);
    const char* fmt = (d.flags & kFlagDiscrete) ? "%.0f" : (a >= 100.0f ? "%.0f" : (a >= 10.0f ? "%.1f" : (a >= 1.0f ? "%.2f" : "%.3f")));
    char num[32];
    std::snprintf (num, sizeof (num), fmt, (double) value);
    std::snprintf (buf, sizeof (buf), "%s%s%s", num, d.unit[0] ? " " : "", d.unit);
    return buf;
}

float applyCurve (Curve c, float x)
{
    const float a = std::fabs (x) > 1.0f ? 1.0f : std::fabs (x);
    const float s = x < 0.0f ? -1.0f : 1.0f;
    switch (c)
    {
        case Curve::Exp: return s * a * a;
        case Curve::SCurve: return s * a * a * (3.0f - 2.0f * a);
        default: return s * a;
    }
}

float macroDelta (const MacroMapping& m, float v)
{
    v = clampf (v, 0.0f, 1.0f);
    if (m.polarity == Polarity::Bipolar)
        return m.amount * applyCurve (m.curve, 2.0f * v - 1.0f);
    // Unipolar: shaped over the full 0..1 throw but still neutral at 0.5.
    const float mid = applyCurve (m.curve, 0.5f);
    return m.amount * (applyCurve (m.curve, v) - mid) / (1.0f - mid);
}

static const char* const kModSourceNames[] = { "none", "lfo1", "lfo2", "modEnv", "velocity", "keytrack", "modWheel", "aftertouch",
                                               "macro1", "macro2", "macro3", "macro4", "macro5", "macro6", "macro7", "macro8" };
static_assert (std::size (kModSourceNames) == (size_t) ModSource::Count, "every ModSource needs a name");
const char* modSourceName (ModSource s) { return kModSourceNames[std::min<int> ((int) s, (int) ModSource::Count - 1)]; }
int modSourceFromName (const std::string& s)
{
    for (int i = 0; i < (int) ModSource::Count; ++i) if (s == kModSourceNames[i]) return i;
    return -1;
}
} // namespace fmlume
