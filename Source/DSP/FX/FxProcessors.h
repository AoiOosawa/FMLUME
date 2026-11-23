// The nine FMLUME effects (SPEC §10). Each processor is stereo, allocation-free after prepare(),
// takes its parameters in physical units (see fxParamDesc) at control rate and smooths internally.
#pragma once
#include "Core/Model.h"
#include "Core/Pcg32.h"
#include "DSP/Util/DspUtil.h"

namespace fmlume::fx
{
struct Smooth
{
    float v = 0.0f, t = 0.0f;
    void snap (float x) { v = t = x; }
    inline float step (float c) { v += c * (t - v); return v; }
};

// ------------------------------------------------------------------------------------- EQ
class Eq
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); dirty = true; }
    void reset() { for (auto& ch : bq) for (auto& b : ch) b.reset(); }
    void set (const float* p, double)
    {
        for (int i = 0; i < 7; ++i) if (p[i] != last[i]) { last[i] = p[i]; dirty = true; }
        if (! dirty) return;
        dirty = false;
        for (auto& ch : bq)
        {
            ch[0].setLowShelf (sr, last[1], last[0]);
            ch[1].setPeak (sr, last[3], last[4], last[2]);
            ch[2].setHighShelf (sr, last[6], last[5]);
        }
        flat = std::fabs (last[0]) < 0.01f && std::fabs (last[2]) < 0.01f && std::fabs (last[5]) < 0.01f;
    }
    void process (float* l, float* r, int n)
    {
        if (flat) return;
        for (int i = 0; i < n; ++i)
        {
            l[i] = sanitize (bq[0][2].process (bq[0][1].process (bq[0][0].process (l[i]))));
            r[i] = sanitize (bq[1][2].process (bq[1][1].process (bq[1][0].process (r[i]))));
        }
    }

private:
    double sr = 48000.0;
    Biquad bq[2][3];
    float last[7] { 0, 120, 0, 1000, 0.9f, 0, 6000 };
    bool dirty = true, flat = true;
};

// ------------------------------------------------------------------------------------- Drive
// tanh waveshaper at 2x oversampling (linear-phase half-band FIR). Fixed latency kLatency samples;
// when disabled the processor still delays by kLatency so the plug-in latency never changes.
class Drive
{
public:
    static constexpr int kK = 3;
    static constexpr int kLatency = 2 * kK + 1; // 7 host samples

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        coef = onePoleCoef (0.01, sr);
        for (auto& d : dry) d.prepare (kLatency + 1);
        reset();
    }
    void reset()
    {
        for (int ch = 0; ch < 2; ++ch) { up[ch] = Halfband<kK>(); down[ch] = Halfband<kK>(); dry[ch].reset(); tone[ch] = 0.0f; }
        first = true;
    }
    void set (const float* p, double)
    {
        gain.t = dbToGain (p[0] * 36.0f);
        toneCoef.t = (float) (1.0 - std::exp (-kTwoPi * (800.0 * std::pow (25.0, (double) p[1])) / (sr * 2.0)));
        // The first 5 % of the gain range fades the shaper in, so gain = 0 is fully transparent.
        wet.t = p[2] * std::min (1.0f, p[0] * 20.0f);
        trim.t = dbToGain (p[3]);
        if (first) { gain.snap (gain.t); toneCoef.snap (toneCoef.t); wet.snap (wet.t); trim.snap (trim.t); first = false; }
    }
    void processDelayOnly (float* l, float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            dry[0].write (l[i]); dry[1].write (r[i]);
            l[i] = dry[0].read (kLatency); r[i] = dry[1].read (kLatency);
        }
    }
    void process (float* l, float* r, int n)
    {
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float g = gain.step (coef), tc = toneCoef.step (coef), w = wet.step (coef), tr = trim.step (coef);
            // unity small-signal gain at g = 1, gentle level hold above. The floor matters: g is 0
            // until the first parameter update, and 1/sqrt(0) * tanh(0) would be inf * 0 = NaN,
            // which then stays in the tone filter for good.
            const float makeup = 1.0f / std::sqrt (std::max (g, 1.0e-3f));
            for (int ch = 0; ch < 2; ++ch)
            {
                const float x = io[ch][i];
                dry[ch].write (x);
                float a, b;
                up[ch].interpolate (x, a, b);
                a = std::tanh (a * g) * makeup;
                b = std::tanh (b * g) * makeup;
                tone[ch] += tc * (a - tone[ch]); a = tone[ch];
                tone[ch] += tc * (b - tone[ch]); b = tone[ch];
                const float y = down[ch].decimateLate (a, b);
                const float d = dry[ch].read (kLatency);
                io[ch][i] = sanitize ((d + w * (y - d)) * tr);
            }
        }
    }

