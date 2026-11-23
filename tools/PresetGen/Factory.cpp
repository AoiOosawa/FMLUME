// FMLUME factory bank: 9 categories x 12 presets. All sounds are original FM designs built from the
// category templates in Core/Templates.cpp; no third-party patches or samples are used.
#include "Builder.h"

namespace gen
{
using F = FxType;
using W = Waveform;
using S = ModSource;
using C = Category;
using FT = FilterType;

std::vector<Preset> buildFactory()
{
    std::vector<Preset> out;
    auto add = [&out] (B& b) { out.push_back (b.p); };

    // ============================================================================ Arpeggios
    // The Arp category: every preset here loads with the arpeggiator running. The voices are
    // built from the other categories' templates and then filed under Arp.
    // Rates: 9 = 1/8, 11 = 1/8T, 12 = 1/16, 14 = 1/16T, 15 = 1/32. "One key" presets add Chord mode,
    // so a single key plays a complete arpeggio.
    auto addArp = [&out] (B& b) { b.p.meta.category = C::Arp; out.push_back (b.p); };
    using A = ArpMode;
    using CH = ChordType;

    // ---- Stair arpeggios, modelled on a user-supplied reference: the chord goes up and back down
    // inside one octave (C E G E), then the whole figure steps to the next octave.
    using AO = ArpOctaveMode;
    { B b (C::Arp); b.name ("Triad Stairs Arp").tags ({ "Arp", "Bright", "Punchy" }).desc ("Bright 16th-note stairs: the chord climbs and falls inside each octave, three octaves up.", "hold a triad; velocity sets the bite.");
      b.clear().op (1, 1.0f, 0.7f, 0.001f, 0.3f, 0.25f, 0.15f)
       .op (2, 1.0f, 1.0f, 0.001f, 0.45f, 0.25f, 0.18f, 0.6f)
       .op (3, 2.0f, 1.0f, 0.001f, 0.4f, 0.2f, 0.18f, 0.9f).det (3, 4.0f)
       .edge (1, 2, 1.7f).edge (1, 3, 1.9f)
       .filter (FT::LP12, 12000.0f, 0.1f)
       .mod (0, S::LFO1, pc::pan, 0.0f).lfo (1, 0.4f)
       .m (5, { { E (1, 2), 0.25f }, { E (1, 3), 0.25f } }).m (6, { { O (3, kOpCarrierGain), -0.4f }, { O (2, kOpCarrierGain), 0.3f } })
       .m (7, { { pc::filterCutoff, -0.3f }, { O (2, kOpEnvD), -0.15f }, { O (3, kOpEnvD), -0.15f } }).m (8, { { FX (F::Reverb, 4), 0.4f }, { FX (F::Delay, 5), 0.3f } })
       .poly (8).arp (A::UpDown, 12, 0.7f, 3).arpOct (AO::Up).delaySync (0.14f, 10, 0.3f).reverb (0.16f, 1.8f); addArp (b); }
    { B b (C::Arp); b.name ("Soft Stairs Arp").tags ({ "Arp", "Soft", "Warm" }).desc ("Pure, rounded eighth notes that ring into each other; the figure starts in the upper octave and steps down.", "hold a triad and let the notes overlap.");
      b.clear().op (1, 1.0f, 0.7f, 0.004f, 1.5f, 0.0f, 1.2f)
       .op (2, 1.0f, 1.0f, 0.006f, 4.0f, 0.0f, 4.0f, 0.8f)
       .op (3, 1.0f, 1.0f, 0.006f, 4.0f, 0.0f, 4.0f, 0.5f).det (3, 2.5f)   // second carrier: gives Character (detune) something to act on
       .edge (1, 2, 0.4f)
       .filter (FT::LP12, 9000.0f, 0.0f)
       .mod (0, S::LFO1, pc::pan, 0.0f).lfo (1, 0.3f).mod (1, S::LFO2, pc::pitch, 0.0f).lfo (2, 5.0f)
       .m (5, { { E (1, 2), 0.35f } }).m (6, { { O (2, kOpEnvA), 0.25f } })
       .m (7, { { pc::filterCutoff, -0.35f }, { O (2, kOpEnvD), -0.2f }, { O (2, kOpEnvR), -0.2f }, { O (3, kOpEnvD), -0.2f }, { O (3, kOpEnvR), -0.2f } }).m (8, { { FX (F::Reverb, 4), 0.4f } }).mu (8, { { DEPTH (0), 0.4f } })
       .poly (16).arp (A::UpDown, 9, 1.0f, 2).arpOct (AO::Down).chorus (0.15f).reverb (0.2f, 2.6f); addArp (b); }
    { B b (C::Arp); b.name ("Neon Stairs Arp").tags ({ "Arp", "Bright", "Modern" }).desc ("Buzzing digital eighths with a dotted echo; the tone slowly opens and closes over each bar.", "hold a triad; ride Brightness (Macro 1) for the build.");
      b.clear().op (1, 1.0f, 0.85f, 0.001f, 0.5f, 0.5f, 0.2f).fb (1, 0.7f)
       .op (2, 1.0f, 1.0f, 0.001f, 0.5f, 0.45f, 0.25f, 1.0f)
       .op (3, 4.0f, 0.5f, 0.001f, 0.18f, 0.0f, 0.12f)
       .op (4, 2.0f, 1.0f, 0.001f, 0.4f, 0.3f, 0.25f, 0.35f).det (4, 6.0f)
       .edge (1, 2, 2.0f).edge (3, 2, 0.8f).edge (1, 4, 1.2f)
       .mod (0, S::LFO1, E (1, 2), 0.14f).lfoSync (1, 0, LfoShape::Triangle).mod (1, S::LFO2, pc::pan, 0.0f).lfo (2, 0.5f)
       .m (5, { { E (3, 2), 0.3f }, { O (1, kOpFeedback), 0.2f } }).m (6, { { O (4, kOpCarrierGain), 0.4f } })
       .m (7, { { O (2, kOpEnvD), -0.2f }, { O (4, kOpEnvD), -0.2f }, { O (2, kOpEnvS), -0.3f } }).m (8, { { FX (F::Delay, 5), 0.4f }, { FX (F::Reverb, 4), 0.3f } })
       .poly (8).arp (A::UpDown, 9, 0.8f, 2).arpOct (AO::Down).delaySync (0.3f, 10, 0.5f, 0.8f).reverb (0.18f, 2.2f); addArp (b); }

    // ---- Pluck: the classic arpeggio voices
    { B b (C::Pluck); b.name ("Starlight Arp").tags ({ "Arp", "Bright", "Modern" }).desc ("Sparkling 16th-note arpeggio over two octaves with a dotted echo.", "hold a chord; velocity sets brightness.");
      b.poly (8).arp (A::Up, 12, 0.5f, 2).env (2, 0.001f, 0.45f, 0.0f, 0.25f).edge (1, 2, 2.2f).delaySync (0.3f, 10, 0.45f).reverb (0.2f, 2.2f); addArp (b); }
    { B b (C::Pluck); b.name ("Cascade Arp").tags ({ "Arp", "Bright", "Cinematic" }).desc ("Falling triplet arpeggio across three octaves, like water over glass.", "hold three or four notes.");
      b.poly (8).arp (A::Down, 14, 0.6f, 3).ratio (1, 4.0f).edge (1, 2, 1.6f).env (2, 0.001f, 0.6f, 0.0f, 0.4f).delaySync (0.26f, 9, 0.5f).reverb (0.3f, 3.2f, 0.7f); addArp (b); }
    { B b (C::Pluck); b.name ("Pendulum Arp").tags ({ "Arp", "Warm", "Soft" }).desc ("Up-and-down 16ths with a light swing; mellow and round.", "even velocity; the swing does the groove.");
      b.poly (8).arp (A::UpDown, 12, 0.55f, 2).swing (0.22f).ratio (1, 2.0f).edge (1, 2, 1.5f).filter (FT::LP12, 5200.0f, 0.2f).env (2, 0.001f, 0.55f, 0.0f, 0.3f).chorus (0.25f).delaySync (0.2f, 9, 0.35f).reverb (0.18f); addArp (b); }
    { B b (C::Pluck); b.name ("Dice Arp").tags ({ "Arp", "Modern", "Punchy" }).desc ("Random-order 16ths: a generative pattern from any held chord.", "hold four or more notes for the most variety.");
      b.poly (8).arp (A::Random, 12, 0.45f, 2).ratio (1, 5.0f).edge (1, 2, 2.0f).env (1, 0.001f, 0.08f, 0.0f, 0.06f).env (2, 0.001f, 0.35f, 0.0f, 0.2f).delaySync (0.3f, 10, 0.5f, 0.9f).reverb (0.16f); addArp (b); }
    { B b (C::Pluck); b.name ("One Key Minor 7").tags ({ "Arp", "Modern", "Warm" }).desc ("One key plays a running minor-seventh arpeggio.", "play single keys; change key to change the harmony.");
      b.poly (8).chord (CH::Min7).arp (A::Up, 12, 0.5f, 2).edge (1, 2, 2.0f).env (2, 0.001f, 0.5f, 0.0f, 0.3f).delaySync (0.28f, 10, 0.45f).reverb (0.22f, 2.4f); addArp (b); }
    { B b (C::Pluck); b.name ("One Key Major 7").tags ({ "Arp", "Bright", "Soft" }).desc ("One key plays a dreamy major-seventh triplet arpeggio.", "single keys, lightly.");
      b.poly (8).chord (CH::Maj7).arp (A::UpDown, 11, 0.7f, 2).ratio (1, 2.0f).edge (1, 2, 1.4f).env (2, 0.001f, 0.9f, 0.0f, 0.5f).chorus (0.3f).delaySync (0.24f, 9, 0.4f).reverb (0.3f, 3.0f); addArp (b); }
    { B b (C::Pluck); b.name ("One Key Sus Runner").tags ({ "Arp", "Bright", "Punchy" }).desc ("One key plays a driving sus4 arpeggio in 16ths over three octaves.", "single keys; great over a moving bass note.");
      b.poly (8).chord (CH::Sus4).arp (A::Up, 12, 0.4f, 3).ratio (1, 3.0f).edge (1, 2, 2.8f).env (2, 0.001f, 0.3f, 0.0f, 0.18f).delaySync (0.22f, 10, 0.4f).reverb (0.14f); addArp (b); }

    // ---- Bell
    { B b (C::Bell); b.name ("Glass Rain Arp").tags ({ "Arp", "Bright", "Cinematic" }).desc ("Random bell drops over three octaves in a long hall.", "hold a chord and let it rain.");
      b.poly (12).arp (A::Random, 12, 0.8f, 3).env (3, 0.001f, 1.4f, 0.0f, 1.0f).env (5, 0.001f, 1.6f, 0.0f, 1.2f).edge (1, 3, 1.6f).delaySync (0.25f, 10, 0.5f, 0.9f).reverb (0.4f, 4.5f, 0.8f); addArp (b); }
    { B b (C::Bell); b.name ("Chime Ladder Arp").tags ({ "Arp", "Metallic", "Bright" }).desc ("Chimes climbing in eighth-note triplets across three octaves.", "hold two to four notes.");
      b.poly (12).arp (A::Up, 11, 0.9f, 3).ratio (1, 5.19f).edge (1, 3, 1.5f).edge (2, 3, 0.5f).env (3, 0.001f, 1.8f, 0.0f, 1.2f).env (5, 0.001f, 1.5f, 0.0f, 1.0f).delaySync (0.2f, 9, 0.4f).reverb (0.32f, 3.5f); addArp (b); }
    { B b (C::Bell); b.name ("Music Box Loop").tags ({ "Arp", "Soft", "Retro" }).desc ("Plays your notes back in the order you pressed them, like a music box cylinder.", "press the notes one by one, then hold.");
      b.poly (8).arp (A::AsPlayed, 9, 0.9f, 1).ratio (1, 4.0f).ratio (2, 8.0f).edge (1, 3, 1.1f).edge (2, 3, 0.4f).env (3, 0.001f, 1.2f, 0.0f, 0.7f).out (5, 0.25f).ratio (5, 2.0f).env (5, 0.001f, 0.8f, 0.0f, 0.5f).reverb (0.24f, 2.0f, 0.4f); addArp (b); }
    { B b (C::Bell); b.name ("One Key Bell Minor").tags ({ "Arp", "Metallic", "Cinematic" }).desc ("One key plays a slow minor bell arpeggio with long tails.", "single keys, sparsely.");
      b.poly (12).chord (CH::Minor).arp (A::UpDown, 9, 1.0f, 2).env (3, 0.001f, 2.4f, 0.0f, 1.8f).env (5, 0.001f, 2.8f, 0.0f, 2.0f).delaySync (0.18f, 10, 0.4f).reverb (0.36f, 4.0f, 0.75f); addArp (b); }

    // ---- Bass
    { B b (C::Bass); b.name ("Sixteen Bass Arp").tags ({ "Arp", "Punchy", "Modern" }).desc ("Tight 16th-note bass line from held notes.", "hold a root and a fifth or octave.");
      b.poly (4).arp (A::Up, 12, 0.45f, 1).env (3, 0.001f, 0.25f, 0.3f, 0.08f).edge (2, 3, 2.6f); addArp (b); }
    { B b (C::Bass); b.name ("Octave Bass Arp").tags ({ "Arp", "Retro", "Punchy" }).desc ("The classic octave-jump disco bass in eighths.", "hold one note; it alternates with the octave above.");
      b.poly (4).arp (A::Up, 9, 0.5f, 2).env (3, 0.001f, 0.3f, 0.5f, 0.1f).edge (2, 3, 2.0f).out (4, 0.3f); addArp (b); }
    { B b (C::Bass); b.name ("Acid Steps Arp").tags ({ "Arp", "Retro", "Modern" }).desc ("Squelchy resonant sequence that follows the order you play.", "press notes one by one; Brightness opens the squelch.");
      b.poly (4).arp (A::AsPlayed, 12, 0.35f, 1).swing (0.15f).fb (1, 0.9f).edge (1, 2, 2.4f).edge (2, 3, 3.0f).filter (FT::LP24, 1400.0f, 0.75f, 0.3f).mod (2, S::ModEnv, pc::filterCutoff, 0.35f).modEnv (0.001f, 0.16f, 0.0f, 0.1f)
       .env (3, 0.001f, 0.22f, 0.2f, 0.07f).drive (0.3f).delaySync (0.14f, 10, 0.35f); addArp (b); }
    { B b (C::Bass); b.name ("One Key Bass Riff").tags ({ "Arp", "Punchy", "Warm" }).desc ("One key plays a rolling minor bass riff.", "single low keys.");
      b.poly (4).chord (CH::Minor).arp (A::UpDown, 12, 0.5f, 1).env (3, 0.001f, 0.3f, 0.4f, 0.1f).edge (2, 3, 1.8f).out (4, 0.25f); addArp (b); }

    // ---- Lead
    { B b (C::Lead); b.name ("Laser Run Arp").tags ({ "Arp", "Bright", "Modern" }).desc ("Very fast 32nd-note runs: the 'laser harp' arpeggio.", "hold a chord; works best in the upper half of the keyboard.");
      b.poly (8).arp (A::Up, 15, 0.6f, 2).env (2, 0.001f, 0.2f, 0.3f, 0.08f).env (4, 0.001f, 0.2f, 0.3f, 0.08f).edge (1, 2, 1.8f).delaySync (0.22f, 9, 0.4f).reverb (0.16f); addArp (b); }
    { B b (C::Lead); b.name ("Trance Gate Arp").tags ({ "Arp", "Bright", "Modern" }).desc ("Up-down trance arpeggio over three octaves with a dotted-eighth echo.", "hold a minor chord and ride the Brightness macro.");
      b.poly (8).arp (A::UpDown, 12, 0.5f, 3).env (2, 0.001f, 0.3f, 0.25f, 0.14f).env (4, 0.001f, 0.3f, 0.25f, 0.14f).edge (1, 2, 2.6f).uni (2, 14.0f, 70.0f).delaySync (0.32f, 10, 0.5f).reverb (0.22f, 2.6f); addArp (b); }
    { B b (C::Lead); b.name ("Chip Hero Arp").tags ({ "Arp", "Retro", "Punchy" }).desc ("Video-game chord shimmer: one key becomes a blazing major arpeggio.", "single keys; try Minor in Rhythm > Chord for the villain.");
      b.clear().op (1, 1.0f, 1.0f, 0.001f, 0.3f, 0.8f, 0.05f, 1.0f).wave (1, W::Pulse).op (2, 2.0f, 0.5f, 0.001f, 0.2f, 0.5f, 0.05f, 0.25f).wave (2, W::Pulse)
       .op (3, 1.0f, 0.4f, 0.001f, 0.12f, 0.0f, 0.05f).edge (3, 1, 0.8f)
       .mod (0, S::LFO1, pc::pitch, 0.0f).lfo (1, 6.0f).mod (1, S::LFO2, pc::pan, 0.0f).lfo (2, 0.8f)
       .m (5, { { E (3, 1), 0.3f } }).m (6, { { O (2, kOpCarrierGain), 0.4f } }).mu (7, { { DEPTH (0), 0.006f } }).mu (8, { { DEPTH (1), 0.5f } })
       .mname (5, "Grit").mname (6, "Octave").mname (7, "Vibrato").mname (8, "Auto Pan")
       .poly (8).chord (CH::Major).arp (A::Up, 15, 0.8f, 2).delaySync (0.12f, 9, 0.3f); addArp (b); }
    { B b (C::Lead); b.name ("One Key Hook").tags ({ "Arp", "Modern", "Bright" }).desc ("One key plays a pop hook: a seventh-chord arpeggio with swing.", "single keys in the middle of the keyboard.");
      b.poly (8).chord (CH::Dom7).arp (A::Up, 12, 0.5f, 2).swing (0.25f).env (2, 0.001f, 0.35f, 0.2f, 0.15f).env (4, 0.001f, 0.35f, 0.2f, 0.15f).edge (1, 2, 1.6f).fb (1, 0.2f).delaySync (0.24f, 9, 0.4f).reverb (0.18f); addArp (b); }

    // ---- Keys / E.Piano
    { B b (C::EPiano); b.name ("Tine Waves Arp").tags ({ "Arp", "Warm", "Soft" }).desc ("Electric-piano arpeggio rolling up and down in eighths.", "hold wide chords with both hands.");
      b.poly (12).arp (A::UpDown, 9, 0.95f, 2).chorus (0.35f).delaySync (0.18f, 10, 0.35f).reverb (0.22f, 2.4f); addArp (b); }
    { B b (C::EPiano); b.name ("One Key Dream Keys").tags ({ "Arp", "Warm", "Cinematic" }).desc ("One key plays a slow minor-seventh electric-piano arpeggio.", "single keys; let each harmony ring.");
      b.poly (12).chord (CH::Min7).arp (A::Up, 9, 1.0f, 2).env (2, 0.002f, 2.6f, 0.0f, 0.8f).env (4, 0.002f, 3.0f, 0.0f, 0.9f).chorus (0.3f).delaySync (0.22f, 10, 0.4f).reverb (0.3f, 3.2f); addArp (b); }
    { B b (C::Keys); b.name ("Clav Groove Arp").tags ({ "Arp", "Punchy", "Retro" }).desc ("Funky swung 16ths that follow the order of your fingers.", "press notes one by one, then hold; play with the swing.");
      b.poly (8).arp (A::AsPlayed, 12, 0.4f, 1).swing (0.35f).ratio (1, 3.0f).edge (1, 2, 2.4f).env (1, 0.001f, 0.2f, 0.0f, 0.1f).env (2, 0.001f, 0.4f, 0.0f, 0.12f).env (4, 0.001f, 0.3f, 0.0f, 0.12f).phaser (0.25f, 0.5f); addArp (b); }
    { B b (C::Keys); b.name ("Organ Pulse Arp").tags ({ "Arp", "Retro", "Warm" }).desc ("Pulsing organ eighths, the minimal-music arpeggio.", "hold big chords and change one note at a time.");
      b.poly (12).arp (A::Up, 9, 0.6f, 1).env (2, 0.003f, 0.4f, 0.8f, 0.08f).env (4, 0.003f, 0.4f, 0.7f, 0.08f).edge (1, 2, 0.9f).chorus (0.3f).reverb (0.2f, 2.0f); addArp (b); }

    // ---- Pad / Brass / SFX
    { B b (C::Pad); b.name ("Ripple Pad Arp").tags ({ "Arp", "Soft", "Cinematic" }).desc ("Soft overlapping notes rippling upward: an arpeggio that behaves like a pad.", "hold chords for a long time.");
      b.poly (16).arp (A::Up, 9, 1.0f, 2).env (1, 0.02f, 1.2f, 0.4f, 1.2f).env (2, 0.02f, 1.4f, 0.5f, 1.4f).env (3, 0.03f, 1.4f, 0.4f, 1.4f).env (4, 0.02f, 1.4f, 0.5f, 1.4f).delaySync (0.22f, 10, 0.45f).reverb (0.38f, 4.5f, 0.8f); addArp (b); }
    { B b (C::Pad); b.name ("Gate Dreams Arp").tags ({ "Arp", "Modern", "Cinematic" }).desc ("Slow arpeggio chopped by the trance gate: two rhythms at once.", "hold a chord; switch the gate pattern in Rhythm for new grooves.");
      b.poly (16).arp (A::UpDown, 9, 1.0f, 2).env (1, 0.01f, 1.0f, 0.6f, 0.8f).env (2, 0.01f, 1.0f, 0.8f, 0.8f).env (3, 0.01f, 1.0f, 0.5f, 0.8f).env (4, 0.01f, 1.0f, 0.8f, 0.8f)
       .gate (12, { 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1 }).delaySync (0.25f, 10, 0.45f).reverb (0.3f, 3.5f); addArp (b); }
    { B b (C::Brass); b.name ("Brass Stabs Arp").tags ({ "Arp", "Punchy", "Retro" }).desc ("One key fires descending major brass stabs in eighths.", "single keys, played hard.");
      b.poly (8).chord (CH::Major).arp (A::Down, 9, 0.4f, 1).env (2, 0.01f, 0.25f, 0.5f, 0.1f).env (4, 0.01f, 0.25f, 0.4f, 0.1f).reverb (0.2f, 1.8f); addArp (b); }
    { B b (C::SFX); b.name ("Zap Sequence Arp").tags ({ "Arp", "Modern", "Metallic" }).desc ("Random metallic zaps over four octaves; instant sci-fi computer.", "hold any cluster of notes.");
      b.clear().op (1, 7.07f, 0.9f, 0.001f, 0.06f, 0.0f, 0.05f).op (2, 1.0f, 1.0f, 0.001f, 0.16f, 0.0f, 0.1f, 1.0f).op (3, 3.5f, 0.6f, 0.001f, 0.1f, 0.0f, 0.08f).edge (1, 2, 3.0f).edge (3, 2, 1.2f)
       .mod (0, S::LFO1, pc::pan, 0.0f).lfo (1, 1.3f).mod (1, S::ModEnv, pc::pitch, 0.0f).modEnv (0.001f, 0.08f, 0.0f, 0.05f)
       .m (5, { { E (1, 2), 0.3f } }).m (6, { { O (2, kOpEnvD), 0.3f } }).mu (7, { { DEPTH (1), 0.2f } }).mu (8, { { DEPTH (0), 0.6f } })
       .mname (5, "Metal").mname (6, "Length").mname (7, "Zap").mname (8, "Auto Pan")
       .poly (8).arp (A::Random, 12, 0.5f, 4).delaySync (0.3f, 10, 0.5f, 0.9f).reverb (0.22f, 2.4f); addArp (b); }

    // ============================================================================ E.Piano
    { B b (C::EPiano); b.name ("Classic Tines").tags ({ "Retro", "Warm" }).desc ("The reference FM electric piano: glassy tine over a round body.", "soft = mellow, hard = barking tine."); add (b); }
    { B b (C::EPiano); b.name ("Soft Suitcase").tags ({ "Soft", "Warm" }).desc ("Mellow ballad piano with gentle stereo tremolo.", "stays round even when played hard.");
      b.edge (1, 2, 0.8f).edge (3, 4, 1.3f).env (2, 0.003f, 2.8f, 0.0f, 0.5f).env (4, 0.003f, 3.4f, 0.0f, 0.5f).mod (0, S::LFO1, pc::pan, 0.35f).lfo (1, 3.2f).chorus (0.3f).reverb (0.2f, 2.2f).eq (1.5f, 0.0f, -2.5f); add (b); }
    { B b (C::EPiano); b.name ("Bright DX Keys").tags ({ "Bright", "Retro", "Punchy" }).desc ("Cutting 80s pop electric piano with a bell layer.", "velocity opens the tine dramatically.");
      b.edge (1, 2, 2.3f).edge (3, 4, 2.5f).out (5, 0.25f).vel (1, 0.5f, 0.9f).chorus (0.35f, 0.5f, 0.8f).eq (0.0f, 0.0f, 3.0f); add (b); }
    { B b (C::EPiano); b.name ("Dyno Bark").tags ({ "Punchy", "Retro" }).desc ("Hard-hitting piano that growls when you dig in.", "wide dynamic range; hit hard for bark.");
      b.edge (3, 4, 3.2f).vel (3, 0.5f, 0.95f).fb (3, 0.25f).level (1, 0.4f).drive (0.22f, 0.7f).comp (-20.0f, 3.0f, 3.0f); add (b); }
    { B b (C::EPiano); b.name ("Glass Tines").tags ({ "Bright", "Cinematic" }).desc ("Crystalline upper tines for arpeggios and intros.", "light touch for sparkle.");
      b.ratio (1, 10.0f).edge (1, 2, 1.8f).out (5, 0.35f).ratio (5, 6.0f).env (5, 0.001f, 1.4f, 0.0f, 0.8f).reverb (0.32f, 3.5f).chorus (0.25f); add (b); }
    { B b (C::EPiano); b.name ("Reed Bite").tags ({ "Retro", "Punchy", "Warm" }).desc ("Reed-style electric piano with a nasal edge and amplitude tremolo.", "harder playing adds bite.");
      b.level (1, 0.2f).edge (3, 4, 2.6f).fb (3, 0.55f).env (4, 0.002f, 1.6f, 0.1f, 0.25f).env (2, 0.002f, 1.2f, 0.0f, 0.25f)
       .mod (0, S::LFO1, pc::level, -0.12f).lfo (1, 5.5f).m (8, {}).mu (8, { { DEPTH (0), -0.3f } }).chorus (0.1f); add (b); }
    { B b (C::EPiano); b.name ("Wide Tremolo EP").tags ({ "Warm", "Retro" }).desc ("Stereo auto-pan piano for slow chords.", "even response.");
      b.mod (0, S::LFO1, pc::pan, 0.6f).lfo (1, 4.0f).edge (1, 2, 1.1f).chorus (0.2f).reverb (0.22f, 2.8f); add (b); }
    { B b (C::EPiano); b.name ("Night Lounge").tags ({ "Soft", "Warm", "Cinematic" }).desc ("Dark, intimate late-night piano.", "soft playing recommended.");
      b.edge (1, 2, 0.6f).edge (3, 4, 1.1f).env (2, 0.008f, 2.6f, 0.0f, 0.6f).env (4, 0.008f, 3.2f, 0.0f, 0.6f).eq (2.0f, 0.0f, -4.0f).reverb (0.35f, 3.2f, 0.7f, 0.7f).chorus (0.15f); add (b); }
    { B b (C::EPiano); b.name ("Clav Hybrid").tags ({ "Punchy", "Modern" }).desc ("Percussive, funky hybrid between clavinet and electric piano.", "short notes; velocity adds snap.");
      b.ratio (1, 3.0f).edge (1, 2, 2.6f).env (1, 0.001f, 0.18f, 0.0f, 0.1f).env (2, 0.001f, 0.9f, 0.0f, 0.15f).ratio (3, 2.0f).edge (3, 4, 2.2f).env (4, 0.001f, 1.0f, 0.0f, 0.15f).phaser (0.25f, 0.6f); add (b); }
    { B b (C::EPiano); b.name ("Tine Pad Layer").tags ({ "Soft", "Cinematic" }).desc ("Electric piano that blooms into a soft sustained layer.", "hold chords and let them ring.");
      b.env (2, 0.03f, 4.0f, 0.35f, 1.4f).env (4, 0.05f, 4.5f, 0.4f, 1.6f).env (3, 0.05f, 3.0f, 0.4f, 1.2f).edge (3, 4, 1.4f).chorus (0.45f, 0.5f, 0.4f).reverb (0.42f, 4.5f); add (b); }
    { B b (C::EPiano); b.name ("Phase Seventy").tags ({ "Retro", "Warm" }).desc ("Phaser-drenched 70s electric piano.", "medium velocity keeps the phaser sweet.");
      b.phaser (0.5f, 0.35f, 0.7f, 0.4f).edge (1, 2, 1.0f).edge (3, 4, 1.9f).chorus (0.0f); add (b); }
    { B b (C::EPiano); b.name ("Lo-Fi Tape Keys").tags ({ "Retro", "Warm", "Modern" }).desc ("Dusty, band-limited piano for lo-fi beats.", "even response; pitch wobble is built in.");
      b.crush (10.0f, 3.0f, 0.5f).eq (2.0f, 1.0f, -7.0f).drive (0.18f, 0.4f).mod (1, S::LFO2, pc::pitch, 0.006f).lfo (2, 0.7f).edge (1, 2, 0.9f); add (b); }

    // ============================================================================ Bell
    // Modelled on a user-supplied reference recording: fundamental + beating octave body
    // (partials 1, 2, 3) under two fast-decaying inharmonic strike partials at 5.37x and 17x.
    { B b (C::Bell); b.name ("Idol Sparkle Bell").tags ({ "Bright", "Metallic", "Modern" }).desc ("Glassy digital bell: a beating octave body under a hard, glittering strike.", "soft = round, hard = more strike.");
      b.clear()
       .op (1, 1.0f, 0.7f, 0.001f, 2.6f, 0.0f, 2.2f)                 // body colour (adds partials 2 and 3)
       .op (2, 1.0f, 1.0f, 0.001f, 3.1f, 0.0f, 2.6f, 0.5f)           // fundamental
       .op (3, 2.0f, 1.0f, 0.001f, 3.1f, 0.0f, 2.6f, 0.4f).det (3, 8.0f)
       .op (6, 2.0f, 1.0f, 0.001f, 3.1f, 0.0f, 2.6f, 0.3f).det (6, -9.0f)
       .op (4, 5.37f, 1.0f, 0.001f, 1.7f, 0.0f, 1.5f, 0.42f).det (4, 6.0f)   // strike, mid
       .op (5, 17.0f, 1.0f, 0.001f, 1.5f, 0.0f, 1.3f, 0.95f)                 // strike, top
       .edge (1, 2, 0.5f).edge (1, 3, 0.9f)
       .vel (4, 0.75f, 0.0f).vel (5, 0.85f, 0.0f)
       .mod (0, ModSource::LFO1, E (1, 2), 0.0f).mod (1, ModSource::LFO2, pc::pitch, 0.0f)
       .m (5, { { O (4, kOpCarrierGain), 0.3f }, { O (5, kOpCarrierGain), 0.4f }, { E (1, 2), 0.15f } })
       .m (6, { { FX (FxType::Chorus, 3), 0.5f } }).mu (6, { { DEPTH (1), 0.004f } })
       .m (7, { { O (2, kOpEnvD), 0.3f }, { O (3, kOpEnvD), 0.3f }, { O (6, kOpEnvD), 0.3f }, { O (2, kOpEnvR), 0.3f }, { O (3, kOpEnvR), 0.3f }, { O (6, kOpEnvR), 0.3f } })
       .mu (8, { { DEPTH (0), 0.12f } })
       .chorus (0.35f, 0.45f, 0.5f).reverb (0.16f, 1.8f, 0.5f, 0.4f); add (b); }
    { B b (C::Bell); b.name ("Crystal Bell").tags ({ "Bright", "Metallic" }).desc ("Clear inharmonic bell with a long natural tail.", "soft = pure, hard = more clang."); add (b); }
    { B b (C::Bell); b.name ("Tubular Tower").tags ({ "Metallic", "Cinematic" }).desc ("Orchestral tubular bell with a slow, heavy decay.", "play single notes firmly.");
      b.ratio (1, 3.5f).edge (1, 3, 3.0f).edge (2, 3, 0.4f).env (3, 0.001f, 7.0f, 0.0f, 4.0f).env (1, 0.001f, 4.5f, 0.0f, 3.0f).env (5, 0.001f, 6.0f, 0.0f, 4.0f).edge (4, 5, 1.2f).reverb (0.38f, 5.0f, 0.8f); add (b); }
    { B b (C::Bell); b.name ("Glocken Spark").tags ({ "Bright", "Metallic", "Punchy" }).desc ("Small bright glockenspiel for melodies and doubling.", "light touch.");
      b.ratio (1, 5.19f).ratio (2, 9.23f).edge (1, 3, 1.4f).edge (2, 3, 0.6f).env (3, 0.001f, 1.6f, 0.0f, 1.0f).env (5, 0.001f, 1.2f, 0.0f, 0.8f).out (5, 0.4f).ratio (5, 2.0f); add (b); }
    { B b (C::Bell); b.name ("Music Box").tags ({ "Soft", "Bright" }).desc ("Delicate wind-up music box.", "gentle, even velocity.");
      b.ratio (1, 4.0f).ratio (2, 8.0f).edge (1, 3, 1.1f).edge (2, 3, 0.3f).env (3, 0.001f, 1.1f, 0.0f, 0.7f).env (1, 0.001f, 0.25f, 0.0f, 0.2f).out (5, 0.0f).level (4, 0.3f).reverb (0.25f, 1.8f).m (5, { { E (1, 3), 0.2f }, { E (2, 3), 0.25f } }).mod (0, S::LFO1, E (1, 3), 0.0f); add (b); }
    { B b (C::Bell); b.name ("Temple Gong").tags ({ "Metallic", "Cinematic" }).desc ("Low, complex gong that swells after the strike.", "strike hard and let it ring.");
      b.ratio (3, 0.5f).ratio (5, 0.5f).ratio (1, 1.41f).ratio (2, 2.76f).ratio (4, 3.5f).edge (1, 3, 4.5f).edge (2, 3, 2.0f).edge (4, 5, 3.0f)
       .env (1, 0.6f, 6.0f, 0.2f, 5.0f).env (3, 0.002f, 9.0f, 0.0f, 6.0f).env (5, 0.002f, 10.0f, 0.0f, 6.0f).env (4, 0.4f, 7.0f, 0.0f, 5.0f).reverb (0.4f, 7.0f, 0.9f); add (b); }
    { B b (C::Bell); b.name ("Vibes Motor").tags ({ "Soft", "Warm" }).desc ("Vibraphone with a rotating-motor tremolo.", "medium velocity; sustain for chords.");
      b.ratio (1, 4.0f).edge (1, 3, 0.9f).edge (2, 3, 0.0f).level (4, 0.25f).env (3, 0.002f, 3.2f, 0.0f, 1.2f).env (5, 0.002f, 3.0f, 0.0f, 1.2f).mod (0, S::LFO1, pc::level, -0.18f).lfo (1, 5.0f)
       .m (8, {}).mu (8, { { DEPTH (0), -0.3f } }).m (5, { { E (1, 3), 0.2f }, { E (4, 5), 0.2f } }); add (b); }
    { B b (C::Bell); b.name ("Celesta Drops").tags ({ "Soft", "Bright" }).desc ("Round celesta for fairy-tale melodies.", "soft to medium.");
      b.ratio (1, 5.0f).edge (1, 3, 0.8f).edge (2, 3, 0.15f).env (3, 0.002f, 1.5f, 0.0f, 0.9f).env (1, 0.001f, 0.4f, 0.0f, 0.3f).out (5, 0.3f).ratio (5, 2.0f).env (5, 0.002f, 0.9f, 0.0f, 0.6f).level (4, 0.3f); add (b); }
    { B b (C::Bell); b.name ("Ice Chimes").tags ({ "Bright", "Metallic", "Cinematic" }).desc ("Frozen high chimes with echo and long reverb.", "sparse notes work best.");
      b.ratio (1, 7.07f).ratio (2, 9.23f).ratio (4, 5.19f).edge (1, 3, 1.6f).edge (2, 3, 1.2f).edge (4, 5, 1.8f).reverb (0.45f, 5.5f).delay (0.22f, 380.0f, 0.45f, 0.8f).eq (-2.0f, 0.0f, 3.0f); add (b); }
    { B b (C::Bell); b.name ("Wood Bell").tags ({ "Warm", "Punchy" }).desc ("Mallet on wood: short, woody and pitched.", "velocity controls the click.");
      b.ratio (1, 4.0f).ratio (2, 10.0f).edge (1, 3, 2.2f).edge (2, 3, 0.8f).env (1, 0.001f, 0.07f, 0.0f, 0.05f).env (2, 0.001f, 0.03f, 0.0f, 0.03f).env (3, 0.001f, 0.7f, 0.0f, 0.3f).out (5, 0.0f).level (4, 0.2f)
       .reverb (0.15f, 1.2f).m (5, { { E (1, 3), 0.25f }, { E (2, 3), 0.25f } }).m (7, { { O (3, kOpEnvD), 0.3f }, { O (3, kOpEnvR), 0.3f } }).mod (0, S::LFO1, E (1, 3), 0.0f).lfo (1, 6.0f); add (b); }
    { B b (C::Bell); b.name ("Shimmer Bells").tags ({ "Bright", "Cinematic", "Soft" }).desc ("Chorused bells with a slow shimmer for ambient beds.", "hold notes; movement is built in.");
      b.chorus (0.55f, 0.6f, 0.5f).mod (1, S::LFO2, pc::pitch, 0.006f).mod (0, S::LFO1, E (4, 5), 0.06f).reverb (0.45f, 6.0f).env (3, 0.01f, 5.0f, 0.0f, 3.5f); add (b); }
    { B b (C::Bell); b.name ("Dark Cathedral").tags ({ "Metallic", "Cinematic", "Warm" }).desc ("Large, dark church bell.", "single low notes, played hard.");
      b.ratio (3, 0.5f).ratio (1, 2.76f).edge (1, 3, 3.4f).edge (2, 3, 0.6f).env (3, 0.002f, 8.0f, 0.0f, 5.0f).env (5, 0.002f, 9.0f, 0.0f, 5.0f).eq (2.0f, 0.0f, -5.0f).reverb (0.42f, 6.5f, 0.85f, 0.7f); add (b); }
    { B b (C::Bell); b.name ("Thumb Piano").tags ({ "Warm", "Soft", "Punchy" }).desc ("Kalimba-like plucked tine.", "medium; harder adds buzz.");
      b.ratio (1, 6.3f).edge (1, 3, 1.3f).edge (2, 3, 0.0f).env (1, 0.001f, 0.09f, 0.0f, 0.06f).env (3, 0.001f, 1.3f, 0.0f, 0.5f).out (5, 0.3f).ratio (5, 1.0f).level (4, 0.15f).env (5, 0.001f, 0.9f, 0.0f, 0.4f).delay (0.12f, 300.0f)
       .m (5, { { E (1, 3), 0.25f }, { O (1, kOpEnvD), 0.2f } }); add (b); }

    // ============================================================================ Bass
    { B b (C::Bass); b.name ("FM Bass One").tags ({ "Punchy", "Retro" }).desc ("The all-round FM bass: tight attack, solid fundamental.", "harder = brighter attack."); add (b); }
    { B b (C::Bass); b.name ("Solid Sub").tags ({ "Warm", "Modern" }).desc ("Clean sub bass with just enough harmonics to be heard.", "even; use Weight for more sub.");
      b.edge (1, 2, 0.5f).edge (2, 3, 0.8f).fb (1, 0.0f).out (4, 0.8f).env (3, 0.003f, 0.8f, 0.9f, 0.15f).env (4, 0.003f, 0.8f, 0.95f, 0.15f).fxOff (F::Chorus); add (b); }
    { B b (C::Bass); b.name ("Growl Machine").tags ({ "Modern", "Punchy" }).desc ("Aggressive growling bass for drops and riffs.", "velocity drives the growl.");
      b.fb (1, 0.95f).edge (1, 2, 3.2f).edge (2, 3, 3.6f).vel (1, 0.3f, 0.8f).vel (2, 0.3f, 0.8f).env (1, 0.001f, 0.6f, 0.5f, 0.12f).env (2, 0.001f, 0.5f, 0.6f, 0.12f).drive (0.3f, 0.5f).out (4, 0.4f); add (b); }
    { B b (C::Bass); b.name ("Rubber Band").tags ({ "Punchy", "Retro" }).desc ("Bouncy, elastic bass with a quick wah-like decay.", "short notes; velocity adds twang.");
      b.env (2, 0.001f, 0.11f, 0.05f, 0.08f).env (1, 0.001f, 0.08f, 0.0f, 0.08f).edge (2, 3, 3.4f).edge (1, 2, 2.0f).env (3, 0.001f, 0.5f, 0.5f, 0.1f); add (b); }
    { B b (C::Bass); b.name ("Slap Pop").tags ({ "Punchy", "Bright" }).desc ("Slap-style bass with a snappy, metallic pop.", "hard hits pop, soft notes thump.");
      b.ratio (1, 3.0f).fb (1, 0.2f).edge (1, 2, 2.4f).edge (2, 3, 2.8f).vel (1, 0.5f, 0.95f).vel (2, 0.5f, 0.9f).env (1, 0.001f, 0.06f, 0.0f, 0.05f).env (2, 0.001f, 0.2f, 0.1f, 0.1f).env (3, 0.001f, 0.7f, 0.3f, 0.12f).comp (-18.0f, 4.0f, 3.0f); add (b); }
    { B b (C::Bass); b.name ("Detuned Reese").tags ({ "Modern", "Warm" }).desc ("Thick detuned saw bass that phases against itself.", "even; hold long notes.");
      b.clear().op (1, 1.0f, 1.0f, 0.005f, 0.5f, 1.0f, 0.2f, 1.0f).op (2, 1.0f, 1.0f, 0.005f, 0.5f, 1.0f, 0.2f, 1.0f).op (3, 0.5f, 0.8f, 0.005f, 0.5f, 1.0f, 0.2f, 0.6f)
       .wave (1, W::Saw).wave (2, W::Saw).det (1, -14.0f).det (2, 14.0f).filter (FT::LP24, 520.0f, 0.2f, 0.2f).mod (0, S::LFO1, pc::filterCutoff, 0.04f).lfo (1, 0.15f).mono (true, 0.04f)
       .m (5, { { pc::filterRes, 0.4f }, { pc::filterDrive, 0.4f } }).m (6, { { O (1, kOpEnvA), -0.2f }, { pc::filterCutoff, 0.15f } }).m (7, { { O (3, kOpCarrierGain), 0.4f }, { FX (F::EQ, 0), 0.15f } }).mu (8, { { FX (F::Drive, 0), 0.6f } }); add (b); }
    { B b (C::Bass); b.name ("Acid Square").tags ({ "Retro", "Punchy", "Bright" }).desc ("Resonant square-wave acid line.", "accented notes open the filter (velocity).");
      b.clear().op (1, 1.0f, 1.0f, 0.001f, 0.4f, 0.7f, 0.08f, 1.0f).wave (1, W::Pulse).filter (FT::LP24, 380.0f, 0.75f, 0.3f).modEnv (0.001f, 0.22f, 0.0f, 0.1f)
       .mod (0, S::ModEnv, pc::filterCutoff, 0.32f).mod (1, S::Velocity, pc::filterCutoff, 0.18f).mono (true, 0.06f)
       .m (5, { { pc::filterRes, 0.25f }, { pc::filterDrive, 0.5f } }).m (6, { { DEPTH (0), 0.2f }, { pc::modEnvD, -0.2f } }).m (7, { { FX (F::EQ, 0), 0.25f }, { pc::filterCutoff, -0.1f } }).mu (8, { { FX (F::Drive, 0), 0.6f } }); add (b); }
    { B b (C::Bass); b.name ("Organ Bass").tags ({ "Warm", "Retro" }).desc ("Round, sustained organ-style house bass.", "even; no velocity snap.");
      b.ratio (1, 2.0f).fb (1, 0.0f).edge (1, 2, 0.9f).edge (2, 3, 1.2f).env (1, 0.002f, 0.4f, 0.8f, 0.1f).env (2, 0.002f, 0.4f, 0.8f, 0.1f).env (3, 0.002f, 0.4f, 0.95f, 0.1f).vel (1, 0.2f, 0.1f).vel (2, 0.2f, 0.1f).out (4, 0.5f); add (b); }
    { B b (C::Bass); b.name ("Tempo Wobble").tags ({ "Modern", "Punchy" }).desc ("Tempo-synced wobble bass (1/8 notes).", "hold notes; Growl changes the wobble tone.");
      b.filter (FT::LP24, 700.0f, 0.45f, 0.25f).lfoSync (1, 9, LfoShape::Sine).mod (0, S::LFO1, pc::filterCutoff, 0.3f).edge (1, 2, 2.4f).edge (2, 3, 3.0f).fb (1, 0.7f)
       .env (1, 0.002f, 0.5f, 0.9f, 0.1f).env (2, 0.002f, 0.5f, 0.9f, 0.1f).env (3, 0.002f, 0.5f, 1.0f, 0.1f).out (4, 0.5f); add (b); }
    { B b (C::Bass); b.name ("Pluck Bass").tags ({ "Punchy", "Bright" }).desc ("Short plucked bass for sequences and arps.", "velocity controls brightness.");
      b.env (3, 0.001f, 0.28f, 0.0f, 0.12f).env (2, 0.001f, 0.15f, 0.0f, 0.1f).env (1, 0.001f, 0.1f, 0.0f, 0.1f).edge (2, 3, 2.8f).poly (8).delay (0.1f, 250.0f, 0.25f); add (b); }
    { B b (C::Bass); b.name ("Fuzz Bass").tags ({ "Modern", "Punchy", "Warm" }).desc ("Overdriven bass with a thick mid-range.", "even; Drive adds more fuzz.");
      b.drive (0.62f, 0.45f).eq (3.0f, 2.0f, -2.0f, 700.0f).edge (1, 2, 1.8f).edge (2, 3, 2.0f).out (4, 0.5f).comp (-16.0f, 4.0f, 2.0f); add (b); }
    { B b (C::Bass); b.name ("Synthwave Bass").tags ({ "Retro", "Warm" }).desc ("Saw bass with a soft filter pluck for 80s lines.", "even eighth notes.");
      b.clear().op (1, 1.0f, 1.0f, 0.002f, 0.5f, 0.8f, 0.15f, 1.0f).op (2, 0.5f, 0.9f, 0.002f, 0.5f, 0.9f, 0.15f, 0.6f).wave (1, W::Saw).filter (FT::LP24, 600.0f, 0.25f, 0.1f)
       .modEnv (0.001f, 0.3f, 0.1f, 0.15f).mod (0, S::ModEnv, pc::filterCutoff, 0.25f).poly (8).chorus (0.2f)
       .m (5, { { pc::filterRes, 0.3f }, { pc::filterDrive, 0.3f } }).m (6, { { DEPTH (0), 0.2f }, { pc::modEnvD, -0.2f } }).m (7, { { O (2, kOpCarrierGain), 0.4f }, { FX (F::EQ, 0), 0.15f } }).mu (8, { { FX (F::Drive, 0), 0.6f } }); add (b); }

    // ============================================================================ Lead
    { B b (C::Lead); b.name ("Classic FM Lead").tags ({ "Bright", "Retro" }).desc ("Expressive mono lead with feedback edge; mod wheel adds vibrato.", "velocity brightens."); b.mod (0, S::ModWheel, pc::pitch, 0.02f); add (b); }
    { B b (C::Lead); b.name ("Soft Sine Lead").tags ({ "Soft", "Warm" }).desc ("Pure, flute-like sine lead.", "even; gentle vibrato built in.");
      b.fb (1, 0.0f).edge (1, 2, 0.5f).edge (3, 4, 0.3f).env (2, 0.03f, 0.3f, 0.9f, 0.25f).env (4, 0.03f, 0.3f, 0.9f, 0.25f).mod (1, S::LFO1, pc::pitch, 0.008f).reverb (0.25f, 2.5f).delay (0.15f, 380.0f); add (b); }
    { B b (C::Lead); b.name ("Screamer").tags ({ "Modern", "Bright", "Punchy" }).desc ("Searing distorted lead for solos.", "hard = more scream.");
      b.fb (1, 1.1f).edge (1, 2, 4.0f).edge (3, 4, 2.6f).vel (1, 0.3f, 0.6f).drive (0.42f, 0.7f).delay (0.22f, 420.0f, 0.4f).eq (0.0f, 2.0f, 2.0f, 2200.0f); add (b); }
    { B b (C::Lead); b.name ("Square Lead").tags ({ "Retro", "Warm" }).desc ("Hollow square-wave lead with a rounded top.", "even.");
      b.clear().op (1, 1.0f, 1.0f, 0.005f, 0.3f, 0.9f, 0.15f, 1.0f).op (2, 2.0f, 0.5f, 0.005f, 0.3f, 0.9f, 0.15f, 0.25f).wave (1, W::Pulse).wave (2, W::Triangle)
       .filter (FT::LP12, 3800.0f, 0.2f).mono (true, 0.05f).mod (0, S::ModWheel, pc::pitch, 0.02f).mod (1, S::LFO1, pc::pitch, 0.01f).lfo (1, 5.5f).delay (0.18f, 400.0f)
       .m (5, { { pc::filterCutoff, 0.25f }, { pc::filterRes, 0.3f } }).mu (6, { { pc::portamento, 0.25f } }).m (7, { { O (2, kOpCarrierGain), 0.4f }, { pc::filterDrive, 0.4f } }).m (8, { { FX (F::Chorus, 3), 0.55f }, { O (2, kOpDetune), 0.08f } }); add (b); }
    { B b (C::Lead); b.name ("Super Saw Lead").tags ({ "Modern", "Bright" }).desc ("Wide unison saw lead for anthems.", "even; Width spreads the stack.");
      b.clear().op (1, 1.0f, 1.0f, 0.004f, 0.4f, 0.85f, 0.25f, 1.0f).op (2, 1.0f, 1.0f, 0.004f, 0.4f, 0.85f, 0.25f, 0.8f).wave (1, W::Saw).wave (2, W::Saw).det (2, 9.0f)
       .filter (FT::LP12, 7000.0f, 0.1f).uni (3, 16.0f, 85.0f).poly (8).mod (0, S::ModWheel, pc::pitch, 0.02f).reverb (0.22f, 2.2f).delaySync (0.2f, 10)
       .m (5, { { pc::filterCutoff, 0.25f }, { pc::filterRes, 0.25f } }).mu (6, { { pc::portamento, 0.25f } }).m (7, { { pc::filterDrive, 0.5f }, { O (2, kOpDetune), 0.1f } }).m (8, { { FX (F::Chorus, 3), 0.55f }, { FX (F::Chorus, 2), 0.3f } }); add (b); }
    { B b (C::Lead); b.name ("Whistler").tags ({ "Soft", "Cinematic" }).desc ("Breathy whistle lead with delayed vibrato feel.", "soft attack; even velocity.");
      b.fb (1, 0.15f).edge (1, 2, 0.35f).out (4, 0.0f).level (3, 0.0f).env (2, 0.06f, 0.4f, 0.9f, 0.3f).mod (1, S::LFO1, pc::pitch, 0.014f).lfo (1, 5.0f).reverb (0.3f, 3.0f)
       .m (5, { { E (1, 2), 0.25f } }).m (7, { { O (1, kOpFeedback), 0.45f } }).m (8, { { FX (F::Chorus, 3), 0.55f } }); add (b); }
    { B b (C::Lead); b.name ("Glide Solo").tags ({ "Retro", "Warm" }).desc ("Fat portamento lead for expressive lines.", "legato playing glides between notes.");
      b.mono (true, 0.14f).edge (1, 2, 1.8f).edge (3, 4, 1.8f).fb (1, 0.35f).det (4, 10.0f).mod (0, S::ModWheel, pc::pitch, 0.02f).delay (0.2f, 440.0f, 0.4f).chorus (0.2f); add (b); }
    { B b (C::Lead); b.name ("Stab Lead").tags ({ "Punchy", "Bright", "Modern" }).desc ("Short polyphonic stabs for riffs and chords.", "velocity = brightness.");
      b.poly (8).env (2, 0.001f, 0.35f, 0.0f, 0.2f).env (4, 0.001f, 0.35f, 0.0f, 0.2f).env (1, 0.001f, 0.2f, 0.0f, 0.15f).env (3, 0.001f, 0.2f, 0.0f, 0.15f).edge (1, 2, 3.0f)
       .m (6, { { O (2, kOpEnvD), 0.25f }, { O (4, kOpEnvD), 0.25f } }).mname (6, "Length").delaySync (0.22f, 10).reverb (0.18f); add (b); }
    { B b (C::Lead); b.name ("Octave Lead").tags ({ "Bright", "Retro" }).desc ("Lead doubled an octave up for extra cut.", "even.");
      b.ratio (4, 2.0f).out (4, 0.6f).det (4, 4.0f).ratio (3, 4.0f).edge (3, 4, 1.2f).mod (0, S::ModWheel, pc::pitch, 0.02f).delay (0.18f, 360.0f); add (b); }
    { B b (C::Lead); b.name ("Hollow Reed").tags ({ "Warm", "Soft" }).desc ("Clarinet-like hollow lead (odd harmonics).", "breath-like response to velocity.");
      b.ratio (1, 2.0f).fb (1, 0.1f).edge (1, 2, 1.6f).ratio (3, 2.0f).edge (3, 4, 1.4f).det (4, 3.0f).env (2, 0.03f, 0.3f, 0.9f, 0.2f).env (4, 0.03f, 0.3f, 0.9f, 0.2f).vel (1, 0.4f, 0.6f).reverb (0.22f, 2.4f); add (b); }
    { B b (C::Lead); b.name ("Talk Box").tags ({ "Modern", "Retro" }).desc ("Vowel-like lead; mod wheel sweeps the formant.", "even; ride the mod wheel.");
      b.filter (FT::BP12, 900.0f, 0.7f, 0.2f).mod (0, S::ModWheel, pc::filterCutoff, 0.3f).mod (2, S::LFO2, pc::filterCutoff, 0.06f).lfo (2, 0.6f).edge (1, 2, 3.0f).fb (1, 0.8f).edge (3, 4, 2.2f).delay (0.15f, 380.0f); add (b); }
    { B b (C::Lead); b.name ("Arp Runner").tags ({ "Arp", "Retro", "Bright", "Modern" }).desc ("Arpeggiated lead: hold a chord and it runs in 16ths.", "even; velocity sets step level.");
      b.poly (8).arp (ArpMode::Up, 12, 0.55f, 2).env (2, 0.001f, 0.4f, 0.2f, 0.15f).env (4, 0.001f, 0.4f, 0.2f, 0.15f).edge (1, 2, 2.0f).delaySync (0.28f, 10, 0.45f).reverb (0.18f)
       .m (6, { { O (2, kOpEnvD), 0.25f }, { O (4, kOpEnvD), 0.25f } }).mname (6, "Length"); add (b); }

    // ============================================================================ Pad
    { B b (C::Pad); b.name ("Warm FM Pad").tags ({ "Warm", "Soft" }).desc ("Slow, warm pad with gentle internal movement.", "even; hold chords."); add (b); }
    { B b (C::Pad); b.name ("Glass Pad").tags ({ "Bright", "Cinematic" }).desc ("Glassy digital pad with shimmering upper partials.", "soft attack; hold.");
      b.ratio (1, 3.0f).ratio (3, 4.0f).edge (1, 2, 0.9f).edge (3, 4, 0.7f).filter (FT::LP12, 9000.0f, 0.1f).reverb (0.42f, 5.0f).eq (0.0f, 0.0f, 2.5f); add (b); }
    { B b (C::Pad); b.name ("Dark Drone").tags ({ "Cinematic", "Warm" }).desc ("Low, brooding drone bed.", "hold low notes; Movement adds unrest.");
      b.ratio (1, 0.5f).ratio (3, 1.0f).edge (1, 2, 2.2f).edge (3, 4, 1.8f).filter (FT::LP24, 900.0f, 0.25f).env (2, 1.5f, 3.0f, 0.9f, 3.0f).env (4, 1.8f, 3.0f, 0.9f, 3.0f).mod (1, S::LFO2, pc::filterCutoff, 0.08f).reverb (0.45f, 8.0f, 0.9f, 0.7f); add (b); }
    { B b (C::Pad); b.name ("Analog Strings").tags ({ "Warm", "Retro" }).desc ("Ensemble string machine built from detuned saws.", "even; velocity is gentle.");
      b.clear().op (1, 1.0f, 1.0f, 0.25f, 1.0f, 0.9f, 0.9f, 1.0f).op (2, 1.0f, 1.0f, 0.3f, 1.0f, 0.9f, 1.0f, 0.9f).op (3, 2.0f, 0.6f, 0.3f, 1.0f, 0.8f, 1.0f, 0.35f).wave (1, W::Saw).wave (2, W::Saw).wave (3, W::Saw)
       .det (1, -8.0f).det (2, 8.0f).filter (FT::LP24, 2600.0f, 0.12f).uni (4, 16.0f, 90.0f).mod (0, S::LFO1, pc::filterCutoff, 0.03f).mod (1, S::LFO2, pc::pan, 0.0f).lfo (1, 0.3f).lfo (2, 0.2f).chorus (0.45f, 0.5f, 0.5f).reverb (0.3f, 3.0f)
       .m (5, { { pc::filterCutoff, -0.3f }, { O (3, kOpCarrierGain), -0.3f } }).m (6, { { FX (F::Chorus, 3), 0.5f }, { FX (F::Chorus, 2), 0.3f } })
       .m (7, { { pc::lfo1Rate, 0.25f } }).mu (7, { { DEPTH (0), 0.12f }, { DEPTH (1), 0.3f } }).m (8, { { FX (F::EQ, 5), 0.25f }, { FX (F::Reverb, 4), 0.3f } }); add (b); }
    { B b (C::Pad); b.name ("Vox Humana").tags ({ "Soft", "Cinematic", "Warm" }).desc ("Choir-like pad with a vocal formant.", "even; sing-along chords.");
      b.fb (1, 0.3f).edge (1, 2, 2.2f).edge (3, 4, 1.6f).filter (FT::BP12, 1100.0f, 0.45f).mod (1, S::LFO2, pc::filterCutoff, 0.05f).env (2, 0.5f, 2.0f, 0.9f, 1.5f).env (4, 0.6f, 2.0f, 0.9f, 1.5f).reverb (0.4f, 4.5f).chorus (0.35f); add (b); }
    { B b (C::Pad); b.name ("Evolving Motion").tags ({ "Modern", "Cinematic" }).desc ("Constantly shifting pad: two slow LFOs and a phaser.", "hold for at least a few bars.");
      b.mod (0, S::LFO1, E (1, 2), 0.14f).mod (1, S::LFO2, pc::filterCutoff, 0.14f).mod (2, S::LFO1, pc::pan, 0.25f).lfo (1, 0.17f, LfoShape::Triangle).lfo (2, 0.09f).phaser (0.4f, 0.12f, 0.8f, 0.4f).reverb (0.4f, 5.0f); add (b); }
    { B b (C::Pad); b.name ("Shimmer Air").tags ({ "Bright", "Cinematic", "Soft" }).desc ("Airy high pad with a very long reverb.", "sparse voicings in the upper register.");
      b.ratio (3, 4.0f).edge (3, 4, 0.9f).filter (FT::HP12, 320.0f, 0.1f).eq (-3.0f, 0.0f, 5.0f).reverb (0.55f, 8.0f, 0.9f, 0.3f).chorus (0.4f)
       .m (5, { { E (1, 2), -0.15f }, { E (3, 4), -0.15f }, { FX (F::EQ, 5), -0.3f } }); add (b); }
    { B b (C::Pad); b.name ("Soft Sweep").tags ({ "Warm", "Retro" }).desc ("Pad whose filter opens slowly on every chord.", "even; the sweep restarts on each new note.");
      b.filter (FT::LP24, 600.0f, 0.3f).modEnv (2.2f, 3.0f, 0.6f, 2.0f).mod (1, S::ModEnv, pc::filterCutoff, 0.35f).edge (1, 2, 2.0f).edge (3, 4, 1.6f)
       .m (7, { { pc::modEnvA, 0.25f } }).mu (7, { { DEPTH (0), 0.12f } }).mname (7, "Sweep"); add (b); }
    { B b (C::Pad); b.name ("Wide Hollow").tags ({ "Retro", "Soft" }).desc ("Hollow triangle/pulse pad with wide stereo.", "even.");
      b.wave (2, W::Triangle).wave (4, W::Pulse).out (4, 0.45f).edge (1, 2, 0.8f).edge (3, 4, 0.4f).filter (FT::LP12, 3200.0f, 0.15f).uni (3, 14.0f, 100.0f).chorus (0.4f); add (b); }
    { B b (C::Pad); b.name ("Cinematic Swell").tags ({ "Cinematic", "Warm" }).desc ("Very slow swell for transitions and endings.", "press and wait: about three seconds to full level.");
      b.env (2, 2.8f, 3.0f, 0.95f, 3.5f).env (4, 3.2f, 3.0f, 0.95f, 3.5f).env (1, 3.5f, 4.0f, 0.8f, 3.0f).env (3, 4.0f, 4.0f, 0.8f, 3.0f).edge (1, 2, 2.4f).reverb (0.5f, 7.0f, 0.9f).uni (3, 12.0f, 90.0f); add (b); }
    { B b (C::Pad); b.name ("Ice Pad").tags ({ "Bright", "Metallic", "Cinematic" }).desc ("Cold, glittering pad with an echo trail.", "upper register; soft.");
      b.ratio (1, 7.0f).edge (1, 2, 0.5f).ratio (3, 5.0f).edge (3, 4, 0.5f).filter (FT::LP12, 12000.0f, 0.05f).delay (0.25f, 460.0f, 0.5f, 0.9f).reverb (0.42f, 6.0f).eq (-2.0f, 0.0f, 3.0f); add (b); }
    { B b (C::Pad); b.name ("Gated Trance Pad").tags ({ "Modern", "Punchy" }).desc ("Rhythmic gated pad (16th-note trance gate) with a ringing reverb tail.", "hold chords; the gate follows the host tempo.");
      b.wave (2, W::Saw).wave (4, W::Saw).edge (1, 2, 0.4f).edge (3, 4, 0.3f).filter (FT::LP24, 3400.0f, 0.2f).env (2, 0.02f, 1.0f, 1.0f, 0.5f).env (4, 0.02f, 1.0f, 1.0f, 0.5f).uni (3, 15.0f, 90.0f)
       .gate (12, { 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1 }).reverb (0.3f, 3.0f).delaySync (0.2f, 10); add (b); }

    // ============================================================================ Pluck
    { B b (C::Pluck); b.name ("Classic Pluck").tags ({ "Bright", "Punchy" }).desc ("Clean digital pluck for arps and melodies.", "velocity = snap."); add (b); }
    { B b (C::Pluck); b.name ("Nylon Fingers").tags ({ "Warm", "Soft" }).desc ("Soft nylon-string style pluck.", "soft = round, hard = more string noise.");
      b.ratio (1, 1.0f).edge (1, 2, 1.6f).env (1, 0.001f, 0.25f, 0.0f, 0.2f).edge (3, 2, 0.6f).env (2, 0.002f, 1.4f, 0.0f, 0.5f).filter (FT::LP12, 5200.0f).reverb (0.2f, 1.8f).fxOff (F::Delay); add (b); }
    { B b (C::Pluck); b.name ("Concert Harp").tags ({ "Soft", "Cinematic", "Warm" }).desc ("Resonant harp with a long ringing decay.", "glissandi and arpeggios.");
      b.ratio (1, 2.0f).edge (1, 2, 1.8f).env (1, 0.001f, 0.3f, 0.0f, 0.3f).env (2, 0.002f, 2.4f, 0.0f, 1.4f).edge (3, 2, 0.5f).reverb (0.35f, 3.2f).delay (0.0f, 350.0f); add (b); }
    { B b (C::Pluck); b.name ("Silk Koto").tags ({ "Bright", "Metallic" }).desc ("Sharp, bright plucked string with a quick bend-like attack.", "hard notes snap.");
      b.ratio (1, 3.0f).edge (1, 2, 3.6f).env (1, 0.001f, 0.09f, 0.0f, 0.08f).env (2, 0.001f, 1.1f, 0.0f, 0.5f).ratio (3, 2.0f).edge (3, 2, 1.2f).modEnv (0.001f, 0.05f, 0.0f, 0.05f).mod (0, S::ModEnv, pc::pitch, 0.012f).reverb (0.22f, 2.0f); add (b); }
    { B b (C::Pluck); b.name ("Synth Pluck").tags ({ "Modern", "Punchy", "Bright" }).desc ("Saw pluck with a fast filter envelope.", "velocity opens the filter.");
      b.clear().op (1, 1.0f, 1.0f, 0.001f, 0.6f, 0.0f, 0.3f, 1.0f).op (2, 2.0f, 0.6f, 0.001f, 0.4f, 0.0f, 0.3f, 0.3f).wave (1, W::Saw).wave (2, W::Saw).filter (FT::LP24, 700.0f, 0.3f)
       .modEnv (0.001f, 0.18f, 0.0f, 0.15f).mod (0, S::ModEnv, pc::filterCutoff, 0.4f).mod (1, S::Velocity, pc::filterCutoff, 0.12f).delaySync (0.2f, 10).reverb (0.15f)
       .m (5, { { DEPTH (0), 0.2f }, { pc::modEnvD, -0.2f } }).m (6, { { O (1, kOpEnvD), 0.25f }, { O (2, kOpCarrierGain), 0.3f } }).m (7, { { pc::filterCutoff, -0.2f }, { O (1, kOpEnvD), -0.15f } }).m (8, { { FX (F::Reverb, 4), 0.35f }, { FX (F::Delay, 5), 0.3f } }); add (b); }
    { B b (C::Pluck); b.name ("Bell Pluck").tags ({ "Metallic", "Bright" }).desc ("Pluck with an inharmonic, bell-like transient.", "medium velocity.");
      b.ratio (1, 3.5f).edge (1, 2, 2.2f).env (1, 0.001f, 0.3f, 0.0f, 0.25f).env (2, 0.001f, 1.2f, 0.0f, 0.7f).reverb (0.3f, 2.8f); add (b); }
    { B b (C::Pluck); b.name ("Muted Strings").tags ({ "Warm", "Punchy" }).desc ("Palm-muted, percussive string pluck.", "tight rhythmic playing.");
      b.env (2, 0.001f, 0.22f, 0.0f, 0.1f).env (1, 0.001f, 0.05f, 0.0f, 0.05f).edge (1, 2, 2.0f).edge (3, 2, 0.8f).filter (FT::LP12, 2400.0f, 0.2f).fxOff (F::Delay).reverb (0.1f, 1.0f); add (b); }
    { B b (C::Pluck); b.name ("Pizzicato").tags ({ "Soft", "Cinematic", "Punchy" }).desc ("Orchestral pizzicato strings.", "medium; layer chords for sections.");
      b.ratio (1, 1.0f).fb (1, 0.4f).edge (1, 2, 2.2f).env (1, 0.001f, 0.12f, 0.0f, 0.1f).env (2, 0.003f, 0.38f, 0.0f, 0.25f).edge (3, 2, 0.4f).filter (FT::LP12, 4200.0f).reverb (0.3f, 1.8f, 0.7f).fxOff (F::Delay); add (b); }
    { B b (C::Pluck); b.name ("Trance Pluck").tags ({ "Modern", "Bright", "Punchy" }).desc ("Unison saw pluck with tempo delay for trance leads.", "even 16ths.");
      b.clear().op (1, 1.0f, 1.0f, 0.001f, 0.35f, 0.0f, 0.25f, 1.0f).op (2, 1.0f, 1.0f, 0.001f, 0.35f, 0.0f, 0.25f, 0.9f).wave (1, W::Saw).wave (2, W::Saw).det (2, 10.0f)
       .filter (FT::LP12, 1800.0f, 0.2f).modEnv (0.001f, 0.2f, 0.0f, 0.2f).mod (0, S::ModEnv, pc::filterCutoff, 0.3f).uni (3, 14.0f, 90.0f).delaySync (0.3f, 10, 0.5f).reverb (0.22f, 2.5f)
       .m (5, { { DEPTH (0), 0.2f }, { pc::modEnvD, -0.2f } }).m (6, { { O (1, kOpEnvD), 0.25f }, { O (2, kOpEnvD), 0.25f } }).m (7, { { pc::filterCutoff, -0.25f } }).m (8, { { FX (F::Reverb, 4), 0.35f }, { FX (F::Delay, 5), 0.3f } }); add (b); }
    { B b (C::Pluck); b.name ("Tine Pluck").tags ({ "Warm", "Soft" }).desc ("Short, rounded thumb-piano pluck.", "soft to medium.");
      b.ratio (1, 6.0f).edge (1, 2, 1.2f).env (1, 0.001f, 0.06f, 0.0f, 0.05f).env (2, 0.001f, 0.9f, 0.0f, 0.4f).edge (3, 2, 0.3f).filter (FT::LP12, 6000.0f).delay (0.14f, 320.0f); add (b); }
    { B b (C::Pluck); b.name ("Hammered Strings").tags ({ "Bright", "Metallic", "Cinematic" }).desc ("Dulcimer-like double-string pluck with shimmer.", "medium; tremolo picking works well.");
      b.clear().op (1, 2.0f, 0.8f, 0.001f, 0.2f, 0.0f, 0.2f).op (2, 1.0f, 1.0f, 0.001f, 1.4f, 0.0f, 0.8f, 1.0f).op (3, 3.0f, 0.7f, 0.001f, 0.15f, 0.0f, 0.15f).op (4, 1.0f, 1.0f, 0.001f, 1.6f, 0.0f, 0.8f, 0.9f)
       .det (2, -5.0f).det (4, 5.0f).edge (1, 2, 2.2f).edge (3, 4, 2.4f).vel (1, 0.5f, 0.7f).vel (3, 0.5f, 0.7f).reverb (0.3f, 2.6f).delay (0.1f, 300.0f)
       .m (5, { { E (1, 2), 0.25f }, { E (3, 4), 0.25f } }).m (6, { { O (2, kOpEnvD), 0.2f }, { O (4, kOpEnvD), 0.2f } }).m (7, { { O (2, kOpEnvD), -0.2f }, { O (4, kOpEnvD), -0.2f }, { FX (F::EQ, 5), -0.3f } }).m (8, { { FX (F::Reverb, 4), 0.35f }, { FX (F::Delay, 5), 0.3f } }); add (b); }
    { B b (C::Pluck); b.name ("Arp Pluck").tags ({ "Arp", "Modern", "Bright" }).desc ("Ready-to-run arpeggio: up/down over two octaves with echo.", "hold a chord; velocity sets level.");
      b.arp (ArpMode::UpDown, 12, 0.6f, 2).delaySync (0.3f, 10, 0.45f).reverb (0.2f).edge (1, 2, 2.2f); add (b); }

    // ============================================================================ Brass
    { B b (C::Brass); b.name ("FM Brass Section").tags ({ "Bright", "Punchy", "Retro" }).desc ("Punchy brass section with a blaring attack.", "hard = brassy, soft = mellow."); add (b); }
    { B b (C::Brass); b.name ("Soft Horns").tags ({ "Soft", "Warm" }).desc ("Mellow French-horn style section.", "soft to medium; swells nicely.");
      b.fb (1, 0.35f).edge (1, 2, 1.3f).edge (3, 4, 0.7f).env (1, 0.12f, 0.6f, 0.6f, 0.25f).env (2, 0.09f, 0.3f, 0.9f, 0.25f).env (4, 0.1f, 0.3f, 0.8f, 0.25f).out (4, 0.3f).eq (2.0f, 0.0f, -3.0f).reverb (0.3f, 2.8f); add (b); }
    { B b (C::Brass); b.name ("Bright Trumpet").tags ({ "Bright", "Punchy" }).desc ("Solo trumpet with a sharp, brilliant tone.", "velocity strongly controls brightness.");
      b.fb (1, 0.9f).edge (1, 2, 3.4f).vel (1, 0.4f, 0.9f).env (1, 0.03f, 0.3f, 0.75f, 0.15f).env (2, 0.02f, 0.2f, 0.95f, 0.15f).out (4, 0.2f).mono (true, 0.02f).mod (0, S::LFO1, pc::pitch, 0.006f).lfo (1, 5.2f).reverb (0.2f, 2.0f); add (b); }
    { B b (C::Brass); b.name ("Synth Brass").tags ({ "Retro", "Warm", "Punchy" }).desc ("Analog-style saw brass with a filter swell.", "velocity opens the filter.");
      b.clear().op (1, 1.0f, 1.0f, 0.02f, 0.4f, 0.9f, 0.2f, 1.0f).op (2, 1.0f, 1.0f, 0.02f, 0.4f, 0.9f, 0.2f, 0.9f).wave (1, W::Saw).wave (2, W::Saw).det (1, -7.0f).det (2, 7.0f)
       .filter (FT::LP24, 900.0f, 0.2f, 0.1f).modEnv (0.05f, 0.5f, 0.55f, 0.2f).mod (0, S::ModEnv, pc::filterCutoff, 0.3f).mod (1, S::Velocity, pc::filterCutoff, 0.12f).chorus (0.2f)
       .m (5, { { DEPTH (0), 0.2f }, { pc::filterRes, 0.3f } }).m (6, { { pc::filterCutoff, 0.15f }, { FX (F::EQ, 0), 0.2f } }).m (7, { { pc::modEnvA, 0.3f }, { O (1, kOpEnvA), 0.2f } }).m (8, { { FX (F::Chorus, 3), 0.5f }, { O (2, kOpDetune), 0.06f } }); add (b); }
    { B b (C::Brass); b.name ("Fanfare Stack").tags ({ "Bright", "Cinematic" }).desc ("Big unison fanfare brass.", "play hard for full power.");
      b.uni (3, 10.0f, 80.0f).chorus (0.25f).reverb (0.3f, 3.0f).edge (1, 2, 3.0f).out (4, 0.6f); add (b); }
    { B b (C::Brass); b.name ("Low Bones").tags ({ "Warm", "Cinematic" }).desc ("Dark trombone section for low parts.", "play below middle C.");
      b.fb (1, 0.55f).edge (1, 2, 2.2f).ratio (4, 1.0f).edge (3, 4, 1.0f).env (1, 0.08f, 0.6f, 0.7f, 0.2f).env (2, 0.06f, 0.3f, 0.9f, 0.22f).eq (3.0f, 0.0f, -3.5f).reverb (0.28f, 2.6f); add (b); }
    { B b (C::Brass); b.name ("Sforzando").tags ({ "Punchy", "Bright", "Cinematic" }).desc ("Hard accent that quickly drops back: fp brass.", "always played with force.");
      b.env (1, 0.01f, 0.22f, 0.35f, 0.15f).env (3, 0.01f, 0.25f, 0.3f, 0.15f).edge (1, 2, 3.8f).fb (1, 0.9f).env (2, 0.01f, 0.4f, 0.75f, 0.18f); add (b); }
    { B b (C::Brass); b.name ("Mellow Flugel").tags ({ "Soft", "Warm" }).desc ("Rounded flugelhorn solo voice.", "soft; gentle vibrato built in.");
      b.fb (1, 0.3f).edge (1, 2, 1.5f).out (4, 0.15f).env (2, 0.05f, 0.3f, 0.9f, 0.2f).mono (true, 0.03f).mod (0, S::LFO1, pc::pitch, 0.007f).lfo (1, 4.8f).eq (1.0f, 0.0f, -4.0f).reverb (0.3f, 2.6f); add (b); }
    { B b (C::Brass); b.name ("Brass Pad").tags ({ "Warm", "Cinematic", "Soft" }).desc ("Slow, sustained brass bed.", "hold chords.");
      b.env (1, 0.3f, 1.0f, 0.7f, 0.8f).env (2, 0.25f, 1.0f, 0.9f, 0.9f).env (3, 0.35f, 1.0f, 0.7f, 0.8f).env (4, 0.3f, 1.0f, 0.85f, 0.9f).edge (1, 2, 2.0f).uni (2, 9.0f, 70.0f).chorus (0.3f).reverb (0.38f, 4.0f); add (b); }
    { B b (C::Brass); b.name ("Stab Brass").tags ({ "Punchy", "Bright", "Modern" }).desc ("Short brass hits for funk and pop.", "hard, short notes.");
      b.env (2, 0.005f, 0.3f, 0.0f, 0.12f).env (4, 0.005f, 0.3f, 0.0f, 0.12f).env (1, 0.005f, 0.22f, 0.0f, 0.1f).env (3, 0.005f, 0.22f, 0.0f, 0.1f).edge (1, 2, 3.4f).comp (-18.0f, 4.0f, 3.0f).reverb (0.15f, 1.4f); add (b); }
    { B b (C::Brass); b.name ("Analog Horns").tags ({ "Retro", "Warm" }).desc ("Pulse-wave synth horns with a soft filter.", "even.");
      b.wave (2, W::Pulse).wave (4, W::Triangle).edge (1, 2, 0.8f).fb (1, 0.2f).edge (3, 4, 0.5f).filter (FT::LP12, 2600.0f, 0.2f).chorus (0.3f); add (b); }
    { B b (C::Brass); b.name ("Epic Low Brass").tags ({ "Cinematic", "Punchy" }).desc ("Massive low brass for trailers.", "low octaves, played hard.");
      b.ratio (2, 0.5f).ratio (1, 0.5f).ratio (4, 1.0f).edge (1, 2, 3.2f).fb (1, 0.85f).edge (3, 4, 1.8f).out (4, 0.7f).uni (2, 8.0f, 60.0f).drive (0.2f, 0.5f).eq (3.0f, 0.0f, 0.0f).reverb (0.38f, 4.5f, 0.85f); add (b); }

    // ============================================================================ Keys
    { B b (C::Keys); b.name ("Digital Keys").tags ({ "Bright", "Retro" }).desc ("Clean digital keyboard for comping.", "velocity adds attack."); add (b); }
    { B b (C::Keys); b.name ("Drawbar Organ").tags ({ "Warm", "Retro" }).desc ("Six-drawbar additive organ.", "no velocity response, like the real thing.");
      b.clear().op (1, 0.5f, 0.8f, 0.004f, 0.3f, 1.0f, 0.06f, 0.8f).op (2, 1.0f, 1.0f, 0.004f, 0.3f, 1.0f, 0.06f, 1.0f).op (3, 2.0f, 0.8f, 0.004f, 0.3f, 1.0f, 0.06f, 0.7f)
       .op (4, 3.0f, 0.6f, 0.004f, 0.3f, 1.0f, 0.06f, 0.5f).op (5, 4.0f, 0.5f, 0.004f, 0.3f, 1.0f, 0.06f, 0.35f).op (6, 6.0f, 0.4f, 0.004f, 0.3f, 1.0f, 0.06f, 0.2f);
      for (int k = 1; k <= 6; ++k) b.vel (k, 0.0f, 0.0f);
      b.chorus (0.35f, 0.6f, 3.5f).drive (0.15f, 0.5f)
       .m (5, { { O (5, kOpCarrierGain), 0.4f }, { O (6, kOpCarrierGain), 0.5f } }).mname (5, "Upper Bars").m (6, { { O (1, kOpCarrierGain), 0.4f }, { FX (F::EQ, 0), 0.2f } })
       .m (7, { { O (4, kOpCarrierGain), 0.5f }, { O (3, kOpCarrierGain), 0.3f } }).mname (7, "Mid Bars").m (8, { { FX (F::Chorus, 3), 0.55f }, { FX (F::Chorus, 0), 0.3f } }).mname (8, "Rotary"); add (b); }
    { B b (C::Keys); b.name ("Click Organ").tags ({ "Retro", "Punchy" }).desc ("Percussive organ with key click.", "velocity only affects the click.");
      b.clear().op (1, 1.0f, 1.0f, 0.002f, 0.3f, 1.0f, 0.05f, 1.0f).op (2, 2.0f, 0.8f, 0.002f, 0.3f, 1.0f, 0.05f, 0.6f).op (3, 3.0f, 1.0f, 0.001f, 0.12f, 0.0f, 0.05f, 0.7f).op (4, 7.0f, 0.8f, 0.001f, 0.02f, 0.0f, 0.02f)
       .edge (4, 3, 1.5f).vel (1, 0.0f, 0.0f).vel (2, 0.0f, 0.0f).vel (3, 0.6f, 0.5f).chorus (0.25f, 0.5f, 2.5f)
       .m (5, { { E (4, 3), 0.3f }, { O (3, kOpCarrierGain), 0.3f } }).mname (5, "Click").m (6, { { O (1, kOpCarrierGain), 0.3f }, { FX (F::EQ, 0), 0.2f } }).m (7, { { O (3, kOpEnvD), 0.3f }, { O (2, kOpCarrierGain), 0.3f } }).mname (7, "Percussion").m (8, { { FX (F::Chorus, 3), 0.55f }, { FX (F::Chorus, 2), 0.3f } }); add (b); }
    { B b (C::Keys); b.name ("Funky Clav").tags ({ "Punchy", "Retro", "Bright" }).desc ("Biting clavinet for funk riffs.", "short notes; velocity = bite.");
      b.ratio (1, 1.0f).fb (1, 0.6f).edge (1, 2, 3.4f).vel (1, 0.4f, 0.8f).env (1, 0.001f, 0.35f, 0.2f, 0.06f).env (2, 0.001f, 0.9f, 0.0f, 0.06f).ratio (3, 4.0f).edge (3, 4, 1.4f).env (4, 0.001f, 0.5f, 0.0f, 0.06f)
       .eq (-2.0f, 3.0f, 2.0f, 1800.0f).phaser (0.2f, 0.5f); add (b); }
    { B b (C::Keys); b.name ("Harpsichord").tags ({ "Bright", "Retro", "Metallic" }).desc ("Plucked baroque harpsichord.", "no dynamics by design; velocity is subtle.");
      b.ratio (1, 3.0f).edge (1, 2, 3.8f).env (1, 0.001f, 0.9f, 0.1f, 0.1f).env (2, 0.001f, 1.6f, 0.0f, 0.12f).ratio (3, 5.0f).edge (3, 4, 2.4f).env (4, 0.001f, 1.2f, 0.0f, 0.12f).out (4, 0.6f).vel (1, 0.15f, 0.15f).vel (3, 0.15f, 0.15f).reverb (0.2f, 1.8f); add (b); }
    { B b (C::Keys); b.name ("Toy Piano").tags ({ "Bright", "Metallic", "Soft" }).desc ("Clanky toy piano.", "light playing.");
      b.ratio (1, 5.19f).edge (1, 2, 1.8f).env (1, 0.001f, 0.2f, 0.0f, 0.15f).env (2, 0.001f, 0.8f, 0.0f, 0.3f).ratio (3, 7.07f).edge (3, 4, 1.2f).env (4, 0.001f, 0.4f, 0.0f, 0.2f).out (4, 0.35f).reverb (0.18f, 1.2f); add (b); }
    { B b (C::Keys); b.name ("Glass Keys").tags ({ "Bright", "Cinematic", "Soft" }).desc ("Glass-harmonica style keys with a soft bloom.", "soft; sustain pedal recommended.");
      b.ratio (1, 4.0f).edge (1, 2, 0.9f).env (2, 0.01f, 2.6f, 0.2f, 1.0f).env (4, 0.01f, 2.0f, 0.1f, 1.0f).edge (3, 4, 0.5f).chorus (0.35f).reverb (0.38f, 4.0f); add (b); }
    { B b (C::Keys); b.name ("Crystal Organ").tags ({ "Bright", "Soft" }).desc ("Thin, sparkling organ for high chords.", "even.");
      b.clear().op (1, 1.0f, 1.0f, 0.01f, 0.3f, 1.0f, 0.15f, 1.0f).op (2, 2.0f, 0.8f, 0.01f, 0.3f, 1.0f, 0.15f, 0.6f).op (3, 4.0f, 0.6f, 0.01f, 0.3f, 1.0f, 0.15f, 0.4f).op (4, 8.0f, 0.5f, 0.005f, 0.6f, 0.2f, 0.15f)
       .edge (4, 3, 0.6f).chorus (0.45f, 0.5f, 0.7f).reverb (0.3f, 3.0f)
       .m (5, { { E (4, 3), 0.3f } }).mu (5, { { FX (F::Bitcrusher, 2), 0.5f } }).m (6, { { O (1, kOpCarrierGain), 0.3f }, { FX (F::EQ, 0), 0.2f } }).m (7, { { O (3, kOpCarrierGain), 0.5f }, { O (2, kOpCarrierGain), 0.3f } }).m (8, { { FX (F::Chorus, 3), 0.55f }, { FX (F::Chorus, 2), 0.3f } }); add (b); }
    { B b (C::Keys); b.name ("House Piano").tags ({ "Punchy", "Bright", "Modern" }).desc ("Bright, compressed piano stab for house chords.", "hard, rhythmic chords.");
      b.ratio (1, 1.0f).edge (1, 2, 2.6f).vel (1, 0.5f, 0.8f).env (1, 0.001f, 0.5f, 0.0f, 0.2f).env (2, 0.001f, 1.8f, 0.0f, 0.3f).ratio (3, 3.0f).ratio (4, 1.0f).edge (3, 4, 2.0f).env (3, 0.001f, 0.15f, 0.0f, 0.1f).env (4, 0.001f, 1.4f, 0.0f, 0.3f).out (4, 0.8f).det (4, 4.0f)
       .comp (-20.0f, 4.0f, 4.0f).eq (0.0f, 1.0f, 3.0f, 2500.0f).reverb (0.18f, 1.6f); add (b); }
    { B b (C::Keys); b.name ("Musette").tags ({ "Warm", "Retro" }).desc ("Accordion-like reed keys with a beating detune.", "even; expression via the mod wheel (level).");
      b.ratio (1, 1.0f).fb (1, 0.5f).edge (1, 2, 1.8f).env (1, 0.03f, 0.3f, 0.9f, 0.12f).env (2, 0.03f, 0.3f, 1.0f, 0.12f).ratio (3, 1.0f).ratio (4, 1.0f).edge (3, 4, 1.6f).env (3, 0.03f, 0.3f, 0.9f, 0.12f).env (4, 0.03f, 0.3f, 1.0f, 0.12f).out (4, 0.9f).det (4, 14.0f)
       .mod (0, S::ModWheel, pc::level, -0.2f); add (b); }
    { B b (C::Keys); b.name ("8-Bit Keys").tags ({ "Retro", "Modern", "Bright" }).desc ("Chiptune pulse keys, crunchy by default.", "even.");
      b.wave (2, W::Pulse).wave (4, W::Pulse).edge (1, 2, 0.3f).edge (3, 4, 0.2f).env (2, 0.001f, 0.6f, 0.5f, 0.08f).env (4, 0.001f, 0.4f, 0.3f, 0.08f).crush (7.0f, 4.0f, 0.6f).fxOff (F::Reverb); add (b); }
    { B b (C::Keys); b.name ("Ring Keys").tags ({ "Metallic", "Cinematic" }).desc ("Keys with a ring-modulated, clangorous overtone.", "medium; Ring macro goes from subtle to strange.");
      b.edge (3, 4, 2.4f).out (4, 0.7f).ratio (3, 3.53f).env (3, 0.001f, 0.9f, 0.2f, 0.4f).env (4, 0.001f, 1.6f, 0.1f, 0.4f).reverb (0.28f, 2.8f); add (b); }

    // ============================================================================ SFX
    { B b (C::SFX); b.name ("Chaos Engine").tags ({ "Modern", "Metallic" }).desc ("Unstable machine tone; the Chaos macro goes from hum to havoc.", "even; hold notes."); add (b); }
    { B b (C::SFX); b.name ("Laser Zap").tags ({ "Retro", "Punchy", "Bright" }).desc ("Classic falling laser shot.", "short notes; velocity = brightness.");
      b.clear().op (1, 3.0f, 0.9f, 0.001f, 0.2f, 0.0f, 0.1f).op (2, 1.0f, 1.0f, 0.001f, 0.35f, 0.0f, 0.15f, 1.0f).edge (1, 2, 3.0f).modEnv (0.001f, 0.25f, 0.0f, 0.1f).mod (0, S::ModEnv, pc::pitch, 0.9f).delay (0.2f, 180.0f, 0.4f)
       .m (5, { { E (1, 2), 0.35f }, { O (1, kOpFeedback), 0.4f } }).m (6, { { DEPTH (0), 0.5f }, { pc::modEnvD, 0.25f } }).mu (7, { { FX (F::Flanger, 3), 0.7f } }).m (8, { { FX (F::Reverb, 4), 0.35f }, { O (2, kOpEnvR), 0.3f }, { O (2, kOpEnvD), 0.3f } }); add (b); }
    { B b (C::SFX); b.name ("Alien Radio").tags ({ "Modern", "Metallic" }).desc ("Garbled transmissions: random modulation of the FM amount.", "hold; every note differs.");
      b.fixed (1, 11.0f).fb (1, 0.9f).edge (1, 2, 5.0f).edge (2, 3, 4.0f).mod (1, S::LFO1, E (2, 3), 0.3f).mod (2, S::LFO2, pc::pitch, 0.08f).lfo (1, 9.0f, LfoShape::RandomSmooth).lfo (2, 3.0f, LfoShape::RandomSmooth)
       .crush (8.0f, 3.0f, 0.4f).filter (FT::BP12, 1800.0f, 0.4f); add (b); }
    { B b (C::SFX); b.name ("Metal Hit").tags ({ "Metallic", "Punchy", "Cinematic" }).desc ("Industrial metal impact.", "single hits, hard.");
      b.clear().op (1, 1.41f, 1.0f, 0.001f, 0.5f, 0.0f, 0.4f).op (2, 2.76f, 0.9f, 0.001f, 0.35f, 0.0f, 0.3f).op (3, 1.0f, 1.0f, 0.001f, 0.9f, 0.0f, 0.6f, 1.0f).op (4, 5.19f, 0.8f, 0.001f, 0.6f, 0.0f, 0.5f, 0.4f)
       .edge (1, 3, 6.0f).edge (2, 3, 4.0f).edge (2, 4, 3.0f).fb (1, 0.6f).modEnv (0.001f, 0.06f, 0.0f, 0.05f).mod (0, S::ModEnv, pc::pitch, 0.15f).reverb (0.4f, 3.5f, 0.8f).drive (0.2f)
       .m (5, { { E (1, 3), 0.3f }, { E (2, 3), 0.3f }, { O (1, kOpFeedback), 0.4f } }).m (6, { { DEPTH (0), 0.5f }, { pc::modEnvD, 0.2f } }).m (7, { { O (4, kOpCarrierGain), 0.3f } }).mu (7, { { FX (F::Flanger, 3), 0.7f } })
       .m (8, { { FX (F::Reverb, 4), 0.35f }, { FX (F::Reverb, 1), 0.25f }, { O (3, kOpEnvR), 0.3f } }); add (b); }
    { B b (C::SFX); b.name ("Solar Wind").tags ({ "Cinematic", "Soft" }).desc ("Noisy, whistling wind made from chaotic feedback FM.", "hold; mod wheel moves the whistle.");
      b.fixed (1, 3100.0f).fb (1, 1.5f).ratio (2, 7.13f).fb (2, 1.5f).edge (1, 2, 12.0f).edge (2, 3, 9.0f).fb (3, 1.2f).env (3, 1.2f, 2.0f, 0.9f, 2.5f).env (1, 0.5f, 2.0f, 1.0f, 2.0f).env (2, 0.5f, 2.0f, 1.0f, 2.0f)
       .filter (FT::BP12, 1400.0f, 0.55f).mod (1, S::LFO1, pc::filterCutoff, 0.18f).mod (2, S::ModWheel, pc::filterCutoff, 0.3f).lfo (1, 0.13f).reverb (0.4f, 6.0f); add (b); }
    { B b (C::SFX); b.name ("Sub Drop").tags ({ "Cinematic", "Punchy", "Modern" }).desc ("Long falling sub boom for impacts.", "one low note, held.");
      b.clear().op (1, 1.0f, 1.0f, 0.002f, 3.5f, 0.0f, 1.5f, 1.0f).op (2, 0.5f, 0.6f, 0.001f, 0.3f, 0.0f, 0.3f).edge (2, 1, 1.5f).modEnv (0.001f, 1.8f, 0.0f, 1.0f).mod (0, S::ModEnv, pc::pitch, 0.7f).drive (0.15f, 0.3f).reverb (0.2f, 2.5f)
       .m (5, { { E (2, 1), 0.35f }, { O (2, kOpFeedback), 0.4f } }).m (6, { { DEPTH (0), 0.5f }, { pc::modEnvD, 0.25f } }).mu (7, { { FX (F::Flanger, 3), 0.7f } }).m (8, { { FX (F::Reverb, 4), 0.35f }, { O (1, kOpEnvD), 0.25f }, { O (1, kOpEnvR), 0.3f } }); add (b); }
    { B b (C::SFX); b.name ("Robot Voice").tags ({ "Retro", "Modern" }).desc ("Stepping robot speech: square LFO on pitch plus bit reduction.", "hold; the pattern is tempo free.");
      b.fixed (1, 40.0f).edge (1, 2, 2.5f).edge (2, 3, 3.0f).mod (1, S::LFO1, pc::pitch, 0.25f).lfo (1, 7.0f, LfoShape::Square).mod (2, S::LFO2, E (2, 3), 0.2f).lfo (2, 2.3f, LfoShape::RandomSmooth)
       .crush (6.0f, 6.0f, 0.7f).filter (FT::BP12, 1500.0f, 0.5f); add (b); }
    { B b (C::SFX); b.name ("Riser").tags ({ "Cinematic", "Modern" }).desc ("Four-second build-up: pitch and brightness rise together.", "hold one note until the drop.");
      b.modEnv (4.0f, 1.0f, 1.0f, 0.3f).mod (0, S::ModEnv, pc::pitch, 0.5f).mod (1, S::ModEnv, E (2, 3), 0.4f).mod (2, S::ModEnv, pc::level, 0.2f).fixed (1, 6.0f).edge (1, 2, 2.0f).edge (2, 3, 1.5f)
       .env (3, 0.5f, 1.0f, 1.0f, 0.2f).env (2, 0.5f, 1.0f, 1.0f, 0.2f).env (1, 0.5f, 1.0f, 1.0f, 0.2f).flanger (0.3f, 0.1f).reverb (0.35f, 4.0f); add (b); }
    { B b (C::SFX); b.name ("Digital Rain").tags ({ "Arp", "Modern", "Bright", "Metallic" }).desc ("Random 32nd-note blips falling through an echo.", "hold a cluster of notes.");
      b.clear().op (1, 5.19f, 0.8f, 0.001f, 0.05f, 0.0f, 0.05f).op (2, 2.0f, 1.0f, 0.001f, 0.12f, 0.0f, 0.1f, 1.0f).edge (1, 2, 2.5f).arp (ArpMode::Random, 15, 0.4f, 3)
       .modEnv (0.001f, 0.1f, 0.0f, 0.1f).mod (0, S::ModEnv, pc::pitch, 0.0f).delaySync (0.4f, 10, 0.55f, 1.0f).reverb (0.35f, 4.0f)
       .m (5, { { E (1, 2), 0.35f }, { O (1, kOpFeedback), 0.4f } }).m (6, { { DEPTH (0), 0.5f }, { pc::modEnvD, 0.25f } }).mu (7, { { FX (F::Flanger, 3), 0.7f } }).m (8, { { FX (F::Reverb, 4), 0.35f }, { FX (F::Reverb, 1), 0.25f }, { O (2, kOpEnvD), 0.3f } }); add (b); }
    { B b (C::SFX); b.name ("Dark Atmosphere").tags ({ "Cinematic", "Warm" }).desc ("Slowly breathing low atmosphere.", "hold one or two low notes for a long time.");
      b.fixed (1, 0.4f).fb (1, 0.5f).ratio (2, 1.41f).edge (1, 2, 4.0f).edge (2, 3, 2.5f).ratio (3, 0.5f).env (3, 2.5f, 3.0f, 0.9f, 4.0f).env (2, 3.0f, 3.0f, 0.8f, 3.0f)
       .filter (FT::LP24, 1100.0f, 0.3f).mod (1, S::LFO1, pc::filterCutoff, 0.15f).lfo (1, 0.07f).reverb (0.5f, 12.0f, 0.95f, 0.6f); add (b); }
    { B b (C::SFX); b.name ("Glitch Perc").tags ({ "Modern", "Punchy" }).desc ("Tiny glitch percussion, different on every key.", "short hits across the keyboard.");
      b.clear().op (1, 7.07f, 1.0f, 0.001f, 0.03f, 0.0f, 0.03f).op (2, 1.41f, 1.0f, 0.001f, 0.06f, 0.0f, 0.04f, 1.0f).edge (1, 2, 8.0f).fb (1, 1.2f).modEnv (0.001f, 0.03f, 0.0f, 0.03f).mod (0, S::ModEnv, pc::pitch, 0.6f).crush (6.0f, 5.0f, 0.6f)
       .m (5, { { E (1, 2), 0.35f }, { O (1, kOpFeedback), 0.2f } }).m (6, { { DEPTH (0), 0.4f }, { pc::modEnvD, 0.3f } }).mu (7, { { FX (F::Flanger, 3), 0.7f } }).m (8, { { FX (F::Reverb, 4), 0.35f }, { O (2, kOpEnvD), 0.3f }, { O (2, kOpEnvR), 0.3f } }); add (b); }
    { B b (C::SFX); b.name ("Saucer").tags ({ "Retro", "Cinematic" }).desc ("Hovering flying-saucer warble.", "hold; pitch bend steers it.");
      b.clear().op (1, 2.0f, 0.7f, 0.05f, 1.0f, 1.0f, 0.5f).op (2, 1.0f, 1.0f, 0.05f, 1.0f, 1.0f, 0.6f, 1.0f).edge (1, 2, 1.6f).mod (0, S::ModEnv, pc::pitch, 0.0f).mod (1, S::LFO1, pc::pitch, 0.2f).mod (2, S::LFO2, E (1, 2), 0.15f)
       .lfo (1, 6.0f).lfo (2, 0.4f).bend (12.0f).flanger (0.45f, 0.3f, 0.7f, 0.5f).reverb (0.3f, 3.0f)
       .m (5, { { E (1, 2), 0.35f }, { O (1, kOpFeedback), 0.4f } }).m (6, { { DEPTH (1), 0.35f }, { pc::lfo1Rate, 0.3f } }).m (7, { { FX (F::Flanger, 3), 0.4f }, { DEPTH (2), 0.3f } }).m (8, { { FX (F::Reverb, 4), 0.35f }, { FX (F::Reverb, 1), 0.25f }, { O (2, kOpEnvR), 0.3f } }); add (b); }

    // ============================================================================ Poly
    // Polysynth voices. The template is two detuned saws through the 24 dB low-pass (operators
    // 1, 2 = carriers, 3 = sub, 4 = FM bite into 1). The set deliberately spreads out: different
    // waveforms, FM instead of the filter, and several sounds with built-in modulation.
    { B b (C::Poly); b.name ("Classic Poly Saw").tags ({ "Warm", "Retro", "Bright" }).desc ("The everyday polysynth: two detuned saws, a snappy filter sweep and chorus.", "harder = brighter sweep."); add (b); }
    { B b (C::Poly); b.name ("Poly Brass").tags ({ "Warm", "Retro", "Punchy" }).desc ("Synth-brass chords: the filter opens a moment after the note starts.", "play chords firmly.");
      b.filter (FT::LP24, 900.0f, 0.15f).modEnv (0.09f, 0.6f, 0.5f, 0.3f).mod (0, S::ModEnv, pc::filterCutoff, 0.5f).env (1, 0.03f, 0.8f, 0.9f, 0.22f).env (2, 0.03f, 0.8f, 0.9f, 0.22f).out (3, 0.25f).uni (1, 0.0f, 40.0f).det (1, -4.0f).det (2, 4.0f).fxOff (F::Chorus).reverb (0.12f, 1.6f); add (b); }
    { B b (C::Poly); b.name ("String Machine").tags ({ "Warm", "Retro", "Soft" }).desc ("Seventies string ensemble: slow bow, thick ensemble chorus, a gentle vibrato.", "hold chords; no hurry.");
      b.env (1, 0.14f, 1.5f, 0.9f, 0.7f).env (2, 0.14f, 1.5f, 0.9f, 0.7f).ratio (2, 2.0f).out (2, 0.45f).filter (FT::HP12, 260.0f, 0.05f).mod (0, S::ModEnv, pc::filterCutoff, 0.0f)
       .mod (2, S::LFO2, pc::pitch, 0.0025f).lfo (2, 5.4f).uni (3, 16.0f, 100.0f).poly (10).chorus (0.65f, 0.7f, 0.8f).eq (-3.0f, 0.0f, 2.0f).reverb (0.26f, 2.8f); add (b); }
    { B b (C::Poly); b.name ("Hollow Pulse").tags ({ "Retro", "Soft" }).desc ("Woody square-wave poly, an octave apart, with a slow phaser.", "even velocity; nice for broken chords.");
      b.wave (1, W::Pulse).wave (2, W::Pulse).ratio (2, 0.5f).det (1, 0.0f).det (2, 3.0f).out (1, 0.7f).out (2, 0.5f).uni (1, 0.0f, 30.0f)
       .filter (FT::LP12, 2000.0f, 0.1f).mod (0, S::ModEnv, pc::filterCutoff, 0.12f).fxOff (F::Chorus).phaser (0.4f, 0.25f, 0.6f, 0.35f).reverb (0.16f, 2.0f); add (b); }
    { B b (C::Poly); b.name ("Digital Glass").tags ({ "Bright", "Modern", "Metallic" }).desc ("No saws, no filter: a pure FM poly, clean and glassy.", "harder = more glass.");
      b.clear().op (1, 3.0f, 0.75f, 0.001f, 0.9f, 0.25f, 0.5f).op (2, 1.0f, 1.0f, 0.002f, 1.6f, 0.55f, 0.5f, 0.8f)
       .op (3, 7.0f, 0.5f, 0.001f, 0.2f, 0.0f, 0.2f).op (4, 2.0f, 1.0f, 0.002f, 1.2f, 0.4f, 0.5f, 0.4f).det (4, 5.0f)
       .edge (1, 2, 1.6f).edge (3, 4, 0.9f).vel (1, 0.5f, 0.7f).vel (3, 0.5f, 0.8f).filter (FT::Off, 20000.0f)
       .mod (0, S::LFO1, E (1, 2), 0.0f).lfo (1, 0.3f).mod (1, S::LFO2, pc::pan, 0.0f).lfo (2, 0.7f)
       .m (5, { { E (1, 2), 0.25f }, { E (3, 4), 0.2f } }).m (6, { { O (4, kOpDetune), 0.15f } }).m (7, { { O (4, kOpCarrierGain), 0.4f } }).mu (8, { { DEPTH (0), 0.1f }, { DEPTH (1), 0.5f } })
       .mname (5, "Glass").mname (6, "Detune").mname (7, "Octave").mname (8, "Motion")
       .uni (1, 0.0f, 40.0f).chorus (0.2f).delaySync (0.16f, 10, 0.35f).reverb (0.22f, 2.4f); add (b); }
    { B b (C::Poly); b.name ("Bite Poly").tags ({ "Bright", "Punchy", "Modern" }).desc ("Aggressive, growling poly: FM bite into the saws and a touch of drive.", "dig in for the growl.");
      b.ratio (4, 1.0f).fb (4, 0.9f).edge (4, 1, 3.0f).edge (4, 2, 2.0f).env (4, 0.002f, 0.5f, 0.35f, 0.25f).vel (4, 0.5f, 0.8f).env (1, 0.002f, 0.6f, 0.45f, 0.2f).env (2, 0.002f, 0.6f, 0.45f, 0.2f).out (3, 0.35f)
       .filter (FT::LP24, 1100.0f, 0.5f, 0.5f).modEnv (0.002f, 0.35f, 0.2f, 0.25f).mod (0, S::ModEnv, pc::filterCutoff, 0.42f).drive (0.55f).uni (1, 0.0f, 30.0f).fxOff (F::Chorus).reverb (0.08f); add (b); }
    { B b (C::Poly); b.name ("Wobble Poly").tags ({ "Modern", "Punchy" }).desc ("The filter wobbles in eighth notes, locked to the tempo.", "hold chords; Motion (Macro 8) sets the depth.");
      b.filter (FT::LP24, 700.0f, 0.5f).mod (0, S::ModEnv, pc::filterCutoff, 0.05f).mod (1, S::LFO1, pc::filterCutoff, 0.32f).lfoSync (1, 9, LfoShape::Sine).out (3, 0.3f).uni (1, 0.0f, 40.0f).chorus (0.15f).reverb (0.14f); add (b); }
    { B b (C::Poly); b.name ("Tremolo Chop").tags ({ "Modern", "Retro" }).desc ("Chords chopped into sixteenths by a square tremolo, with a ping-pong echo.", "hold chords; the rhythm follows the tempo.");
      b.wave (2, W::Pulse).filter (FT::LP24, 3000.0f, 0.2f).mod (0, S::ModEnv, pc::filterCutoff, 0.15f).mod (2, S::LFO2, pc::level, -0.85f).lfoSync (2, 12, LfoShape::Square)
       .env (1, 0.002f, 0.8f, 0.9f, 0.12f).env (2, 0.002f, 0.8f, 0.9f, 0.12f).fxOff (F::Chorus).delaySync (0.28f, 10, 0.45f, 1.0f).reverb (0.14f); add (b); }
    { B b (C::Poly); b.name ("Phase Poly").tags ({ "Warm", "Retro", "Cinematic" }).desc ("Pulse-wave chords swirling through a deep, slow phaser.", "hold long chords.");
      b.wave (1, W::Pulse).wave (2, W::Pulse).det (1, -10.0f).det (2, 10.0f).filter (FT::LP12, 2400.0f, 0.05f).mod (0, S::ModEnv, pc::filterCutoff, 0.06f).env (1, 0.09f, 1.2f, 0.85f, 0.7f).env (2, 0.09f, 1.2f, 0.85f, 0.7f)
       .uni (1, 0.0f, 60.0f).fxOff (F::Chorus).phaser (0.8f, 0.12f, 0.95f, 0.65f).reverb (0.22f, 2.8f); add (b); }
    { B b (C::Poly); b.name ("Fifth Stack").tags ({ "Bright", "Modern", "Punchy" }).desc ("Every key plays a root and a fifth: instant power chords and rave stabs.", "single notes and octaves work best.");
      b.ratio (2, 1.5f).det (1, -5.0f).det (2, 5.0f).out (3, 0.3f).uni (3, 18.0f, 90.0f).poly (8).env (1, 0.002f, 0.55f, 0.12f, 0.3f).env (2, 0.002f, 0.55f, 0.12f, 0.3f).env (3, 0.002f, 0.55f, 0.12f, 0.3f)
       .filter (FT::LP24, 5200.0f, 0.2f).mod (0, S::ModEnv, pc::filterCutoff, 0.2f).modEnv (0.002f, 0.4f, 0.1f, 0.3f).chorus (0.12f).delaySync (0.18f, 10, 0.35f).reverb (0.24f, 2.2f); add (b); }
    { B b (C::Poly); b.name ("Stab Poly").tags ({ "Punchy", "Bright", "Retro" }).desc ("Short chord stabs that get out of the way.", "play rhythmically; velocity sets the brightness.");
      b.env (1, 0.002f, 0.22f, 0.0f, 0.12f).env (2, 0.002f, 0.22f, 0.0f, 0.12f).env (3, 0.002f, 0.22f, 0.0f, 0.12f).modEnv (0.002f, 0.16f, 0.0f, 0.12f).filter (FT::LP24, 1500.0f, 0.3f).mod (0, S::ModEnv, pc::filterCutoff, 0.45f).uni (1, 0.0f, 50.0f).fxOff (F::Chorus).delaySync (0.2f, 10, 0.4f).reverb (0.16f, 1.4f); add (b); }
    { B b (C::Poly); b.name ("Sweep Poly").tags ({ "Modern", "Cinematic" }).desc ("A resonant filter that opens and closes once per bar.", "hold long chords and let the sweep do the work.");
      b.filter (FT::LP24, 500.0f, 0.65f).mod (0, S::ModEnv, pc::filterCutoff, 0.0f).mod (1, S::LFO1, pc::filterCutoff, 0.42f).lfoSync (1, 0, LfoShape::Triangle).wave (1, W::Pulse).chorus (0.25f).delaySync (0.2f, 10, 0.45f).reverb (0.24f, 2.8f); add (b); }
    { B b (C::Poly); b.name ("Dream Drift").tags ({ "Soft", "Cinematic", "Warm" }).desc ("Slow-blooming chords that drift from side to side through a flanger.", "hold chords; leave space between changes.");
      b.wave (1, W::Triangle).wave (2, W::Saw).out (1, 0.9f).out (2, 0.35f).ratio (2, 2.0f).env (1, 0.25f, 2.0f, 0.85f, 1.6f).env (2, 0.4f, 2.0f, 0.7f, 1.6f).modEnv (0.3f, 1.5f, 0.5f, 1.2f)
       .filter (FT::LP12, 2400.0f, 0.1f).mod (0, S::ModEnv, pc::filterCutoff, 0.15f).mod (2, S::LFO2, pc::pan, 0.6f).lfo (2, 0.22f).fxOff (F::Chorus).flanger (0.45f, 0.12f, 0.7f, 0.5f).delaySync (0.22f, 10, 0.5f, 0.9f).reverb (0.4f, 4.6f, 0.85f); add (b); }

    return out;
}
} // namespace gen
