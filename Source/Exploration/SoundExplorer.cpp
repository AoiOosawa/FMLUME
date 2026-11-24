#include "Exploration/SoundExplorer.h"
#include "Core/Params.h"
#include "Core/PatchCompiler.h"
#include "Core/Pcg32.h"
#include "Core/Templates.h"
#include "DSP/FM/SynthEngine.h"
#include "Presets/PresetSerializer.h"
#include <algorithm>
#include <cmath>

namespace fmlume
{
namespace
{
const char* const kDirNames[] = { "Surprise", "Brighter", "Darker", "Warmer", "More Metallic", "Softer", "Punchier", "More Motion" };
const char* const kStrengthNames[] = { "Subtle", "Medium", "Wild" };
const float kSigma[] = { 0.08f, 0.20f, 0.35f };
const float kMinDistance[] = { 0.004f, 0.010f, 0.018f };
// Ratios a modulator may be snapped to in Wild mode (never free-floating values).
const float kHarmonicRatios[] = { 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 10.0f, 14.0f };
const float kMetallicRatios[] = { 1.41f, 2.76f, 3.5f, 5.19f, 7.07f, 9.23f, 11.3f };

bool macroTargets (const SoundPatch& s, int macro, int code)
{
    const auto& m = s.macros[(size_t) macro];
    for (int k = 0; k < m.numMappings; ++k)
        if (m.mappings[(size_t) k].target == code) return true;
    return false;
}

bool lockedByMacro (const PatchData& d, const ExploreLocks& locks, int code)
{
    for (int i = 0; i < kNumMacros; ++i)
        if (locks.macros[(size_t) i] && macroTargets (d.sound, i, code)) return true;
    return false;
}

bool isEnvCode (int code)
{
    if (pc::isOp (code)) { const int f = code % kOpFieldCount; return f >= kOpEnvA && f <= kOpEnvR; }
    return code >= pc::modEnvA && code <= pc::modEnvR;
}
bool isRatioCode (int code)
{
    if (! pc::isOp (code)) return false;
    const int f = code % kOpFieldCount;
    return f == kOpRatio || f == kOpFixedHz || f == kOpKeyTracking;
}

// Is this parameter something the Explorer may vary for this patch at all?
bool explorable (const PatchData& d, int code)
{
    if (code == pc::portamento) return false;
    const auto& desc = paramDesc (code);
    if (desc.name == nullptr || (desc.flags & (kFlagDiscrete | kFlagVirtual))) return false;
    if (! paramExists (d, code)) return false;
    const auto& e = d.sound.engine;
    if (pc::isOp (code))
    {
        const auto& op = e.operators[(size_t) (code / kOpFieldCount)];
        if (! op.enabled) return false;
        const int f = code % kOpFieldCount;
        if (isRatioCode (code)) return false;               // ratios are handled by explicit snapping only
        if (f == kOpPhaseOffset) return false;
        if (f == kOpCarrierGain && op.carrierGain <= 0.0f) return false; // do not create new carriers
        return true;
    }
    if (pc::isEdge (code))
    {
        const int rel = code - pc::edgeBase;
        return e.operators[(size_t) (rel / kNumOps)].enabled && e.operators[(size_t) (rel % kNumOps)].enabled;
    }
    if (code == pc::filterCutoff || code == pc::filterRes || code == pc::filterDrive) return e.filter.type != FilterType::Off;
    if (code >= pc::matrixDepthBase && code < pc::engineCount)
    {
        const auto& m = d.sound.matrix[(size_t) (code - pc::matrixDepthBase)];
        return m.enabled;
    }
    if (code == pc::lfo1Rate || code == pc::lfo2Rate)
    {
        const ModSource src = code == pc::lfo1Rate ? ModSource::LFO1 : ModSource::LFO2;
        for (auto& m : d.sound.matrix) if (m.enabled && m.source == src) return true;
        return false;
    }
    if (code >= pc::modEnvA && code <= pc::modEnvR)
    {
        for (auto& m : d.sound.matrix) if (m.enabled && m.source == ModSource::ModEnv) return true;
        return false;
    }
    if (pc::isFx (code)) return d.fx.proc[(size_t) ((code - pc::fxBase) / kFxParams)].enabled;
    return false;
}

float groupScale (int code)
{
    const uint32_t f = paramDesc (code).flags;
    if (f & kGroupIndex) return 1.0f;
    if (f & kGroupLevel) return 0.45f;
    if (f & kGroupEnv) return 0.6f;
    if (f & kGroupFilter) return 0.7f;
    if (f & kGroupMod) return 0.5f;
    if (f & kGroupFx) return 0.45f;
    return 0.35f;
}

bool nonInteger (float ratio) { return std::fabs (ratio - std::round (ratio)) > 0.04f && std::fabs (ratio * 2.0f - std::round (ratio * 2.0f)) > 0.04f; }

// Direction bias in units of sigma for one parameter.
float directionBias (ExploreDirection dir, const PatchData& d, int code)
{
    const auto& e = d.sound.engine;
    const uint32_t flags = paramDesc (code).flags;
    const bool isEdgeIdx = pc::isEdge (code);
    const bool isFb = pc::isOp (code) && code % kOpFieldCount == kOpFeedback;
    const int field = pc::isOp (code) ? code % kOpFieldCount : -1;
    const bool carrier = pc::isOp (code) && e.operators[(size_t) (code / kOpFieldCount)].carrierGain > 0.0f;
    switch (dir)
    {
        case ExploreDirection::Brighter:
            if (isEdgeIdx || isFb) return 0.8f;
            if (code == pc::filterCutoff) return 0.9f;
            if (code == pc::fx (FxType::EQ, 5)) return 0.5f;
            break;
        case ExploreDirection::Darker:
            if (isEdgeIdx || isFb) return -0.8f;
            if (code == pc::filterCutoff) return -0.9f;
            if (code == pc::fx (FxType::EQ, 5)) return -0.5f;
            break;
        case ExploreDirection::Warmer:
            if (isEdgeIdx) return -0.4f;
            if (code == pc::filterCutoff) return -0.5f;
            if (code == pc::fx (FxType::EQ, 0)) return 0.6f;
            if (code == pc::fx (FxType::EQ, 5)) return -0.4f;
            if (code == pc::fx (FxType::Drive, 0)) return 0.6f;
            break;
        case ExploreDirection::MoreMetallic:
            if (isEdgeIdx)
            {
                const int from = (code - pc::edgeBase) / kNumOps;
                return nonInteger (e.operators[(size_t) from].ratio) ? 1.0f : 0.25f;
            }
            if (isFb) return 0.4f;
            break;
        case ExploreDirection::Softer:
            if (field == kOpEnvA) return 0.8f;
            if (isEdgeIdx) return -0.4f;
            if (field == kOpVelToIndex) return -0.4f;
            if (code == pc::filterCutoff) return -0.3f;
            break;
        case ExploreDirection::Punchier:
            if (field == kOpEnvA) return -0.7f;
            if (field == kOpEnvD) return carrier ? -0.2f : -0.6f;
            if (isEdgeIdx) return 0.4f;
            if (field == kOpVelToIndex) return 0.4f;
            break;
        case ExploreDirection::MoreMotion:
            if (flags & kGroupMod) return 0.0f; // handled explicitly (depth magnitude)
            if (code == pc::fx (FxType::Chorus, 1) || code == pc::fx (FxType::Phaser, 1)) return 0.5f;
            break;
        default: break;
    }
    return 0.0f;
}

uint64_t mix64 (uint64_t x)
{
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33;
    return x;
}

// Offline render of a short phrase: a three-note chord held (0.8 s, longer for slow swells), then released.
// rms / peak / finite feed the safety check. `loudnessDb` is what the candidates are levelled
// by: A-weighted (the ear hears a brighter sound as louder at the same RMS) and taken over the
// loudest 400 ms, so a slower attack or a longer tail is not mistaken for a quieter sound
// (docs/DECISIONS.md D-058).
float renderRms (const PatchData& d, bool& finite, float& peak, float* loudnessDb = nullptr, double holdSeconds = 0.8)
{
    SynthEngine eng;
    const double sr = 44100.0;
    eng.prepare (sr, 512);
    finite = true;
    peak = 0.0f;
    if (loudnessDb != nullptr) *loudnessDb = -200.0f;
    if (! eng.setPatch (d)) { finite = false; return 0.0f; }
    HostParams h = d.host;
    h.quality = 0;
    h.gainDb = 0.0f;
    h.arpEnabled = false;
    h.gateEnabled = false;
    h.fxBypassAll = false;
    constexpr int kBlock = 512;
    float l[kBlock], r[kBlock];
    double acc = 0.0;
    int count = 0;
    const int blocks = (int) ((holdSeconds + 0.3) * sr / kBlock), offBlock = (int) (holdSeconds * sr / kBlock);
    static const uint8_t kNotes[3] = { 48, 55, 64 };

    // A-weighting: two high-pass poles at 20.6 Hz, one at 107.7 Hz and one at 737.9 Hz, two
    // low-pass poles at 12.2 kHz (first-order sections, bilinear transform).
    struct OnePole
    {
        double b0 = 1, b1 = 0, a1 = 0, x1 = 0, y1 = 0;
        static OnePole highPass (double fc, double sr) { const double k = std::tan (3.14159265358979 * fc / sr); OnePole f; f.b0 = 1.0 / (1.0 + k); f.b1 = -f.b0; f.a1 = (1.0 - k) / (1.0 + k); return f; }
        static OnePole lowPass (double fc, double sr) { const double k = std::tan (3.14159265358979 * fc / sr); OnePole f; f.b0 = k / (1.0 + k); f.b1 = f.b0; f.a1 = (1.0 - k) / (1.0 + k); return f; }
        double tick (double x) { const double y = b0 * x + b1 * x1 + a1 * y1; x1 = x; y1 = y; return y; }
    };
    OnePole aw[6] = { OnePole::highPass (20.6, sr), OnePole::highPass (20.6, sr), OnePole::highPass (107.7, sr),
                      OnePole::highPass (737.9, sr), OnePole::lowPass (12194.0, sr), OnePole::lowPass (12194.0, sr) };
    const int window = (int) (0.4 * sr);
    std::vector<double> energy ((size_t) window, 0.0); // ring of squared weighted samples (non-realtime: fine to allocate)
    double windowSum = 0.0, loudest = 0.0;
    int ring = 0;

    for (int b = 0; b < blocks; ++b)
    {
        MidiEvent ev[3];
        int n = 0;
        if (b == 0 || b == offBlock)
            for (uint8_t note : kNotes) { ev[n].type = b == 0 ? MidiEvent::NoteOn : MidiEvent::NoteOff; ev[n].data1 = note; ev[n].data2 = 100; ++n; }
        eng.process (l, r, kBlock, ev, n, h, {});
        for (int i = 0; i < kBlock; ++i)
        {
            if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) finite = false;
            peak = std::max (peak, std::max (std::fabs (l[i]), std::fabs (r[i])));
            acc += 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]);
            ++count;
            double x = 0.5 * ((double) l[i] + (double) r[i]);
            for (auto& f : aw) x = f.tick (x);
            windowSum += x * x - energy[(size_t) ring];
            energy[(size_t) ring] = x * x;
            ring = (ring + 1) % window;
            loudest = std::max (loudest, windowSum);
        }
    }
    if (loudnessDb != nullptr && finite) *loudnessDb = (float) (10.0 * std::log10 (std::max (loudest, 0.0) / window + 1.0e-18) + 3.0); // +3: A-weighting is about -3 dB on a sum of the two channels
    return (float) std::sqrt (acc / std::max (count, 1));
}