private:
    double sr = 48000.0;
    float coef = 0.01f;
    Halfband<kK> up[2], down[2];
    DelayLine dry[2];
    float tone[2] {};
    Smooth gain, toneCoef, wet, trim;
    bool first = true;
};

// ------------------------------------------------------------------------------------- Chorus
class Chorus
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        coef = onePoleCoef (0.02, sr);
        for (auto& d : line) d.prepare ((int) (0.05 * sr));
        reset();
    }
    void reset() { for (auto& d : line) d.reset(); phase = 0.0f; first = true; }
    void set (const float* p, double)
    {
        rate = p[0];
        depth.t = p[1];
        width.t = p[2];
        mix.t = p[3];
        voices = std::min (std::max ((int) std::lround (p[4]), 1), 4);
        if (first) { depth.snap (depth.t); width.snap (width.t); mix.snap (mix.t); first = false; }
    }
    void process (float* l, float* r, int n)
    {
        const float inc = rate / (float) sr;
        const float base = (float) (0.012 * sr), swing = (float) (0.006 * sr);
        const float norm = 1.0f / std::sqrt ((float) voices);
        for (int i = 0; i < n; ++i)
        {
            const float d = depth.step (coef), w = width.step (coef), m = mix.step (coef);
            line[0].write (l[i]);
            line[1].write (r[i]);
            float wl = 0.0f, wr = 0.0f;
            for (int v = 0; v < voices; ++v)
            {
                const float off = (float) v / (float) voices;
                const float pl = phase + off, pr = phase + off + 0.25f * w;
                const float ml = std::sin (kTwoPiF * pl), mr = std::sin (kTwoPiF * pr);
                const float stagger = 1.0f + 0.18f * (float) v;
                wl += line[0].readFrac (base * stagger + swing * d * ml);
                wr += line[1].readFrac (base * stagger + swing * d * mr);
            }
            wl *= norm; wr *= norm;
            l[i] = sanitize (l[i] + m * (wl - l[i] * 0.3f));
            r[i] = sanitize (r[i] + m * (wr - r[i] * 0.3f));
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
        }
    }

private:
    static constexpr float kTwoPiF = 6.2831853f;
    double sr = 48000.0;
    float coef = 0.01f, rate = 0.6f, phase = 0.0f;
    int voices = 2;
    DelayLine line[2];
    Smooth depth, width, mix;
    bool first = true;
};

