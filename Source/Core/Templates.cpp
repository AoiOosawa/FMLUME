#include "Core/Templates.h"
#include <iterator>
#include "Core/Params.h"
#include "Core/PatchCompiler.h"
#include <algorithm>

namespace fmlume
{
namespace
{
#define M(...) (uint8_t) (0 __VA_OPT__(| maskOf ({ __VA_ARGS__ })))
constexpr uint8_t maskOf (std::initializer_list<int> ops)
{
    uint8_t m = 0;
    for (int o : ops) m = (uint8_t) (m | (1u << (o - 1)));
    return m;
}
// Edges are written 1-based for readability and converted on access.
const AlgorithmTemplate kTemplates[] = {
    { "Solo", "1", 0, {}, M (1), M (1), -1 },
    { "Stack 2", "1>2", 1, { { 1, 2 } }, M (2), M (1, 2), -1 },
    { "Stack 3", "1>2>3", 2, { { 1, 2 }, { 2, 3 } }, M (3), M (1, 2, 3), -1 },
    { "Stack 4", "1>2>3>4", 3, { { 1, 2 }, { 2, 3 }, { 3, 4 } }, M (4), M (1, 2, 3, 4), -1 },
    { "Dual Carrier", "1 + 2", 0, {}, M (1, 2), M (1, 2), -1 },
    { "Triple Carrier", "1 + 2 + 3", 0, {}, M (1, 2, 3), M (1, 2, 3), -1 },
    { "Split", "1>(2,3)", 2, { { 1, 2 }, { 1, 3 } }, M (2, 3), M (1, 2, 3), -1 },
    { "Merge", "(1,2)>3", 2, { { 1, 3 }, { 2, 3 } }, M (3), M (1, 2, 3), -1 },
    { "Twin Stacks", "1>2 + 3>4", 2, { { 1, 2 }, { 3, 4 } }, M (2, 4), M (1, 2, 3, 4), -1 },
    { "Triple Stacks", "1>2 + 3>4 + 5>6", 3, { { 1, 2 }, { 3, 4 }, { 5, 6 } }, M (2, 4, 6), M (1, 2, 3, 4, 5, 6), -1 },
    { "Shared Mod", "1>(2,3,4)", 3, { { 1, 2 }, { 1, 3 }, { 1, 4 } }, M (2, 3, 4), M (1, 2, 3, 4), -1 },
    { "Feedback Solo", "1*", 0, {}, M (1), M (1), 0 },
    { "Feedback Stack", "1*>2", 1, { { 1, 2 } }, M (2), M (1, 2), 0 },
    { "Stack + Carrier", "1>2>3 + 4", 2, { { 1, 2 }, { 2, 3 } }, M (3, 4), M (1, 2, 3, 4), -1 },
    { "Bell Pair", "(1,2)>3 + 4>5", 3, { { 1, 3 }, { 2, 3 }, { 4, 5 } }, M (3, 5), M (1, 2, 3, 4, 5), -1 },
    { "Stack 4 + Pair", "1>2>3>4 + 5>6", 4, { { 1, 2 }, { 2, 3 }, { 3, 4 }, { 5, 6 } }, M (4, 6), M (1, 2, 3, 4, 5, 6), -1 },
    { "Organ", "1+2+3+4+5+6", 0, {}, M (1, 2, 3, 4, 5, 6), M (1, 2, 3, 4, 5, 6), -1 },
    { "Wide Merge", "(1,2,3)>4 + 5*>6", 4, { { 1, 4 }, { 2, 4 }, { 3, 4 }, { 5, 6 } }, M (4, 6), M (1, 2, 3, 4, 5, 6), 4 },
};
#undef M
constexpr int kNumTemplates = (int) (sizeof (kTemplates) / sizeof (kTemplates[0]));

const char* const kCategoryMacros[][4] = {
    { "Snap", "Body", "Damping", "Space" },         // Arp (built on the Pluck voice)
    { "Tine", "Body", "Bell", "Tremolo" },          // E.Piano
    { "Metallic", "Shimmer", "Decay", "Motion" },   // Bell
    { "Growl", "Punch", "Weight", "Drive" },        // Bass
    { "Bite", "Glide", "Edge", "Width" },           // Lead
    { "Softness", "Width", "Movement", "Air" },     // Pad
    { "Sweep", "Detune", "Sub", "Motion" },         // Poly
    { "Snap", "Body", "Damping", "Space" },         // Pluck
    { "Bite", "Body", "Attack Shape", "Ensemble" }, // Brass
    { "Digital", "Body", "Ring", "Width" },         // Keys
    { "Chaos", "Pitch Sweep", "Texture", "Tail" },  // SFX
};
static_assert (std::size (kCategoryMacros) == (size_t) Category::Count, "every Category needs its four macro names");
const char* const kCommonMacros[4] = { "Brightness", "Attack", "Release", "Character" };

void enableFx (PatchData& d, FxType t, std::initializer_list<std::pair<int, float>> params = {})
{
    auto& pr = d.fx.proc[(size_t) t];
    pr.enabled = true;
    for (auto& kv : params) pr.p[(size_t) kv.first] = kv.second;
}
} // namespace

int numAlgorithmTemplates() { return kNumTemplates; }
const AlgorithmTemplate& algorithmTemplate (int index) { return kTemplates[std::min (std::max (index, 0), kNumTemplates - 1)]; }

void applyAlgorithm (EnginePatch& e, int templateIndex, float defaultIndex)
{
    const auto& t = algorithmTemplate (templateIndex);
    const EnginePatch old = e;
    e.numEdges = 0;
    for (int i = 0; i < t.numEdges; ++i)
    {
        const int from = t.edges[i][0] - 1, to = t.edges[i][1] - 1;
        const int prev = findEdge (old, from, to);
        e.edges[(size_t) e.numEdges++] = { (int8_t) from, (int8_t) to, prev >= 0 ? old.edges[(size_t) prev].index : defaultIndex };
    }
    for (int k = 0; k < kNumOps; ++k)
    {
        auto& op = e.operators[(size_t) k];
        op.enabled = (t.usedMask >> k) & 1u;
        const bool carrier = (t.carrierMask >> k) & 1u;
        op.carrierGain = carrier ? (old.operators[(size_t) k].carrierGain > 0.0f ? old.operators[(size_t) k].carrierGain : 1.0f) : 0.0f;
        if (op.enabled && op.level <= 0.0f) op.level = 1.0f;
        if (t.feedbackOp == k) op.feedback = old.operators[(size_t) k].feedback > 0.0f ? old.operators[(size_t) k].feedback : 0.6f;
        else if (t.feedbackOp >= 0 || ! op.enabled) op.feedback = 0.0f;
    }
}

int matchAlgorithm (const EnginePatch& e)
{
    // Pass 0 requires the feedback layout to match too; pass 1 ignores feedback on plain templates.
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < kNumTemplates; ++i)
        {
            const auto& t = kTemplates[i];
            if (t.numEdges != e.numEdges) continue;
            bool ok = true;
            for (int k = 0; k < kNumOps && ok; ++k)
            {
                const auto& op = e.operators[(size_t) k];
                ok = op.enabled == (bool) ((t.usedMask >> k) & 1u) && (! op.enabled || (op.carrierGain > 0.0f) == (bool) ((t.carrierMask >> k) & 1u));
                if (ok && op.enabled && pass == 0) ok = (op.feedback > 0.0f) == (t.feedbackOp == k);
            }
            for (int j = 0; j < t.numEdges && ok; ++j) ok = findEdge (e, t.edges[j][0] - 1, t.edges[j][1] - 1) >= 0;
            if (ok && t.feedbackOp >= 0 && e.operators[(size_t) t.feedbackOp].feedback <= 0.0f) ok = false;
            if (ok) return i;
        }
    return -1;
}

