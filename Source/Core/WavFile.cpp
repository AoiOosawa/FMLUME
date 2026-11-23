#include "Core/WavFile.h"
#include <cstdint>
#include <algorithm>
#include <cstdio>

namespace fmlume
{
bool writeWavFloat32 (const std::string& path, const std::vector<float>& left, const std::vector<float>& right, int sampleRate)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (f == nullptr) return false;
    const uint32_t frames = (uint32_t) std::min (left.size(), right.size());
    const uint32_t dataBytes = frames * 8u;
    auto w32 = [f] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [f] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36u + dataBytes); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); w32 (16); w16 (3); w16 (2); w32 ((uint32_t) sampleRate);
    w32 ((uint32_t) sampleRate * 8u); w16 (8); w16 (32);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < frames; ++i) { std::fwrite (&left[i], 4, 1, f); std::fwrite (&right[i], 4, 1, f); }
    std::fclose (f);
    return true;
}
} // namespace fmlume
