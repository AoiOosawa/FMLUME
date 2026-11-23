// 16-step tempo-synced trance gate (SPEC §11.3). Applied after the filter, before the FX chain.
#pragma once
#include "Core/Model.h"
#include "DSP/Util/DspUtil.h"

namespace fmlume
{
class TranceGate
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { gain = gainR = 1.0f; }

    // Step levels of both channels. Lowering the width moves both towards "open if either is
    // open" (the larger of the two): the rhythm stays, only the panning collapses (as in POLYBRISK).
    static void stepLevels (const TranceGateState& st, int step, float& left, float& right)
    {
        left = right = st.steps[(size_t) step];
        if (! st.stereo) return;
        right = st.stepsRight[(size_t) step];
        const float both = std::max (left, right);
        const float width = std::min (1.0f, std::max (0.0f, st.width));
        left = both + (left - both) * width;
        right = both + (right - both) * width;
    }

    static int stepAt (double ppq, double stepBeats, float swing)
    {
        const double pair = 2.0 * stepBeats;
        double pos = std::fmod (ppq, pair * (kGateSteps / 2));
        if (pos < 0.0) pos += pair * (kGateSteps / 2);
        const int p = (int) (pos / pair);
        const double inPair = pos - p * pair;
        return (p * 2 + (inPair >= stepBeats * (1.0 + 0.5 * swing) ? 1 : 0)) % kGateSteps;
    }

    void process (float* l, float* r, int n, const TranceGateState& st, bool enabled, double ppq, double beatsPerSample)
    {
        if (! enabled && gain > 0.9999f && gainR > 0.9999f) { gain = gainR = 1.0f; return; }
        const double stepBeats = syncDivBeats (st.rate);
        const float coef = onePoleCoef (std::max (0.0005, (double) st.smoothMs * 0.001), sr);
        for (int i = 0; i < n; ++i)
        {
            float target = 1.0f, targetR = 1.0f;
            if (enabled)
            {
                const int step = stepAt (ppq + i * beatsPerSample, stepBeats, st.swing);
                float sl, sr2;
                stepLevels (st, step, sl, sr2);
                target = 1.0f - st.mix * (1.0f - sl);
                targetR = 1.0f - st.mix * (1.0f - sr2);
            }
            gain += coef * (target - gain);
            gainR += coef * (targetR - gainR);
            l[i] *= gain;
            r[i] *= gainR;
        }
    }

private:
    double sr = 48000.0;
    float gain = 1.0f, gainR = 1.0f;
};
} // namespace fmlume