int matchAlgorithmShape (const EnginePatch& e, bool* renumbered)
{
    if (renumbered != nullptr) *renumbered = false;
    const int exact = matchAlgorithm (e);
    if (exact >= 0) return exact;

    // Audible part of the engine: carriers plus everything that modulates them (transitively).
    bool live[kNumOps] {}, carrier[kNumOps] {}, adj[kNumOps][kNumOps] {};
    for (int k = 0; k < kNumOps; ++k) live[k] = carrier[k] = e.operators[(size_t) k].enabled && e.operators[(size_t) k].carrierGain > 0.0f;
    for (int pass = 0; pass < kNumOps; ++pass)
        for (int i = 0; i < e.numEdges; ++i)
        {
            const auto& ed = e.edges[(size_t) i];
            if (live[ed.to] && e.operators[(size_t) ed.from].enabled) live[ed.from] = true;
        }
    int liveCount = 0, edgeCount = 0, carrierCount = 0;
    bool anyFeedback = false;
    for (int k = 0; k < kNumOps; ++k)
    {
        liveCount += live[k] ? 1 : 0;
        carrierCount += carrier[k] ? 1 : 0;
        anyFeedback = anyFeedback || (live[k] && e.operators[(size_t) k].feedback > 0.0f);
    }
    for (int i = 0; i < e.numEdges; ++i)
    {
        const auto& ed = e.edges[(size_t) i];
        if (live[ed.from] && live[ed.to] && ! adj[ed.from][ed.to]) { adj[ed.from][ed.to] = true; ++edgeCount; }
    }
    if (liveCount == 0) return -1;

    int perm[kNumOps];
    // pass 0: the feedback layout must agree as well; pass 1: routing only.
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < kNumTemplates; ++i)
        {
            const auto& t = kTemplates[i];
            int used = 0, tCarriers = 0;
            for (int k = 0; k < kNumOps; ++k) { used += (t.usedMask >> k) & 1u; tCarriers += (t.usedMask >> k) & (t.carrierMask >> k) & 1u; }
            if (used != liveCount || t.numEdges != edgeCount || tCarriers != carrierCount) continue;
            if (pass == 0 && (t.feedbackOp >= 0) != anyFeedback) continue;
            for (int k = 0; k < kNumOps; ++k) perm[k] = k;
            do
            {
                // perm maps template operator k -> engine operator perm[k]
                bool ok = true;
                for (int k = 0; k < kNumOps && ok; ++k)
                {
                    const bool tUsed = (t.usedMask >> k) & 1u;
                    ok = tUsed == live[perm[k]] && (! tUsed || (bool) ((t.carrierMask >> k) & 1u) == carrier[perm[k]]);
                }
                for (int j = 0; j < t.numEdges && ok; ++j) ok = adj[perm[t.edges[j][0] - 1]][perm[t.edges[j][1] - 1]];
                if (ok && pass == 0 && t.feedbackOp >= 0) ok = e.operators[(size_t) perm[t.feedbackOp]].feedback > 0.0f;
                if (ok)
                {
                    if (renumbered != nullptr)
                        for (int k = 0; k < kNumOps; ++k)
                            if (((t.usedMask >> k) & 1u) && perm[k] != k) *renumbered = true;
                    return i;
                }
            } while (std::next_permutation (perm, perm + kNumOps));
        }
    return -1;
}

