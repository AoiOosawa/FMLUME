// Tiny WAV writer/reader used by tools and tests (non-realtime).
#pragma once
#include <string>
#include <vector>

namespace fmlume
{
bool writeWavFloat32 (const std::string& path, const std::vector<float>& left, const std::vector<float>& right, int sampleRate);
} // namespace fmlume
