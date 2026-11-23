#include "Core/MidiFile.h"
#include <algorithm>
#include <cstdio>

namespace fmlume
{
namespace
{
struct Raw { uint64_t tick; int order; MidiEvent ev; bool tempo; double usPerQuarter; };
}

bool parseMidiData (const std::vector<uint8_t>& b, MidiSequence& out, std::string* error)
{
    auto fail = [error] (const char* m) { if (error) *error = m; return false; };
    auto u32 = [&b] (size_t p) { return ((uint32_t) b[p] << 24) | ((uint32_t) b[p + 1] << 16) | ((uint32_t) b[p + 2] << 8) | b[p + 3]; };
    auto u16 = [&b] (size_t p) { return (uint32_t) ((b[p] << 8) | b[p + 1]); };
    if (b.size() < 14 || b[0] != 'M' || b[1] != 'T' || b[2] != 'h' || b[3] != 'd') return fail ("not a MIDI file");
    const uint32_t headerLen = u32 (4);
    const uint32_t numTracks = u16 (10), division = u16 (12);
    if (division == 0 || (division & 0x8000u)) return fail ("SMPTE time division is not supported");
    size_t pos = 8 + (size_t) headerLen;
    std::vector<Raw> raw;
    int order = 0;
    for (uint32_t t = 0; t < numTracks; ++t)
    {
        if (pos + 8 > b.size() || b[pos] != 'M' || b[pos + 1] != 'T' || b[pos + 2] != 'r' || b[pos + 3] != 'k') return fail ("bad track header");
        const size_t end = pos + 8 + (size_t) u32 (pos + 4);
        if (end > b.size()) return fail ("truncated track");
        pos += 8;
        uint64_t tick = 0;
        uint8_t running = 0;
        while (pos < end)
        {
            uint32_t delta = 0;
            for (int i = 0; i < 4 && pos < end; ++i) { const uint8_t c = b[pos++]; delta = (delta << 7) | (c & 0x7fu); if (! (c & 0x80u)) break; }
            tick += delta;
            if (pos >= end) break;
            uint8_t status = b[pos];
            if (status & 0x80u) ++pos; else status = running;
            if (status == 0xff)
            {
                if (pos + 1 > end) return fail ("truncated meta event");
                const uint8_t type = b[pos++];
                uint32_t len = 0;
                for (int i = 0; i < 4 && pos < end; ++i) { const uint8_t c = b[pos++]; len = (len << 7) | (c & 0x7fu); if (! (c & 0x80u)) break; }
                if (pos + len > end) return fail ("truncated meta event");
                if (type == 0x51 && len == 3)
                    raw.push_back ({ tick, order++, {}, true, (double) (((uint32_t) b[pos] << 16) | ((uint32_t) b[pos + 1] << 8) | b[pos + 2]) });
                pos += len;
                continue;
            }
            if (status == 0xf0 || status == 0xf7)
            {
                uint32_t len = 0;
                for (int i = 0; i < 4 && pos < end; ++i) { const uint8_t c = b[pos++]; len = (len << 7) | (c & 0x7fu); if (! (c & 0x80u)) break; }
                pos += len;
                continue;
            }
            if (! (status & 0x80u)) return fail ("corrupt event stream");
            running = status;
            const uint8_t kind = status & 0xf0u;
            const int dataBytes = (kind == 0xc0 || kind == 0xd0) ? 1 : 2;
            if (pos + (size_t) dataBytes > end) return fail ("truncated event");
            const uint8_t d1 = b[pos], d2 = dataBytes == 2 ? b[pos + 1] : 0;
            pos += (size_t) dataBytes;
            MidiEvent e;
            bool keep = true;
            switch (kind)
            {
                case 0x90: e.type = d2 > 0 ? MidiEvent::NoteOn : MidiEvent::NoteOff; e.data1 = d1; e.data2 = d2; break;
                case 0x80: e.type = MidiEvent::NoteOff; e.data1 = d1; break;
                case 0xb0:
                    if (d1 == 123) e.type = MidiEvent::AllNotesOff;
                    else if (d1 == 120) e.type = MidiEvent::AllSoundOff;
                    else { e.type = MidiEvent::Controller; e.data1 = d1; e.data2 = d2; }
                    break;
                case 0xd0: e.type = MidiEvent::ChannelPressure; e.data1 = d1; e.data2 = d1; break;
                case 0xe0: e.type = MidiEvent::PitchBend; e.bend = (float) ((int) (d1 | (d2 << 7)) - 8192) / 8192.0f; break;
                default: keep = false; break;
            }
            if (keep) raw.push_back ({ tick, order++, e, false, 0.0 });
        }
        pos = end;
    }
    std::stable_sort (raw.begin(), raw.end(), [] (const Raw& a, const Raw& c) { return a.tick != c.tick ? a.tick < c.tick : a.order < c.order; });
    out = MidiSequence {};
    double usPerQuarter = 500000.0, seconds = 0.0;
    uint64_t lastTick = 0;
    bool tempoSeen = false;
    for (auto& r : raw)
    {
        seconds += (double) (r.tick - lastTick) / (double) division * usPerQuarter * 1.0e-6;
        lastTick = r.tick;
        if (r.tempo)
        {
            usPerQuarter = r.usPerQuarter > 0.0 ? r.usPerQuarter : 500000.0;
            if (! tempoSeen) { out.bpm = 60.0e6 / usPerQuarter; tempoSeen = true; }
            continue;
        }
        out.events.push_back ({ seconds, r.ev });
    }
    out.lengthSeconds = seconds;
    return true;
}

bool readMidiFile (const std::string& path, MidiSequence& out, std::string* error)
{
    FILE* f = std::fopen (path.c_str(), "rb");
    if (f == nullptr) { if (error) *error = "cannot open " + path; return false; }
    std::vector<uint8_t> bytes;
    uint8_t buf[4096];
    size_t n = 0;
    while ((n = std::fread (buf, 1, sizeof (buf), f)) > 0) bytes.insert (bytes.end(), buf, buf + n);
    std::fclose (f);
    return parseMidiData (bytes, out, error);
}
} // namespace fmlume
