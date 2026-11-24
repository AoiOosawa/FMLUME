// fmlume-render: offline MIDI -> WAV renderer and preset utility (no host required).
#include "Core/MidiFile.h"
#include "Core/Templates.h"
#include "Core/WavFile.h"
#include "DSP/FM/SynthEngine.h"
#include "Presets/PresetManager.h"
#include "Presets/PresetSerializer.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace fmlume;

static int usage()
{
    std::printf (
        "fmlume-render - offline FMLUME renderer\n\n"
        "  --preset <file.fmlume>     load a preset (default: init patch)\n"
        "  --template <Category>      start from a category template (E.Piano, Bell, Bass, Lead, Pad, Pluck, Brass, Keys, SFX)\n"
        "  --midi <file.mid>          play a Standard MIDI File (all channels merged)\n"
        "  --note <n> [--note <n>..]  or hold these notes (default 69) ...\n"
        "  --vel <1-127> --dur <s>    ... with this velocity for this long (default 100, 2 s)\n"
        "  --tail <s>                 extra time after the last event (default 2 s)\n"
        "  --macro <1-8>=<0..1>       set a macro value (repeatable)\n"
        "  --quality <eco|standard|high>  --sr <hz>  --block <frames>  --bpm <bpm>  --gain <dB>\n"
        "  --arp / --gate             enable arpeggiator / trance gate\n"
        "  --no-arp                   switch the preset's arpeggiator and chord mode off (to hear the bare voice)\n"
        "  --out <file.wav>           write 32-bit float stereo WAV\n"
        "  --dump-preset <file>       write the loaded patch as .fmlume and exit\n"
        "  --validate <file.fmlume>   validate a preset file and exit (0 = valid)\n");
    return 2;
}

