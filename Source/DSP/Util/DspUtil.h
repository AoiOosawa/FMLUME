// Small allocation-free DSP building blocks shared by the engine and effects.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace fmlume
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.283185307179586476925;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float sanitize (float v) { return std::isfinite (v) ? v : 0.0f; }
inline float onePoleCoef (double timeSeconds, double sampleRate)
{
    if (timeSeconds <= 0.0) return 1.0f;
    return (float) (1.0 - std::exp (-1.0 / (timeSeconds * sampleRate)));
}

// Linear-phase half-band FIR (Kaiser windowed sinc), 4K+3 taps. Stereo-agnostic: one instance per channel.
template <int K>
class Halfband
{
public:
    static constexpr int kTaps = 4 * K + 3;
    static constexpr int kCentre = 2 * K + 1;

    Halfband()
    {
        const double beta = 8.0;
        auto i0 = [] (double x) {
            double sum = 1.0, term = 1.0;
            for (int n = 1; n < 40; ++n) { term *= (x / (2.0 * n)) * (x / (2.0 * n)); sum += term; }
            return sum;
        };
        double h[K + 1];
        double dc = 0.5;
        for (int j = 0; j <= K; ++j)
        {
            const double n = 2.0 * j + 1.0;
            const double r = n / (double) (kCentre + 1);
            const double w = i0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0 (beta);
            h[j] = std::sin (kPi * n / 2.0) / (kPi * n) * w;
            dc += 2.0 * h[j];
        }
        for (int j = 0; j <= K; ++j) coef[j] = (float) (h[j] * 0.5 / (dc - 0.5)); // odd taps sum to 0.5 (unity DC gain)
        reset();
    }
    void reset() { std::memset (buf, 0, sizeof (buf)); pos = 0; }

    // --- decimation: feed two input samples, get one output.
    // Delay relative to the first sample of the pair: 2K samples at the input rate (= K output samples).
    inline float decimate (float x0, float x1)
    {
        push (x0);
        push (x1);
        return filterAtHead();
    }
    // Variant with delay 2K+1 input samples (used after an interpolator so the total is an integer).
    inline float decimateLate (float x0, float x1)
    {
        push (x0);
        const float y = filterAtHead();
        push (x1);
        return y;
    }
    // --- interpolation: feed one input sample, get two outputs. Delay 2K+1 output samples.
    inline void interpolate (float x, float& y0, float& y1)
    {
        push (x);
        float acc = 0.0f;
        for (int j = 0; j <= K; ++j)
            acc += coef[j] * (at (K - j) + at (K + 1 + j));
        y0 = 2.0f * acc;
        y1 = at (K);
    }

private:
    inline void push (float x) { pos = (pos + 1) & kMask; buf[pos] = x; }
    inline float at (int delay) const { return buf[(pos - delay) & kMask]; }
    inline float filterAtHead() const
    {
        float acc = 0.5f * at (kCentre);
        for (int j = 0; j <= K; ++j)
            acc += coef[j] * (at (kCentre - (2 * j + 1)) + at (kCentre + (2 * j + 1)));
        return acc;
    }
    static constexpr int kSize = 64, kMask = kSize - 1;
    static_assert (kTaps <= kSize, "increase buffer");
    float coef[K + 1] {};
    float buf[kSize] {};
    int pos = 0;
};

// RBJ biquad, transposed direct form II.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0.0f; }
    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
    void setLowShelf (double sr, double hz, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = kTwoPi * std::min (hz, sr * 0.45) / sr;
        const double c = std::cos (w), s = std::sin (w), al = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void setHighShelf (double sr, double hz, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = kTwoPi * std::min (hz, sr * 0.45) / sr;
        const double c = std::cos (w), s = std::sin (w), al = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
    void setPeak (double sr, double hz, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = kTwoPi * std::min (hz, sr * 0.45) / sr;
        const double c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void setLowpass (double sr, double hz, double q = 0.7071)
    {
        const double w = kTwoPi * std::min (hz, sr * 0.45) / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void setHighpass (double sr, double hz, double q = 0.7071)
    {
        const double w = kTwoPi * std::min (hz, sr * 0.45) / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
};

// Power-of-two delay line with linear-interpolated read. Storage is allocated in prepare().
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        int size = 16;
        while (size < maxSamples + 4) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        pos = 0;
        valid = 0;
    }
    // Constant time, safe on the audio thread: the buffer (up to several MB for the delay and
    // reverb) is not cleared. Instead `valid` counts the samples written since the reset, and
    // anything older reads as silence.
    void reset() { valid = 0; }
    inline void write (float x)
    {
        pos = (pos + 1) & mask;
        buf[(size_t) pos] = x;
        if (valid <= mask) ++valid;
    }
    inline float read (int delay) const { return delay < valid ? buf[(size_t) ((pos - delay) & mask)] : 0.0f; }
    inline float readFrac (float delay) const
    {
        const int d = (int) delay;
        const float f = delay - (float) d;
        const float a = read (d), b = read (d + 1);
        return a + f * (b - a);
    }
    int capacity() const { return mask - 3; }

private:
    std::vector<float> buf;
    int mask = 0, pos = 0;
    int valid = 0; // samples written since the last reset (saturates at the buffer size)
};
} // namespace fmlume
