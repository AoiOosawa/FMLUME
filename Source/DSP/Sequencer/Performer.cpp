#include "DSP/Sequencer/Performer.h"
#include <iterator>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace fmlume
{
void Performer::reset()
{
    blockStart = 0;
    wasHostPlaying = false;
    numSched = 0;
    orderCounter = 0;
    std::memset (chordCount, 0, sizeof (chordCount));
    std::memset (outRef, 0, sizeof (outRef));
    numHeld = 0;
    arpWasEnabled = false;
    patternPos = 0;
    lastStep = INT64_MIN;
    origin = 0.0;
    arpIdle = true;
    numArpSounding = 0;
    rng.reseed (0xA11CEu);
}

void Performer::schedule (int64_t time, MidiEvent ev)
{
    if (numSched >= kMaxSched)
    {
        // Queue full: never drop a note-off. Drop the incoming note-on instead.
        if (ev.type == MidiEvent::NoteOn) return;
        for (int i = 0; i < numSched; ++i)
            if (sched[i].ev.type == MidiEvent::NoteOn) { sched[i] = sched[--numSched]; break; }
        if (numSched >= kMaxSched) return;
    }
    sched[numSched++] = { time, ev, orderCounter++ };
}

int Performer::buildChord (int note, const ChordState& c, int* notes)
{
    static const int8_t kIntervals[][4] = {
        { 0, -1, -1, -1 }, { 0, 4, 7, -1 }, { 0, 3, 7, -1 }, { 0, 2, 7, -1 }, { 0, 5, 7, -1 },
        { 0, 3, 6, -1 }, { 0, 4, 8, -1 }, { 0, 4, 7, 10 }, { 0, 3, 7, 10 }, { 0, 4, 7, 11 } };
    static_assert (std::size (kIntervals) == (size_t) ChordType::Count, "every ChordType needs its intervals");
    int iv[4], n = 0;
    for (int i = 0; i < 4; ++i)
        if (kIntervals[(int) c.type][i] >= 0) iv[n++] = kIntervals[(int) c.type][i];
    // Inversions: raise the lowest note(s) by an octave (only meaningful for 3+ note chords).
    const int inversions = n >= 3 ? std::min ((int) c.voicing, n - 1) : 0;
    for (int k = 0; k < inversions; ++k)
    {
        const int first = iv[0] + 12;
        for (int i = 0; i + 1 < n; ++i) iv[i] = iv[i + 1];
        iv[n - 1] = first;
    }
    int count = 0;
    for (int i = 0; i < n; ++i)
    {
        const int v = note + iv[i] + (c.type == ChordType::Off ? 0 : c.octave * 12);
        if (v < 0 || v > 127) continue;
        bool dup = false;
        for (int j = 0; j < count; ++j) dup = dup || notes[j] == v;
        if (! dup) notes[count++] = v;
    }
    if (count == 0) notes[count++] = std::min (std::max (note, 0), 127);
    return count;
}

void Performer::genNoteOn (int note, int vel, int64_t time)
{
    if (outRef[note] < 255) ++outRef[note];
    MidiEvent e;
    e.type = MidiEvent::NoteOn;
    e.data1 = (uint8_t) note;
    e.data2 = (uint8_t) vel;
    schedule (time, e);
}

void Performer::genNoteOff (int note, int64_t time)
{
    if (outRef[note] == 0) return;
    if (--outRef[note] > 0) return; // another key still holds this pitch
    // A strummed note-on that has not sounded yet is cancelled. The note-off is sent all the
    // same: the pitch may already be sounding from an earlier key (harmless if it is not).
    for (int i = 0; i < numSched;)
    {
        if (sched[i].ev.type == MidiEvent::NoteOn && sched[i].ev.data1 == note && sched[i].time > time) sched[i] = sched[--numSched];
        else ++i;
    }
    MidiEvent e;
    e.type = MidiEvent::NoteOff;
    e.data1 = (uint8_t) note;
    schedule (time, e);
}

void Performer::arpAdd (int note, int vel)
{
    for (int i = 0; i < numHeld; ++i)
        if (held[i].note == note) return;
    if (numHeld < kMaxHeld) held[numHeld++] = { (uint8_t) note, (uint8_t) vel };
}

void Performer::arpRemove (int note)
{
    for (int i = 0; i < numHeld; ++i)
        if (held[i].note == note)
        {
            for (int j = i; j + 1 < numHeld; ++j) held[j] = held[j + 1];
            --numHeld;
            return;
        }
}

void Performer::arpFlush (int64_t now)
{
    // Remove pending arp note-offs and send them immediately so nothing hangs.
    for (int i = 0; i < numArpSounding; ++i)
    {
        const int note = arpSounding[i];
        for (int k = 0; k < numSched;)
        {
            if (sched[k].ev.type == MidiEvent::NoteOff && sched[k].ev.data1 == note) sched[k] = sched[--numSched];
            else ++k;
        }
        MidiEvent e;
        e.type = MidiEvent::NoteOff;
        e.data1 = (uint8_t) note;
        schedule (now, e);
    }
    numArpSounding = 0;
    patternPos = 0;
    lastStep = INT64_MIN;
}

void Performer::flushAll (int64_t now)
{
    arpFlush (now);
    for (int k = 0; k < numSched;)
    {
        if (sched[k].ev.type == MidiEvent::NoteOn) sched[k] = sched[--numSched];
        else ++k;
    }
    for (int note = 0; note < 128; ++note)
        if (outRef[note] > 0)
        {
            outRef[note] = 0;
            MidiEvent e;
            e.type = MidiEvent::NoteOff;
            e.data1 = (uint8_t) note;
            schedule (now, e);
        }
    std::memset (chordCount, 0, sizeof (chordCount));
    numHeld = 0;
    arpIdle = true;
}

void Performer::inputNoteOn (int note, int vel, int64_t now, const PerformanceState& perf, bool arp)
{
    if (chordCount[note] > 0) inputNoteOff (note, now, arp); // re-press without release
    int notes[4];
    const int n = buildChord (note, perf.chord, notes);
    chordCount[note] = (int8_t) n;
    const double strum = perf.chord.type == ChordType::Off ? 0.0 : (double) perf.chord.strumMs * 0.001 * sr;
    for (int i = 0; i < n; ++i)
    {
        chordNotes[note][i] = (uint8_t) notes[i];
        if (arp) arpAdd (notes[i], vel);
        else genNoteOn (notes[i], vel, now + (int64_t) std::llround (strum * i));
    }
}

void Performer::inputNoteOff (int note, int64_t now, bool arp)
{
    const int n = chordCount[note];
    chordCount[note] = 0;
    for (int i = 0; i < n; ++i)
    {
        const int g = chordNotes[note][i];
        if (arp)
        {
            // Keep the pitch while another held key still generates it.
            bool stillHeld = false;
            for (int k = 0; k < 128 && ! stillHeld; ++k)
                for (int j = 0; j < chordCount[k]; ++j) stillHeld = stillHeld || chordNotes[k][j] == g;
            if (! stillHeld) arpRemove (g);
        }
        else genNoteOff (g, now);
    }
}

// Index into n items walked up and back down without repeating the ends: 0 1 2 1 | 0 1 2 1 ...
static int pingPong (int pos, int n)
{
    const int period = n > 1 ? 2 * n - 2 : 1;
    const int p = pos % period;
    return p < n ? p : period - p;
}

void Performer::fireStep (int64_t time, double stepSamples, const ArpState& arp)
{
    if (numHeld == 0) return;
    // Build the pattern for this step.
    uint8_t seq[kMaxHeld * 4];
    uint8_t vel[kMaxHeld * 4];
    Held sorted[kMaxHeld];
    std::memcpy (sorted, held, sizeof (Held) * (size_t) numHeld);
    if (arp.mode != ArpMode::AsPlayed)
        std::sort (sorted, sorted + numHeld, [] (const Held& a, const Held& b) { return a.note < b.note; });
    const int octaves = std::min (std::max (arp.octaves, 1), 4);
    int note = 0, velocity = 0;
    if (arp.octaveMode == ArpOctaveMode::Range)
    {
        // The note order runs across the whole range.
        int len = 0;
        for (int o = 0; o < octaves; ++o)
            for (int i = 0; i < numHeld; ++i)
            {
                const int v = sorted[i].note + 12 * o;
                if (v > 127) continue;
                seq[len] = (uint8_t) v;
                vel[len] = sorted[i].vel;
                ++len;
            }
        if (len == 0) return;
        int idx = 0;
        switch (arp.mode)
        {
            case ArpMode::Down: idx = len - 1 - (patternPos % len); break;
            case ArpMode::UpDown: idx = pingPong (patternPos, len); break;
            case ArpMode::Random: idx = rng.nextInt (len); break;
            default: idx = patternPos % len; break;
        }
        note = seq[idx];
        velocity = vel[idx];
    }
    else
    {
        // The note order is played inside one octave, then the whole figure moves by an octave.
        const int n = numHeld;
        const int inner = arp.mode == ArpMode::UpDown && n > 1 ? 2 * n - 2 : n;
        const int p = patternPos % inner;
        int idx = p;
        switch (arp.mode)
        {
            case ArpMode::Down: idx = n - 1 - p; break;
            case ArpMode::UpDown: idx = pingPong (p, n); break;
            case ArpMode::Random: idx = rng.nextInt (n); break;
            default: break;
        }
        const int figure = patternPos / inner;
        int oct = figure % octaves;
        if (arp.octaveMode == ArpOctaveMode::Down) oct = octaves - 1 - oct;
        else if (arp.octaveMode == ArpOctaveMode::UpDown) oct = pingPong (figure, octaves);
        note = sorted[idx].note + 12 * oct;
        while (note > 127) note -= 12;
        velocity = sorted[idx].vel;
    }
    ++patternPos;

    // If this pitch is still sounding from an earlier step (long gate + swing, or the tempo rose),
    // its note-off is pulled forward to now: left where it is, it would cut the note started here.
    for (int i = 0; i < numSched; ++i)
        if (sched[i].ev.type == MidiEvent::NoteOff && sched[i].ev.data1 == note && sched[i].time > time) sched[i].time = time;

    MidiEvent on;
    on.type = MidiEvent::NoteOn;
    on.data1 = (uint8_t) note;
    on.data2 = (uint8_t) velocity;
    schedule (time, on);
    MidiEvent off;
    off.type = MidiEvent::NoteOff;
    off.data1 = (uint8_t) note;
    const int64_t gateLen = std::max<int64_t> (1, (int64_t) std::llround (stepSamples * std::min (std::max (arp.gate, 0.05f), 1.0f)));
    schedule (time + gateLen, off);
    if (numArpSounding == 8)
    {
        for (int i = 0; i + 1 < 8; ++i) arpSounding[i] = arpSounding[i + 1];
        --numArpSounding;
    }
    arpSounding[numArpSounding++] = (uint8_t) note;
}

int Performer::process (const MidiEvent* in, int numIn, int n, const PerformanceState& perf, bool arpEnabled,
                        const Clock& clock, MidiEvent* out, int maxOut)
{
    const int64_t now0 = blockStart;

    if (arpEnabled != arpWasEnabled)
    {
        flushAll (now0); // mode change: release everything we generated (no stuck notes)
        arpWasEnabled = arpEnabled;
    }
    if (arpEnabled && (clock.jumped || (clock.stopped && perf.arp.resetOnStop)))
    {
        arpFlush (now0);
        arpIdle = true;
    }

    // The step grid is counted from 0 while the host plays and from `origin` in free-run. When the
    // host starts or stops without a reset, the old step index does not belong to the new grid.
    if (clock.hostPlaying != wasHostPlaying)
    {
        wasHostPlaying = clock.hostPlaying;
        lastStep = INT64_MIN;
        if (! clock.hostPlaying) origin = clock.ppq; // free-run continues from here
    }

    const double stepBeats = syncDivBeats (perf.arp.rate);
    const double bps = clock.beatsPerSample > 0.0 ? clock.beatsPerSample : 120.0 / 60.0 / sr;
    const double stepSamples = stepBeats / bps;
    auto stepTime = [&] (int64_t k) {
        const bool odd = ((k % 2) + 2) % 2 == 1;
        return (double) k * stepBeats + (odd ? perf.arp.swing * 0.5 * stepBeats : 0.0);
    };

    int evIdx = 0;
    bool reanchored = false;
    auto consumeInputUpTo = [&] (int offsetLimit) {
        while (evIdx < numIn && std::min (std::max (in[evIdx].offset, 0), n - 1) <= offsetLimit)
        {
            const MidiEvent& e = in[evIdx++];
            const int off = std::min (std::max (e.offset, 0), std::max (n - 1, 0));
            const int64_t t = now0 + off;
            switch (e.type)
            {
                case MidiEvent::NoteOn:
                    if (e.data2 > 0 && e.data1 < 128)
                    {
                        const bool startNow = arpEnabled && numHeld == 0 && arpIdle && ! clock.hostPlaying;
                        inputNoteOn (e.data1, e.data2, t, perf, arpEnabled);
                        if (startNow)
                        {
                            // Free-run: the first key starts the pattern immediately (step 0 = now).
                            origin = clock.ppq + off * bps;
                            patternPos = 0;
                            lastStep = 0;
                            reanchored = true; // the step grid moved: the caller's loop must start over
                            fireStep (t, stepSamples, perf.arp);
                        }
                        if (arpEnabled) arpIdle = false;
                        break;
                    }
                    [[fallthrough]];
                case MidiEvent::NoteOff:
                    if (e.data1 < 128) inputNoteOff (e.data1, t, arpEnabled);
                    if (arpEnabled && numHeld == 0) { arpIdle = true; patternPos = 0; }
                    break;
                case MidiEvent::AllNotesOff:
                case MidiEvent::AllSoundOff:
                    flushAll (t);
                    schedule (t, e);
                    break;
                default:
                    schedule (t, e);
                    break;
            }
        }
    };

    if (arpEnabled)
    {
        // A first key in free-run mode re-anchors the step grid (origin, lastStep) while input is
        // being consumed. The step index of the old grid is then meaningless, so the walk starts
        // over on the new grid (at most once per such key).
        for (bool again = true; again;)
        {
            again = false;
            const double base = clock.hostPlaying ? 0.0 : origin;
            const double p0 = clock.ppq - base, p1 = p0 + n * bps;
            for (int64_t k = (int64_t) std::floor (p0 / stepBeats) - 1;; ++k)
            {
                const double t = stepTime (k);
                if (t >= p1) break;
                if (t < p0 - 1.0e-9 || k <= lastStep) continue;
                const int off = std::min (std::max ((int) std::floor ((t - p0) / bps + 1.0e-6), 0), n - 1);
                consumeInputUpTo (off);
                if (reanchored) { reanchored = false; again = true; break; }
                lastStep = k;
                fireStep (now0 + off, stepSamples, perf.arp);
            }
        }
    }
    consumeInputUpTo (n);

    // Emit everything due in this block, ordered by time (note-offs before note-ons at equal times).
    int count = 0;
    const int64_t blockEnd = now0 + n;
    while (count < maxOut)
    {
        int best = -1;
        for (int i = 0; i < numSched; ++i)
        {
            if (sched[i].time >= blockEnd) continue;
            if (best < 0) { best = i; continue; }
            const auto& a = sched[i];
            const auto& b = sched[best];
            const int pa = a.ev.type == MidiEvent::NoteOn ? 1 : 0, pb = b.ev.type == MidiEvent::NoteOn ? 1 : 0;
            if (a.time < b.time || (a.time == b.time && (pa < pb || (pa == pb && a.order < b.order)))) best = i;
        }
        if (best < 0) break;
        MidiEvent e = sched[best].ev;
        e.offset = (int) std::min<int64_t> (std::max<int64_t> (sched[best].time - now0, 0), n - 1);
        if (e.type == MidiEvent::NoteOff)
            for (int i = 0; i < numArpSounding; ++i)
                if (arpSounding[i] == e.data1) { for (int j = i; j + 1 < numArpSounding; ++j) arpSounding[j] = arpSounding[j + 1]; --numArpSounding; break; }
        out[count++] = e;
        sched[best] = sched[--numSched];
    }
    blockStart = blockEnd;
    return count;
}
} // namespace fmlume
