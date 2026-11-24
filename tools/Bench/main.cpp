// fmlume-bench: measures engine CPU cost as a fraction of real time (SPEC §17.3).
#include "Core/Templates.h"
#include "DSP/FM/SynthEngine.h"
#include <chrono>
#include <cstdio>
#include <vector>

using namespace fmlume;

static double run (const char* label, PatchData d, HostParams h, int notes, double seconds = 20.0)
{
    const double sr = 48000.0;
    const int block = 256;
    SynthEngine eng;
    eng.prepare (sr, block);
    eng.setPatch (d);
    std::vector<float> l ((size_t) block), r ((size_t) block);
    const int blocks = (int) (seconds * sr / block);
    int maxVoices = 0;
    double worst = 0.0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
    {
        MidiEvent ev[64];
        int n = 0;
        // Re-strike the whole chord every second so attacks, releases and steals are part of the load.
        const int phase = b % (int) (sr / block);
        if (phase == 0)
            for (int k = 0; k < notes; ++k) { ev[n].type = MidiEvent::NoteOn; ev[n].data1 = (uint8_t) (36 + (k * 5) % 49); ev[n].data2 = 100; ++n; }
        if (phase == (int) (0.8 * sr / block))
            for (int k = 0; k < notes; ++k) { ev[n].type = MidiEvent::NoteOff; ev[n].data1 = (uint8_t) (36 + (k * 5) % 49); ++n; }
        const auto b0 = std::chrono::steady_clock::now();
        eng.process (l.data(), r.data(), block, ev, n, h, {});
        worst = std::max (worst, std::chrono::duration<double> (std::chrono::steady_clock::now() - b0).count());
        maxVoices = std::max (maxVoices, eng.getActiveSubVoiceCount());
    }
    const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double load = elapsed / seconds * 100.0;
    std::printf ("%-58s avg %5.1f %%   worst block %5.1f %%   max sub-voices %3d\n", label, load, worst / (block / sr) * 100.0, maxVoices);
    return load;
}

int main()
{
    std::printf ("FMLUME engine benchmark - 48 kHz, 256 frames, single thread, %% of one real-time core\n\n");
    // Reference case of SPEC §17.3: Standard 2x, 16 notes x 1 unison, EQ + Chorus + Delay + Reverb.
    PatchData ref = makeCategoryPreset (Category::EPiano).data;
    for (auto& p : ref.fx.proc) p.enabled = false;
    for (FxType t : { FxType::EQ, FxType::Chorus, FxType::Delay, FxType::Reverb }) ref.fx.proc[(size_t) t].enabled = true;
    ref.fx.proc[(size_t) FxType::Chorus].p[3] = 0.3f;
    ref.fx.proc[(size_t) FxType::Delay].p[5] = 0.25f;
    ref.fx.proc[(size_t) FxType::Reverb].p[4] = 0.3f;
    ref.fx.proc[(size_t) FxType::EQ].p[0] = 2.0f;
    HostParams h;
    h.polyphony = 16;
    const double target = run ("REFERENCE Standard 2x, 16 notes, 5-op E.Piano, 4 FX", ref, h, 16);
    h.quality = 0;
    run ("Eco 1x, same patch", ref, h, 16);
    h.quality = 2;
    run ("High 4x, same patch", ref, h, 16);
    h.quality = 1;

    PatchData six = ref;
    for (int k = 0; k < kNumOps; ++k) six.sound.engine.operators[(size_t) k] = makeOp (1.0f + k, 0.8f, 0.01f, 1.0f, 0.8f, 0.5f);
    applyAlgorithm (six.sound.engine, 15); // 1>2>3>4 + 5>6
    run ("Standard 2x, 16 notes, all 6 operators", six, h, 16);
    h.polyphony = 32;
    run ("Standard 2x, 32 notes, all 6 operators", six, h, 32);
    h.polyphony = 8;
    h.unison = 8;
    run ("Standard 2x, 8 notes x 8 unison (64 sub-voices), 6 ops", six, h, 8);

    PatchData worst = six;
    for (auto& p : worst.fx.proc) p.enabled = true;
    run ("WORST CASE 8 x 8 unison, 6 ops, all 9 FX, 2x", worst, h, 8);
    h.quality = 2;
    run ("WORST CASE 8 x 8 unison, 6 ops, all 9 FX, 4x", worst, h, 8);

    std::printf ("\nTarget (SPEC 17.3): reference case below 20 %% -> %s (%.1f %%)\n", target < 20.0 ? "MET" : "NOT MET", target);
    return 0;
}
