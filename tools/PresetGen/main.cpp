// fmlume-presetgen: builds the factory bank, verifies every preset and writes Resources/Presets/*.fmlume.
#include "Builder.h"
#include "DSP/FM/SynthEngine.h"
#include "Presets/PresetManager.h"
#include "Presets/PresetSerializer.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>

namespace gen { std::vector<fmlume::Preset> buildFactory(); }
using namespace fmlume;
namespace fs = std::filesystem;

namespace
{
struct Render { std::vector<float> l, r; float peak = 0.0f; double rms = 0.0; bool finite = true; };

Render render (const PatchData& d, HostParams h, Category cat, float gainDb)
{
    const double sr = 48000.0;
    SynthEngine eng;
    eng.prepare (sr, 256);
    eng.setPatch (d);
    h.gainDb = gainDb;
    h.quality = 1;
    TransportInfo tr;
    tr.valid = tr.playing = true;
    tr.bpm = 120.0;
    const int base = cat == Category::Bass ? 36 : (cat == Category::Lead ? 67 : 55);
    const int notes[3] = { base, base + (cat == Category::Bass || cat == Category::Lead ? 7 : 4), base + (cat == Category::Bass ? 12 : 9) };
    const int total = (int) (2.4 * sr);
    Render out;
    out.l.assign ((size_t) total, 0.0f);
    out.r.assign ((size_t) total, 0.0f);
    const bool mono = d.perf.mono;
    for (int pos = 0; pos < total; pos += 256)
    {
        MidiEvent ev[8];
        int n = 0;
        auto at = [&] (double t, MidiEvent::Type type, int note) {
            const int s = (int) (t * sr);
            if (s >= pos && s < pos + 256) { ev[n].offset = s - pos; ev[n].type = type; ev[n].data1 = (uint8_t) note; ev[n].data2 = 100; ++n; }
        };
        if (mono)
        {
            // phrase with a legato step so glide / legato behaviour is exercised
            at (0.0, MidiEvent::NoteOn, notes[0]); at (0.45, MidiEvent::NoteOn, notes[1]); at (0.5, MidiEvent::NoteOff, notes[0]);
            at (0.9, MidiEvent::NoteOff, notes[1]); at (1.1, MidiEvent::NoteOn, notes[2]); at (1.6, MidiEvent::NoteOff, notes[2]);
        }
        else
        {
            for (int k = 0; k < 3; ++k) { at (0.0, MidiEvent::NoteOn, notes[k]); at (0.9, MidiEvent::NoteOff, notes[k]); }
            at (1.1, MidiEvent::NoteOn, notes[1] + 12); at (1.6, MidiEvent::NoteOff, notes[1] + 12);
        }
        eng.process (out.l.data() + pos, out.r.data() + pos, std::min (256, total - pos), ev, n, h, tr);
        tr.ppq += 256.0 / sr * 2.0;
    }
    double acc = 0.0;
    for (int i = 0; i < total; ++i)
    {
        const float a = out.l[(size_t) i], b = out.r[(size_t) i];
        if (! std::isfinite (a) || ! std::isfinite (b)) out.finite = false;
        out.peak = std::max (out.peak, std::max (std::fabs (a), std::fabs (b)));
        acc += 0.5 * ((double) a * a + (double) b * b);
    }
    out.rms = std::sqrt (acc / total);
    return out;
}

double diff (const Render& a, const Render& b)
{
    double m = 0.0;
    for (size_t i = 0; i < a.l.size(); ++i) m = std::max (m, (double) std::max (std::fabs (a.l[i] - b.l[i]), std::fabs (a.r[i] - b.r[i])));
    return m;
}

std::string slug (const std::string& name)
{
    std::string s;
    for (const char c : name)
    {
        if (std::isalnum ((unsigned char) c)) s.push_back (c);
        else if (! s.empty() && s.back() != '-') s.push_back ('-');
    }
    while (! s.empty() && s.back() == '-') s.pop_back();
    return s;
}
} // namespace

