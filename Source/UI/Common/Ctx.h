// The editor-wide context every panel receives: access to the processor / model and a few
// pieces of shared UI state.
#pragma once
#include "Core/Params.h"
#include "Plugin/PluginProcessor.h"

namespace ui
{
// Shared editor services handed to every panel.
struct Ctx
{
    explicit Ctx (FmlumeAudioProcessor& p) : proc (p) {}
    FmlumeAudioProcessor& proc;
    std::function<void (const juce::String&, bool isError)> status;
    std::function<void()> refreshAll;
    std::function<void()> openBrowser; // full-page preset browser
    int selectedOp = 0;             // operator selected in the Operators / Algorithm tabs
    bool snapRatios = true;

    fmlume::Preset snapshot() { return proc.withModel ([] (fmlume::PatchController& c) { return c.preset(); }); }
    float get (int code) { return proc.withModel ([=] (fmlume::PatchController& c) { return c.getParam (code); }); }
    bool exists (int code) { return proc.withModel ([=] (fmlume::PatchController& c) { return fmlume::paramExists (c.data(), code); }); }
    bool set (int code, float value) { return proc.withModel ([=] (fmlume::PatchController& c) { return c.setParam (code, value); }); }
    // Runs a model command; when it is refused, shows `errorPrefix` + the model's reason in the status line.
    bool run (const std::function<bool (fmlume::PatchController&)>& command, const juce::String& errorPrefix = "Not applied: ");
    // Runs a model edit; reports a rejected edit in the status line.
    bool edit (const std::function<void (fmlume::Preset&)>& fn, bool forceNew = false);
    void say (const juce::String& text, bool error = false) { if (status) status (text, error); }
};
} // namespace ui
