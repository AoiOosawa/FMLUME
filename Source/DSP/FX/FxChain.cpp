#include "DSP/FX/FxChain.h"

namespace fmlume
{
void FxChain::prepare (double sampleRate)
{
    sr = sampleRate;
    eq.prepare (sr); drive.prepare (sr); chorus.prepare (sr); phaser.prepare (sr); flanger.prepare (sr);
    comp.prepare (sr); crusher.prepare (sr); delay.prepare (sr); reverb.prepare (sr);
    fadeStep = (float) (1.0 / (0.005 * sr));
    reset();
}

void FxChain::reset()
{
    for (int t = 0; t < kNumFx; ++t) resetOne (t);
    started = false;
    fade = 1.0f;
    fadeDir = 0;
    std::memset (counts, 0, sizeof (counts));
}

void FxChain::resetOne (int type)
{
    switch ((FxType) type)
    {
        case FxType::EQ: eq.reset(); break;
        case FxType::Drive: drive.reset(); break;
        case FxType::Chorus: chorus.reset(); break;
        case FxType::Phaser: phaser.reset(); break;
        case FxType::Flanger: flanger.reset(); break;
        case FxType::Compressor: comp.reset(); break;
        case FxType::Bitcrusher: crusher.reset(); break;
        case FxType::Delay: delay.reset(); break;
        case FxType::Reverb: reverb.reset(); break;
        default: break;
    }
}

void FxChain::setParams (const FxState& state, const float eff[kNumFx][kFxParams], bool bypassAll, double bpm)
{
    for (int i = 0; i < kNumFx; ++i)
    {
        wanted.order[i] = state.order[(size_t) i] < kNumFx ? state.order[(size_t) i] : (uint8_t) i;
        wanted.enabled[i] = ! bypassAll && state.proc[(size_t) i].enabled;
    }
    if (! started)
    {
        active = wanted;
        started = true;
    }
    // Only enabled processors receive parameters (their smoothers snap on first use after reset).
    if (active.enabled[(int) FxType::EQ]) eq.set (eff[(int) FxType::EQ], bpm);
    if (active.enabled[(int) FxType::Drive]) drive.set (eff[(int) FxType::Drive], bpm);
    if (active.enabled[(int) FxType::Chorus]) chorus.set (eff[(int) FxType::Chorus], bpm);
    if (active.enabled[(int) FxType::Phaser]) phaser.set (eff[(int) FxType::Phaser], bpm);
    if (active.enabled[(int) FxType::Flanger]) flanger.set (eff[(int) FxType::Flanger], bpm);
    if (active.enabled[(int) FxType::Compressor]) comp.set (eff[(int) FxType::Compressor], bpm);
    if (active.enabled[(int) FxType::Bitcrusher]) crusher.set (eff[(int) FxType::Bitcrusher], bpm);
    if (active.enabled[(int) FxType::Delay]) delay.set (eff[(int) FxType::Delay], bpm);
    if (active.enabled[(int) FxType::Reverb]) reverb.set (eff[(int) FxType::Reverb], bpm);
}

void FxChain::runOne (int type, float* l, float* r, int n)
{
    if (! active.enabled[type])
    {
        if (type == (int) FxType::Drive) drive.processDelayOnly (l, r, n); // keeps latency constant
        return;
    }
    ++counts[type];
    switch ((FxType) type)
    {
        case FxType::EQ: eq.process (l, r, n); break;
        case FxType::Drive: drive.process (l, r, n); break;
        case FxType::Chorus: chorus.process (l, r, n); break;
        case FxType::Phaser: phaser.process (l, r, n); break;
        case FxType::Flanger: flanger.process (l, r, n); break;
        case FxType::Compressor: comp.process (l, r, n); break;
        case FxType::Bitcrusher: crusher.process (l, r, n); break;
        case FxType::Delay: delay.process (l, r, n); break;
        case FxType::Reverb: reverb.process (l, r, n); break;
        default: break;
    }
}

void FxChain::process (float* l, float* r, int n)
{
    if (! (wanted == active) && fadeDir == 0) fadeDir = -1;

    for (int i = 0; i < kNumFx; ++i) runOne (active.order[i], l, r, n);

    if (fadeDir != 0)
    {
        // Click-free reconfiguration: dip the chain output, swap order / enables at zero, fade back in.
        for (int i = 0; i < n; ++i)
        {
            fade += fadeStep * (float) fadeDir;
            if (fade <= 0.0f)
            {
                fade = 0.0f;
                for (int t = 0; t < kNumFx; ++t)
                    if (wanted.enabled[t] && ! active.enabled[t]) resetOne (t); // fresh state for newly enabled FX
                active = wanted;
                fadeDir = 1;
            }
            else if (fade >= 1.0f) { fade = 1.0f; fadeDir = 0; }
            l[i] *= fade;
            r[i] *= fade;
        }
    }
}
} // namespace fmlume
