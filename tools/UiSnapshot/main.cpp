// Development tool: renders the plug-in editor off-screen to PNG files (no host needed).
#include "Plugin/PluginEditor.h"
#include <cstdio>
#include <functional>
#include <typeinfo>

static void save (juce::Component& c, const juce::File& file)
{
    const auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 2.0f);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    png.writeImageToStream (image, out);
}

static void clickButton (juce::Component& root, const juce::String& text)
{
    for (auto* child : root.getChildren())
    {
        if (auto* b = dynamic_cast<juce::Button*> (child))
            if (b->getButtonText() == text && b->isShowing()) { b->triggerClick(); return; }
        clickButton (*child, text);
    }
}

template <typename T>
static T* findOf (juce::Component& root)
{
    for (auto* child : root.getChildren())
    {
        if (auto* t = dynamic_cast<T*> (child)) return t;
        if (auto* t = findOf<T> (*child)) return t;
    }
    return nullptr;
}

struct SnapshotApp final : public juce::JUCEApplication
{
    const juce::String getApplicationName() override { return "fmlume-uisnap"; }
    const juce::String getApplicationVersion() override { return "1"; }
    void initialise (const juce::String& args) override
    {
        // "--icon <in.svg> <out.png> [size]": rasterise a branding SVG with a transparent background.
        const auto tokens = juce::StringArray::fromTokens (args, true);
        if (tokens.size() >= 3 && tokens[0] == "--icon")
        {
            const int size = tokens.size() > 3 ? tokens[3].getIntValue() : 1024;
            if (auto svg = juce::Drawable::createFromSVGFile (juce::File::getCurrentWorkingDirectory().getChildFile (tokens[1].unquoted())))
            {
                juce::Image image (juce::Image::ARGB, size, size, true);
                juce::Graphics g (image);
                // Scale by the SVG's own canvas size (argument 5, default 1024) so margins are kept.
                const float canvas = tokens.size() > 4 ? tokens[4].getFloatValue() : 1024.0f;
                svg->draw (g, 1.0f, juce::AffineTransform::scale ((float) size / canvas));
                const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (tokens[2].unquoted());
                out.deleteFile();
                juce::FileOutputStream stream (out);
                juce::PNGImageFormat().writeImageToStream (image, stream);
            }
            quit();
            return;
        }
        // "--check-keys": every component of every page must hand the keyboard to the sink unless it
        // is a text field, and every text field must give it back after typing (D-049).
        if (tokens.size() >= 1 && tokens[0] == "--check-keys")
        {
            auto processor = std::make_unique<FmlumeAudioProcessor>();
            processor->prepareToPlay (48000.0, 512);
            std::unique_ptr<juce::AudioProcessorEditor> ed (processor->createEditor());
            auto* editor = dynamic_cast<FmlumeAudioProcessorEditor*> (ed.get());
            ed->setVisible (true);
            ed->addToDesktop (0);
            auto pump = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (150); };
            int checked = 0, textFields = 0, problems = 0;
            std::function<void (juce::Component&)> walk = [&] (juce::Component& c) {
                ++checked;
                const bool text = dynamic_cast<juce::TextEditor*> (&c) != nullptr;
                auto* target = editor->focusTargetFor (&c);
                if (text)
                {
                    ++textFields;
                    if (target != nullptr) { std::printf ("PROBLEM: a text field would lose the keyboard\n"); ++problems; }
                    if (! c.getProperties().contains ("fmlumeReleasesFocus")) { std::printf ("PROBLEM: text field '%s' never gives the keyboard back\n", c.getName().toRawUTF8()); ++problems; }
                    return;
                }
                if (target != editor) { std::printf ("PROBLEM: %s '%s' can keep the keyboard\n", typeid (c).name(), c.getName().toRawUTF8()); ++problems; }
                for (auto* child : c.getChildren()) walk (*child);
            };
            const auto walkAll = [&] { for (auto* child : ed->getChildren()) walk (*child); };
            pump(); walkAll();                                   // Simple
            if (auto* header = findOf<ui::Header> (*ed)) if (header->onBrowse) header->onBrowse();
            pump(); walkAll();                                   // browser
            clickButton (*ed, "Close");
            clickButton (*ed, "ADVANCED");
            for (int i = 0; i < 5; ++i)
            {
                if (auto* t = findOf<juce::TabbedComponent> (*ed)) t->setCurrentTabIndex (i);
                pump(); walkAll();                               // every Advanced tab
            }
            std::printf ("key handling: %d components checked (%d text fields), %d problem(s)\n", checked, textFields, problems);
            ed.reset();
            processor.reset();
            setApplicationReturnValue (problems == 0 ? 0 : 1);
            quit();
            return;
        }
        // "--check-fit": does "Fit macros 1-4 to routing" ask before it replaces the assignments?
        if (tokens.size() >= 1 && tokens[0] == "--check-fit")
        {
            auto processor = std::make_unique<FmlumeAudioProcessor>();
            processor->prepareToPlay (48000.0, 512);
            std::unique_ptr<juce::AudioProcessorEditor> ed (processor->createEditor());
            ed->setVisible (true);
            ed->addToDesktop (0);
            auto pump = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (300); };
            clickButton (*ed, "ADVANCED");
            pump();
            const auto alerts = [] {
                int n = 0;
                for (int i = 0; i < juce::TopLevelWindow::getNumTopLevelWindows(); ++i)
                    if (dynamic_cast<juce::AlertWindow*> (juce::TopLevelWindow::getTopLevelWindow (i)) != nullptr) ++n;
                return n;
            };
            for (int step = 0; step < 2; ++step)
            {
                if (step == 1) processor->withModel ([] (fmlume::PatchController& pc) { return pc.addEdge (1, 2); }); // a new connection by hand (OP 2 > OP 3, both on)
                pump();
                bool enabled = false;
                std::function<void (juce::Component&)> state = [&] (juce::Component& c) {
                    if (auto* b = dynamic_cast<juce::Button*> (&c)) if (b->getButtonText() == "Fit macros 1-4 to routing") enabled = b->isEnabled();
                    for (auto* ch : c.getChildren()) state (*ch);
                };
                state (*ed);
                std::printf ("  button %s\n", enabled ? "enabled" : "disabled");
                clickButton (*ed, "Fit macros 1-4 to routing");
                pump();
                juce::String status;
                std::function<void (juce::Component&)> find = [&] (juce::Component& c) {
                    if (auto* l = dynamic_cast<juce::Label*> (&c)) if (l->getText().contains ("acros 1-4")) status = l->getText();
                    for (auto* ch : c.getChildren()) find (*ch);
                };
                find (*ed);
                std::printf ("%s: alert windows %d, modal %d, status \"%s\"\n", step == 0 ? "as loaded" : "after adding a connection", alerts(),
                             juce::ModalComponentManager::getInstance()->getNumModalComponents(), status.toRawUTF8());
                for (int i = juce::TopLevelWindow::getNumTopLevelWindows(); --i >= 0;)
                    if (auto* a = dynamic_cast<juce::AlertWindow*> (juce::TopLevelWindow::getTopLevelWindow (i))) a->exitModalState (0);
                pump();
            }
            ed.reset();
            processor.reset();
            quit();
            return;
        }
        // "--check-edited <dir>": the "*" beside the preset name follows the host parameters a preset
        // stores (macros, ...), through the real processor and editor. Writes the header before / after.
        if (tokens.size() >= 1 && tokens[0] == "--check-edited")
        {
            const juce::File out (tokens.size() > 1 ? tokens[1].unquoted() : juce::File::getCurrentWorkingDirectory().getFullPathName());
            out.createDirectory();
            auto processor = std::make_unique<FmlumeAudioProcessor>();
            processor->prepareToPlay (48000.0, 512);
            std::unique_ptr<juce::AudioProcessorEditor> ed (processor->createEditor());
            ed->setVisible (true);
            ed->addToDesktop (0);
            auto pump = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (200); };
            int problems = 0;
            const auto expect = [&] (bool wanted, const char* what) {
                const bool got = processor->isPresetDirty();
                std::printf ("%s %s: edited=%d (expected %d)\n", got == wanted ? "ok     " : "PROBLEM", what, (int) got, (int) wanted);
                if (got != wanted) ++problems;
            };
            const auto setNorm = [&] (const char* id, float v) { processor->apvts.getParameter (id)->setValueNotifyingHost (v); };
            auto* header = findOf<ui::Header> (*ed);
            pump();
            expect (false, "after loading the first preset");
            if (header != nullptr) save (*header, out.getChildFile ("header_clean.png"));
            const float macro = processor->apvts.getParameter ("macro.1")->getValue();
            setNorm ("macro.1", macro > 0.5f ? macro - 0.3f : macro + 0.3f);
            pump();
            expect (true, "macro 1 moved");
            if (header != nullptr) save (*header, out.getChildFile ("header_edited.png"));
            setNorm ("macro.1", macro);
            pump();
            expect (false, "macro 1 back on the stored value");
            const float gain = processor->apvts.getParameter ("master.gainDb")->getValue();
            setNorm ("master.gainDb", gain * 0.5f);
            expect (false, "master gain moved (session setting, not in the preset)");
            setNorm ("fx.quick.space", 0.93f);
            expect (true, "quick FX Space moved");
            juce::MemoryBlock state;
            processor->getStateInformation (state);
            processor->setStateInformation (state.getData(), (int) state.getSize());
            pump();
            expect (false, "project state saved and restored");
            processor->stepPreset (1);
            pump();
            expect (false, "next preset loaded");
            if (header != nullptr) save (*header, out.getChildFile ("header_next.png"));
            ed.reset();
            processor.reset();
            setApplicationReturnValue (problems == 0 ? 0 : 1);
            quit();
            return;
        }
        // "--guide <dir>": the pictures of docs/USER_GUIDE.md, with a preset that uses most operators.
        if (tokens.size() >= 2 && tokens[0] == "--guide")
        {
            const juce::File dir (tokens[1]);
            dir.createDirectory();
            auto processor = std::make_unique<FmlumeAudioProcessor>();
            processor->prepareToPlay (48000.0, 512);
            const auto& list = processor->presetManager().entries();
            for (int i = 0; i < (int) list.size(); ++i)
                if (list[(size_t) i].name == "Classic Tines") processor->loadPresetIndex (i);
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
            auto pump = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (200); };
            auto tab = [&] (int i) { if (auto* t = findOf<juce::TabbedComponent> (*editor)) t->setCurrentTabIndex (i); pump(); };
            editor->setVisible (true);
            editor->addToDesktop (0);
            pump();
            save (*editor, dir.getChildFile ("simple.png"));
            clickButton (*editor, "Generate");
            for (int i = 0; i < 60; ++i)
            {
                pump();
                if (! processor->withModel ([] (fmlume::PatchController& pc) { return pc.exploreBusy(); })) break;
            }
            processor->withModel ([] (fmlume::PatchController& pc) { return pc.preview (1); });
            pump();
            save (*editor, dir.getChildFile ("variations.png"));
            clickButton (*editor, "Done");
            if (auto* header = findOf<ui::Header> (*editor); header != nullptr && header->onBrowse) header->onBrowse();
            pump();
            save (*editor, dir.getChildFile ("browser.png"));
            clickButton (*editor, "Close");
            clickButton (*editor, "ADVANCED");
            tab (0);
            save (*editor, dir.getChildFile ("operators.png"));
            clickButton (*editor, "Templates...");
            pump();
            save (*editor, dir.getChildFile ("templates.png"));
            if (auto* box = findOf<juce::CallOutBox> (*editor)) box->dismiss();
            for (int i = 0; i < 5; ++i) pump();
            clickButton (*editor, "Expand");
            pump();
            save (*editor, dir.getChildFile ("operators_expanded.png"));
            clickButton (*editor, "Collapse");
            tab (1);
            save (*editor, dir.getChildFile ("filter_mod.png"));
            tab (2);
            save (*editor, dir.getChildFile ("fx.png"));
            tab (3);
            if (auto* box = findOf<ui::GatePatternBox> (*editor))
                for (int k = 0; k < box->getNumItems(); ++k)
                    if (box->getItemText (k) == "Split Gallop") box->setSelectedItemIndex (k, juce::sendNotificationSync);
            pump();
            save (*editor, dir.getChildFile ("rhythm.png"));
            tab (4);
            save (*editor, dir.getChildFile ("performance.png"));
            editor.reset();
            processor.reset();
            quit();
            return;
        }
        const juce::File dir (args.unquoted().isEmpty() ? juce::File::getCurrentWorkingDirectory().getFullPathName() : args.unquoted());
        dir.createDirectory();
        auto processor = std::make_unique<FmlumeAudioProcessor>();
        processor->prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
        auto pump = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (200); };
        editor->setVisible (true);
        editor->addToDesktop (0);
        pump();
        save (*editor, dir.getChildFile ("simple.png"));
        clickButton (*editor, "Generate");
        // variations are made in the background: one frame while they arrive, then the finished set
        juce::MessageManager::getInstance()->runDispatchLoopUntil (110);
        save (*editor, dir.getChildFile ("simple_variations_making.png"));
        for (int i = 0; i < 40; ++i)
        {
            pump();
            if (! processor->withModel ([] (fmlume::PatchController& pc) { return pc.exploreBusy(); })) break;
        }
        pump();
        save (*editor, dir.getChildFile ("simple_variations.png"));
        clickButton (*editor, "Done");
        auto* header = findOf<ui::Header> (*editor);
        if (header != nullptr && header->onBrowse) header->onBrowse();
        pump();
        save (*editor, dir.getChildFile ("browser.png"));
        clickButton (*editor, "Close");
        clickButton (*editor, "ADVANCED");
        pump();
        static const char* const tabs[] = { "Operators", "FilterMod", "FX", "Rhythm", "Performance" };
        for (int i = 0; i < 5; ++i)
        {
            if (auto* t = findOf<juce::TabbedComponent> (*editor)) t->setCurrentTabIndex (i);
            pump();
            save (*editor, dir.getChildFile (juce::String ("advanced_") + tabs[i] + ".png"));
            if (i == 0)
            {
                clickButton (*editor, "Expand");
                pump();
                save (*editor, dir.getChildFile ("advanced_Operators_expanded.png"));
                clickButton (*editor, "Collapse");
            }
            if (juce::String (tabs[i]) == "Rhythm")
            {
                // a stereo factory pattern with the gate switched on
                if (auto* box = findOf<ui::GatePatternBox> (*editor))
                    for (int k = 0; k < box->getNumItems(); ++k)
                        if (box->getItemText (k) == "Split Gallop") box->setSelectedItemIndex (k, juce::sendNotificationSync);
                clickButton (*editor, "Power");
                pump();
                save (*editor, dir.getChildFile ("advanced_Rhythm_stereo.png"));
                clickButton (*editor, "Power");
                clickButton (*editor, "Clear");
                clickButton (*editor, "Stereo");
            }
        }
        // other presets: is the current algorithm recognisable?
        if (auto* t = findOf<juce::TabbedComponent> (*editor)) t->setCurrentTabIndex (0);
        for (int i = 0; i < 3; ++i)
        {
            for (int k = 0; k < 13; ++k) clickButton (*editor, ">");
            pump();
            save (*editor, dir.getChildFile ("algorithm_preset_" + juce::String (i) + ".png"));
        }
        // A wide routing with feedback on a top-row operator: the loop must stay inside the diagram.
        for (int t : { 14, 15, 17 })
        {
            processor->withModel ([t] (fmlume::PatchController& pc) {
                pc.applyAlgorithmTemplate (t);
                for (int k = 0; k < fmlume::kNumOps; ++k)
                    if (pc.data().sound.engine.operators[(size_t) k].enabled && pc.data().sound.engine.operators[(size_t) k].carrierGain <= 0.0f)
                        pc.setParam (fmlume::pc::op (k, fmlume::kOpFeedback), 0.8f);
                return true;
            });
            if (auto* tc = findOf<juce::TabbedComponent> (*editor)) tc->setCurrentTabIndex (0);
            pump();
            save (*editor, dir.getChildFile ("feedback_" + juce::String (t + 1) + ".png"));
        }
        clickButton (*editor, "SIMPLE");
        editor->setSize (960, 660); // the smallest size the editor allows (setResizeLimits)
        pump();
        save (*editor, dir.getChildFile ("simple_min_size.png"));
        clickButton (*editor, "ADVANCED");
        for (int i = 0; i < 1; ++i)
        {
            if (auto* t = findOf<juce::TabbedComponent> (*editor)) t->setCurrentTabIndex (i);
            pump();
            save (*editor, dir.getChildFile (juce::String ("advanced_min_") + tabs[i] + ".png"));
            clickButton (*editor, "Expand");
            pump();
            save (*editor, dir.getChildFile ("advanced_min_Operators_expanded.png"));
            clickButton (*editor, "Collapse");
        }
        editor.reset();
        processor.reset();
        quit();
    }
    void shutdown() override {}
};

START_JUCE_APPLICATION (SnapshotApp)