std::string describe (const ExploreScores& p, const ExploreScores& c, std::string& tendency)
{
    struct Item { float delta; const char* up; const char* down; };
    Item items[] = { { c.brightness - p.brightness, "Brighter", "Darker" }, { c.metallic - p.metallic, "Metallic", "Purer" },
                     { c.softness - p.softness, "Softer", "Sharper" }, { c.punch - p.punch, "Punchier", "Smoother" },
                     { c.motion - p.motion, "Moving", "Steadier" }, { c.warmth - p.warmth, "Warmer", "Cooler" } };
    std::sort (std::begin (items), std::end (items), [] (const Item& a, const Item& b) { return std::fabs (a.delta) > std::fabs (b.delta); });
    std::string name;
    tendency.clear();
    int used = 0;
    for (auto& it : items)
    {
        if (std::fabs (it.delta) < 0.01f || used >= 2) break;
        const char* word = it.delta > 0.0f ? it.up : it.down;
        name += (used ? " / " : "") + std::string (word);
        tendency += (used ? ", " : "") + std::string (std::fabs (it.delta) > 0.12f ? "much " : "slightly ") + word;
        ++used;
    }
    if (name.empty()) { name = "Close variation"; tendency = "very similar to the parent"; }
    for (auto& ch : tendency) ch = (char) std::tolower ((unsigned char) ch);
    return name;
}
} // namespace

