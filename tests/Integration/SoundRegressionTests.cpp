// Sound regression (COMPATIBILITY.md): a project saved with one version must sound the same in the
// next. Representative factory presets are rendered and compared with fingerprints stored in
// tests/Golden/sound_fingerprints.json: per 20 ms, the level of each channel and of a low and a
// high band, in dB. The comparison allows small differences (other CPUs and compilers round
// differently) but catches changed envelopes, levels, filters, effects and timbre.
//
// After a deliberate change of the sound, record new fingerprints and say why in DECISIONS.md:
//   FMLUME_UPDATE_SOUND_FINGERPRINTS=1 build-core/tests/FMLUMETests integration
#include "TestFramework.h"
#include "TestUtil.h"
#include "Core/Json.h"
#include "Presets/PresetManager.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace fmtest;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 256;
constexpr double kSeconds = 2.5;
constexpr int kWindow = 960; // 20 ms
constexpr float kFloorDb = -90.0f;
constexpr float kToleranceDb = 1.0f;
constexpr float kAudibleDb = -60.0f; // quieter windows only have to stay quiet

// the first preset of every category, plus LFO / tremolo, effects and the trance gate
const char* const kPresets[] = {
    "Arp-01-Triad-Stairs-Arp",   "E-Piano-01-Classic-Tines", "Bell-01-Idol-Sparkle-Bell", "Bass-01-FM-Bass-One",
    "Lead-01-Classic-FM-Lead",   "Pad-01-Warm-FM-Pad",       "Poly-01-Classic-Poly-Saw",  "Pluck-01-Classic-Pluck",
    "Brass-01-FM-Brass-Section", "Keys-01-Digital-Keys",     "SFX-01-Chaos-Engine",
    "E-Piano-07-Wide-Tremolo-EP", "Poly-08-Tremolo-Chop",    "Arp-28-Gate-Dreams-Arp",
};

struct Fingerprint { std::vector<float> l, r, low, high; };

float toDb (double meanSquare) { return std::max (kFloorDb, (float) (10.0 * std::log10 (meanSquare + 1.0e-20))); }
float round1 (float v) { return std::round (v * 10.0f) / 10.0f; }

Fingerprint fingerprint (const Rendered& a)
{
    Fingerprint f;
    // one-pole filters on the mono sum: below ~300 Hz, above ~3 kHz
    const float kLow = 1.0f - std::exp (-2.0f * 3.14159265f * 300.0f / (float) kRate);
    const float kHigh = 1.0f - std::exp (-2.0f * 3.14159265f * 3000.0f / (float) kRate);
    float lp = 0.0f, lp3k = 0.0f;
    for (size_t start = 0; start + kWindow <= a.l.size(); start += kWindow)
    {
        double sl = 0, sr = 0, slo = 0, shi = 0;
        for (size_t i = start; i < start + kWindow; ++i)
        {
            const float m = 0.5f * (a.l[i] + a.r[i]);
            lp += kLow * (m - lp);
            lp3k += kHigh * (m - lp3k);
            sl += (double) a.l[i] * a.l[i];
            sr += (double) a.r[i] * a.r[i];
            slo += (double) lp * lp;
            shi += (double) (m - lp3k) * (m - lp3k);
        }
        f.l.push_back (round1 (toDb (sl / kWindow)));
        f.r.push_back (round1 (toDb (sr / kWindow)));
        f.low.push_back (round1 (toDb (slo / kWindow)));
        f.high.push_back (round1 (toDb (shi / kWindow)));
    }
    return f;
}

bool renderPreset (const std::string& name, Fingerprint& out)
{
    Preset p;
    std::string err;
    if (! PresetManager::importFrom (std::string (FMLUME_SOURCE_DIR) + "/Resources/Presets/" + name + ".fmlume", p, &err))
    {
        std::printf ("    %s: %s\n", name.c_str(), err.c_str());
        return false;
    }
    SynthEngine e;
    e.prepare (kRate, kBlock);
    if (! e.setPatch (p.data)) return false;
    // a held C major triad, then a single high note over the release
    const auto r = renderSeq (e, kRate, kBlock, kSeconds,
                              { { 0.0, noteOn (48, 100) }, { 0.0, noteOn (52, 90) }, { 0.0, noteOn (55, 80) },
                                { 1.0, noteOff (48) }, { 1.0, noteOff (52) }, { 1.0, noteOff (55) },
                                { 1.2, noteOn (72, 110) }, { 1.6, noteOff (72) } },
                              p.data.host);
    out = fingerprint (r);
    return true;
}

Json toJson (const std::vector<float>& v)
{
    Json a = Json::array();
    for (float x : v) a.push (x);
    return a;
}

std::vector<float> fromJson (const Json* a)
{
    std::vector<float> v;
    if (a != nullptr && a->isArray()) for (auto& x : a->items) v.push_back ((float) x.number);
    return v;
}

const std::string kFile = std::string (FMLUME_SOURCE_DIR) + "/tests/Golden/sound_fingerprints.json";
} // namespace

FM_TEST ("integration", "Compatibility: representative presets still sound as recorded (sound fingerprints)")
{
    const bool update = std::getenv ("FMLUME_UPDATE_SOUND_FINGERPRINTS") != nullptr;
    Json stored;
    if (! update)
    {
        std::ifstream in (kFile);
        std::stringstream ss;
        ss << in.rdbuf();
        std::string err;
        CHECK (Json::parse (ss.str(), stored, &err));
        if (! stored.isObject()) return;
    }

    Json recorded = Json::object();
    for (const char* name : kPresets)
    {
        Fingerprint now;
        CHECK (renderPreset (name, now));
        if (update)
        {
            Json o = Json::object();
            o.set ("l", toJson (now.l));
            o.set ("r", toJson (now.r));
            o.set ("low", toJson (now.low));
            o.set ("high", toJson (now.high));
            recorded.set (name, std::move (o));
            continue;
        }
        const Json* ref = stored.find (name);
        CHECK (ref != nullptr);
        if (ref == nullptr) continue;
        const std::vector<float>* nowSeries[] = { &now.l, &now.r, &now.low, &now.high };
        const char* const keys[] = { "l", "r", "low", "high" };
        int bad = 0;
        for (int s = 0; s < 4; ++s)
        {
            const auto want = fromJson (ref->find (keys[s]));
            const auto& got = *nowSeries[s];
            CHECK (want.size() == got.size());
            for (size_t i = 0; i < std::min (want.size(), got.size()); ++i)
            {
                const bool ok = want[i] < kAudibleDb ? got[i] < kAudibleDb + kToleranceDb
                                                     : std::fabs (got[i] - want[i]) <= kToleranceDb;
                if (! ok && bad++ < 3)
                    std::printf ("    %s %s at %.2f s: %.1f dB, recorded %.1f dB\n", name, keys[s], (double) i * kWindow / kRate, (double) got[i], (double) want[i]);
            }
        }
        CHECK (bad == 0);
    }
    if (update)
    {
        std::ofstream out (kFile);
        out << recorded.dump (0) << "\n";
        std::printf ("    wrote %s\n", kFile.c_str());
    }
}
