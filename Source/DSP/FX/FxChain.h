// Reorderable FX chain (SPEC §10.1). All nine processors always exist; only the processing order
// array and the enable flags change, so nothing is allocated on the audio thread.
#pragma once
#include "DSP/FX/FxProcessors.h"

namespace fmlume
{
class FxChain
{
public:
    // Fixed latency of the chain (Drive's oversampler). Reported whether or not Drive is enabled.
    static constexpr int kLatencySamples = fx::Drive::kLatency;

    void prepare (double sampleRate);
    void reset();
    // Control-rate update. `eff` holds effective parameter values in physical units.
    void setParams (const FxState& state, const float eff[kNumFx][kFxParams], bool bypassAll, double bpm);
    void process (float* l, float* r, int n);

    // Number of process() calls each processor has run (diagnostics / FX-01 test).
    uint64_t getProcessCount (FxType t) const { return counts[(int) t]; }

private:
    void resetOne (int type);
    void runOne (int type, float* l, float* r, int n);

    double sr = 48000.0;
    fx::Eq eq;
    fx::Drive drive;
    fx::Chorus chorus;
    fx::Phaser phaser;
    fx::Flanger flanger;
    fx::Compressor comp;
    fx::Bitcrusher crusher;
    fx::Delay delay;
    fx::Reverb reverb;

    struct Config
    {
        uint8_t order[kNumFx] { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
        bool enabled[kNumFx] {};
        bool operator== (const Config& o) const
        {
            return std::memcmp (order, o.order, sizeof (order)) == 0 && std::memcmp (enabled, o.enabled, sizeof (enabled)) == 0;
        }
    };
    Config active, wanted;
    bool started = false;
    float fade = 1.0f;       // chain crossfade gain (dip while the configuration changes)
    int fadeDir = 0;         // -1 fading out, +1 fading in
    float fadeStep = 0.001f;
    uint64_t counts[kNumFx] {};
};
} // namespace fmlume