const char* exploreDirectionName (ExploreDirection d) { return kDirNames[std::min<int> ((int) d, 7)]; }
const char* exploreStrengthName (ExploreStrength s) { return kStrengthNames[std::min<int> ((int) s, 2)]; }

ExploreScores SoundExplorer::score (const PatchData& d)
{
    ExploreScores s;
    const auto& e = d.sound.engine;
    float idxSum = 0.0f, metal = 0.0f, attack = 0.0f, modDecay = 0.0f;
    int carriers = 0, mods = 0;
    for (int i = 0; i < e.numEdges; ++i)
    {
        const auto& ed = e.edges[(size_t) i];
        const auto& from = e.operators[(size_t) ed.from];
        if (! from.enabled || ! e.operators[(size_t) ed.to].enabled) continue;
        const float contrib = ed.index / 16.0f * from.level * (0.5f + 0.5f * std::min (from.ratio, 8.0f) / 8.0f);
        idxSum += contrib;
        if (from.pitchMode == PitchMode::Ratio && nonInteger (from.ratio)) metal += contrib;
    }
    for (auto& op : e.operators)
    {
        if (! op.enabled) continue;
        idxSum += op.feedback / 1.5f * 0.15f;
        const float aN = toNorm (paramDesc (pc::op (0, kOpEnvA)), op.env.a);
        if (op.carrierGain > 0.0f) { attack += aN; ++carriers; }
        else { modDecay += toNorm (paramDesc (pc::op (0, kOpEnvD)), op.env.d); ++mods; }
    }
    const float cutoffN = e.filter.type == FilterType::Off ? 1.0f : toNorm (paramDesc (pc::filterCutoff), e.filter.cutoffHz);
    const auto& eq = d.fx.proc[(size_t) FxType::EQ];
    const float eqHigh = eq.enabled ? eq.p[5] / 18.0f : 0.0f, eqLow = eq.enabled ? eq.p[0] / 18.0f : 0.0f;
    const auto& dr = d.fx.proc[(size_t) FxType::Drive];
    s.brightness = idxSum * 1.2f + (cutoffN - 1.0f) * 0.8f + eqHigh * 0.2f;
    s.metallic = metal * 2.0f;
    const float avgAttack = carriers > 0 ? attack / (float) carriers : 0.0f;
    s.softness = avgAttack - idxSum * 0.3f;
    s.punch = (1.0f - avgAttack) * 0.5f + (mods > 0 ? (1.0f - modDecay / (float) mods) * 0.3f : 0.0f) + idxSum * 0.3f;
    float motion = 0.0f;
    for (auto& m : d.sound.matrix)
        if (m.enabled && (m.source == ModSource::LFO1 || m.source == ModSource::LFO2))
            motion += std::fabs (m.depth) * (0.5f + toNorm (paramDesc (pc::lfo1Rate), e.lfo[m.source == ModSource::LFO1 ? 0 : 1].rateHz));
    const auto& ch = d.fx.proc[(size_t) FxType::Chorus];
    s.motion = motion * 0.8f + (ch.enabled ? ch.p[1] * ch.p[3] * 0.15f : 0.0f);
    s.warmth = eqLow * 0.4f - eqHigh * 0.2f + (dr.enabled ? dr.p[0] * dr.p[2] * 0.5f : 0.0f) - idxSum * 0.3f - (cutoffN - 1.0f) * -0.0f + (1.0f - cutoffN) * 0.4f;
    return s;
}