// ------------------------------------------------------------------------------------- Phaser
class Phaser
{
public:
    void prepare (double sampleRate) { sr = sampleRate; coef = onePoleCoef (0.02, sr); reset(); }
    void reset() { std::memset (z, 0, sizeof (z)); fbState[0] = fbState[1] = 0.0f; phase = 0.0f; first = true; }
    void set (const float* p, double)
    {
        rate = p[0];
        depth.t = p[1]; feedback.t = p[2]; wet.t = p[3];
        if (first) { depth.snap (p[1]); feedback.snap (p[2]); wet.snap (p[3]); first = false; }
    }
    void process (float* l, float* r, int n)
    {
        float* io[2] = { l, r };
        const float inc = rate / (float) sr;
        for (int i = 0; i < n; ++i)
        {
            const float d = depth.step (coef), fb = feedback.step (coef), w = wet.step (coef);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float lfo = 0.5f + 0.5f * std::sin (6.2831853f * (phase + 0.25f * (float) ch));
                const float hz = 300.0f * std::pow (12.0f, 0.5f + (lfo - 0.5f) * d);
                const float t = std::tan (3.14159265f * std::min (hz, (float) sr * 0.45f) / (float) sr);
                const float a = (t - 1.0f) / (t + 1.0f);
                const float x = io[ch][i];
                float y = x + fb * fbState[ch];
                for (int s = 0; s < 6; ++s)
                {
                    const float out = a * y + z[ch][s];
                    z[ch][s] = y - a * out;
                    y = out;
                }
                fbState[ch] = sanitize (y);
                io[ch][i] = sanitize (x * (1.0f - 0.5f * w) + y * 0.5f * w);
            }
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
        }
    }

private:
    double sr = 48000.0;
    float coef = 0.01f, rate = 0.4f, phase = 0.0f;
    float z[2][6] {}, fbState[2] {};
    Smooth depth, feedback, wet;
    bool first = true;
};

// ------------------------------------------------------------------------------------- Flanger
class Flanger
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        coef = onePoleCoef (0.02, sr);
        for (auto& d : line) d.prepare ((int) (0.02 * sr));
        reset();
    }
    void reset() { for (auto& d : line) d.reset(); fb[0] = fb[1] = 0.0f; phase = 0.0f; first = true; }
    void set (const float* p, double)
    {
        rate = p[0];
        depth.t = p[1]; feedback.t = p[2]; wet.t = p[3];
        if (first) { depth.snap (p[1]); feedback.snap (p[2]); wet.snap (p[3]); first = false; }
    }
    void process (float* l, float* r, int n)
    {
        float* io[2] = { l, r };
        const float inc = rate / (float) sr;
        const float minD = (float) (0.0005 * sr), range = (float) (0.007 * sr);
        for (int i = 0; i < n; ++i)
        {
            const float d = depth.step (coef), f = feedback.step (coef), w = wet.step (coef);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float lfo = 0.5f + 0.5f * std::sin (6.2831853f * (phase + 0.25f * (float) ch));
                const float x = io[ch][i];
                line[ch].write (x + f * fb[ch]);
                const float y = line[ch].readFrac (minD + range * d * lfo + 1.0f);
                fb[ch] = sanitize (y);
                io[ch][i] = sanitize (x + w * 0.7f * y - w * 0.15f * x);
            }
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
        }
    }

private:
    double sr = 48000.0;
    float coef = 0.01f, rate = 0.25f, phase = 0.0f, fb[2] {};
    DelayLine line[2];
    Smooth depth, feedback, wet;
    bool first = true;
};

// ------------------------------------------------------------------------------------- Compressor
class Compressor
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { env = 0.0f; first = true; }
    void set (const float* p, double)
    {
        threshold = p[0];
        ratio = std::max (p[1], 1.0f);
        attack = onePoleCoef ((double) p[2] * 0.001, sr);
        release = onePoleCoef ((double) p[3] * 0.001, sr);
        makeup.t = dbToGain (p[4]);
        wet.t = p[5];
        if (first) { makeup.snap (makeup.t); wet.snap (wet.t); first = false; }
    }
    void process (float* l, float* r, int n)
    {
        const float c = onePoleCoef (0.02, sr);
        for (int i = 0; i < n; ++i)
        {
            const float peak = std::max (std::fabs (l[i]), std::fabs (r[i]));
            env += (peak > env ? attack : release) * (peak - env);
            const float levelDb = gainToDb (env);
            const float over = levelDb - threshold;
            const float grDb = over > 0.0f ? -over * (1.0f - 1.0f / ratio) : 0.0f;
            const float g = dbToGain (grDb) * makeup.step (c);
            const float w = wet.step (c);
            l[i] = sanitize (l[i] * (1.0f + w * (g - 1.0f)));
            r[i] = sanitize (r[i] * (1.0f + w * (g - 1.0f)));
        }
    }

