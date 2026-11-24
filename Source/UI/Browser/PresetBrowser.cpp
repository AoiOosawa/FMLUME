#include "UI/Browser/PresetBrowser.h"

namespace ui
{
using namespace fmlume;

PresetBrowser::PresetBrowser (Ctx& c) : ctx (c)
{
    styleLabel (title, 18.0f, col::text);
    title.setFont (font (18.0f, true));
    styleLabel (counts, 13.0f);
    styleLabel (info, 13.5f, col::text, juce::Justification::topLeft);
    for (auto* l : { &title, &counts, &info }) addAndMakeVisible (l);

    search.setTextToShowWhenEmpty ("Search name, tag, description...", col::textDim);
    search.setFont (font (15.0f));
    search.onTextChange = [this] { rebuild(); };
    search.onEscapeKey = [this] { if (onClose) onClose(); };
    addAndMakeVisible (search);

    category.addItem ("All categories", 1);
    for (int i = 0; i < (int) Category::Count; ++i) category.addItem (categoryName ((Category) i), i + 2);
    category.setSelectedId (1, juce::dontSendNotification);
    category.onChange = [this] { rebuild(); };
    addAndMakeVisible (category);

    favOnly.setClickingTogglesState (true);
    favOnly.setTooltip ("Show only the sounds you marked with a star");
    favOnly.onClick = [this] { rebuild(); };
    clearTags.setTooltip ("Clear the tag filter");
    clearTags.onClick = [this] { activeTag.clear(); for (auto* t : tags) t->setToggleState (false, juce::dontSendNotification); rebuild(); };
    close.onClick = [this] { if (onClose) onClose(); };

    init.setTooltip ("Start a new sound from a category's starting point");
    init.onClick = [this] {
        juce::PopupMenu m;
        m.addSectionHeader ("New sound from...");
        for (int i = 0; i < (int) Category::Count; ++i) m.addItem (i + 1, categoryName ((Category) i));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&init), [safe = juce::Component::SafePointer<PresetBrowser> (this)] (int r) {
            if (safe == nullptr || r <= 0) return;
            const Category cat = (Category) (r - 1);
            safe->ctx.proc.withModel ([cat] (PatchController& pc) { pc.newFromCategory (cat); return 0; });
            safe->ctx.say (juce::String ("New ") + categoryName (cat) + " sound. Shape it with the macros, then Save.");
            if (safe->onClose) safe->onClose();
        });
    };
    saveAs.setTooltip ("Save the current sound as a user preset");
    saveAs.onClick = [this] { if (onSaveAs) onSaveAs(); };
    import.setTooltip ("Open a .fmlume file");
    import.onClick = [this] { if (onImport) onImport(); };
    exportBtn.setTooltip ("Write the current sound to a .fmlume file");
    exportBtn.onClick = [this] { if (onExport) onExport(); };
    del.setTooltip ("Delete the selected user preset (factory presets cannot be deleted)");
    del.onClick = [this] {
        const int row = list.getSelectedRow();
        const auto* e = entryAt (row);
        if (e == nullptr || e->factory) return;
        const int index = rows[(size_t) row];
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::WarningIcon)
                                          .withTitle ("Delete preset?")
                                          .withMessage ("\"" + utf8 (e->name) + "\" will be removed from your user presets.")
                                          .withButton ("Delete", 1)
                                          .withButton ("Cancel", 0)
                                          .withAssociatedComponent (this),
                                      [safe = juce::Component::SafePointer<PresetBrowser> (this), index] (int r) {
                                          if (safe == nullptr || r != 1) return;
                                          std::string err;
                                          if (! safe->ctx.proc.presetManager().deleteUser (index, &err)) safe->ctx.say ("Delete failed: " + utf8 (err), true);
                                          safe->knownCount = -1;
                                          safe->refresh (safe->ctx.snapshot());
                                      });
    };
    folder.setTooltip ("Open the user preset folder in Finder");
    folder.onClick = [this] { const juce::File d (ctx.proc.presetManager().getUserDirectory()); d.createDirectory(); d.revealToUser(); };
    rescan.setTooltip ("Re-read the user preset folder");
    rescan.onClick = [this] {
        std::vector<std::string> errors;
        ctx.proc.presetManager().rescanUser (&errors);
        ctx.say (errors.empty() ? juce::String ("User presets rescanned.") : juce::String ((int) errors.size()) + " file(s) skipped: " + utf8 (errors[0]), ! errors.empty());
        knownCount = -1;
        refresh (ctx.snapshot());
    };
    for (auto* b : { &favOnly, &clearTags, &close, &init, &saveAs, &del, &import, &exportBtn, &folder, &rescan }) addAndMakeVisible (b);

    list.setModel (this);
    list.setRowHeight (30);
    list.setOutlineThickness (1);
    addAndMakeVisible (list);
    title.setText ("PRESETS", juce::dontSendNotification);
}