float SoundExplorer::distance (const PatchData& a, const PatchData& b)
{
    float sum = 0.0f;
    int n = 0;
    for (int c = 0; c < pc::total; ++c)
    {
        const auto& d = paramDesc (c);
        if (d.name == nullptr || (d.flags & kFlagVirtual)) continue;
        const bool ea = paramExists (a, c), eb = paramExists (b, c);
        if (! ea && ! eb) continue;
        ++n;
        if (ea != eb) { sum += 0.5f; continue; }
        sum += std::fabs (toNorm (d, getParam (a, c)) - toNorm (d, getParam (b, c)));
    }
    return n > 0 ? sum / (float) n : 0.0f;
}

bool SoundExplorer::locksRespected (const PatchData& parent, const PatchData& cand, const ExploreLocks& locks)
{
    const auto& pe = parent.sound.engine;
    const auto& ce = cand.sound.engine;
    if (locks.graph)
    {
        if (pe.numEdges != ce.numEdges) return false;
        for (int i = 0; i < pe.numEdges; ++i)
            if (findEdge (ce, pe.edges[(size_t) i].from, pe.edges[(size_t) i].to) < 0) return false;
        for (int k = 0; k < kNumOps; ++k)
            if (pe.operators[(size_t) k].enabled != ce.operators[(size_t) k].enabled || pe.operators[(size_t) k].waveform != ce.operators[(size_t) k].waveform
                || (pe.operators[(size_t) k].carrierGain > 0.0f) != (ce.operators[(size_t) k].carrierGain > 0.0f)) return false;
    }
    for (int c = 0; c < pc::total; ++c)
    {
        const auto& d = paramDesc (c);
        if (d.name == nullptr || (d.flags & kFlagVirtual)) continue;
        bool locked = false;
        if (locks.ratios && isRatioCode (c)) locked = true;
        if (locks.envelopes && isEnvCode (c)) locked = true;
        if (locks.fx && pc::isFx (c)) locked = true;
        if (lockedByMacro (parent, locks, c)) locked = true;
        if (! locked) continue;
        if (paramExists (parent, c) != paramExists (cand, c)) return false;
        if (paramExists (parent, c) && getParam (parent, c) != getParam (cand, c)) return false;
    }
    if (locks.fx)
    {
        if (parent.fx.order != cand.fx.order) return false;
        for (int t = 0; t < kNumFx; ++t)
            if (parent.fx.proc[(size_t) t].enabled != cand.fx.proc[(size_t) t].enabled) return false;
    }
    for (int i = 0; i < kNumMacros; ++i)
        if (locks.macros[(size_t) i])
        {
            const auto& pm = parent.sound.macros[(size_t) i];
            const auto& cm = cand.sound.macros[(size_t) i];
            if (pm.numMappings != cm.numMappings) return false;
            for (int k = 0; k < pm.numMappings; ++k)
                if (pm.mappings[(size_t) k].target != cm.mappings[(size_t) k].target || pm.mappings[(size_t) k].amount != cm.mappings[(size_t) k].amount) return false;
        }
    return true;
}