OperatorState makeOp (float ratio, float level, float a, float d, float s, float r, float carrierGain)
{
    OperatorState op;
    op.enabled = true;
    op.ratio = ratio;
    op.level = level;
    op.env = { a, d, s, r };
    op.carrierGain = carrierGain;
    return op;
}

bool addEdge (EnginePatch& e, int from, int to, float index)
{
    if (edgeWouldBeInvalid (e, from, to)) return false;
    e.edges[(size_t) e.numEdges++] = { (int8_t) from, (int8_t) to, clampf (index, 0.0f, 16.0f) };
    return true;
}

bool removeEdge (EnginePatch& e, int from, int to)
{
    const int i = findEdge (e, from, to);
    if (i < 0) return false;
    for (int j = i; j + 1 < e.numEdges; ++j) e.edges[(size_t) j] = e.edges[(size_t) j + 1];
    --e.numEdges;
    return true;
}

bool addMapping (Macro& m, int target, float amount, Curve curve, Polarity pol, float min, float max)
{
    if (m.numMappings >= kMaxMacroMappings || ! isMacroTarget (target)) return false;
    for (int i = 0; i < m.numMappings; ++i)
        if (m.mappings[(size_t) i].target == target) { m.mappings[(size_t) i].amount = clampf (amount, -1.0f, 1.0f); return true; }
    auto& mp = m.mappings[(size_t) m.numMappings++];
    mp.target = (int16_t) target;
    mp.amount = clampf (amount, -1.0f, 1.0f);
    mp.curve = curve;
    mp.polarity = pol;
    mp.min = min;
    mp.max = max;
    return true;
}

void setModSlot (EngineSide& s, int slot, ModSource src, int target, float depth)
{
    auto& m = s.matrix[(size_t) slot];
    m.source = src;
    m.target = (int16_t) target;
    m.depth = depth;
    m.curve = Curve::Linear;
    m.enabled = true;
}

