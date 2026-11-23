// Control-rate LFO and exponential ADSR used by the global modulation system (SPEC §7).
#pragma once
#include "Core/Model.h"
#include "Core/Pcg32.h"
#include "DSP/Util/DspUtil.h"

namespace fmlume
{
struct Lfo
{
    float phase = 0.0f; // 0..1
    float randPrev = 0.0f, randNext = 0.0f;
    Pcg32 rng;

    void reset (uint64_t seed, float startPhase)
    {
        rng.reseed (seed, 7u);
        phase = startPhase - std::floor (startPhase);
        randPrev = rng.nextBipolar();
        randNext = rng.nextBipolar();
    }
    void retrigger (float startPhase) { phase = startPhase - std::floor (startPhase); }

    // Advances by dt seconds at rateHz and returns the bipolar value in [-1, 1].
    float tick (LfoShape shape, float rateHz, float dt)
    {
        const float p = phase;
        float v = 0.0f;
        switch (shape)
        {
            case LfoShape::Sine: v = std::sin ((float) kTwoPi * p); break;
            case LfoShape::Triangle: v = p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f); break;
            case LfoShape::Saw: v = 2.0f * p - 1.0f; break;
            case LfoShape::Square: v = p < 0.5f ? 1.0f : -1.0f; break;
            case LfoShape::RandomSmooth:
            {
                const float t = p * p * (3.0f - 2.0f * p);
                v = randPrev + t * (randNext - randPrev);
                break;
            }
            default: break;
        }
        phase += rateHz * dt;
        while (phase >= 1.0f)
        {
            phase -= 1.0f;
            randPrev = randNext;
            randNext = rng.nextBipolar();
        }
        return v;
    }
};

// Exponential-segment ADSR. Times: attack = time to reach 1.0, decay / release = time to fall 60 dB.
struct EnvCoefs
{
    float attack = 1.0f, decayMul = 0.0f, sustain = 0.6f, releaseMul = 0.0f;
    void set (const ADSR& e, double sampleRate) { set (e.a, e.d, e.s, e.r, sampleRate); }
    void set (float a, float d, float s, float r, double sampleRate)
    {
        attack = (float) (1.0 - std::exp (-1.6094379 / (std::max (a, 0.0005f) * sampleRate)));
        decayMul = (float) std::exp (-6.9077553 / (std::max (d, 0.0005f) * sampleRate));
        sustain = s;
        releaseMul = (float) std::exp (-6.9077553 / (std::max (r, 0.0005f) * sampleRate));
    }
};

struct EnvState
{
    enum Stage : uint8_t { Idle, Attack, Decay, Release };
    float value = 0.0f;
    Stage stage = Idle;

    void gateOn() { stage = Attack; }
    void gateOff() { if (stage != Idle) stage = Release; }
    void kill() { stage = Idle; value = 0.0f; }
    inline float tick (const EnvCoefs& c)
    {
        switch (stage)
        {
            case Attack:
                value += c.attack * (1.25f - value);
                if (value >= 1.0f) { value = 1.0f; stage = Decay; }
                break;
            case Decay: value = c.sustain + (value - c.sustain) * c.decayMul; break;
            case Release:
                value *= c.releaseMul;
                if (value < 1.0e-5f) { value = 0.0f; stage = Idle; }
                break;
            default: break;
        }
        return value;
    }
};
} // namespace fmlume
