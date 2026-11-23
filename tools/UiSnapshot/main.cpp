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
            for (int i = 0; i < 6; ++i)
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
        static const char* const tabs[] = { "Operators", "Algorithm", "FilterMod", "FX", "Rhythm", "Performance" };
        for (int i = 0; i < 6; ++i)
        {
            if (auto* t = findOf<juce::TabbedComponent> (*editor)) t->setCurrentTabIndex (i);
            pump();
            save (*editor, dir.getChildFile (juce::String ("advanced_") + tabs[i] + ".png"));
            if (i == 4)
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
        if (auto* t = findOf<juce::TabbedComponent> (*editor)) t->setCurrentTabIndex (1);
        for (int i = 0; i < 3; ++i)
        {
            for (int k = 0; k < 13; ++k) clickButton (*editor, ">");
            pump();
            save (*editor, dir.getChildFile ("algorithm_preset_" + juce::String (i) + ".png"));
        }
        clickButton (*editor, "SIMPLE");
        editor->setSize (960, 620);
        pump();
        save (*editor, dir.getChildFile ("simple_min_size.png"));
        clickButton (*editor, "ADVANCED");
        for (int i = 0; i < 2; ++i)
        {
            if (auto* t = findOf<juce::TabbedComponent> (*editor)) t->setCurrentTabIndex (i);
            pump();
            save (*editor, dir.getChildFile (juce::String ("advanced_min_") + tabs[i] + ".png"));
        }
        editor.reset();
        processor.reset();
        quit();
    }
    void shutdown() override {}
};

START_JUCE_APPLICATION (SnapshotApp)
