#include "TestFramework.h"
#include "Core/Params.h"

using namespace fmlume;

FM_TEST ("unit", "Parameter ids round-trip and normalisation is invertible")
{
    int named = 0;
    for (int c = 0; c < pc::total; ++c)
    {
        const auto& d = paramDesc (c);
        if (d.name == nullptr) continue;
        ++named;
        const std::string id = paramCodeToId (c);
        CHECK (! id.empty());
        CHECK (paramIdToCode (id) == c);
        CHECK_NEAR (toNorm (d, d.def) >= 0.0f && toNorm (d, d.def) <= 1.0f ? 1.0 : 0.0, 1.0, 0.0);
        for (float n : { 0.0f, 0.25f, 0.5f, 1.0f })
            CHECK_NEAR (toNorm (d, fromNorm (d, n)), n, 1.0e-4);
    }
    CHECK (named > 150);
    CHECK (paramIdToCode ("op1.level") == pc::op (0, kOpLevel));
    CHECK (paramIdToCode ("edge.2.1.index") == pc::edge (1, 0));
    CHECK (paramIdToCode ("nonsense") == -1);
}

FM_TEST ("unit", "Macro delta is zero at neutral for every curve and polarity")
{
    for (int c = 0; c < (int) Curve::Count; ++c)
        for (int p = 0; p < (int) Polarity::Count; ++p)
        {
            MacroMapping m;
            m.amount = 0.7f;
            m.curve = (Curve) c;
            m.polarity = (Polarity) p;
            CHECK_NEAR (macroDelta (m, 0.5f), 0.0, 1.0e-7);
            CHECK_NEAR (macroDelta (m, 1.0f), 0.7, 1.0e-6);
            CHECK (macroDelta (m, 0.0f) < 0.0f);
        }
}

// Factory trance-gate patterns (shared vocabulary with POLYBRISK).
#include "DSP/Sequencer/GatePatterns.h"
#include "Core/PatchCompiler.h"
#include <cstring>
#include <set>

FM_TEST ("unit", "Factory gate patterns are valid and can be recognised again")
{
    std::set<std::string> names;
    for (int i = 0; i < fmlume::kNumFactoryGatePatterns; ++i)
    {
        const auto& f = fmlume::kFactoryGatePatterns[(size_t) i];
        CHECK (std::strlen (f.steps16) == (size_t) fmlume::kGateSteps);
        CHECK (f.rate >= 6 && f.rate <= 18);
        CHECK (f.right16 == nullptr || std::strlen (f.right16) == (size_t) fmlume::kGateSteps);
        CHECK (names.insert (f.name).second); // unique names
        fmlume::PatchData d = fmlume::makeInitPatchData();
        fmlume::applyGatePattern (d.perf.gate, i);
        CHECK (fmlume::validatePatch (d).ok);
        bool anyOpen = false;
        for (float s : d.perf.gate.steps) { CHECK (s >= 0.0f && s <= 1.0f); anyOpen = anyOpen || s > 0.0f; }
        CHECK (anyOpen);
        // Patterns with identical steps exist at different rates; the rate disambiguates.
        const int m = fmlume::matchGatePattern (d.perf.gate);
        CHECK (m >= 0);
        CHECK (std::strcmp (fmlume::kFactoryGatePatterns[(size_t) m].steps16, f.steps16) == 0);
        CHECK ((fmlume::kFactoryGatePatterns[(size_t) m].right16 != nullptr) == d.perf.gate.stereo);
    }
    fmlume::PatchData d = fmlume::makeInitPatchData();
    d.perf.gate.steps = { 1, 0.3f, 0, 0, 1, 0, 0.3f, 0, 1, 0, 0, 0, 1, 1, 0, 0.3f };
    CHECK (fmlume::matchGatePattern (d.perf.gate) == -1);
}
