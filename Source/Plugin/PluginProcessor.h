#pragma once
#include "Core/PatchController.h"
#include "DSP/FM/SynthEngine.h"
#include "Presets/PresetManager.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <mutex>

// Host-facing wrapper. The audio thread only touches `engine`, the parameter atomics and the
// lock-free UI note queue; the editable model lives in `controller` behind `modelMutex`
// (message thread + state save/restore threads, never the audio thread).
class FmlumeAudioProcessor final : public juce::AudioProcessor,
                                   private juce::Timer
{
public:
    FmlumeAudioProcessor();
    ~FmlumeAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // The development build is a separate plug-in (docs/DECISIONS.md D-047).
   #if FMLUME_DEV_BUILD
    static constexpr bool kDevBuild = true;
    const juce::String getName() const override { return "FMLUME Dev"; }
   #else
    static constexpr bool kDevBuild = false;
    const juce::String getName() const override { return "FMLUME"; }
   #endif
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; } // longest reverb decay

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- model access for the editor (message thread) -----------------------------------
    template <typename Fn>
    auto withModel (Fn&& fn)
    {
        std::lock_guard<std::recursive_mutex> lock (modelMutex);
        return fn (controller);
    }
    fmlume::PresetManager& presetManager() { return presets; }
    uint32_t getModelVersion() const { return modelVersion.load(); }
    fmlume::HostParams currentHostParams() const;
    // Edited since it was loaded / saved: model edits or a moved preset-stored host parameter (macro, ...).
    bool isPresetDirty();

    // Preset operations (message thread). Return an error message, empty on success.
    juce::String loadPresetIndex (int listIndex);
    juce::String savePreset (const juce::String& name, bool overwrite);
    juce::String exportPreset (const juce::File& file);
    juce::String importPreset (const juce::File& file);
    int currentPresetIndex();
    // Why the last project-state restore was refused (empty if it worked). Reading clears it.
    juce::String takeStateError();
    void stepPreset (int delta);

    // On-screen keyboard / audition notes (lock-free hand-over to the audio thread).
    void uiNote (int note, float velocity, bool on);
    void auditionChord();
    static constexpr int kAuditionNotes[2] = { 60, 67 }; // C4 + G4
    // On-screen wheels (bend -1..1, mod 0..1) and the values last received over MIDI (for display).
    void setUiPitchBend (float v) noexcept { uiBend.store (v); uiBendDirty.store (true); }
    void setUiModWheel (float v) noexcept { uiMod.store (v); uiModDirty.store (true); }
    bool takeMidiPitchBend (float& v) noexcept { if (! midiBendDirty.exchange (false)) return false; v = midiBend.load(); return true; }
    bool takeMidiModWheel (float& v) noexcept { if (! midiModDirty.exchange (false)) return false; v = midiMod.load(); return true; }

    // Meters
    float getPeak (int ch) const { return engine.getPeak (ch); }

    juce::AudioProcessorValueTreeState apvts;

    static constexpr const char* kMacroIds[8] = { "macro.1", "macro.2", "macro.3", "macro.4", "macro.5", "macro.6", "macro.7", "macro.8" };
    static constexpr const char* kQuickIds[4] = { "fx.quick.warmth", "fx.quick.width", "fx.quick.space", "fx.quick.echo" };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void applyHostValues (const fmlume::HostParams& h);
    void setParam (const char* id, float plainValue);
    void loadFactoryPresets();

    fmlume::SynthEngine engine;
    fmlume::PatchController controller;
    fmlume::PresetManager presets;
    std::recursive_mutex modelMutex;
    std::atomic<uint32_t> modelVersion { 1 };
    juce::String stateError; // guarded by modelMutex

    // Parameter atomics (owned by APVTS)
    std::atomic<float>* pGain = nullptr; std::atomic<float>* pPan = nullptr; std::atomic<float>* pPoly = nullptr;
    std::atomic<float>* pUnison = nullptr; std::atomic<float>* pDetune = nullptr; std::atomic<float>* pSpread = nullptr;
    std::atomic<float>* pQuality = nullptr; std::atomic<float>* pMacro[8] {};
    std::atomic<float>* pFxBypass = nullptr; std::atomic<float>* pQuick[4] {};
    std::atomic<float>* pArp = nullptr; std::atomic<float>* pGate = nullptr;

    // UI -> audio note queue
    struct UiNote { uint8_t note, velocity; bool on; };
    juce::AbstractFifo uiFifo { 128 };
    UiNote uiNotes[128] {};
    int auditionOffCountdown = -1;
    std::atomic<float> uiBend { 0.0f }, uiMod { 0.0f }, midiBend { 0.0f }, midiMod { 0.0f };
    std::atomic<bool> uiBendDirty { false }, uiModDirty { false }, midiBendDirty { false }, midiModDirty { false };
    std::atomic<bool> wasBypassed { false };

    std::vector<fmlume::MidiEvent> eventBuffer;
    std::vector<float> scratchR;
    double currentSampleRate = 48000.0;
    int reportedLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FmlumeAudioProcessor)
};
