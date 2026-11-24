// FMLUME canonical model (SPEC §3, §4, §12).
// Everything the audio engine needs is trivially copyable ("PatchData") so that a
// complete patch can be handed to the audio thread without heap allocation.
// Strings (names, tags, ...) live in PresetMeta and never reach the audio thread.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fmlume
{
constexpr int kNumOps = 6;
constexpr int kMaxEdges = 30;
constexpr int kNumMacros = 8;
constexpr int kMaxMacroMappings = 8;
constexpr int kNumModSlots = 8;
constexpr int kNumFx = 9;
constexpr int kFxParams = 8;
constexpr int kNumQuick = 4;
constexpr int kGateSteps = 16;
constexpr int kMaxPolyphony = 32;
constexpr int kMaxUnison = 8;
constexpr int kMaxSubVoices = 64;
constexpr int kSchemaVersion = 2;

enum class Waveform : uint8_t { Sine, Triangle, Saw, Pulse, Count };
enum class PitchMode : uint8_t { Ratio, Fixed, Count };
enum class FilterType : uint8_t { Off, LP12, LP24, HP12, BP12, Count };
enum class LfoShape : uint8_t { Sine, Triangle, Saw, Square, RandomSmooth, Count };
enum class Curve : uint8_t { Linear, Exp, SCurve, Count };
enum class Polarity : uint8_t { Bipolar, Unipolar, Count };
enum class ModSource : uint8_t { None, LFO1, LFO2, ModEnv, Velocity, Keytrack, ModWheel, Aftertouch,
                                 Macro1, Macro2, Macro3, Macro4, Macro5, Macro6, Macro7, Macro8, Count };
enum class FxType : uint8_t { EQ, Drive, Chorus, Phaser, Flanger, Compressor, Bitcrusher, Delay, Reverb, Count };
enum class ArpMode : uint8_t { Up, Down, UpDown, Random, AsPlayed, Count };
// How the arpeggiator walks through its octaves. Range: the note order runs across the whole
// range (C E G C' E' G' ...). The other three play the note order inside one octave, then move
// a whole octave up / down / up-and-down (with Up-Down notes: C E G E | C' E' G' E' | ...).
enum class ArpOctaveMode : uint8_t { Range, Up, Down, UpDown, Count };
enum class ChordType : uint8_t { Off, Major, Minor, Sus2, Sus4, Dim, Aug, Dom7, Min7, Maj7, Count };
enum class Voicing : uint8_t { Root, First, Second, Count };
// Order = order in the browser. Files store the name, not the number.
enum class Category : uint8_t { Arp, EPiano, Bell, Bass, Lead, Pad, Poly, Pluck, Brass, Keys, SFX, Count };
enum class Quality : uint8_t { Eco, Standard, High, Count };

// Tempo divisions: index = base * 3 + modifier, base 0..6 = 1/1,1/2,1/4,1/8,1/16,1/32,1/64,
// modifier 0 = straight, 1 = dotted, 2 = triplet.
constexpr int kNumSyncDivs = 21;
// Divisions offered for rhythm: 1/4 .. 1/32T for the arpeggiator, up to 1/64 for the trance gate.
constexpr int kRateFirst = 6, kArpRateLast = 17, kGateRateLast = 18;
double syncDivBeats (int div);          // length in quarter-note beats
const char* syncDivName (int div);      // "1/16", "1/8.", "1/8T"
int syncDivFromName (const std::string& s); // -1 if unknown

struct ADSR
{
    float a = 0.01f, d = 0.5f, s = 0.6f, r = 0.3f;
};

struct OperatorState
{
    bool enabled = false;
    Waveform waveform = Waveform::Sine;
    PitchMode pitchMode = PitchMode::Ratio;
    float ratio = 1.0f;
    float fixedHz = 440.0f;
    float detuneCents = 0.0f;
    float level = 1.0f;
    float velocityToLevel = 0.5f;
    float velocityToIndex = 0.3f;
    float keyTracking = 1.0f;
    ADSR env;
    float phaseOffset = 0.0f;
    bool keySync = true;
    float feedback = 0.0f;
    float carrierGain = 0.0f;
};

struct Edge
{
    int8_t from = 0, to = 0;
    float index = 0.0f; // radians
};

struct FilterState
{
    FilterType type = FilterType::Off;
    float cutoffHz = 20000.0f;
    float resonance = 0.0f;
    float drive = 0.0f;
};

struct LFOState
{
    LfoShape shape = LfoShape::Sine;
    bool sync = false;
    float rateHz = 2.0f;
    uint8_t syncDiv = 9; // 1/8
    float phase = 0.0f;  // 0..1
    bool retrig = true;
};

struct EnginePatch
{
    std::array<OperatorState, kNumOps> operators {};
    std::array<Edge, kMaxEdges> edges {};
    int numEdges = 0;
    FilterState filter;
    std::array<LFOState, 2> lfo {};
    ADSR modEnv;
    float pitchBendRange = 2.0f;
};

struct MacroMapping
{
    int16_t target = -1;  // parameter code (Params.h)
    float amount = 0.0f;  // -1..1, normalised-domain delta at full throw
    float min = 0.0f, max = 1.0f; // normalised clamp window for the result
    Curve curve = Curve::Linear;
    Polarity polarity = Polarity::Bipolar;
};

struct Macro
{
    float defaultValue = 0.5f;
    int numMappings = 0;
    std::array<MacroMapping, kMaxMacroMappings> mappings {};
};

struct ModSlot
{
    ModSource source = ModSource::None;
    int16_t target = -1;
    float depth = 0.0f; // -1..1
    Curve curve = Curve::Linear;
    bool enabled = false;
};

// The sound itself: engine + macro assignments + mod matrix (SPEC §12.1).
struct SoundPatch
{
    EnginePatch engine;
    std::array<Macro, kNumMacros> macros {};
    std::array<ModSlot, kNumModSlots> matrix {};
};

struct ArpState
{
    ArpMode mode = ArpMode::Up;
    uint8_t rate = 12; // 1/16
    float gate = 0.8f; // 0.05..1
    int octaves = 1;   // 1..4
    float swing = 0.0f; // 0..0.6
    bool resetOnStop = true;
    ArpOctaveMode octaveMode = ArpOctaveMode::Range;
};

struct ChordState
{
    ChordType type = ChordType::Off;
    Voicing voicing = Voicing::Root;
    int octave = 0;       // -2..2
    float strumMs = 0.0f; // 0..100
};

struct TranceGateState
{
    std::array<float, kGateSteps> steps { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    uint8_t rate = 12;      // 1/16 (6..18 = 1/4 .. 1/64)
    float swing = 0.0f;     // 0..0.6
    float smoothMs = 5.0f;  // 0..30
    float mix = 1.0f;
    // Stereo gate (docs/DECISIONS.md D-030): `steps` drives the left channel and `stepsRight`
    // the right one. With `stereo` off both channels follow `steps`.
    bool stereo = false;
    std::array<float, kGateSteps> stepsRight { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    float width = 1.0f;     // 0..1 separation of the two channels when stereo
};

struct PerformanceState
{
    bool mono = false;
    bool legato = false;
    float portamento = 0.0f; // 0..2 s
    ArpState arp;
    ChordState chord;
    TranceGateState gate;
};

struct FxProcessorState
{
    bool enabled = false;
    std::array<float, kFxParams> p {}; // physical units, see fxParamDesc()
};

struct FxState
{
    std::array<uint8_t, kNumFx> order { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    std::array<FxProcessorState, kNumFx> proc {};
    std::array<Macro, kNumQuick> quick {}; // Warmth / Width / Space / Echo -> fx params
};

struct OutputState
{
    float levelDb = 0.0f; // per-preset trim, -24..+12 dB
};

// Values whose live truth is the host (APVTS). A preset stores the values it wants applied
// when it is loaded; master gain / pan / quality / fx.bypassAll are deliberately not part of
// a preset (see docs/DECISIONS.md D-010) but are part of the project state.
struct HostParams
{
    float gainDb = 0.0f;   // -60..+12 dB (same range and default as POLYBRISK's output gain)
    float pan = 0.0f;
    int polyphony = 16;
    int unison = 1;
    float detune = 10.0f;
    float spread = 50.0f;
    int quality = 1;
    std::array<float, kNumMacros> macro { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    bool fxBypassAll = false;
    std::array<float, kNumQuick> quick { 0.5f, 0.5f, 0.5f, 0.5f };
    bool arpEnabled = false;
    bool gateEnabled = false;
};

struct PatchData
{
    SoundPatch sound;
    PerformanceState perf;
    FxState fx;
    OutputState out;
    // Preset-stored host values (polyphony, unison, detune, spread, quick, arp/gate enable).
    HostParams host;
};

struct PresetMeta
{
    std::string pluginVersion;
    std::string patchId;
    std::string displayName = "Init";
    std::string author;
    Category category = Category::Keys;
    std::vector<std::string> tags;
    std::string description;
    std::string createdAt, modifiedAt;
    bool favorite = false;
    std::array<std::string, kNumMacros> macroNames;
    std::string metadataJson = "{}"; // unknown extra metadata, preserved verbatim
};

struct Preset
{
    PresetMeta meta;
    PatchData data;
};

struct TransportInfo
{
    bool valid = false;   // host supplied tempo/position
    bool playing = false;
    double bpm = 120.0;
    double ppq = 0.0;     // quarter notes at block start
};

struct MidiEvent
{
    enum Type : uint8_t { NoteOn, NoteOff, PitchBend, Controller, ChannelPressure, AllNotesOff, AllSoundOff };
    int offset = 0;  // sample offset in block
    Type type = NoteOn;
    uint8_t data1 = 0; // note / cc number
    uint8_t data2 = 0; // velocity / cc value / pressure
    float bend = 0.0f; // -1..1 for PitchBend
};

const char* categoryName (Category c);
int categoryFromName (const std::string& s); // -1 if unknown
const char* fxTypeName (FxType t);            // "eq", "drive", ...
int fxTypeFromName (const std::string& s);

// Factory defaults.
EnginePatch makeInitEngine();   // SPEC §4.3 initial tone
PatchData makeInitPatchData();
Preset makeInitPreset();
} // namespace fmlume