void PresetBrowser::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (16, 10);
    auto top = r.removeFromTop (30);
    close.setBounds (top.removeFromRight (90));
    title.setBounds (top.removeFromLeft (110));
    counts.setBounds (top.removeFromLeft (190));
    r.removeFromTop (8);

    auto side = r.removeFromRight (juce::jlimit (230, 300, r.getWidth() * 28 / 100));
    r.removeFromRight (14);

    auto row = r.removeFromTop (28);
    favOnly.setBounds (row.removeFromRight (100));
    row.removeFromRight (6);
    category.setBounds (row.removeFromRight (190));
    row.removeFromRight (6);
    search.setBounds (row);
    r.removeFromTop (8);

    // Tag chips flow over at most three rows.
    int x = r.getX(), y = r.getY(), maxRows = getHeight() < 560 ? 2 : 3, line = 0;
    auto place = [&] (juce::TextButton& b) {
        const int w = 22 + juce::GlyphArrangement::getStringWidthInt (font (13.0f, true), b.getButtonText());
        if (x + w > r.getRight()) { x = r.getX(); y += 28; ++line; }
        b.setVisible (line < maxRows);
        b.setBounds (x, y, w, 24);
        x += w + 5;
    };
    for (auto* t : tags) place (*t);
    place (clearTags);
    r.removeFromTop ((juce::jmin (line, maxRows - 1) + 1) * 28);
    r.removeFromTop (4);
    list.setBounds (r);

    info.setBounds (side.removeFromTop (juce::jmax (110, side.getHeight() * 36 / 100)));
    side.removeFromTop (8);
    auto button = [&side] (juce::Button& b) { b.setBounds (side.removeFromTop (30)); side.removeFromTop (8); };
    button (init);
    button (saveAs);
    button (del);
    auto pair = side.removeFromTop (30);
    import.setBounds (pair.removeFromLeft (pair.getWidth() / 2 - 4));
    exportBtn.setBounds (pair.withTrimmedLeft (8));
    side.removeFromTop (8);
    pair = side.removeFromTop (30);
    folder.setBounds (pair.removeFromLeft (pair.getWidth() / 2 - 4));
    rescan.setBounds (pair.withTrimmedLeft (8));
}

void PresetBrowser::open()
{
    refresh (ctx.snapshot()); // the search field takes the keyboard only when it is clicked
}

void PresetBrowser::rebuildTags()
{
    tags.clear();
    for (auto& t : ctx.proc.presetManager().allTags())
    {
        auto* b = tags.add (new juce::TextButton (utf8 (t)));
        b->setClickingTogglesState (true);
        b->setToggleState (b->getButtonText() == activeTag, juce::dontSendNotification);
        b->onClick = [this, b] {
            activeTag = b->getToggleState() ? b->getButtonText() : juce::String();
            for (auto* other : tags) if (other != b) other->setToggleState (false, juce::dontSendNotification);
            rebuild();
        };
        addAndMakeVisible (b);
    }
    resized();
}

