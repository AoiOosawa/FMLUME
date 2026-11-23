#include "DSP/Util/Tables.h"
#include "DSP/Util/DspUtil.h"
#include <vector>

namespace fmlume
{
Tables::Tables()
{
    for (int i = 0; i <= kSineSize; ++i) sine[i] = (float) std::sin (kTwoPi * (double) i / kSineSize);

    std::vector<double> acc ((size_t) kWtSize);
    for (int w = 0; w < 3; ++w)
    {
        std::fill (acc.begin(), acc.end(), 0.0);
        int done = 0;
        for (int level = 0; level < kWtLevels; ++level)
        {
            const int maxHarm = 1 << level;
            for (int h = done + 1; h <= maxHarm; ++h)
            {
                double amp = 0.0;
                if (w == 0) amp = (h % 2 == 1) ? ((h / 2) % 2 == 0 ? 1.0 : -1.0) / ((double) h * h) : 0.0; // triangle
                else if (w == 1) amp = 1.0 / (double) h;                                                      // saw (falling)
                else amp = (h % 2 == 1) ? 1.0 / (double) h : 0.0;                                             // 50% pulse
                if (amp == 0.0) continue;
                for (int i = 0; i < kWtSize; ++i)
                    acc[(size_t) i] += amp * std::sin (kTwoPi * (double) h * (double) i / kWtSize);
            }
            done = maxHarm;
            // Fixed scale per waveform so switching mip level never changes loudness.
            const double fundamentalScale = (w == 0) ? 0.81 : (w == 1 ? 0.6 : 1.0);
            for (int i = 0; i < kWtSize; ++i) wt[w][level][i] = (float) (acc[(size_t) i] * fundamentalScale);
            wt[w][level][kWtSize] = wt[w][level][0];
        }
    }
}

const Tables& Tables::get()
{
    static const Tables t;
    return t;
}
} // namespace fmlume
