// PCG32 (O'Neill, pcg-random.org minimal C implementation, Apache-2.0 algorithm) — re-implemented here.
#pragma once
#include <cstdint>

namespace fmlume
{
struct Pcg32
{
    uint64_t state = 0x853c49e6748fea9bULL;
    uint64_t inc = 0xda3e39cb94b95bdbULL;

    Pcg32() = default;
    explicit Pcg32 (uint64_t seed, uint64_t stream = 54u) { reseed (seed, stream); }
    void reseed (uint64_t seed, uint64_t stream = 54u)
    {
        state = 0u;
        inc = (stream << 1u) | 1u;
        next();
        state += seed;
        next();
    }
    uint32_t next()
    {
        const uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
        const uint32_t xorshifted = (uint32_t) (((old >> 18u) ^ old) >> 27u);
        const uint32_t rot = (uint32_t) (old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
    }
    float nextFloat() { return (float) (next() >> 8) * (1.0f / 16777216.0f); } // [0,1)
    float nextBipolar() { return nextFloat() * 2.0f - 1.0f; }
    int nextInt (int n) { return n <= 0 ? 0 : (int) (next() % (uint32_t) n); }
    // Approximate standard normal (sum of 4 uniforms), deterministic and cheap.
    float nextGaussian() { return (nextFloat() + nextFloat() + nextFloat() + nextFloat() - 2.0f) * 1.7320508f; }
};
} // namespace fmlume
