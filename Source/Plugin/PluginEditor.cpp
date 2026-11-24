#include "Plugin/PluginEditor.h"

using namespace fmlume;

// ================================================================================== editor
FmlumeAudioProcessorEditor::FmlumeAudioProcessorEditor (FmlumeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), ctx (p), header (ctx), simple (ctx), advanced (ctx), browser (ctx)
{
    setLookAndFeel (&lookAndFeel);
    ctx.status = [this] (const juce::String& t, bool e) { setStatus (t, e); };
    ctx.refreshAll = [this] { refreshAll(); };
    ctx.openBrowser = [this] { showBrowser (true); };

    addAndMakeVisible (header);
    addAndMakeVisible (simple);
    addChildComponent (advanced);
    addChildComponent (browser);
    header.simpleBtn.onClick = [this] { if (header.simpleBtn.getToggleState()) showPage (false); };
    header.advancedBtn.onClick = [this] { if (header.advancedBtn.getToggleState()) showPage (true); };
    header.onSave = [this] { savePresetDialog(); };
    header.onBrowse = [this] { showBrowser (! showingBrowser); };
    browser.onClose = [this] { showBrowser (false); };
    browser.onSaveAs = [this] { savePresetDialog(); };
    browser.onImport = [this] { fileDialog (false); };
    browser.onExport = [this] { fileDialog (true); };

    keyboardState.addListener (this);
    keyboard.setAvailableRange (lowestKey, highestKey);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffd9dde6));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, ui::col::accent.withAlpha (0.75f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, ui::col::accent.withAlpha (0.25f));
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);
    bendWheel.setTooltip ("Pitch bend. Drag up or down; it springs back when released.");
    modWheel.setTooltip ("Mod wheel (MIDI CC 1). Stays where you leave it; double-click to reset.");
    bendWheel.onChange = [this] (float v) { processor.setUiPitchBend (v); };
    modWheel.onChange = [this] (float v) { processor.setUiModWheel (v); };
    addAndMakeVisible (bendWheel);
    addAndMakeVisible (modWheel);

    ui::styleLabel (status, 12.5f, ui::col::textDim);
    status.setText ("Ready. Hover over any control for help.", juce::dontSendNotification);
    addAndMakeVisible (status);

    setResizable (true, true);
    setResizeLimits (960, 660, 1920, 1240);
    setSize (1200, 800);
    refreshAll();
    // The host keeps its keys (docs/DECISIONS.md D-043, D-049): only text fields hold the keyboard.
    // The editor itself is the sink: it accepts focus but handles no key, so keys go to the host.
    ui::markAsFocusSink (*this);
    ui::releaseKeysAfterTextEntry (*this);
    juce::Desktop::getInstance().addFocusChangeListener (this);
    startTimerHz (30);
}

FmlumeAudioProcessorEditor::~FmlumeAudioProcessorEditor()
{
    stopTimer();
    juce::Desktop::getInstance().removeFocusChangeListener (this);
    // Keys or the bend wheel may still be held on screen when the window closes.
    keyboardState.allNotesOff (0);
    processor.setUiPitchBend (0.0f);
    keyboardState.removeListener (this);
    // An unfinished variation audition must not outlive the editor.
    processor.withModel ([] (PatchController& pc) { pc.cancelPreview(); return 0; });
    setLookAndFeel (nullptr);
}

juce::Component* FmlumeAudioProcessorEditor::focusTargetFor (juce::Component* focused)
{
    // other windows and popup menus are none of our business
    if (focused == nullptr || focused == this || ! isParentOf (focused)) return nullptr;
    return ui::mayHoldKeyboardFocus (*focused) ? nullptr : this;
}

void FmlumeAudioProcessorEditor::globalFocusChanged (juce::Component* focused)
{
    if (auto* target = focusTargetFor (focused)) target->grabKeyboardFocus();
}

void FmlumeAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (ui::col::bg); }

void FmlumeAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    header.setBounds (r.removeFromTop (54));
    auto strip = r.removeFromBottom (keyboardHeight);
    status.setBounds (r.removeFromBottom (18).reduced (14, 0));
    auto wheels = strip.removeFromLeft (wheelAreaWidth).reduced (5, 2);
    bendWheel.setBounds (wheels.removeFromLeft (wheelWidth));
    wheels.removeFromLeft (6);
    modWheel.setBounds (wheels.removeFromLeft (wheelWidth));
    keyboard.setKeyWidth ((float) strip.getWidth() / (float) whiteKeys); // 88 keys fill the strip
    keyboard.setBounds (strip);
    simple.setBounds (r);
    advanced.setBounds (r);
    browser.setBounds (r);
}

void FmlumeAudioProcessorEditor::showPage (bool adv)
{
    showingAdvanced = adv;
    showBrowser (false);
}

void FmlumeAudioProcessorEditor::showBrowser (bool show)
{
    showingBrowser = show;
    header.browserOpen = show;
    browser.setVisible (show);
    simple.setVisible (! show && ! showingAdvanced);
    advanced.setVisible (! show && showingAdvanced);
    if (show) browser.open();
    refreshAll();
}

