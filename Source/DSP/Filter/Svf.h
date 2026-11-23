// Stereo TPT state-variable filter (Zavalishin) with drive. One instance per engine side (SPEC §5.3).
#pragma once
#include "Core/Model.h"
#include "DSP/Util/DspUtil.h"

namespace fmlume
{
class StereoFilter
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset()
    {
        std::memset (s, 0, sizeof (s));
        first = true;
    }
    // Called at control rate; coefficients glide per sample towards the target.
    void setTarget (FilterType t, float cutoffHz, float resonance, float drive)
    {
        if (t != type) { type = t; std::memset (s, 0, sizeof (s)); first = true; }
        const double fc = std::min ((double) std::max (cutoffHz, 20.0f), sr * 0.49);
        gT = (float) std::tan (kPi * fc / sr);
        kT = 2.0f - 2.0f * std::min (std::max (resonance, 0.0f), 0.95f);
        dT = std::min (std::max (drive, 0.0f), 1.0f);
        if (first) { g = gT; k = kT; d = dT; first = false; }
    }
    bool isIdentity() const { return type == FilterType::Off && dT <= 0.0f && d <= 1.0e-5f; }

    void process (float* l, float* r, int n)
    {
        if (isIdentity()) return;
        const float c = onePoleCoef (0.002, sr);
        for (int i = 0; i < n; ++i)
        {
            g += c * (gT - g);
            k += c * (kT - k);
            d += c * (dT - d);
            float x[2] = { l[i], r[i] };
            if (d > 1.0e-5f)
            {
                const float pre = 1.0f + d * 7.0f;
                const float post = 1.0f / (1.0f + d * 2.5f);
                x[0] = std::tanh (x[0] * pre) * post * (1.0f + d * 1.5f);
                x[1] = std::tanh (x[1] * pre) * post * (1.0f + d * 1.5f);
            }
            if (type != FilterType::Off)
            {
                const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
                for (int ch = 0; ch < 2; ++ch)
                {
                    float v = stage (x[ch], s[ch][0], s[ch][1], a1, a2, a3);
                    if (type == FilterType::LP24) v = stage (v, s[ch][2], s[ch][3], a1, a2, a3);
                    x[ch] = sanitize (v);
                }
            }
            l[i] = x[0];
            r[i] = x[1];
        }
    }

private:
    inline float stage (float in, float& ic1, float& ic2, float a1, float a2, float a3) const
    {
        const float v3 = in - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        switch (type)
        {
            case FilterType::HP12: return in - k * v1 - v2;
            case FilterType::BP12: return v1;
            default: return v2; // LP12 / LP24
        }
    }
    double sr = 48000.0;
    FilterType type = FilterType::Off;
    float g = 0.5f, k = 2.0f, d = 0.0f, gT = 0.5f, kT = 2.0f, dT = 0.0f;
    float s[2][4] {};
    bool first = true;
};
} // namespace fmlume