int main (int argc, char** argv)
{
    std::vector<int> notes;
    int vel = 100, sr = 48000, block = 256;
    double dur = 2.0, tail = 2.0, bpm = 0.0;
    std::string out, presetPath, midiPath, dumpPath, validatePath, templ;
    Preset preset = makeInitPreset();
    HostParams host;
    bool hostFromPreset = true, gainSet = false;
    std::vector<std::pair<int, float>> macros;
    bool arp = false, gate = false, noArp = false;

    for (int i = 1; i < argc; ++i)
    {
        auto arg = [&] (const char* name) { return std::strcmp (argv[i], name) == 0 && i + 1 < argc; };
        if (arg ("--note")) notes.push_back (std::atoi (argv[++i]));
        else if (arg ("--vel")) vel = std::atoi (argv[++i]);
        else if (arg ("--sr")) sr = std::atoi (argv[++i]);
        else if (arg ("--block")) block = std::atoi (argv[++i]);
        else if (arg ("--dur")) dur = std::atof (argv[++i]);
        else if (arg ("--tail")) tail = std::atof (argv[++i]);
        else if (arg ("--bpm")) bpm = std::atof (argv[++i]);
        else if (arg ("--gain")) { host.gainDb = (float) std::atof (argv[++i]); gainSet = true; }
        else if (arg ("--out")) out = argv[++i];
        else if (arg ("--preset")) presetPath = argv[++i];
        else if (arg ("--template")) templ = argv[++i];
        else if (arg ("--midi")) midiPath = argv[++i];
        else if (arg ("--dump-preset")) dumpPath = argv[++i];
        else if (arg ("--validate")) validatePath = argv[++i];
        else if (arg ("--quality"))
        {
            const std::string q = argv[++i];
            host.quality = q == "eco" ? 0 : (q == "high" ? 2 : 1);
        }
        else if (arg ("--macro"))
        {
            int idx = 0;
            float v = 0.5f;
            if (std::sscanf (argv[++i], "%d=%f", &idx, &v) != 2 || idx < 1 || idx > 8) return usage();
            macros.push_back ({ idx - 1, v });
        }
        else if (std::strcmp (argv[i], "--arp") == 0) arp = true;
        else if (std::strcmp (argv[i], "--gate") == 0) gate = true;
        else if (std::strcmp (argv[i], "--no-arp") == 0) noArp = true;
        else return usage();
    }

    std::string err;
    if (! validatePath.empty())
    {
        Preset p;
        std::vector<std::string> warnings;
        if (! presetFromJson ([&] { std::string t; FILE* f = std::fopen (validatePath.c_str(), "rb"); if (f) { char b[4096]; size_t n; while ((n = std::fread (b, 1, sizeof (b), f)) > 0) t.append (b, n); std::fclose (f); } return t; }(), p, &err, &warnings))
        {
            std::printf ("INVALID %s: %s\n", validatePath.c_str(), err.c_str());
            return 1;
        }
        for (auto& w : warnings) std::printf ("warning: %s\n", w.c_str());
        std::printf ("OK %s (%s / %s)\n", validatePath.c_str(), categoryName (p.meta.category), p.meta.displayName.c_str());
        return 0;
    }
    if (! templ.empty())
    {
        const int c = categoryFromName (templ);
        if (c < 0) { std::printf ("unknown category '%s'\n", templ.c_str()); return 2; }
        preset = makeCategoryPreset ((Category) c);
    }
    if (! presetPath.empty() && ! PresetManager::importFrom (presetPath, preset, &err))
    {
        std::printf ("cannot load preset: %s\n", err.c_str());
        return 1;
    }
    if (! dumpPath.empty())
    {
        if (! PresetManager::exportTo (dumpPath, preset, &err)) { std::printf ("%s\n", err.c_str()); return 1; }
        std::printf ("wrote %s\n", dumpPath.c_str());
        return 0;
    }
    if (out.empty()) return usage();

    if (hostFromPreset)
    {
        const HostParams keep = host;
        host = preset.data.host;
        host.quality = keep.quality;
        host.gainDb = gainSet ? keep.gainDb : 0.0f;
        host.pan = 0.0f;
        host.fxBypassAll = false;
    }
    for (auto& m : macros) host.macro[(size_t) m.first] = m.second;
    host.arpEnabled = host.arpEnabled || arp;
    host.gateEnabled = host.gateEnabled || gate;
    if (noArp) { host.arpEnabled = false; preset.data.perf.chord.type = ChordType::Off; }

    MidiSequence seq;
    if (! midiPath.empty())
    {
        if (! readMidiFile (midiPath, seq, &err)) { std::printf ("cannot read MIDI: %s\n", err.c_str()); return 1; }
    }
    else
    {
        if (notes.empty()) notes.push_back (69);
        for (int n : notes) { TimedMidiEvent e; e.event.type = MidiEvent::NoteOn; e.event.data1 = (uint8_t) n; e.event.data2 = (uint8_t) vel; seq.events.push_back (e); }
        for (int n : notes) { TimedMidiEvent e; e.seconds = dur; e.event.type = MidiEvent::NoteOff; e.event.data1 = (uint8_t) n; seq.events.push_back (e); }
        seq.lengthSeconds = dur;
    }
    if (bpm <= 0.0) bpm = seq.bpm;

    SynthEngine engine;
    engine.prepare (sr, block);
    if (! engine.setPatch (preset.data, true, &err)) { std::printf ("invalid patch: %s\n", err.c_str()); return 1; }
    TransportInfo tr;
    tr.valid = tr.playing = true;
    tr.bpm = bpm;

    const int total = (int) ((seq.lengthSeconds + tail) * sr);
    std::vector<float> L ((size_t) total), R ((size_t) total);
    std::vector<MidiEvent> evs;
    size_t next = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        evs.clear();
        while (next < seq.events.size() && (int) std::llround (seq.events[next].seconds * sr) < pos + n)
        {
            MidiEvent e = seq.events[next].event;
            e.offset = std::max (0, (int) std::llround (seq.events[next].seconds * sr) - pos);
            evs.push_back (e);
            ++next;
        }
        engine.process (L.data() + pos, R.data() + pos, n, evs.data(), (int) evs.size(), host, tr);
        tr.ppq += (double) n / sr * bpm / 60.0;
    }
    float peak = 0.0f;
    double sum = 0.0;
    for (int i = 0; i < total; ++i) { peak = std::max (peak, std::max (std::fabs (L[(size_t) i]), std::fabs (R[(size_t) i]))); sum += (double) L[(size_t) i] * L[(size_t) i]; }
    if (! writeWavFloat32 (out, L, R, sr)) { std::printf ("cannot write %s\n", out.c_str()); return 1; }
    std::printf ("wrote %s  (%s, %d frames @ %d Hz, latency %d, peak %.1f dBFS, rms %.1f dBFS)\n", out.c_str(), preset.meta.displayName.c_str(), total, sr,
                 engine.getLatencySamples(), 20.0 * std::log10 (std::max (peak, 1.0e-9f)), 10.0 * std::log10 (std::max (sum / std::max (total, 1), 1.0e-18)));
    return 0;
}