void assignCommonMacros (PatchData& d, bool sideB)
{
    EngineSide& s = sideB ? d.b : d.a;
    const EnginePatch& e = s.engine;
    for (int i = 0; i < 4; ++i) s.macros[(size_t) i].numMappings = 0;
    auto& bright = s.macros[0];
    auto& attack = s.macros[1];
    auto& release = s.macros[2];
    auto& character = s.macros[3];

    // Brightness: modulation indices up, filter opens, slight level compensation so "bright != loud".
    int edgesUsed = 0;
    for (int i = 0; i < e.numEdges && edgesUsed < 5; ++i)
    {
        const auto& ed = e.edges[(size_t) i];
        if (! e.operators[(size_t) ed.from].enabled || ! e.operators[(size_t) ed.to].enabled) continue;
        addMapping (bright, pc::edge (ed.from, ed.to), 0.22f);
        ++edgesUsed;
    }
    if (e.filter.type != FilterType::Off) addMapping (bright, pc::filterCutoff, 0.3f);
    if (edgesUsed == 0 && e.filter.type == FilterType::Off)
        for (int k = 0; k < kNumOps && bright.numMappings < 3; ++k)
            if (e.operators[(size_t) k].enabled) addMapping (bright, pc::op (k, kOpFeedback), 0.35f);
    addMapping (bright, pc::level, -0.05f);

    // Attack / Release: relative (log-domain) envelope time changes. Carriers first.
    for (int pass = 0; pass < 2; ++pass)
        for (int k = 0; k < kNumOps; ++k)
        {
            const auto& op = e.operators[(size_t) k];
            if (! op.enabled || (op.carrierGain > 0.0f) != (pass == 0)) continue;
            addMapping (attack, pc::op (k, kOpEnvA), pass == 0 ? 0.35f : 0.2f);
            addMapping (release, pc::op (k, kOpEnvR), 0.3f);
            // Percussive carriers (no sustain) ring out through their decay stage, so "Release"
            // has to lengthen the decay as well or it would do nothing on held notes.
            if (pass == 0 && op.env.s < 0.05f) addMapping (release, pc::op (k, kOpEnvD), 0.2f);
        }

    // Character: self feedback on the first modulator (or carrier), plus detune between carriers.
    int fbOp = -1;
    for (int i = 0; i < e.numEdges && fbOp < 0; ++i)
        if (e.operators[(size_t) e.edges[(size_t) i].from].enabled) fbOp = e.edges[(size_t) i].from;
    for (int k = 0; k < kNumOps && fbOp < 0; ++k)
        if (e.operators[(size_t) k].enabled) fbOp = k;
    if (fbOp >= 0) addMapping (character, pc::op (fbOp, kOpFeedback), 0.4f);
    int carriers = 0;
    for (int k = 0; k < kNumOps; ++k)
    {
        const auto& op = e.operators[(size_t) k];
        if (! op.enabled || op.carrierGain <= 0.0f) continue;
        if (carriers > 0 && carriers < 3) addMapping (character, pc::op (k, kOpDetune), (carriers % 2 ? 0.06f : -0.06f));
        ++carriers;
    }
    for (int k = 0; k < kNumOps; ++k)
        if (e.operators[(size_t) k].enabled && e.operators[(size_t) k].carrierGain <= 0.0f && k != fbOp)
        {
            addMapping (character, pc::op (k, kOpLevel), 0.25f);
            break;
        }
}

void assignQuickControls (PatchData& d)
{
    for (auto& q : d.fx.quick) q = Macro {};
    // Warmth
    addMapping (d.fx.quick[0], pc::fx (FxType::Drive, 0), 0.45f, Curve::Linear, Polarity::Bipolar);
    addMapping (d.fx.quick[0], pc::fx (FxType::EQ, 0), 0.2f);
    addMapping (d.fx.quick[0], pc::fx (FxType::EQ, 5), -0.15f);
    // Width
    addMapping (d.fx.quick[1], pc::fx (FxType::Chorus, 3), 0.6f);
    addMapping (d.fx.quick[1], pc::fx (FxType::Chorus, 2), 0.3f);
    // Space
    addMapping (d.fx.quick[2], pc::fx (FxType::Reverb, 4), 0.6f);
    addMapping (d.fx.quick[2], pc::fx (FxType::Reverb, 1), 0.2f);
    // Echo
    addMapping (d.fx.quick[3], pc::fx (FxType::Delay, 5), 0.6f);
    addMapping (d.fx.quick[3], pc::fx (FxType::Delay, 3), 0.25f);
}

const char* categoryMacroName (Category c, int macroIndex)
{
    if (macroIndex < 4) return kCommonMacros[std::max (macroIndex, 0)];
    return kCategoryMacros[std::min<int> ((int) c, (int) Category::Count - 1)][std::min (macroIndex - 4, 3)];
}