void FmlumeAudioProcessorEditor::refreshAll()
{
    seenVersion = processor.getModelVersion();
    const Preset p = ctx.snapshot();
    header.refresh (p);
    // Every page edits the same model; only the visible one needs updating now.
    if (showingBrowser) browser.refresh (p);
    else if (showingAdvanced) advanced.refresh (p);
    else simple.refresh (p);
    const juce::String stateError = processor.takeStateError();
    if (stateError.isNotEmpty())
    {
        setStatus ("The sound saved in this project could not be restored (" + stateError + "). The current sound was kept.", true);
        statusCountdown = 30 * 30; // stays for 30 s
    }
}

void FmlumeAudioProcessorEditor::timerCallback()
{
    header.tick();
    float wheel = 0.0f;
    if (processor.takeMidiPitchBend (wheel)) bendWheel.setDisplayValue (wheel);
    if (processor.takeMidiModWheel (wheel)) modWheel.setDisplayValue (wheel);
    // Refresh on a model change, a tab change, or a change of the host parameters that some
    // read-outs are derived from (voice budget); those do not bump the model version.
    const int tab = advanced.tabs.getCurrentTabIndex();
    const fmlume::HostParams h = processor.currentHostParams();
    const int hostKey = h.polyphony | (h.unison << 6) | (h.quality << 10) | ((h.arpEnabled ? 1 : 0) << 13) | ((h.gateEnabled ? 1 : 0) << 14);
    if (processor.getModelVersion() != seenVersion || tab != seenTab || hostKey != seenHostKey)
    {
        seenTab = tab;
        seenHostKey = hostKey;
        refreshAll();
    }
    if (statusCountdown > 0 && --statusCountdown == 0)
    {
        status.setColour (juce::Label::textColourId, ui::col::textDim);
        status.setText ({}, juce::dontSendNotification);
    }
}

void FmlumeAudioProcessorEditor::setStatus (const juce::String& text, bool error)
{
    status.setColour (juce::Label::textColourId, error ? ui::col::bad : ui::col::text);
    status.setText ((error ? "! " : "") + text, juce::dontSendNotification);
    statusCountdown = 30 * (error ? 8 : 5);
}

void FmlumeAudioProcessorEditor::handleNoteOn (juce::MidiKeyboardState*, int, int note, float velocity) { processor.uiNote (note, velocity, true); }
void FmlumeAudioProcessorEditor::handleNoteOff (juce::MidiKeyboardState*, int, int note, float) { processor.uiNote (note, 0.0f, false); }

void FmlumeAudioProcessorEditor::savePresetDialog()
{
    const Preset p = ctx.snapshot();
    auto* w = new juce::AlertWindow ("Save Preset", "Saved to your user preset folder. Factory presets are never overwritten.", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", ui::utf8 (p.meta.displayName), "Name");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<FmlumeAudioProcessorEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result) {
        const juce::String name = w->getTextEditorContents ("name").trim();
        if (safe == nullptr || result != 1 || name.isEmpty()) return;
        auto doSave = [safe, name] (bool overwrite) {
            if (safe == nullptr) return;
            const juce::String err = safe->processor.savePreset (name, overwrite);
            if (err.isNotEmpty()) safe->setStatus ("Save failed: " + err, true);
            else { safe->setStatus ("Saved \"" + name + "\".", false); safe->refreshAll(); }
        };
        if (safe->processor.presetManager().userPresetExists (name.toStdString()))
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                              .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                              .withTitle ("Overwrite preset?")
                                              .withMessage ("A user preset named \"" + name + "\" already exists.")
                                              .withButton ("Overwrite", 1)
                                              .withButton ("Cancel", 0)
                                              .withAssociatedComponent (safe.getComponent()),
                                          [doSave] (int r) { if (r == 1) doSave (true); });
        }
        else doSave (false);
    }), true);
}

void FmlumeAudioProcessorEditor::fileDialog (bool save)
{
    const juce::File userDir (processor.presetManager().getUserDirectory());
    chooser = std::make_unique<juce::FileChooser> (save ? "Export preset" : "Import preset", userDir.exists() ? userDir : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.fmlume");
    const int flags = save ? (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting)
                           : (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles);
    juce::Component::SafePointer<FmlumeAudioProcessorEditor> safe (this);
    chooser->launchAsync (flags, [safe, save] (const juce::FileChooser& fc) {
        if (safe == nullptr) return;
        juce::File f = fc.getResult();
        if (f == juce::File()) return;
        if (save && ! f.hasFileExtension ("fmlume")) f = f.withFileExtension ("fmlume");
        juce::String imported;
        const juce::String err = save ? safe->processor.exportPreset (f) : safe->processor.importPreset (f, &imported);
        if (err.isNotEmpty()) safe->setStatus ((save ? "Export failed: " : "Import refused: ") + err, true);
        else
        {
            safe->setStatus (save ? "Exported " + f.getFileName() : "Imported as user preset \"" + imported + "\".", false);
            safe->refreshAll(); // the browser stays open and shows the new user preset
        }
    });
}
