// Simple view. Laid out like POLYBRISK's: big macros on top, then FX / rhythm switches, then
// the FMLUME special (Variations).
#pragma once
#include "UI/Common/Bindings.h"

namespace ui
{
// Macros 1-4 (always the same four) or 5-8 (per category).
class MacroPanel final : public juce::Component
{
public:
    MacroPanel (Ctx&, int firstMacro, const juce::String& title, bool large);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (const fmlume::Preset&);

private:
    Ctx& ctx;
    int first;
    juce::String title;
    juce::OwnedArray<HostKnob> knobs;
};

class QuickFxPanel final : public juce::Component
{
public:
    explicit QuickFxPanel (Ctx&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::OwnedArray<HostKnob> knobs;
    HostToggle bypass;
};

// Trance gate / arpeggiator switches with their one or two most useful choices.
class RhythmPanel final : public juce::Component
{
public:
    RhythmPanel (Ctx&, bool arp);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (const fmlume::Preset&);

private:
    bool isArp;
    HostToggle power;
    GatePatternBox pattern;
    BoundCombo mode, rate;
    BoundToggle stereo;  // gate only
    BoundSlider width;   // gate only
    juce::Label caption1, caption2;
};

// Sound Explorer, presented as "Variations": generate -> click tiles to listen -> keep one.
class ExplorerPanel final : public juce::Component, private juce::Timer
{
public:
    explicit ExplorerPanel (Ctx&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (const fmlume::Preset&);

private:
    struct Tile final : public juce::Component, public juce::SettableTooltipClient
    {
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent& e) override { if (isEnabled() && getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        juce::String letter, name, detail;
        bool selected = false, original = false;
        bool pending = false;  // being made: a shimmer instead of text
        float reveal = 1.0f;   // 0 -> 1 as a finished variation fades in
        float phase = 0.0f;    // shimmer position
        std::function<void()> onClick;
    };
    void pushSettings();
    bool run (const std::function<bool (fmlume::PatchController&)>& fn);
    void keepMenu();
    void timerCallback() override;

    Ctx& ctx;
    juce::ComboBox direction, strength;
    juce::TextButton generate { "Generate" }, keepUnchanged { "Keep unchanged..." }, use { "Keep this sound" }, more { "More like this" },
        back { "Previous set" }, done { "Done" };
    juce::Label guide;
    Tile originalTile;
    juce::OwnedArray<Tile> tiles;
    fmlume::ExploreLocks locks;
    int previewing = -1;
    bool busy = false;
};

class SimplePage final : public juce::Component
{
public:
    explicit SimplePage (Ctx&);
    void resized() override;
    void refresh (const fmlume::Preset&);

private:
    MacroPanel macros, character;
    QuickFxPanel quick;
    RhythmPanel gate, arp;
    ExplorerPanel explorer;
    juce::Label about;
};
} // namespace ui
