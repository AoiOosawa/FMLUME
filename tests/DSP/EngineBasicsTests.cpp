#include "TestFramework.h"
#include "TestUtil.h"

using namespace fmtest;

FM_TEST ("dsp", "DSP-01 A4 sine fundamental is 440 Hz at 44.1/48/96 kHz")
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        SynthEngine eng;
        eng.prepare (sr, 256);
        auto out = renderSeq (eng, sr, 256, 1.5, { { 0.0, noteOn (69) } });
        CHECK (allFinite (out.l));
        CHECK_NEAR (zeroCrossHz (out.l, sr, (size_t) (0.3 * sr), (size_t) (1.4 * sr)), 440.0, 0.5);
        CHECK (peak (out.l) > 0.05f);
        CHECK (maxAbsDiff (out.l, out.r) < 1.0e-6);
    }
}

FM_TEST ("dsp", "DSP-07 note off / velocity 0 / all notes off leave no hanging voices")
{
    const double sr = 48000.0;
    SynthEngine eng;
    eng.prepare (sr, 128);
    auto out = renderSeq (eng, sr, 128, 4.0, { { 0.0, noteOn (60) }, { 0.0, noteOn (64) }, { 0.1, noteOn (67) },
                                                { 0.5, noteOff (60) }, { 0.5, noteOn (64, 0) }, { 0.6, MidiEvent { 0, MidiEvent::AllNotesOff } } });
    CHECK (peak (out.l, 0, (size_t) (0.4 * sr)) > 0.05f);
    CHECK (peak (out.l, (size_t) (3.0 * sr)) < 1.0e-4f);
    CHECK (eng.getActiveVoiceCount() == 0);
}

FM_TEST ("dsp", "DSP-11 zero-length blocks, empty MIDI and variable block sizes are safe")
{
    const double sr = 48000.0;
    SynthEngine eng;
    eng.prepare (sr, 2048);
    HostParams host;
    TransportInfo tr;
    std::vector<float> l (2048), r (2048);
    eng.process (l.data(), r.data(), 0, nullptr, 0, host, tr);
    MidiEvent on = noteOn (72);
    eng.process (l.data(), r.data(), 0, &on, 1, host, tr); // event in an empty block must not be lost or crash
    for (int block : { 32, 1, 2048, 64, 0, 333, 512, 7 })
    {
        eng.process (l.data(), r.data(), block, nullptr, 0, host, tr);
        for (int i = 0; i < block; ++i) CHECK (std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]));
    }
    CHECK (eng.getActiveVoiceCount() >= 1);
}

FM_TEST ("dsp", "Offline render is deterministic")
{
    const double sr = 48000.0;
    std::vector<NoteEvent> seq { { 0.0, noteOn (57, 90) }, { 0.2, noteOn (64, 110) }, { 0.7, noteOff (57) }, { 0.9, noteOff (64) } };
    SynthEngine a, b;
    a.prepare (sr, 256);
    b.prepare (sr, 256);
    auto ra = renderSeq (a, sr, 256, 1.5, seq), rb = renderSeq (b, sr, 256, 1.5, seq);
    CHECK (maxAbsDiff (ra.l, rb.l) == 0.0);
    CHECK (maxAbsDiff (ra.r, rb.r) == 0.0);
}