void PresetBrowser::rebuild()
{
    auto& pm = ctx.proc.presetManager();
    rows = pm.search (search.getText().toStdString(), category.getSelectedId() - 2, activeTag.toStdString(), favOnly.getToggleState());
    clearTags.setEnabled (activeTag.isNotEmpty());
    list.updateContent();
    list.deselectAllRows();
    for (size_t i = 0; i < rows.size(); ++i)
        if (pm.entries()[(size_t) rows[i]].id == currentId) list.selectRow ((int) i, false, true);
    list.repaint();
    updateInfo();
}

void PresetBrowser::refresh (const Preset& p)
{
    auto& pm = ctx.proc.presetManager();
    const int count = (int) pm.entries().size();
    if (count != knownCount)
    {
        knownCount = count;
        int factory = 0;
        for (auto& e : pm.entries()) factory += e.factory ? 1 : 0;
        counts.setText (juce::String (factory) + " factory / " + juce::String (count - factory) + " user", juce::dontSendNotification);
        rebuildTags();
    }
    currentId = p.meta.patchId;
    rebuild();
}

const PresetEntry* PresetBrowser::entryAt (int row) const
{
    if (row < 0 || row >= (int) rows.size()) return nullptr;
    return &ctx.proc.presetManager().entries()[(size_t) rows[(size_t) row]];
}

void PresetBrowser::updateInfo()
{
    const auto* e = entryAt (list.getSelectedRow());
    del.setEnabled (e != nullptr && ! e->factory);
    if (e == nullptr)
    {
        info.setText ("Click a sound to load it.\nClick the star to mark a favorite.", juce::dontSendNotification);
        return;
    }
    juce::String t;
    t << utf8 (e->name) << "\n" << categoryName (e->category) << (e->factory ? "  (Factory)" : "  (User)") << "\n" << joinTags (e->tags) << "\n\n" << utf8 (e->description);
    info.setText (t, juce::dontSendNotification);
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    const auto* e = entryAt (row);
    if (e == nullptr) return;
    auto& pm = ctx.proc.presetManager();
    const bool current = e->id == currentId;
    if (current) { g.setColour (col::accent.withAlpha (0.22f)); g.fillRect (0, 0, w, h); }
    else if (selected) { g.setColour (col::panelHi); g.fillRect (0, 0, w, h); }

    // Favourite: filled star when set, outline otherwise (shape, not colour alone).
    juce::Path star;
    star.addStar ({ 17.0f, (float) h * 0.5f }, 5, 3.6f, 8.0f);
    if (pm.isFavourite (e->id)) { g.setColour (col::warm); g.fillPath (star); }
    else { g.setColour (col::textDim.withAlpha (0.7f)); g.strokePath (star, juce::PathStrokeType (1.1f)); }

    const int nameW = juce::jmax (150, w * 42 / 100), catW = juce::jmax (70, w * 16 / 100);
    g.setColour (col::text);
    g.setFont (font (14.5f, current));
    g.drawText (utf8 (e->name), 36, 0, nameW - 40, h, juce::Justification::centredLeft, true);
    g.setColour (col::textDim);
    g.setFont (font (12.5f));
    g.drawText (e->factory ? juce::String (categoryName (e->category)) : juce::String (categoryName (e->category)) + " (User)", nameW, 0, catW, h, juce::Justification::centredLeft, true);
    g.drawText (joinTags (e->tags), nameW + catW, 0, w - nameW - catW - 10, h, juce::Justification::centredLeft, true);
}

void PresetBrowser::choose (int row)
{
    if (entryAt (row) == nullptr) return;
    const int index = rows[(size_t) row];
    const juce::String err = ctx.proc.loadPresetIndex (index);
    if (err.isNotEmpty()) ctx.say ("Could not load preset: " + err, true);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    const auto* entry = entryAt (row);
    if (entry == nullptr) return;
    if (e.x < 34)
    {
        auto& pm = ctx.proc.presetManager();
        pm.setFavourite (entry->id, ! pm.isFavourite (entry->id));
        if (favOnly.getToggleState()) rebuild(); else list.repaintRow (row);
        return;
    }
    choose (row);
}

void PresetBrowser::selectedRowsChanged (int) { updateInfo(); }
void PresetBrowser::returnKeyPressed (int row) { choose (row); }
} // namespace ui