private:
    double sr = 48000.0;
    float threshold = -18.0f, ratio = 3.0f, attack = 0.01f, release = 0.001f, env = 0.0f;
    Smooth makeup, wet;
    bool first = true;
};

// ------------------------------------------------------------------------------------- Bitcrusher
class Bitcrusher
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { hold[0] = hold[1] = 0.0f; counter = 0.0f; first = true; }
    void set (const float* p, double)
    {
        bits = p[0];
        factor = std::max (p[1], 1.0f);
        wet.t = p[2];
        if (first) { wet.snap (wet.t); first = false; }
    }
    void process (float* l, float* r, int n)
    {
        const float c = onePoleCoef (0.02, sr);
        const float levels = std::pow (2.0f, bits - 1.0f);
        for (int i = 0; i < n; ++i)
        {
            counter += 1.0f;
            if (counter >= factor)
            {
                counter -= factor;
                hold[0] = std::round (l[i] * levels) / levels;
                hold[1] = std::round (r[i] * levels) / levels;
            }
            const float w = wet.step (c);
            l[i] += w * (hold[0] - l[i]);
            r[i] += w * (hold[1] - r[i]);
        }
    }

private:
    double sr = 48000.0;
    float bits = 12.0f, factor = 1.0f, hold[2] {}, counter = 0.0f;
    Smooth wet;
    bool first = true;
};

// ------------------------------------------------------------------------------------- Delay
class Delay
{
public:
    static constexpr double kMaxSeconds = 4.0;
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& d : line) d.prepare ((int) (kMaxSeconds * sr) + 8);
        reset();
    }
    void reset() { for (auto& d : line) d.reset(); lp[0] = lp[1] = 0.0f; first = true; }
    void set (const float* p, double bpm)
    {
        double seconds = (double) p[0] * 0.001;
        if (p[1] >= 0.5f) seconds = syncDivBeats ((int) std::lround (p[2])) * 60.0 / std::max (bpm, 20.0);
        seconds = std::min (std::max (seconds, 0.001), kMaxSeconds - 0.01);
        time.t = (float) (seconds * sr);
        feedback.t = std::min (p[3], 0.9f);
        stereo.t = p[4];
        wet.t = p[5];
        if (first) { time.snap (time.t); feedback.snap (feedback.t); stereo.snap (p[4]); wet.snap (p[5]); first = false; }
    }
    void process (float* l, float* r, int n)
    {
        const float c = onePoleCoef (0.02, sr), ct = onePoleCoef (0.08, sr);
        const float damp = onePoleCoef (1.0 / (kTwoPi * 6000.0), sr);
        for (int i = 0; i < n; ++i)
        {
            const float t = time.step (ct), f = feedback.step (c), st = stereo.step (c), w = wet.step (c);
            const float dl = line[0].readFrac (t), dr = line[1].readFrac (t);
            lp[0] += damp * (dl - lp[0]);
            lp[1] += damp * (dr - lp[1]);
            // stereo = 0: independent L/R repeats, 1: full ping-pong (mono input feeds left, cross feedback).
            const float mono = 0.5f * (l[i] + r[i]);
            const float inL = l[i] + st * (mono * 1.4f - l[i]);
            const float inR = r[i] * (1.0f - st);
            line[0].write (sanitize (inL + f * (lp[0] + st * (lp[1] - lp[0]))));
            line[1].write (sanitize (inR + f * (lp[1] + st * (lp[0] - lp[1]))));
            l[i] += w * dl;
            r[i] += w * dr;
        }
    }

