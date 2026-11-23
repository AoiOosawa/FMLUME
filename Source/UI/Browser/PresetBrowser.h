// Full-page preset browser (same layout as POLYBRISK's): search, category, favourites, tag
// chips, list, and a side pane with the selected sound's details and library actions.
#pragma once
#include "UI/Common/Widgets.h"

namespace ui
{
class PresetBrowser final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (Ctx&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void open (bool pickForSlotB);
    void refresh (const fmlume::Preset&);
    void rebuild();
    std::function<void()> onClose, onSaveAs, onImport, onExport;

private:
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int row) override;
    void returnKeyPressed (int row) override;
    void choose (int row);
    void updateInfo();
    void rebuildTags();
    const fmlume::PresetEntry* entryAt (int row) const;

    Ctx& ctx;
    juce::Label title, counts, info, pickHint;
    juce::TextEditor search;
    juce::ComboBox category;
    juce::TextButton favOnly { "Favorites" }, clearTags { "Clear" }, close { "Close" };
    juce::TextButton init { "Init..." }, saveAs { "Save As..." }, del { "Delete" }, import { "Import..." }, exportBtn { "Export..." },
        folder { "Show Folder" }, rescan { "Rescan" };
    juce::OwnedArray<juce::TextButton> tags;
    juce::ListBox list;
    std::vector<int> rows;
    std::string currentId;
    juce::String activeTag;
    int knownCount = -1;
    bool pickMode = false;
};
} // namespace ui
