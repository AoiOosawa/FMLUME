#include "Plugin/PluginProcessor.h"
#include <limits>
#include "Plugin/PluginEditor.h"
#include "Core/Templates.h"
#include "Presets/PresetSerializer.h"
#if FMLUME_HAS_FACTORY_DATA
 #include "FactoryData.h"
#endif

using namespace fmlume;

namespace
{
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

// centre: when inside (lo, hi), the value that sits in the middle of the control's travel.
std::unique_ptr<juce::AudioParameterFloat> floatParam (const char* id, const char* name, float lo, float hi, float def, const char* unit, int decimals = 2,
                                                       float centre = std::numeric_limits<float>::quiet_NaN())
{
    auto attr = juce::AudioParameterFloatAttributes()
                    .withLabel (unit)
                    .withStringFromValueFunction ([decimals] (float v, int) { return juce::String (v, decimals); });
    juce::NormalisableRange<float> range (lo, hi);
    if (centre > lo && centre < hi) range.setSkewForCentre (centre);
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def, attr);
}
std::unique_ptr<juce::AudioParameterFloat> percentParam (const char* id, const juce::String& name, float def)
{
    auto attr = juce::AudioParameterFloatAttributes()
                    .withLabel ("%")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v * 100.0f, 1); })
                    .withValueFromStringFunction ([] (const juce::String& s) { return juce::jlimit (0.0f, 1.0f, s.getFloatValue() / 100.0f); });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 1.0f), def, attr);
}
} // namespace

Layout FmlumeAudioProcessor::createLayout()
{
    Layout l;
    // SPEC §13: these ids are permanent. Never remove or reuse one after a release.
    l.add (floatParam ("master.gainDb", "Master Gain", -60.0f, 12.0f, 0.0f, "dB", 1, 0.0f) /* 0 dB in the middle, as in POLYBRISK */);
    l.add (floatParam ("master.pan", "Master Pan", -1.0f, 1.0f, 0.0f, "", 2));
    l.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "master.polyphony", 1 }, "Polyphony", 1, 32, 16));
    l.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "master.unison", 1 }, "Unison Voices", 1, 8, 1));
    l.add (floatParam ("master.detune", "Unison Detune", 0.0f, 50.0f, 10.0f, "ct", 1));
    l.add (floatParam ("master.spread", "Stereo Spread", 0.0f, 100.0f, 50.0f, "%", 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "master.quality", 1 }, "Quality",
                                                         juce::StringArray { "Eco", "Standard", "High" }, 1));
    for (int i = 0; i < 8; ++i)
        l.add (percentParam (kMacroIds[i], "Macro " + juce::String (i + 1), 0.5f)); // host names are fixed (SPEC §6.1)
    l.add (percentParam ("morph.amount", "Morph Amount", 0.0f));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "morph.enabled", 1 }, "Morph Enabled", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "fx.bypassAll", 1 }, "FX Bypass All", false));
    static const char* const quickNames[4] = { "FX Warmth", "FX Width", "FX Space", "FX Echo" };
    for (int i = 0; i < 4; ++i) l.add (percentParam (kQuickIds[i], quickNames[i], 0.5f));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "arp.enabled", 1 }, "Arp Enabled", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "tranceGate.enabled", 1 }, "Trance Gate Enabled", false));
    return l;
}

