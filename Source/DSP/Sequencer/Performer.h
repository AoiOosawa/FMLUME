// Chord mode + arpeggiator + sample-accurate event scheduler (SPEC §11.1, §11.2).
// Fixed capacity, allocation free; runs on the audio thread in front of the voice allocator.
#pragma once
#include "Core/Model.h"
#include "Core/Pcg32.h"
#include <cstdint>

namespace fmlume
{
class Performer
{
public:
    struct Clock
    {
        double ppq = 0.0;            // quarter notes at block start
        double beatsPerSample = 0.0;
        bool hostPlaying = false;
        bool jumped = false;         // transport started or position discontinuity
        bool stopped = false;        // transport just stopped
    };

    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset();

    // Transforms the block's input events into engine events (sorted by offset). Returns the count.
    int process (const MidiEvent* in, int numIn, int n, const PerformanceState& perf, bool arpEnabled,
                 const Clock& clock, MidiEvent* out, int maxOut);

    int getNumScheduled() const { return numSched; }
    int getNumHeld() const { return numHeld; }

private:
    struct Scheduled { int64_t time; MidiEvent ev; uint32_t order; };
    struct Held { uint8_t note; uint8_t vel; };

    void schedule (int64_t time, MidiEvent ev);
    void inputNoteOn (int note, int vel, int64_t now, const PerformanceState& perf, bool arp);
    void inputNoteOff (int note, int64_t now, bool arp);
    void genNoteOn (int note, int vel, int64_t time);
    void genNoteOff (int note, int64_t time);
    void arpAdd (int note, int vel);
    void arpRemove (int note);
    void arpFlush (int64_t now);
    void flushAll (int64_t now);
    void fireStep (int64_t time, double stepSamples, const ArpState& arp);
    static int buildChord (int note, const ChordState& c, int* notes);

    static constexpr int kMaxSched = 512;
    static constexpr int kMaxHeld = 48;

    double sr = 48000.0;
    int64_t blockStart = 0;
    Scheduled sched[kMaxSched];
    int numSched = 0;
    uint32_t orderCounter = 0;

    // Chord bookkeeping: which generated notes each physical key produced.
    int8_t chordCount[128] {};
    uint8_t chordNotes[128][4] {};
    uint8_t outRef[128] {};       // reference count of sounding generated notes (non-arp path)

    // Arp state
    Held held[kMaxHeld];
    int numHeld = 0;
    bool arpWasEnabled = false;
    int patternPos = 0;
    int64_t lastStep = INT64_MIN;
    double origin = 0.0;          // free-run: ppq at which step 0 fires
    bool arpIdle = true;
    bool wasHostPlaying = false;
    int arpSounding[8];
    int numArpSounding = 0;
    Pcg32 rng { 0xA11CEu };
};
} // namespace fmlume