double SoundExplorer::measuringHold (const PatchData& d)
{
    // A slow swell only reaches its level after its attack: hold the measuring chord that long.
    double hold = 0.8;
    for (auto& op : d.sound.engine.operators)
        if (op.enabled && op.carrierGain > 0.0f) hold = std::max (hold, (double) op.env.a * 1.2 + 0.4);
    return std::min (hold, 3.0);
}

float SoundExplorer::loudnessDb (const PatchData& d, double holdSeconds)
{
    bool finite = true;
    float peak = 0.0f, loudness = -200.0f;
    renderRms (d, finite, peak, &loudness, holdSeconds);
    return loudness;
}

bool SoundExplorer::isSafe (const PatchData& d, float* rmsOut, std::string* why)
{
    auto no = [why] (const char* msg) { if (why) *why = msg; return false; };
    const auto v = validatePatch (d);
    if (! v.ok) { if (why) *why = v.error; return false; }
    bool finite = true;
    float peak = 0.0f;
    const float rms = renderRms (d, finite, peak);
    if (rmsOut) *rmsOut = rms;
    if (! finite) return no ("non-finite output");
    if (rms < 0.003f) return no ("silent");
    if (peak > 0.999f) return no ("output too hot");
    return true;
}

std::vector<ExploreCandidate> SoundExplorer::generate (const PatchData& parent, const ExploreSettings& settings, uint64_t seed)
{
    std::vector<ExploreCandidate> result;
    result.reserve (kNumCandidates);
    const int strength = std::min<int> ((int) settings.strength, 2);
    const float sigma = kSigma[strength];
    const float pick = strength == 0 ? 0.5f : (strength == 1 ? 0.6f : 0.7f);
    const uint64_t parentSum = patchChecksum (parent);
    const ExploreScores parentScores = score (parent);
    const double hold = measuringHold (parent);
    float parentLoudness = 0.0f;
    {
        bool finite = true;
        float peak = 0.0f;
        renderRms (parent, finite, peak, &parentLoudness, hold);
    }
    bool anyMacroLock = false;
    for (bool b : settings.locks.macros) anyMacroLock = anyMacroLock || b;
    const bool mayChangeGraph = settings.strength == ExploreStrength::Wild && ! settings.locks.graph && ! settings.locks.ratios && ! anyMacroLock;

    for (int i = 0; i < kNumCandidates; ++i)
    {
        ExploreCandidate best;
        bool found = false;
        for (int attempt = 0; attempt < 50 && ! found; ++attempt)
        {
            const uint64_t candSeed = mix64 (seed ^ mix64 ((uint64_t) i * 0x9E3779B97F4A7C15ULL + (uint64_t) attempt * 0xD1B54A32D192ED03ULL + 1u));
            Pcg32 rng (candSeed, (uint64_t) i + 11u);
            PatchData d = parent;
            auto& e = d.sound.engine;

            // 1. Optional routing swap (Wild, unlocked only): another validated template with the same operator count.
            if (mayChangeGraph && rng.nextFloat() < 0.35f)
            {
                int used = 0;
                for (auto& op : e.operators) used += op.enabled ? 1 : 0;
                int options[32], numOptions = 0;
                const int current = matchAlgorithm (e);
                for (int t = 0; t < numAlgorithmTemplates(); ++t)
                {
                    int ops = 0;
                    for (int k = 0; k < kNumOps; ++k) ops += (algorithmTemplate (t).usedMask >> k) & 1u;
                    if (ops == used && t != current) options[numOptions++] = t;
                }
                if (numOptions > 0)
                {
                    applyAlgorithm (e, options[rng.nextInt (numOptions)], 1.5f);
                    sanitizeTargets (d);
                    assignCommonMacros (d);
                }
            }

            // 2. Ratio snapping (Wild, unlocked): modulators only, from fixed musical sets.
            if (settings.strength == ExploreStrength::Wild && ! settings.locks.ratios)
                for (int k = 0; k < kNumOps; ++k)
                {
                    auto& op = e.operators[(size_t) k];
                    if (! op.enabled || op.carrierGain > 0.0f || op.pitchMode != PitchMode::Ratio) continue;
                    if (lockedByMacro (parent, settings.locks, pc::op (k, kOpRatio))) continue;
                    const bool metallic = settings.direction == ExploreDirection::MoreMetallic;
                    if (rng.nextFloat() < (metallic ? 0.45f : 0.2f))
                    {
                        if (metallic || rng.nextFloat() < 0.25f) op.ratio = kMetallicRatios[rng.nextInt (7)];
                        else op.ratio = kHarmonicRatios[rng.nextInt (11)];
                    }
                }

            // 3. More Motion: make sure there is something moving.
            if (settings.direction == ExploreDirection::MoreMotion)
            {
                bool hasLfo = false;
                int freeSlot = -1;
                for (int s = 0; s < kNumModSlots; ++s)
                {
                    const auto& m = d.sound.matrix[(size_t) s];
                    if (m.enabled && (m.source == ModSource::LFO1 || m.source == ModSource::LFO2) && std::fabs (m.depth) > 0.005f) hasLfo = true;
                    if (! m.enabled && freeSlot < 0) freeSlot = s;
                }
                if (! hasLfo && freeSlot >= 0)
                {
                    int target = e.filter.type != FilterType::Off ? pc::filterCutoff : -1;
                    for (int k = 0; k < e.numEdges && target < 0; ++k) target = pc::edge (e.edges[(size_t) k].from, e.edges[(size_t) k].to);
                    if (target < 0) target = pc::pan;
                    setModSlot (d.sound, freeSlot, ModSource::LFO1, target, 0.04f + 0.08f * rng.nextFloat());
                    if (! lockedByMacro (parent, settings.locks, pc::lfo1Rate)) e.lfo[0].rateHz = 0.2f * std::pow (20.0f, rng.nextFloat());
                }
            }

            // 4. Continuous parameters in the normalised domain.
            for (int c = 0; c < pc::total; ++c)
            {
                if (! explorable (d, c)) continue;
                if (settings.locks.envelopes && isEnvCode (c)) continue;
                if (settings.locks.fx && pc::isFx (c)) continue;
                if (lockedByMacro (parent, settings.locks, c)) continue;
                const auto& desc = paramDesc (c);
                const float scale = groupScale (c);
                const float bias = directionBias (settings.direction, d, c);
                const bool chosen = rng.nextFloat() < pick;
                const float noise = rng.nextGaussian();
                if (! chosen && bias == 0.0f) continue;
                float n = toNorm (desc, getParam (d, c));
                float delta = (chosen ? noise * sigma * scale * (bias != 0.0f ? 0.5f : 1.0f) : 0.0f) + bias * sigma * scale;
                if (settings.direction == ExploreDirection::MoreMotion && c >= pc::matrixDepthBase && c < pc::engineCount)
                {
                    // push depth away from zero, keeping its sign
                    const float sign = n >= 0.5f ? 1.0f : -1.0f;
                    delta = sign * std::fabs (noise * 0.5f + 0.8f) * sigma * 0.35f;
                }
                delta = clampf (delta, -2.0f * sigma, 2.0f * sigma);
                n = clampf (n + delta, 0.0f, 1.0f);
                // Safety limits per kind.
                if (pc::isOp (c))
                {
                    const int f = c % kOpFieldCount;
                    if (f == kOpCarrierGain) n = std::max (n, 0.25f);
                    if (f == kOpLevel) n = std::max (n, d.sound.engine.operators[(size_t) (c / kOpFieldCount)].carrierGain > 0.0f ? 0.4f : 0.05f);
                    if (f == kOpEnvS && d.sound.engine.operators[(size_t) (c / kOpFieldCount)].carrierGain > 0.0f) n = std::max (n, 0.0f);
                    if (f == kOpDetune) n = clampf (n, 0.35f, 0.65f);
                }
                if (pc::isEdge (c)) n = std::min (n, 0.75f);                 // <= 12 rad
                if (c == pc::filterCutoff) n = std::max (n, 0.25f);         // >= ~110 Hz
                if (c == pc::filterRes) n = std::min (n, 0.9f);
                if (c == pc::fx (FxType::Delay, 3) || c == pc::fx (FxType::Phaser, 2) || c == pc::fx (FxType::Flanger, 2)) n = std::min (n, 0.85f);
                if (c == pc::fx (FxType::Drive, 3) || c == pc::fx (FxType::Compressor, 4)) continue; // output trims stay put
                setParam (d, c, fromNorm (desc, n));
            }

            // 5. Validate, check locks / safety / uniqueness, normalise loudness.
            if (! validatePatch (d).ok) continue;
            if (! locksRespected (parent, d, settings.locks)) continue;
            if (! isSafe (d)) continue;
            float loudness = 0.0f;
            {
                bool finite = true;
                float peak = 0.0f;
                renderRms (d, finite, peak, &loudness, hold);
            }
            // Level it to the original. A candidate that would need more than 9 dB of correction is
            // far quieter or louder than the original (a carrier pushed out of range, a filter shut):
            // not a variation of the same sound, so it is dropped rather than half-corrected.
            const float gainDb = parentLoudness - loudness;
            const float level = parent.out.levelDb + gainDb;
            if (std::fabs (gainDb) > 9.0f || level < -24.0f || level > 12.0f) continue;
            d.out.levelDb = level;
            if (distance (parent, d) < kMinDistance[strength]) continue;
            bool duplicate = false;
            for (auto& other : result) duplicate = duplicate || distance (other.data, d) < kMinDistance[strength] * 0.6f;
            if (duplicate) continue;

            best.data = d;
            best.seed = candSeed;
            found = true;
        }
        if (! found)
        {
            // Fallback: a guaranteed-safe small change relative to the parent.
            PatchData d = parent;
            const float step = 0.03f * (float) (i + 1) * ((i % 2) ? -1.0f : 1.0f);
            bool changed = false;
            for (int c = 0; c < pc::total && ! changed; ++c)
            {
                if (! explorable (d, c) || lockedByMacro (parent, settings.locks, c)) continue;
                if ((settings.locks.envelopes && isEnvCode (c)) || (settings.locks.fx && pc::isFx (c))) continue;
                const auto& desc = paramDesc (c);
                const float n = toNorm (desc, getParam (d, c));
                const float n2 = clampf (n + step, 0.05f, 0.75f);
                if (pc::isOp (c) && c % kOpFieldCount == kOpCarrierGain) continue;
                if (std::fabs (n2 - n) < 1.0e-4f) continue;
                PatchData trial = d;
                setParam (trial, c, fromNorm (desc, n2));
                if (isSafe (trial)) { d = trial; changed = true; }
            }
            best.data = d;
            best.seed = mix64 (seed ^ (uint64_t) (i + 1000));
            best.fallback = true;
        }
        best.parentChecksum = parentSum;
        best.scores = score (best.data);
        best.name = describe (parentScores, best.scores, best.tendency);
        result.push_back (std::move (best));
    }
    return result;
}
} // namespace fmlume