FmlumeAudioProcessor::FmlumeAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FMLUME_PARAMS", createLayout())
{
    pGain = apvts.getRawParameterValue ("master.gainDb");
    pPan = apvts.getRawParameterValue ("master.pan");
    pPoly = apvts.getRawParameterValue ("master.polyphony");
    pUnison = apvts.getRawParameterValue ("master.unison");
    pDetune = apvts.getRawParameterValue ("master.detune");
    pSpread = apvts.getRawParameterValue ("master.spread");
    pQuality = apvts.getRawParameterValue ("master.quality");
    for (int i = 0; i < 8; ++i) pMacro[i] = apvts.getRawParameterValue (kMacroIds[i]);
    pMorph = apvts.getRawParameterValue ("morph.amount");
    pMorphOn = apvts.getRawParameterValue ("morph.enabled");
    pFxBypass = apvts.getRawParameterValue ("fx.bypassAll");
    for (int i = 0; i < 4; ++i) pQuick[i] = apvts.getRawParameterValue (kQuickIds[i]);
    pArp = apvts.getRawParameterValue ("arp.enabled");
    pGate = apvts.getRawParameterValue ("tranceGate.enabled");
    eventBuffer.resize (2048);
    scratchR.assign (8192, 0.0f);

    controller.sendPatch = [this] (const PatchData& d, bool forceNew) { return engine.setPatch (d, forceNew); };
    controller.applyHostValues = [this] (const HostParams& h) { applyHostValues (h); };
    controller.changed = [this] { modelVersion.fetch_add (1); };

    loadFactoryPresets();
    // Release and Dev plug-in share the user presets (as in POLYBRISK): they are the same instrument.
    presets.setUserDirectory (PresetManager::defaultUserDirectory());

    // Start on the first factory preset when available, otherwise on the E.Piano template.
    Preset first;
    if (! presets.entries().empty() && presets.load (0, first)) controller.loadPreset (first);
    else controller.loadPreset (makeCategoryPreset (Category::EPiano));
    startTimerHz (15);
}

FmlumeAudioProcessor::~FmlumeAudioProcessor() { stopTimer(); }

void FmlumeAudioProcessor::loadFactoryPresets()
{
   #if FMLUME_HAS_FACTORY_DATA
    for (int i = 0; i < FactoryData::namedResourceListSize; ++i)
    {
        int size = 0;
        if (const char* data = FactoryData::getNamedResource (FactoryData::namedResourceList[i], size))
            presets.addFactoryPreset (std::string (data, (size_t) size));
    }
   #endif
}

HostParams FmlumeAudioProcessor::currentHostParams() const
{
    HostParams h;
    h.gainDb = pGain->load();
    h.pan = pPan->load();
    h.polyphony = juce::roundToInt (pPoly->load());
    h.unison = juce::roundToInt (pUnison->load());
    h.detune = pDetune->load();
    h.spread = pSpread->load();
    h.quality = juce::roundToInt (pQuality->load());
    for (int i = 0; i < 8; ++i) h.macro[(size_t) i] = pMacro[i]->load();
    h.morphAmount = pMorph->load();
    h.morphEnabled = pMorphOn->load() > 0.5f;
    h.fxBypassAll = pFxBypass->load() > 0.5f;
    for (int i = 0; i < 4; ++i) h.quick[(size_t) i] = pQuick[i]->load();
    h.arpEnabled = pArp->load() > 0.5f;
    h.gateEnabled = pGate->load() > 0.5f;
    return h;
}

void FmlumeAudioProcessor::setParam (const char* id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        const float norm = p->convertTo0to1 (plainValue);
        if (std::abs (p->getValue() - norm) < 1.0e-7f) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }
}

// Preset-stored host values. Master gain / pan / quality / FX bypass are session settings and
// deliberately not touched by a preset change (docs/DECISIONS.md D-010).
void FmlumeAudioProcessor::applyHostValues (const HostParams& h)
{
    for (int i = 0; i < 8; ++i) setParam (kMacroIds[i], h.macro[(size_t) i]);
    for (int i = 0; i < 4; ++i) setParam (kQuickIds[i], h.quick[(size_t) i]);
    setParam ("master.polyphony", (float) h.polyphony);
    setParam ("master.unison", (float) h.unison);
    setParam ("master.detune", h.detune);
    setParam ("master.spread", h.spread);
    setParam ("morph.amount", h.morphAmount);
    setParam ("morph.enabled", h.morphEnabled ? 1.0f : 0.0f);
    setParam ("arp.enabled", h.arpEnabled ? 1.0f : 0.0f);
    setParam ("tranceGate.enabled", h.gateEnabled ? 1.0f : 0.0f);
}

void FmlumeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    {
        // prepare() resets the engine state; hand it the current patch again.
        std::lock_guard<std::recursive_mutex> lock (modelMutex);
        engine.setPatch (controller.data(), true);
    }
    scratchR.assign ((size_t) juce::jmax (samplesPerBlock, 8192), 0.0f);
    reportedLatency = SynthEngine::latencyForQuality (juce::roundToInt (pQuality->load()));
    setLatencySamples (reportedLatency);
}

bool FmlumeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void FmlumeAudioProcessor::timerCallback()
{
    // The engine switches oversampling at a silent block boundary; follow it with the latency report.
    const int latency = engine.getLatencySamples();
    if (latency != reportedLatency)
    {
        reportedLatency = latency;
        setLatencySamples (latency);
    }
}

void FmlumeAudioProcessor::uiNote (int note, float velocity, bool on)
{
    const auto scope = uiFifo.write (1);
    if (scope.blockSize1 > 0)
        uiNotes[scope.startIndex1] = { (uint8_t) juce::jlimit (0, 127, note), (uint8_t) juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f)), on };
}

void FmlumeAudioProcessor::auditionChord()
{
    // Two notes at a moderate velocity: about as loud as one firmly played key. (The earlier
    // three-note chord at velocity 0.8 was ~6 dB louder than that and startled.)
    for (int note : kAuditionNotes) uiNote (note, 0.5f, true);
    const auto scope = uiFifo.write (1);
    if (scope.blockSize1 > 0) uiNotes[scope.startIndex1] = { 254, 0, false }; // sentinel: schedule the automatic release
}

void FmlumeAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // The engine is not run while bypassed; when processing resumes it first silences any voice
    // that was sounding (their note-offs may have been missed), so nothing hangs.
    wasBypassed.store (true, std::memory_order_relaxed);
    midi.clear();
    buffer.clear();
}

void FmlumeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    const int maxEvents = (int) eventBuffer.size();
    int numEvents = 0;

    if (wasBypassed.exchange (false, std::memory_order_relaxed))
    {
        MidiEvent e;
        e.type = MidiEvent::AllSoundOff;
        eventBuffer[(size_t) numEvents++] = e;
    }
    // On-screen wheels (offset 0).
    if (uiBendDirty.exchange (false, std::memory_order_relaxed) && numEvents < maxEvents)
    {
        MidiEvent e;
        e.type = MidiEvent::PitchBend;
        e.bend = uiBend.load (std::memory_order_relaxed);
        eventBuffer[(size_t) numEvents++] = e;
    }
    if (uiModDirty.exchange (false, std::memory_order_relaxed) && numEvents < maxEvents)
    {
        MidiEvent e;
        e.type = MidiEvent::Controller;
        e.data1 = 1;
        e.data2 = (uint8_t) juce::jlimit (0, 127, juce::roundToInt (uiMod.load (std::memory_order_relaxed) * 127.0f));
        eventBuffer[(size_t) numEvents++] = e;
    }
    // Notes from the on-screen keyboard / audition button first (offset 0).
    {
        const auto scope = uiFifo.read (uiFifo.getNumReady());
        auto take = [&] (int start, int count) {
            for (int i = 0; i < count && numEvents < maxEvents; ++i)
            {
                const UiNote u = uiNotes[start + i];
                if (u.note == 254) { auditionOffCountdown = (int) (currentSampleRate * 0.9); continue; }
                MidiEvent e;
                e.type = u.on ? MidiEvent::NoteOn : MidiEvent::NoteOff;
                e.data1 = u.note;
                e.data2 = u.velocity;
                eventBuffer[(size_t) numEvents++] = e;
            }
        };
        take (scope.startIndex1, scope.blockSize1);
        take (scope.startIndex2, scope.blockSize2);
    }
    if (auditionOffCountdown >= 0)
    {
        auditionOffCountdown -= n;
        if (auditionOffCountdown < 0)
            for (int note : kAuditionNotes)
                if (numEvents < maxEvents) { MidiEvent e; e.type = MidiEvent::NoteOff; e.data1 = (uint8_t) note; eventBuffer[(size_t) numEvents++] = e; }
    }

    for (const auto meta : midi)
    {
        if (numEvents >= maxEvents) break;
        if (meta.numBytes > 3) continue; // SysEx etc.: unused, and building the message would allocate
        const auto m = meta.getMessage();
        MidiEvent e;
        e.offset = juce::jlimit (0, juce::jmax (0, n - 1), meta.samplePosition);
        if (m.isNoteOn()) { e.type = MidiEvent::NoteOn; e.data1 = (uint8_t) m.getNoteNumber(); e.data2 = (uint8_t) m.getVelocity(); }
        else if (m.isNoteOff()) { e.type = MidiEvent::NoteOff; e.data1 = (uint8_t) m.getNoteNumber(); }
        else if (m.isPitchWheel())
        {
            e.type = MidiEvent::PitchBend;
            e.bend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
            midiBend.store (e.bend, std::memory_order_relaxed);
            midiBendDirty.store (true, std::memory_order_relaxed);
        }
        else if (m.isAllNotesOff()) { e.type = MidiEvent::AllNotesOff; }
        else if (m.isAllSoundOff()) { e.type = MidiEvent::AllSoundOff; }
        else if (m.isController())
        {
            e.type = MidiEvent::Controller;
            e.data1 = (uint8_t) m.getControllerNumber();
            e.data2 = (uint8_t) m.getControllerValue();
            if (e.data1 == 1)
            {
                midiMod.store ((float) e.data2 / 127.0f, std::memory_order_relaxed);
                midiModDirty.store (true, std::memory_order_relaxed);
            }
        }
        else if (m.isChannelPressure()) { e.type = MidiEvent::ChannelPressure; e.data1 = e.data2 = (uint8_t) m.getChannelPressureValue(); }
        else continue;
        eventBuffer[(size_t) numEvents++] = e;
    }
    midi.clear();
    if (numCh == 0) return;

    const HostParams host = currentHostParams();
    TransportInfo transport;
    if (auto* ph = getPlayHead())
        if (const auto pos = ph->getPosition())
        {
            if (const auto bpm = pos->getBpm()) { transport.bpm = *bpm; transport.valid = true; }
            if (const auto ppq = pos->getPpqPosition()) transport.ppq = *ppq; else transport.valid = false;
            transport.playing = pos->getIsPlaying();
        }

    float* left = buffer.getWritePointer (0);
    float* right = numCh > 1 ? buffer.getWritePointer (1) : scratchR.data();
    const int maxChunk = numCh > 1 ? juce::jmax (1, n) : (int) scratchR.size();
    int done = 0;
    do
    {
        const int len = juce::jmin (n - done, maxChunk);
        engine.process (left + done, right + (numCh > 1 ? done : 0), len, eventBuffer.data(), done == 0 ? numEvents : 0, host, transport);
        if (numCh == 1)
            for (int i = 0; i < len; ++i) left[done + i] = 0.5f * (left[done + i] + right[i]);
        if (transport.playing) transport.ppq += (double) len / currentSampleRate * transport.bpm / 60.0;
        done += len;
    } while (done < n);
    for (int ch = 2; ch < numCh; ++ch) buffer.clear (ch, 0, n);
}

juce::AudioProcessorEditor* FmlumeAudioProcessor::createEditor() { return new FmlumeAudioProcessorEditor (*this); }

// ---------------------------------------------------------------------------------------- state
// Binary ValueTree holding the complete canonical model (same JSON as a .fmlume file) plus every
// host-owned value, so a project restores without any preset file on disk (SPEC §12.1).
void FmlumeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    std::string json, error;
    {
        std::lock_guard<std::recursive_mutex> lock (modelMutex);
        const HostParams host = currentHostParams();
        if (! stateToJson (controller.presetForSave (host), host, json, &error)) return;
    }
    juce::ValueTree tree ("FMLUME_STATE");
    tree.setProperty ("version", 1, nullptr);
    tree.setProperty ("json", juce::String (juce::CharPointer_UTF8 (json.c_str())), nullptr);
    juce::MemoryOutputStream out (destData, false);
    tree.writeToStream (out);
}

void FmlumeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0) return;
    // A refused blob leaves the plug-in exactly as it was, but the user is told (the editor
    // shows the reason in its status line) instead of silently getting another sound.
    const auto refuse = [this] (const juce::String& why) {
        std::lock_guard<std::recursive_mutex> lock (modelMutex);
        stateError = why;
        modelVersion.fetch_add (1);
    };
    const auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (! tree.isValid() || ! tree.hasType ("FMLUME_STATE")) { refuse ("the saved data is not FMLUME state"); return; }
    const std::string json = tree.getProperty ("json").toString().toStdString();

    // Parse and validate completely before touching anything: a bad blob leaves the plug-in as it was.
    Preset preset;
    HostParams host;
    std::string error;
    if (! stateFromJson (json, preset, host, &error)) { refuse (juce::String (juce::CharPointer_UTF8 (error.c_str()))); return; }

    // Apply as one transaction: audio is suspended (silence) while parameters and patch change together.
    suspendProcessing (true);
    {
        std::lock_guard<std::recursive_mutex> lock (modelMutex);
        setParam ("master.gainDb", host.gainDb);
        setParam ("master.pan", host.pan);
        setParam ("master.quality", (float) host.quality);
        setParam ("fx.bypassAll", host.fxBypassAll ? 1.0f : 0.0f);
        applyHostValues (host);
        controller.setState (preset);
    }
    suspendProcessing (false);
    modelVersion.fetch_add (1);
}

// ---------------------------------------------------------------------------------------- presets
juce::String FmlumeAudioProcessor::loadPresetIndex (int listIndex)
{
    Preset p;
    std::string error;
    if (! presets.load (listIndex, p, &error)) return error;
    std::lock_guard<std::recursive_mutex> lock (modelMutex);
    controller.loadPreset (p);
    return {};
}

juce::String FmlumeAudioProcessor::takeStateError()
{
    std::lock_guard<std::recursive_mutex> lock (modelMutex);
    const juce::String e = stateError;
    stateError.clear();
    return e;
}

int FmlumeAudioProcessor::currentPresetIndex()
{
    std::lock_guard<std::recursive_mutex> lock (modelMutex);
    return presets.findById (controller.preset().meta.patchId);
}

void FmlumeAudioProcessor::stepPreset (int delta)
{
    const int count = (int) presets.entries().size();
    if (count == 0) return;
    int idx = currentPresetIndex();
    idx = idx < 0 ? 0 : ((idx + delta) % count + count) % count;
    loadPresetIndex (idx);
}

juce::String FmlumeAudioProcessor::savePreset (const juce::String& name, bool overwrite)
{
    std::lock_guard<std::recursive_mutex> lock (modelMutex);
    Preset p = controller.presetForSave (currentHostParams());
    if (p.meta.author.empty() || p.meta.author == "FMLUME") p.meta.author = "User";
    std::string error;
    if (! presets.saveUser (p, name.toStdString(), overwrite, &error)) return error;
    // Adopt the saved identity (id / name) so the header shows the user preset and Dirty clears.
    for (auto& e : presets.entries())
        if (! e.factory && e.name == name.toStdString())
        {
            Preset saved;
            if (presets.load (presets.findById (e.id), saved)) controller.markSaved (saved);
            break;
        }
    return {};
}

juce::String FmlumeAudioProcessor::exportPreset (const juce::File& file)
{
    std::lock_guard<std::recursive_mutex> lock (modelMutex);
    std::string error;
    Preset p = controller.presetForSave (currentHostParams());
    if (p.meta.patchId.empty()) p.meta.patchId = makePatchId();
    return PresetManager::exportTo (file.getFullPathName().toStdString(), p, &error) ? juce::String() : juce::String (error);
}

juce::String FmlumeAudioProcessor::importPreset (const juce::File& file)
{
    Preset p;
    std::string error;
    if (! PresetManager::importFrom (file.getFullPathName().toStdString(), p, &error)) return error;
    std::lock_guard<std::recursive_mutex> lock (modelMutex);
    controller.loadPreset (p);
    return {};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FmlumeAudioProcessor(); }