int main (int argc, char** argv)
{
    // "--template-trims": prints the level each category starting point ("Init" in the browser)
    // needs to sit at the same loudness as the factory presets. The values are copied into
    // makeCategoryPreset() (Source/Core/Templates.cpp).
    if (argc > 1 && std::string (argv[1]) == "--template-trims")
    {
        for (int c = 0; c < (int) Category::Count; ++c)
        {
            Preset p = makeCategoryPreset ((Category) c);
            p.data.out.levelDb = 0.0f;
            const Render ref = render (p.data, p.data.host, (Category) c, -12.0f);
            const float peakDb = 20.0f * std::log10 (std::max (ref.peak, 1.0e-6f)) + 12.0f;
            const float rmsDb = 20.0f * (float) std::log10 (std::max (ref.rms, 1.0e-9)) + 12.0f;
            float trim = 0.5f * (-6.0f - peakDb) + 0.5f * (-20.0f - rmsDb);
            trim = std::min (trim, -3.0f - peakDb);
            std::printf ("%-8s peak %6.1f dB  rms %6.1f dB  trim %+5.1f dB\n", categoryName ((Category) c), peakDb, rmsDb, std::round (clampf (trim, -18.0f, 12.0f) * 10.0f) / 10.0f);
        }
        return 0;
    }
    const std::string outDir = argc > 1 ? argv[1] : "Resources/Presets";
    const bool check = argc > 2 && std::string (argv[2]) == "--check";
    auto bank = gen::buildFactory();
    std::map<int, int> perCategory;
    int problems = 0;
    std::error_code ec;
    fs::create_directories (outDir, ec);
    if (! check)
        for (auto& it : fs::directory_iterator (outDir, ec))
            if (it.path().extension() == ".fmlume") fs::remove (it.path(), ec);

    for (auto& p : bank)
    {
        PatchData& d = p.data;
        const Category cat = p.meta.category;
        const int number = ++perCategory[(int) cat];
        std::string catSlug = slug (categoryName (cat));
        char num[8];
        std::snprintf (num, sizeof (num), "%02d", number);
        p.meta.patchId = "factory." + catSlug + "." + num + "." + slug (p.meta.displayName);
        p.meta.author = "FMLUME Factory";
        p.meta.createdAt = p.meta.modifiedAt = "2026-11-23T00:00:00Z";
        p.meta.pluginVersion = FMLUME_VERSION_STRING;
        if (p.meta.tags.empty() || p.meta.description.empty()) { std::printf ("PROBLEM %s: missing tags / description\n", p.meta.displayName.c_str()); ++problems; }

        // Time-based FX and Drive/EQ stay available (wet / gain 0 when unused) so the FX quick controls always respond.
        auto ensure = [&d] (FxType t, int wetParam) {
            auto& pr = d.fx.proc[(size_t) t];
            if (! pr.enabled) { pr.enabled = true; pr.p[(size_t) wetParam] = 0.0f; }
        };
        ensure (FxType::Chorus, 3);
        ensure (FxType::Delay, 5);
        ensure (FxType::Reverb, 4);
        ensure (FxType::Drive, 0);
        d.fx.proc[(size_t) FxType::EQ].enabled = true;

        sanitizeTargets (d);
        assignCommonMacros (d, false);
        // Character (macro 4) is feedback on the first modulator plus carrier detune. On these sounds the
        // screening (scripts/audition.py) measured no audible change, so the first carrier's own feedback
        // is added: it always turns the tone edgier (docs/DECISIONS.md D-054).
        static const char* const kCharacterNeedsEdge[] = { "Chip Hero Arp", "Wood Bell", "Thumb Piano", "Analog Horns", "Toy Piano",
                                                           "Nylon Fingers", "Tine Pluck", "Sub Drop", "Pendulum Arp" };
        for (const char* name : kCharacterNeedsEdge)
            if (p.meta.displayName == name)
                for (int k = 0; k < kNumOps; ++k)
                    if (d.a.engine.operators[(size_t) k].enabled && d.a.engine.operators[(size_t) k].carrierGain > 0.0f)
                    {
                        addMapping (d.a.macros[3], pc::op (k, kOpFeedback), 0.45f);
                        break;
                    }
        for (int i = 0; i < kNumMacros; ++i)
        {
            if (d.a.macros[(size_t) i].numMappings == 0)
            {
                std::printf ("PROBLEM %s: macro %d (%s) has no mapping\n", p.meta.displayName.c_str(), i + 1, p.meta.macroNames[(size_t) i].c_str());
                ++problems;
            }
            d.a.macros[(size_t) i].defaultValue = 0.5f;
        }
        d.host.macro.fill (0.5f);
        d.host.quick.fill (0.5f);
        d.hasB = false;
        d.b = d.a;
        p.meta.macroNamesB = p.meta.macroNames;

        const auto v = validatePatch (d);
        if (! v.ok) { std::printf ("PROBLEM %s: invalid: %s\n", p.meta.displayName.c_str(), v.error.c_str()); ++problems; continue; }
        for (auto& w : v.warnings) { std::printf ("PROBLEM %s: %s\n", p.meta.displayName.c_str(), w.c_str()); ++problems; }

        // Loudness: measured 12 dB down (clear of the limiter), then trimmed so that the test phrase
        // peaks around -6 dBFS at master gain 0 dB.
        d.out.levelDb = 0.0f;
        Render ref = render (d, d.host, cat, -12.0f);
        if (! ref.finite || ref.peak < 1.0e-4f) { std::printf ("PROBLEM %s: silent or non-finite\n", p.meta.displayName.c_str()); ++problems; continue; }
        const float peakDb = 20.0f * std::log10 (ref.peak) + 12.0f;
        const float rmsDb = 20.0f * (float) std::log10 (ref.rms) + 12.0f;
        // blend of peak and RMS targets keeps percussive and sustained sounds comparable
        float trim = 0.5f * (-6.0f - peakDb) + 0.5f * (-20.0f - rmsDb);
        trim = std::min (trim, -3.0f - peakDb);
        // Corrections by ear on top of the measured trim (listening pass, docs/DECISIONS.md D-053):
        // pure, dull tones measure as loud as bright ones but sound quieter.
        static const std::pair<const char*, float> kLevelByEar[] = { { "Soft Stairs Arp", 4.0f } };
        for (auto& kv : kLevelByEar) if (p.meta.displayName == kv.first) trim += kv.second;
        d.out.levelDb = std::round (clampf (trim, -18.0f, 12.0f) * 10.0f) / 10.0f;

        // Macro loudness guard: no macro may push the phrase into the limiter (peak above -1.5 dBFS) or
        // make it more than 9 dB louder (RMS; longer tails alone stay well below that) at either end of its throw. Where one does, a level mapping
        // that only acts on that half of the throw takes the excess back out (D-054).
        {
            const auto measure = [&] (const HostParams& h) {
                const Render r = render (d, h, cat, -12.0f);
                return std::pair<float, float> (20.0f * std::log10 (std::max (r.peak, 1.0e-6f)) + 12.0f, 20.0f * (float) std::log10 (std::max (r.rms, 1.0e-9)) + 12.0f);
            };
            const auto base = measure (d.host);
            for (int i = 0; i < kNumMacros; ++i)
            {
                Macro& mc = d.a.macros[(size_t) i];
                int guardedHalf = -1; // 0 = lower, 1 = upper
                for (int half = 0; half < 2; ++half)
                    for (int pass = 0; pass < 5; ++pass)
                    {
                        HostParams h = d.host;
                        h.macro[(size_t) i] = (float) half;
                        const auto m = measure (h);
                        const float over = std::max (m.first + 1.5f, m.second - base.second - 9.0f);
                        if (over < 0.2f) break;
                        MacroMapping* mp = nullptr;
                        for (int k = 0; k < mc.numMappings; ++k) if (mc.mappings[(size_t) k].target == pc::level) mp = &mc.mappings[(size_t) k];
                        if (guardedHalf >= 0 && guardedHalf != half) { std::printf ("NOTE %s: macro %d is too loud at both ends; only one end is guarded\n", p.meta.displayName.c_str(), i + 1); break; }
                        if (mp == nullptr)
                        {
                            if (! addMapping (mc, pc::level, 0.0f)) { std::printf ("NOTE %s: macro %d has no free mapping for the loudness guard\n", p.meta.displayName.c_str(), i + 1); break; }
                            mp = &mc.mappings[(size_t) (mc.numMappings - 1)];
                        }
                        if (guardedHalf < 0) { mp->amount = 0.0f; mp->curve = Curve::Linear; mp->polarity = Polarity::Bipolar; mp->min = 0.0f; mp->max = 0.5f; }
                        guardedHalf = half;
                        // preset level is 0..2 (normalised 0.5 = unity): |amount| a gives a gain of 1 - 2a at the end of the throw
                        const float gain = (1.0f - 2.0f * std::fabs (mp->amount)) * std::pow (10.0f, -over / 20.0f);
                        const float a = clampf ((1.0f - gain) * 0.5f, 0.0f, 0.45f);
                        mp->amount = half == 1 ? -a : a;
                        if (pass == 0) std::printf ("guard %-22s macro %d (%s) at %d: %+.1f dB over\n", p.meta.displayName.c_str(), i + 1, p.meta.macroNames[(size_t) i].c_str(), half, over);
                    }
            }
            d.b = d.a;
        }

        // Every macro must be audible in at least one direction.
        const Render neutral = render (d, d.host, cat, -12.0f);
        for (int i = 0; i < kNumMacros; ++i)
        {
            double best = 0.0;
            for (float value : { 0.0f, 1.0f })
            {
                HostParams h = d.host;
                h.macro[(size_t) i] = value;
                const Render r = render (d, h, cat, -12.0f);
                if (! r.finite) { std::printf ("PROBLEM %s: macro %d non-finite\n", p.meta.displayName.c_str(), i + 1); ++problems; }
                best = std::max (best, diff (neutral, r));
            }
            if (best < 2.0e-3)
            {
                std::printf ("PROBLEM %s: macro %d (%s) is inaudible (%.5f)\n", p.meta.displayName.c_str(), i + 1, p.meta.macroNames[(size_t) i].c_str(), best);
                ++problems;
            }
        }
        for (int q = 0; q < kNumQuick; ++q)
        {
            HostParams h = d.host;
            h.quick[(size_t) q] = 1.0f;
            if (diff (neutral, render (d, h, cat, -12.0f)) < 1.0e-3) { std::printf ("PROBLEM %s: quick control %d is inaudible\n", p.meta.displayName.c_str(), q + 1); ++problems; }
        }

        std::string err;
        const std::string path = outDir + "/" + catSlug + "-" + num + "-" + slug (p.meta.displayName) + ".fmlume";
        if (! check && ! PresetManager::exportTo (path, p, &err)) { std::printf ("PROBLEM %s: %s\n", p.meta.displayName.c_str(), err.c_str()); ++problems; }
        std::printf ("%-9s %-22s peak %6.1f dB  rms %6.1f dB  trim %+5.1f dB\n", categoryName (cat), p.meta.displayName.c_str(), peakDb, rmsDb, d.out.levelDb);
    }
    std::printf ("\n%d presets", (int) bank.size());
    for (auto& kv : perCategory) std::printf ("  %s:%d", categoryName ((Category) kv.first), kv.second);
    std::printf ("\n%d problem(s)\n", problems);
    return problems == 0 ? 0 : 1;
}
