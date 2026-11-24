// FMLUME synth engine: 6-operator PM voices, unison, global filter, modulation, trance gate, FX chain.
// Host independent. process() is real-time safe: no allocation, locks, I/O or logging.
#pragma once
#include "Core/Model.h"
#include "Core/Params.h"
#include "Core/PatchCompiler.h"
#include "DSP/FX/FxChain.h"
#include "DSP/Filter/Svf.h"
#include "DSP/Modulation/Lfo.h"
#include "DSP/Sequencer/Performer.h"
#include "DSP/Sequencer/TranceGate.h"
#include "DSP/Util/DspUtil.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace fmlume
{
class SynthEngine
{
public:
    static constexpr int kControlInterval = 32; // host samples between control updates
    static constexpr int kMaxOversample = 4;
    static constexpr int kPatchSlots = 10;      // current + up to 9 older generations (SPEC §3.1: >= 8)

    SynthEngine();
    ~SynthEngine();

    // ---- non-realtime API -------------------------------------------------------------
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    // Validates, compiles and hands a patch to the audio thread (lock-free mailbox, latest wins).
    // forceNewGeneration: true for preset loads / Explorer previews; false for incremental edits,
    // which update the sounding patch in place when the structure (graph, waveforms) is unchanged.
    bool setPatch (const PatchData& data, bool forceNewGeneration = true, std::string* error = nullptr);
    static int latencyForQuality (int quality); // host samples, includes the FX chain's fixed latency
    int getLatencySamples() const { return latencyForQuality (activeQuality.load()); }
    // Largest number of simultaneous notes for a polyphony / unison setting (SPEC §5.1 budget).
    static int effectiveMaxNotes (int polyphony, int unison);

    // ---- realtime API -----------------------------------------------------------------
    // Renders n frames. Events must be sorted by offset; offsets are clamped to the block.
    void process (float* left, float* right, int n, const MidiEvent* events, int numEvents,
                  const HostParams& host, const TransportInfo& transport);

    // ---- meters (any thread) ----------------------------------------------------------
    int getActiveVoiceCount() const { return meterNotes.load (std::memory_order_relaxed); }
    int getActiveSubVoiceCount() const { return meterSubs.load (std::memory_order_relaxed); }
    float getPeak (int channel) const { return meterPeak[channel & 1].load (std::memory_order_relaxed); }
    uint32_t getAppliedGeneration() const { return consumedGen.load (std::memory_order_acquire); }
    int getActiveQuality() const { return activeQuality.load(); }

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    std::atomic<int> meterNotes { 0 }, meterSubs { 0 };
    std::atomic<float> meterPeak[2] { { 0.0f }, { 0.0f } };
    std::atomic<uint32_t> consumedGen { 0 };
    std::atomic<int> activeQuality { 1 };
};
} // namespace fmlume