private:
    double sr = 48000.0;
    DelayLine line[2];
    float lp[2] {};
    Smooth time, feedback, stereo, wet;
    bool first = true;
};

// ------------------------------------------------------------------------------------- Reverb
// 8-line feedback delay network (Householder mixing) with input diffusion, damping and pre-delay.
class Reverb
{
public:
    static constexpr int kLines = 8;
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        static const double kLen[kLines] = { 0.0297, 0.0371, 0.0411, 0.0437, 0.0533, 0.0617, 0.0671, 0.0733 };
        for (int i = 0; i < kLines; ++i)
        {
            baseLen[i] = (float) (kLen[i] * sr);
            line[i].prepare ((int) (kLen[i] * sr * 1.6) + 8);
        }
        static const double kAp[4] = { 0.0047, 0.0036, 0.0127, 0.0093 };
        for (int ch = 0; ch < 2; ++ch)
            for (int k = 0; k < 2; ++k)
            {
                apLen[ch][k] = (int) (kAp[k * 2 + ch] * sr);
                ap[ch][k].prepare (apLen[ch][k] + 4);
            }
        for (auto& d : pre) d.prepare ((int) (0.21 * sr));
        reset();
    }
    void reset()
    {
        for (auto& d : line) d.reset();
        for (auto& ch : ap) for (auto& d : ch) d.reset();
        for (auto& d : pre) d.reset();
        std::memset (lp, 0, sizeof (lp));
        first = true;
    }
    void set (const float* p, double)
    {
        size.t = 0.5f + p[0];
        decay = p[1];
        preDelay = (int) std::min ((double) p[2] * 0.001 * sr, 0.2 * sr);
        damping = p[3];
        wet.t = p[4];
        if (first) { size.snap (size.t); wet.snap (wet.t); first = false; }
    }
    void process (float* l, float* r, int n)
    {
        const float c = onePoleCoef (0.03, sr);
        const float dampCoef = 1.0f - 0.85f * damping * damping;
        for (int i = 0; i < n; ++i)
        {
            const float sz = size.step (c * 0.1f), w = wet.step (c);
            pre[0].write (l[i]);
            pre[1].write (r[i]);
            float in[2] = { pre[0].read (preDelay), pre[1].read (preDelay) };
            for (int ch = 0; ch < 2; ++ch)
                for (int k = 0; k < 2; ++k)
                {
                    const float delayed = ap[ch][k].read (apLen[ch][k]);
                    const float v = in[ch] - 0.6f * delayed;
                    ap[ch][k].write (v);
                    in[ch] = delayed + 0.6f * v;
                }
            float out[kLines], sum = 0.0f;
            for (int k = 0; k < kLines; ++k)
            {
                const float len = baseLen[k] * sz;
                const float y = line[k].readFrac (len);
                // decay: -60 dB after `decay` seconds
                const float g = std::exp (-6.9077553f * len / (decay * (float) sr));
                lp[k] += dampCoef * (y * g - lp[k]);
                out[k] = lp[k];
                sum += out[k];
            }
            sum *= 2.0f / kLines;
            for (int k = 0; k < kLines; ++k)
                line[k].write (sanitize (out[k] - sum + in[k & 1] * 0.5f));
            const float wl = (out[0] - out[2] + out[4] - out[6]) * 0.5f;
            const float wr = (out[1] - out[3] + out[5] - out[7]) * 0.5f;
            l[i] += w * (wl - 0.25f * l[i]);
            r[i] += w * (wr - 0.25f * r[i]);
        }
    }

private:
    double sr = 48000.0;
    DelayLine line[kLines], ap[2][2], pre[2];
    float baseLen[kLines] {}, lp[kLines] {};
    int apLen[2][2] {};
    int preDelay = 0;
    float decay = 2.5f, damping = 0.5f;
    Smooth size, wet;
    bool first = true;
};
} // namespace fmlume::fx
