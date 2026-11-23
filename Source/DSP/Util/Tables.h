// Shared lookup tables: sine and band-limited triangle / saw / pulse wavetables (SPEC §4.5).
#pragma once
#include "Core/Model.h"
#include <cmath>

namespace fmlume
{
struct Tables
{
    static constexpr int kSineSize = 4096;
    static constexpr int kWtSize = 2048;
    static constexpr int kWtLevels = 11; // level L holds harmonics 1 .. 2^L

    float sine[kSineSize + 1];
    float wt[3][kWtLevels][kWtSize + 1]; // triangle, saw, pulse

    // Built once on first use (call from a non-realtime thread, e.g. prepare()).
    static const Tables& get();

    // Table for a waveform at a given fundamental; `size` receives the table length.
    inline const float* select (Waveform w, double freqHz, double sampleRate, int& size) const
    {
        if (w == Waveform::Sine) { size = kSineSize; return sine; }
        size = kWtSize;
        const double maxHarm = sampleRate * 0.5 / (freqHz > 1.0 ? freqHz : 1.0);
        int level = 0;
        while (level + 1 < kWtLevels && (double) (2 << level) <= maxHarm) ++level;
        return wt[(int) w - 1][level];
    }

private:
    Tables();
};

// Linear-interpolated lookup. `t` is the phase in table samples (any real value).
inline float tableLookup (const float* table, int mask, float t)
{
    const float fl = std::floor (t);
    const int i = (int) fl & mask;
    const float fr = t - fl;
    return table[i] + fr * (table[i + 1] - table[i]);
}
} // namespace fmlume
