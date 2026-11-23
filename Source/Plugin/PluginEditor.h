#pragma once
#include "Plugin/PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include "UI/Advanced/AdvancedPage.h"
#include "UI/Browser/PresetBrowser.h"
#include "UI/Common/Header.h"
#include "UI/Simple/SimplePage.h"

class FmlumeAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer,
                                         private juce::MidiKeyboardState::Listener,
                                         private juce::FocusChangeListener
{
public:
    explicit FmlumeAudioProcessorEditor (FmlumeAudioProcessor&);
    ~FmlumeAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    // Where keyboard focus must move when `focused` receives it (nullptr = leave it there):
    // anything in this editor that is neither a text field nor the sink hands it to the sink.
    juce::Component* focusTargetFor (juce::Component* focused);

private:
    void timerCallback() override;
    void globalFocusChanged (juce::Component* focused) override;
    void refreshAll();
    void showPage (bool advanced);
    void showBrowser (bool show, bool pickForSlotB = false);
    void savePresetDialog();
    void fileDialog (bool save);
    void setStatus (const juce::String& text, bool error);
    void handleNoteOn (juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int channel, int note, float velocity) override;

    FmlumeAudioProcessor& processor;
    ui::LumeLookAndFeel lookAndFeel;
    ui::Ctx ctx;
    juce::TooltipWindow tooltips { this, 600 };
    ui::Header header;
    ui::SimplePage simple;
    ui::AdvancedPage advanced;
    ui::PresetBrowser browser;
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };
    ui::PerformanceWheel bendWheel { ui::PerformanceWheel::Kind::pitchBend, "BEND" };
    ui::PerformanceWheel modWheel { ui::PerformanceWheel::Kind::modulation, "MOD" };
    // Keyboard strip as in POLYBRISK: 88 keys (A0..C8) filling the width, wheels on the left.
    static constexpr int keyboardHeight = 84, wheelWidth = 34, wheelAreaWidth = 2 * wheelWidth + 16;
    static constexpr int lowestKey = 21, highestKey = 108, whiteKeys = 52;
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;
    uint32_t seenVersion = 0;
    int statusCountdown = 0, seenTab = -1, seenHostKey = -1;
    bool showingAdvanced = false, showingBrowser = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FmlumeAudioProcessorEditor)
};
