// Factory trance-gate patterns. The table is the same as POLYBRISK's (names, groups, step
// strings, stereo patterns and the rate each pattern is written for) so that both plug-ins of
// the series offer the same pattern vocabulary (docs/DECISIONS.md D-027, D-030).
#pragma once
#include "Core/Model.h"
#include <array>

namespace fmlume
{
struct FactoryGatePattern
{
    const char* group;
    const char* name;
    const char* steps16; // '1' / 'x' = open, 'h' = 75 %, 'm' = 50 %, 'l' = 25 %, anything else = closed
    uint8_t rate = 12;   // sync division the pattern is written for (12 = 1/16, 15 = 1/32, 18 = 1/64)
    const char* right16 = nullptr; // right channel (stereo patterns only); nullptr = same as left
};

constexpr float gateStepLevel (char c) noexcept
{
    return (c == '1' || c == 'x') ? 1.0f : c == 'h' ? 0.75f : c == 'm' ? 0.5f : c == 'l' ? 0.25f : 0.0f;
}

inline constexpr auto kFactoryGatePatterns = std::to_array<FactoryGatePattern> ({
    { "Basic", "Straight 8ths",  "1010101010101010" },
    { "Basic", "Pulse 16",       "1110111011101110" },
    { "Basic", "Offbeat",        "0101010101010101" },
    { "Basic", "Syncopated",     "1011010010110100" },
    { "Basic", "Build Up",       "1000100010101111" },
    { "Basic", "Full",           "1111111111111111" },

    { "Straight", "Quarter Stabs",   "1000100010001000" },
    { "Straight", "Half Notes",      "1111111100000000" },
    { "Straight", "Half Chop",       "1100110011001100" },
    { "Straight", "Long Short",      "1110101011101010" },
    { "Straight", "Quarter Chop",    "1111000011110000" },

    { "Trance", "Gallop",            "1011101110111011" },
    { "Trance", "Reverse Gallop",    "1101110111011101" },
    { "Trance", "Offbeat Long",      "0011001100110011" },
    { "Trance", "Pump",              "0111011101110111" },
    { "Trance", "Late 16ths",        "0001000100010001" },
    { "Trance", "Trance Drive",      "1011011010110110" },
    { "Trance", "Rolling 16",        "1011101010111010" },
    { "Trance", "Uplift",            "1010101110101011" },

    { "Fast 1/32", "Chop Tail",      "1100110011001110", 18 },
    { "Fast 1/32", "Long Start",     "1111101010101110", 15 },
    { "Fast 1/32", "Double Long",    "1010111011101011", 15 },
    { "Fast 1/32", "Front Heavy",    "1110110010101010", 15 },
    { "Fast 1/32", "Shifted Long",   "1010111010111010", 15 },
    { "Fast 1/32", "Short Long",     "1010111010101110", 15 },

    { "Syncopated", "Dotted 8ths",   "1001001001001001" },
    { "Syncopated", "Tresillo",      "1001001010010010" },
    { "Syncopated", "Tresillo Long", "1101101011011010" },
    { "Syncopated", "Clave 3-2",     "1001001000101000" },
    { "Syncopated", "Clave 2-3",     "0010100010010010" },
    { "Syncopated", "Push",          "1001001100100110" },
    { "Syncopated", "Two Step",      "1010011010100110" },
    { "Syncopated", "Skip",          "1101011011010110" },

    { "Accent", "Accent 4",          "xlllxlllxlllxlll" },
    { "Accent", "Accent Offbeat",    "llxlllxlllxlllxl" },
    { "Accent", "Ghost Gallop",      "xlxxxlxxxlxxxlxx" },
    { "Accent", "Decay",             "xhmlxhmlxhmlxhml" },
    { "Accent", "Swell",             "lmhxlmhxlmhxlmhx" },
    { "Accent", "Half Duck",         "mxxxmxxxmxxxmxxx" },
    { "Accent", "Ramp Up",           "llllmmmmhhhhxxxx" },
    { "Accent", "Ramp Down",         "xxxxhhhhmmmmllll" },

    { "Fill", "Stutter End",         "1000100010001111" },
    { "Fill", "Double Time End",     "1010101011111111" },
    { "Fill", "Break",               "1110111011100000" },
    { "Fill", "Pickup",              "0000000000001011" },

    // Stereo (left / right). Choosing one switches the gate to stereo.
    { "Stereo", "Ping Pong 16",      "1010101010101010", 12, "0101010101010101" },
    { "Stereo", "Ping Pong 8",       "1100110011001100", 12, "0011001100110011" },
    { "Stereo", "Ping Pong Beat",    "1111000011110000", 12, "0000111100001111" },
    { "Stereo", "Side Echo",         "1000100010001000", 12, "0010001000100010" },
    { "Stereo", "Split Gallop",      "1000100010001000", 12, "0011001100110011" },
    { "Stereo", "Cross Pulse",       "1110111011101110", 12, "1011101110111011" },
    { "Stereo", "Call Response",     "1110000011100000", 12, "0000111000001110" },
});

inline constexpr int kNumFactoryGatePatterns = (int) kFactoryGatePatterns.size();

inline void applyGatePattern (TranceGateState& gate, int index) noexcept
{
    const auto& f = kFactoryGatePatterns[(size_t) index];
    const char* right = f.right16 != nullptr ? f.right16 : f.steps16;
    for (int i = 0; i < kGateSteps; ++i)
    {
        gate.steps[(size_t) i] = gateStepLevel (f.steps16[i]);
        gate.stepsRight[(size_t) i] = gateStepLevel (right[i]);
    }
    gate.stereo = f.right16 != nullptr;
    gate.rate = f.rate;
}

// Index of the factory pattern whose steps equal the gate's current steps, or -1 ("Custom").
inline int matchGatePattern (const TranceGateState& gate) noexcept
{
    int fallback = -1;
    for (int p = 0; p < kNumFactoryGatePatterns; ++p)
    {
        const auto& f = kFactoryGatePatterns[(size_t) p];
        if ((f.right16 != nullptr) != gate.stereo) continue;
        const char* right = f.right16 != nullptr ? f.right16 : f.steps16;
        bool same = true;
        for (int i = 0; i < kGateSteps && same; ++i)
        {
            const float d = gate.steps[(size_t) i] - gateStepLevel (f.steps16[i]);
            const float dr = gate.stereo ? gate.stepsRight[(size_t) i] - gateStepLevel (right[i]) : 0.0f;
            same = d > -0.01f && d < 0.01f && dr > -0.01f && dr < 0.01f;
        }
        if (! same) continue;
        if (f.rate == gate.rate) return p;
        if (fallback < 0) fallback = p;
    }
    return fallback;
}
} // namespace fmlume
