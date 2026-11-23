// Display names for model enums, in enum order. Every combo box that edits one of these enums
// takes its items from here, and each table is checked against the enum's Count, so adding an
// enum value without a label (or the other way round) does not compile.
#pragma once
#include "Core/Model.h"
#include <juce_core/juce_core.h>
#include <iterator>

namespace ui::labels
{
#define FMLUME_LABELS(name, Enum, ...) \
    inline const char* const name[] = { __VA_ARGS__ }; \
    static_assert (std::size (name) == (size_t) fmlume::Enum::Count, #name " must label every " #Enum)

FMLUME_LABELS (waveform, Waveform, "Sine", "Triangle", "Saw", "Pulse");
FMLUME_LABELS (pitchMode, PitchMode, "Ratio (follows the key)", "Fixed Hz");
FMLUME_LABELS (filterType, FilterType, "Off", "Low-pass 12 dB", "Low-pass 24 dB", "High-pass 12 dB", "Band-pass 12 dB");
FMLUME_LABELS (lfoShape, LfoShape, "Sine", "Triangle", "Saw", "Square", "Random");
FMLUME_LABELS (curve, Curve, "Linear", "Exp", "S-Curve");
FMLUME_LABELS (polarity, Polarity, "Bipolar", "Unipolar");
FMLUME_LABELS (modSource, ModSource, "(none)", "LFO 1", "LFO 2", "Mod Env", "Velocity", "Key Track", "Mod Wheel", "Aftertouch",
               "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Macro 5", "Macro 6", "Macro 7", "Macro 8");
FMLUME_LABELS (morphMode, MorphMode, "Blend: Auto", "Blend: Smooth", "Blend: Layered");
FMLUME_LABELS (arpMode, ArpMode, "Up", "Down", "Up-Down", "Random", "As played");
FMLUME_LABELS (arpOctaveMode, ArpOctaveMode, "Across range", "Step up", "Step down", "Step up-down");
FMLUME_LABELS (chordType, ChordType, "Off", "Major", "Minor", "Sus2", "Sus4", "Dim", "Aug", "7", "m7", "maj7");
FMLUME_LABELS (voicing, Voicing, "Root position", "1st inversion", "2nd inversion");
FMLUME_LABELS (quality, Quality, "Eco (1x)", "Standard (2x)", "High (4x)");
#undef FMLUME_LABELS

template <size_t N>
juce::StringArray list (const char* const (&table)[N])
{
    juce::StringArray a;
    for (auto* s : table) a.add (s);
    return a;
}
} // namespace ui::labels
