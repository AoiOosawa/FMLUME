// Factory + user preset library: scanning, search, favourites, save / import / export (SPEC §12).
// Non-realtime; call from the message thread.
#pragma once
#include "Core/Model.h"
#include <set>
#include <string>
#include <vector>

namespace fmlume
{
struct PresetEntry
{
    std::string id, name, author, description;
    Category category = Category::Keys;
    std::vector<std::string> tags;
    bool factory = false;
    std::string path;     // user presets only
    int storeIndex = -1;  // index into the in-memory factory store
};

class PresetManager
{
public:
    static std::string defaultUserDirectory(); // ~/Library/Application Support/FMLUME/Presets

    void setUserDirectory (const std::string& dir); // creates it on demand when saving
    const std::string& getUserDirectory() const { return userDir; }

    // Factory presets are handed over as JSON text (embedded resources) or loaded from a directory.
    bool addFactoryPreset (const std::string& jsonText, std::string* error = nullptr);
    int loadFactoryDirectory (const std::string& dir, std::vector<std::string>* errors = nullptr);
    void rescanUser (std::vector<std::string>* errors = nullptr);

    const std::vector<PresetEntry>& entries() const { return list; }
    int findById (const std::string& id) const;
    // Case-insensitive free text over name / author / tags / description. category < 0 = all.
    std::vector<int> search (const std::string& text, int category = -1, const std::string& tag = {}, bool favouritesOnly = false) const;
    std::vector<std::string> allTags() const;

    bool load (int index, Preset& out, std::string* error = nullptr) const;
    // Writes <userDir>/<name>.fmlume. Fails if it exists and overwrite is false.
    bool saveUser (Preset preset, const std::string& name, bool overwrite, std::string* error = nullptr);
    bool userPresetExists (const std::string& name) const;
    bool deleteUser (int index, std::string* error = nullptr);
    static bool exportTo (const std::string& path, const Preset& preset, std::string* error = nullptr);
    static bool importFrom (const std::string& path, Preset& out, std::string* error = nullptr);
    // Import as the browser does it: the file is copied into the user presets under its own name
    // (numbered if that name is taken) and with a new id, so it can never stand in for the factory
    // or user preset it was exported from. `out` is the stored user preset.
    bool importIntoUser (const std::string& path, Preset& out, std::string* error = nullptr);

    bool isFavourite (const std::string& id) const { return favourites.count (id) > 0; }
    void setFavourite (const std::string& id, bool on);

    static std::string sanitiseFileName (const std::string& name);

private:
    void rebuildOrder();
    void loadFavourites();
    void saveFavourites() const;
    std::string userPathFor (const std::string& name) const;

    std::string userDir;
    std::vector<Preset> factoryStore;
    std::vector<PresetEntry> factoryEntries, userEntries, list;
    std::set<std::string> favourites;
};
} // namespace fmlume
