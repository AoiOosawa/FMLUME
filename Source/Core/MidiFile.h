// Minimal Standard MIDI File (format 0/1) reader for the offline renderer and tests.
#pragma once
#include "Core/Model.h"
#include <string>
#include <vector>

namespace fmlume
{
struct TimedMidiEvent
{
    double seconds = 0.0;
    MidiEvent event;
};

struct MidiSequence
{
    std::vector<TimedMidiEvent> events; // sorted by time
    double bpm = 120.0;                 // first tempo found
    double lengthSeconds = 0.0;
};

bool readMidiFile (const std::string& path, MidiSequence& out, std::string* error = nullptr);
bool parseMidiData (const std::vector<uint8_t>& bytes, MidiSequence& out, std::string* error = nullptr);
} // namespace fmlume
