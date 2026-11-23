#pragma once
#include "DSP/FM/SynthEngine.h"
#include <cmath>
#include <vector>

namespace fmtest
{
using namespace fmlume;

struct NoteEvent { double time; MidiEvent ev; };

inline MidiEvent noteOn (int note, int vel = 100) { MidiEvent e; e.type = MidiEvent::NoteOn; e.data1 = (uint8_t) note; e.data2 = (uint8_t) vel; return e; }
inline MidiEvent noteOff (int note) { MidiEvent e; e.type = MidiEvent::NoteOff; e.data1 = (uint8_t) note; return e; }
inline MidiEvent cc (int num, int val) { MidiEvent e; e.type = MidiEvent::Controller; e.data1 = (uint8_t) num; e.data2 = (uint8_t) val; return e; }
inline MidiEvent bend (float v) { MidiEvent e; e.type = MidiEvent::PitchBend; e.bend = v; return e; }

struct Rendered { std::vector<float> l, r; };

// Renders `seconds` of audio feeding the timed events (times in seconds, sorted).
inline Rendered renderSeq (SynthEngine& eng, double sr, int block, double seconds, std::vector<NoteEvent> seq,
                           const HostParams& host = {}, TransportInfo tr = {})
{
    const int total = (int) (seconds * sr);
    Rendered out;
    out.l.assign ((size_t) total, 0.0f);
    out.r.assign ((size_t) total, 0.0f);
    size_t next = 0;
    std::vector<MidiEvent> evs;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        evs.clear();
        while (next < seq.size() && (int) std::llround (seq[next].time * sr) < pos + n)
        {
            MidiEvent e = seq[next].ev;
            e.offset = std::max (0, (int) std::llround (seq[next].time * sr) - pos);
            evs.push_back (e);
            ++next;
        }
        eng.process (out.l.data() + pos, out.r.data() + pos, n, evs.data(), (int) evs.size(), host, tr);
        if (tr.playing) tr.ppq += (double) n / sr * tr.bpm / 60.0;
    }
    return out;
}

inline bool allFinite (const std::vector<float>& v)
{
    for (float x : v) if (! std::isfinite (x)) return false;
    return true;
}
inline float peak (const std::vector<float>& v, size_t from = 0, size_t to = (size_t) -1)
{
    float p = 0.0f;
    to = std::min (to, v.size());
    for (size_t i = from; i < to; ++i) p = std::max (p, std::fabs (v[i]));
    return p;
}
inline double rms (const std::vector<float>& v, size_t from = 0, size_t to = (size_t) -1)
{
    double s = 0.0;
    to = std::min (to, v.size());
    if (to <= from) return 0.0;
    for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / (double) (to - from));
}
// Frequency estimate from interpolated positive-going zero crossings.
inline double zeroCrossHz (const std::vector<float>& v, double sr, size_t from, size_t to)
{
    double first = -1.0, last = -1.0;
    int count = 0;
    for (size_t i = from + 1; i < to && i < v.size(); ++i)
        if (v[i - 1] < 0.0f && v[i] >= 0.0f)
        {
            const double t = (double) (i - 1) + (double) (-v[i - 1]) / (double) (v[i] - v[i - 1]);
            if (first < 0.0) first = t;
            last = t;
            ++count;
        }
    return count < 2 ? 0.0 : (double) (count - 1) * sr / (last - first);
}
// Magnitude of a single frequency bin (Goertzel-style direct correlation), Hann windowed.
inline double toneMag (const std::vector<float>& v, double sr, double hz, size_t from, size_t to)
{
    double re = 0.0, im = 0.0, wsum = 0.0;
    to = std::min (to, v.size());
    const double n = (double) (to - from);
    for (size_t i = from; i < to; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (6.283185307179586 * (double) (i - from) / n);
        const double ph = 6.283185307179586 * hz * (double) i / sr;
        re += w * v[i] * std::cos (ph);
        im += w * v[i] * std::sin (ph);
        wsum += w;
    }
    return 2.0 * std::sqrt (re * re + im * im) / wsum;
}
inline double maxAbsDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    double m = 0.0;
    for (size_t i = 0; i < std::min (a.size(), b.size()); ++i) m = std::max (m, (double) std::fabs (a[i] - b[i]));
    return a.size() == b.size() ? m : 1.0e9;
}
} // namespace fmtest