Preset makeCategoryPreset (Category c)
{
    Preset p = makeInitPreset();
    p.meta.category = c;
    p.meta.displayName = std::string ("Init ") + categoryName (c);
    p.meta.patchId = std::string ("template.") + categoryName (c);
    p.meta.description = std::string ("Starting point for ") + categoryName (c) + " sounds.";
    PatchData& d = p.data;
    EngineSide& s = d.a;
    EnginePatch& e = s.engine;
    auto& ops = e.operators;
    for (auto& op : ops) op = OperatorState {};
    e.numEdges = 0;
    auto& m5 = s.macros[4];
    auto& m6 = s.macros[5];
    auto& m7 = s.macros[6];
    auto& m8 = s.macros[7];

    // Time-based FX are present (wet = 0 where unused) so the FX quick controls always respond.
    enableFx (d, FxType::Chorus, { { 3, 0.0f } });
    enableFx (d, FxType::Delay, { { 5, 0.0f } });
    enableFx (d, FxType::Reverb, { { 4, 0.12f } });
    enableFx (d, FxType::EQ);
    enableFx (d, FxType::Drive, { { 0, 0.0f }, { 2, 1.0f } });

    // The Arp category starts from the Pluck voice with the arpeggiator running (see below).
    switch (c == Category::Arp ? Category::Pluck : c)
    {
        case Category::EPiano:
            ops[0] = makeOp (14.0f, 0.55f, 0.001f, 0.25f, 0.0f, 0.2f);       // tine modulator
            ops[1] = makeOp (1.0f, 1.0f, 0.002f, 2.2f, 0.0f, 0.35f, 1.0f);    // tine carrier
            ops[2] = makeOp (1.0f, 0.75f, 0.002f, 1.6f, 0.15f, 0.3f);         // body modulator
            ops[3] = makeOp (1.0f, 1.0f, 0.002f, 2.8f, 0.0f, 0.4f, 0.9f);     // body carrier
            ops[4] = makeOp (4.0f, 0.8f, 0.001f, 0.9f, 0.0f, 0.5f, 0.0f);     // bell partial (Macro 7)
            ops[0].velocityToIndex = 0.8f; ops[2].velocityToIndex = 0.7f;
            ops[3].detuneCents = 3.0f;
            addEdge (e, 0, 1, 1.4f);
            addEdge (e, 2, 3, 1.8f);
            setModSlot (s, 0, ModSource::LFO1, pc::pan, 0.0f);
            e.lfo[0].rateHz = 4.5f; e.lfo[0].retrig = false;
            addMapping (m5, pc::edge (0, 1), 0.2f); addMapping (m5, pc::op (0, kOpEnvD), 0.15f);
            addMapping (m6, pc::edge (2, 3), 0.25f); addMapping (m6, pc::op (3, kOpEnvD), 0.12f);
            addMapping (m7, pc::op (4, kOpCarrierGain), 0.6f, Curve::Linear, Polarity::Unipolar);
            addMapping (m8, pc::matrixDepthBase + 0, 0.35f, Curve::Linear, Polarity::Unipolar, 0.5f, 1.0f);
            d.fx.proc[(size_t) FxType::Chorus].p[3] = 0.2f;
            break;

        case Category::Bell:
            ops[0] = makeOp (3.5f, 0.8f, 0.001f, 1.8f, 0.0f, 1.5f);
            ops[1] = makeOp (7.07f, 0.5f, 0.001f, 0.6f, 0.0f, 0.6f);
            ops[2] = makeOp (1.0f, 1.0f, 0.001f, 3.5f, 0.0f, 2.5f, 1.0f);
            ops[3] = makeOp (2.76f, 0.7f, 0.001f, 2.2f, 0.0f, 2.0f);
            ops[4] = makeOp (1.0f, 0.9f, 0.001f, 4.5f, 0.0f, 3.0f, 0.7f);
            ops[4].detuneCents = 6.0f;
            addEdge (e, 0, 2, 2.2f);
            addEdge (e, 1, 2, 1.0f);
            addEdge (e, 3, 4, 1.6f);
            setModSlot (s, 0, ModSource::LFO1, pc::edge (3, 4), 0.0f);
            setModSlot (s, 1, ModSource::LFO2, pc::pitch, 0.0f);
            e.lfo[0].rateHz = 0.7f; e.lfo[1].rateHz = 5.2f;
            addMapping (m5, pc::edge (0, 2), 0.2f); addMapping (m5, pc::edge (1, 2), 0.25f); addMapping (m5, pc::edge (3, 4), 0.15f);
            addMapping (m6, pc::fx (FxType::Chorus, 3), 0.5f); addMapping (m6, pc::matrixDepthBase + 1, 0.004f, Curve::Linear, Polarity::Unipolar, 0.5f, 1.0f);
            addMapping (m7, pc::op (2, kOpEnvD), 0.3f); addMapping (m7, pc::op (4, kOpEnvD), 0.3f);
            addMapping (m7, pc::op (2, kOpEnvR), 0.3f); addMapping (m7, pc::op (4, kOpEnvR), 0.3f);
            addMapping (m8, pc::matrixDepthBase + 0, 0.12f, Curve::Linear, Polarity::Unipolar, 0.5f, 1.0f);
            d.fx.proc[(size_t) FxType::Reverb].p[4] = 0.3f;
            break;

        case Category::Bass:
            ops[0] = makeOp (1.0f, 0.8f, 0.001f, 0.25f, 0.2f, 0.12f);
            ops[1] = makeOp (1.0f, 0.9f, 0.001f, 0.35f, 0.3f, 0.12f);
            ops[2] = makeOp (1.0f, 1.0f, 0.001f, 0.6f, 0.7f, 0.12f, 1.0f);
            ops[3] = makeOp (0.5f, 1.0f, 0.001f, 0.8f, 0.8f, 0.12f, 0.0f); // sub (Macro 7)
            ops[0].feedback = 0.4f;
            addEdge (e, 0, 1, 1.6f);
            addEdge (e, 1, 2, 2.2f);
            d.perf.mono = true;
            addMapping (m5, pc::edge (0, 1), 0.25f); addMapping (m5, pc::op (0, kOpFeedback), 0.35f);
            addMapping (m6, pc::edge (1, 2), 0.2f); addMapping (m6, pc::op (1, kOpEnvD), -0.2f);
            addMapping (m7, pc::op (3, kOpCarrierGain), 0.7f, Curve::Linear, Polarity::Unipolar); addMapping (m7, pc::fx (FxType::EQ, 0), 0.15f);
            addMapping (m8, pc::fx (FxType::Drive, 0), 0.6f, Curve::Linear, Polarity::Unipolar);
            d.fx.proc[(size_t) FxType::Reverb].p[4] = 0.0f;
            break;

        case Category::Lead:
            ops[0] = makeOp (1.0f, 0.8f, 0.005f, 0.4f, 0.7f, 0.2f);
            ops[1] = makeOp (1.0f, 1.0f, 0.005f, 0.3f, 0.85f, 0.2f, 1.0f);
            ops[2] = makeOp (2.0f, 0.6f, 0.005f, 0.5f, 0.5f, 0.2f);
            ops[3] = makeOp (1.0f, 1.0f, 0.005f, 0.3f, 0.85f, 0.2f, 0.8f);
            ops[0].feedback = 0.5f;
            ops[3].detuneCents = 7.0f;
            addEdge (e, 0, 1, 2.4f);
            addEdge (e, 2, 3, 1.5f);
            d.perf.mono = true; d.perf.legato = true;
            setModSlot (s, 0, ModSource::ModWheel, pc::pitch, 0.0f);
            setModSlot (s, 1, ModSource::LFO1, pc::pitch, 0.012f);
            s.matrix[1].enabled = true; e.lfo[0].rateHz = 5.5f;
            addMapping (m5, pc::edge (0, 1), 0.25f); addMapping (m5, pc::edge (2, 3), 0.2f);
            addMapping (m6, pc::portamento, 0.25f, Curve::Linear, Polarity::Unipolar);
            addMapping (m7, pc::op (0, kOpFeedback), 0.45f); addMapping (m7, pc::op (2, kOpLevel), 0.3f);
            addMapping (m8, pc::fx (FxType::Chorus, 3), 0.55f); addMapping (m8, pc::op (3, kOpDetune), 0.08f);
            d.fx.proc[(size_t) FxType::Delay].p[5] = 0.18f;
            break;

        case Category::Pad:
            ops[0] = makeOp (1.0f, 0.6f, 0.8f, 2.0f, 0.7f, 1.8f);
            ops[1] = makeOp (1.0f, 1.0f, 0.6f, 2.0f, 0.85f, 1.8f, 1.0f);
            ops[2] = makeOp (2.0f, 0.5f, 1.2f, 2.5f, 0.6f, 2.0f);
            ops[3] = makeOp (1.0f, 1.0f, 0.7f, 2.0f, 0.85f, 2.0f, 0.9f);
            ops[1].detuneCents = -6.0f; ops[3].detuneCents = 6.0f;
            addEdge (e, 0, 1, 1.5f);
            addEdge (e, 2, 3, 1.2f);
            e.filter.type = FilterType::LP12; e.filter.cutoffHz = 5200.0f; e.filter.resonance = 0.15f;
            setModSlot (s, 0, ModSource::LFO1, pc::edge (0, 1), 0.04f);
            setModSlot (s, 1, ModSource::LFO2, pc::filterCutoff, 0.0f);
            e.lfo[0].rateHz = 0.25f; e.lfo[0].retrig = false; e.lfo[1].rateHz = 0.13f; e.lfo[1].retrig = false;
            d.host.unison = 2; d.host.detune = 12.0f; d.host.spread = 80.0f;
            addMapping (m5, pc::edge (0, 1), -0.15f); addMapping (m5, pc::edge (2, 3), -0.15f); addMapping (m5, pc::filterCutoff, -0.25f);
            addMapping (m6, pc::fx (FxType::Chorus, 3), 0.5f); addMapping (m6, pc::fx (FxType::Chorus, 2), 0.3f);
            addMapping (m7, pc::matrixDepthBase + 0, 0.12f, Curve::Linear, Polarity::Unipolar, 0.5f, 1.0f);
            addMapping (m7, pc::matrixDepthBase + 1, 0.1f, Curve::Linear, Polarity::Unipolar, 0.5f, 1.0f); addMapping (m7, pc::lfo1Rate, 0.25f);
            addMapping (m8, pc::fx (FxType::EQ, 5), 0.25f); addMapping (m8, pc::fx (FxType::Reverb, 4), 0.3f);
            d.fx.proc[(size_t) FxType::Chorus].p[3] = 0.3f;
            d.fx.proc[(size_t) FxType::Reverb].p[4] = 0.35f;
            d.fx.proc[(size_t) FxType::Reverb].p[1] = 4.0f;
            break;

        case Category::Poly:
            // Polysynth: two detuned saw carriers through a 24 dB low-pass with an envelope sweep,
            // a sine sub (Macro 7) and a modulator that can add FM bite. Brighter and more
            // immediate than Pad, more sustained and "synth" than Keys.
            ops[0] = makeOp (1.0f, 1.0f, 0.004f, 0.8f, 0.8f, 0.3f, 0.7f);
            ops[1] = makeOp (1.0f, 1.0f, 0.004f, 0.8f, 0.8f, 0.3f, 0.7f);
            ops[2] = makeOp (0.5f, 1.0f, 0.004f, 0.8f, 0.8f, 0.3f, 0.0f);   // sub (Macro 7)
            ops[3] = makeOp (2.0f, 0.6f, 0.002f, 0.4f, 0.3f, 0.3f);         // FM bite
            ops[0].waveform = Waveform::Saw; ops[1].waveform = Waveform::Saw;
            ops[0].detuneCents = -7.0f; ops[1].detuneCents = 7.0f;
            addEdge (e, 3, 0, 0.0f);
            e.filter.type = FilterType::LP24; e.filter.cutoffHz = 2600.0f; e.filter.resonance = 0.22f;
            e.modEnv = { 0.004f, 0.45f, 0.25f, 0.35f };
            setModSlot (s, 0, ModSource::ModEnv, pc::filterCutoff, 0.3f);
            setModSlot (s, 1, ModSource::LFO1, pc::filterCutoff, 0.0f);
            e.lfo[0].rateHz = 0.35f; e.lfo[0].retrig = false;
            d.host.unison = 2; d.host.detune = 12.0f; d.host.spread = 75.0f;
            addMapping (m5, pc::matrixDepthBase + 0, 0.25f); addMapping (m5, pc::filterRes, 0.15f);
            addMapping (m6, pc::op (0, kOpDetune), -0.1f); addMapping (m6, pc::op (1, kOpDetune), 0.1f);
            addMapping (m7, pc::op (2, kOpCarrierGain), 0.5f, Curve::Linear, Polarity::Unipolar, 0.5f, 1.0f);
            addMapping (m8, pc::matrixDepthBase + 1, 0.2f, Curve::Linear, Polarity::Unipolar, 0.5f, 1.0f); addMapping (m8, pc::lfo1Rate, 0.25f);
            d.fx.proc[(size_t) FxType::Chorus].p[3] = 0.3f;
            d.fx.proc[(size_t) FxType::Reverb].p[4] = 0.15f;
            break;

        case Category::Pluck:
            ops[0] = makeOp (3.0f, 0.8f, 0.001f, 0.12f, 0.0f, 0.1f);
            ops[1] = makeOp (1.0f, 1.0f, 0.001f, 0.7f, 0.0f, 0.35f, 1.0f);
            ops[2] = makeOp (1.0f, 0.6f, 0.001f, 0.4f, 0.0f, 0.2f);
            ops[0].velocityToIndex = 0.7f;
            addEdge (e, 0, 1, 2.6f);
            addEdge (e, 2, 1, 1.0f);
            e.filter.type = FilterType::LP12; e.filter.cutoffHz = 9000.0f;
            addMapping (m5, pc::edge (0, 1), 0.25f); addMapping (m5, pc::op (0, kOpEnvD), -0.15f);
            addMapping (m6, pc::edge (2, 1), 0.2f); addMapping (m6, pc::op (1, kOpEnvD), 0.2f);
            addMapping (m7, pc::filterCutoff, -0.3f); addMapping (m7, pc::op (1, kOpEnvD), -0.15f);
            addMapping (m8, pc::fx (FxType::Reverb, 4), 0.35f); addMapping (m8, pc::fx (FxType::Delay, 5), 0.3f);
            d.fx.proc[(size_t) FxType::Delay].p[5] = 0.15f;
            break;

        case Category::Brass:
            ops[0] = makeOp (1.0f, 0.85f, 0.06f, 0.5f, 0.7f, 0.2f);
            ops[1] = makeOp (1.0f, 1.0f, 0.04f, 0.3f, 0.9f, 0.18f, 1.0f);
            ops[2] = makeOp (1.0f, 0.6f, 0.08f, 0.6f, 0.6f, 0.2f);
            ops[3] = makeOp (2.0f, 1.0f, 0.05f, 0.3f, 0.8f, 0.18f, 0.5f);
            ops[0].feedback = 0.7f; ops[0].velocityToIndex = 0.7f;
            ops[3].detuneCents = 4.0f;
            addEdge (e, 0, 1, 2.6f);
            addEdge (e, 2, 3, 1.4f);
            addMapping (m5, pc::edge (0, 1), 0.25f); addMapping (m5, pc::op (0, kOpFeedback), 0.25f);
            addMapping (m6, pc::edge (2, 3), 0.2f); addMapping (m6, pc::fx (FxType::EQ, 0), 0.2f);
            addMapping (m7, pc::op (0, kOpEnvA), 0.3f); addMapping (m7, pc::op (2, kOpEnvA), 0.3f);
            addMapping (m8, pc::fx (FxType::Chorus, 3), 0.5f); addMapping (m8, pc::op (3, kOpDetune), 0.06f);
            break;

        case Category::Keys:
            ops[0] = makeOp (2.0f, 0.7f, 0.001f, 0.5f, 0.2f, 0.25f);
            ops[1] = makeOp (1.0f, 1.0f, 0.002f, 1.5f, 0.3f, 0.3f, 1.0f);
            ops[2] = makeOp (3.01f, 0.5f, 0.001f, 0.35f, 0.0f, 0.25f);
            ops[3] = makeOp (2.0f, 0.8f, 0.002f, 1.0f, 0.2f, 0.3f, 0.45f);
            addEdge (e, 0, 1, 1.6f);
            addEdge (e, 2, 3, 0.8f);
            enableFx (d, FxType::Bitcrusher, { { 0, 10.0f }, { 1, 2.0f }, { 2, 0.0f } });
            addMapping (m5, pc::fx (FxType::Bitcrusher, 2), 0.7f, Curve::Linear, Polarity::Unipolar); addMapping (m5, pc::edge (0, 1), 0.12f);
            addMapping (m6, pc::op (1, kOpEnvD), 0.2f); addMapping (m6, pc::fx (FxType::EQ, 0), 0.2f);
            addMapping (m7, pc::edge (2, 3), 0.3f); addMapping (m7, pc::op (3, kOpCarrierGain), 0.3f);
            addMapping (m8, pc::fx (FxType::Chorus, 3), 0.55f); addMapping (m8, pc::fx (FxType::Chorus, 2), 0.3f);
            break;

        case Category::SFX:
        default:
            ops[0] = makeOp (0.5f, 0.9f, 0.01f, 1.5f, 0.5f, 1.0f);
            ops[1] = makeOp (2.37f, 0.9f, 0.01f, 1.2f, 0.4f, 1.0f);
            ops[2] = makeOp (1.0f, 1.0f, 0.01f, 2.0f, 0.5f, 1.5f, 1.0f);
            ops[0].pitchMode = PitchMode::Fixed; ops[0].fixedHz = 3.0f; ops[0].keyTracking = 0.0f; ops[0].feedback = 0.3f;
            addEdge (e, 0, 1, 3.0f);
            addEdge (e, 1, 2, 3.5f);
            e.modEnv = { 0.01f, 0.8f, 0.0f, 0.5f };
            setModSlot (s, 0, ModSource::ModEnv, pc::pitch, 0.0f);
            enableFx (d, FxType::Flanger, { { 3, 0.0f } });
            addMapping (m5, pc::edge (0, 1), 0.35f); addMapping (m5, pc::edge (1, 2), 0.3f); addMapping (m5, pc::op (0, kOpFeedback), 0.4f);
            addMapping (m6, pc::matrixDepthBase + 0, 0.5f); addMapping (m6, pc::modEnvD, 0.2f);
            addMapping (m7, pc::fx (FxType::Flanger, 3), 0.7f, Curve::Linear, Polarity::Unipolar); addMapping (m7, pc::op (1, kOpLevel), 0.2f);
            addMapping (m8, pc::fx (FxType::Reverb, 4), 0.35f); addMapping (m8, pc::fx (FxType::Reverb, 1), 0.25f); addMapping (m8, pc::op (2, kOpEnvR), 0.3f);
            d.fx.proc[(size_t) FxType::Reverb].p[4] = 0.3f;
            break;
    }

    assignCommonMacros (d, false);
    assignQuickControls (d);
    d.b = d.a;
    // Preset level of each starting point, so that "Init" is as loud as the factory presets
    // (phrase peak around -6 dBFS at master 0 dB). Measured with `fmlume-presetgen --template-trims`;
    // re-measure when a template above changes.
    // Arp uses the Pluck value (not its own arpeggiated measurement, +1.1) so that it is not too
    // loud once the arpeggiator is switched off and chords sound together.
    static const float kTemplateLevelDb[] = { -0.5f, -4.3f, -3.9f, 1.3f, 0.6f, -5.9f, -2.1f, -0.5f, -4.8f, -3.3f, -4.6f };
    static_assert (std::size (kTemplateLevelDb) == (size_t) Category::Count, "every Category needs a template level");
    d.out.levelDb = kTemplateLevelDb[std::min<int> ((int) c, (int) Category::Count - 1)];

    if (c == Category::Arp)
    {
        d.host.arpEnabled = true;
        d.host.polyphony = 8;
        d.perf.arp = ArpState {};
        d.perf.arp.mode = ArpMode::Up;
        d.perf.arp.rate = 12;     // 16ths
        d.perf.arp.gate = 0.5f;
        d.perf.arp.octaves = 2;
    }
    for (int i = 0; i < kNumMacros; ++i)
    {
        p.meta.macroNames[(size_t) i] = categoryMacroName (c, i);
        p.meta.macroNamesB[(size_t) i] = p.meta.macroNames[(size_t) i];
    }
    return p;
}
} // namespace fmlume
