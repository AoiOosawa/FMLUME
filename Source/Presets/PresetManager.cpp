#include "Presets/PresetManager.h"
#include "Core/Json.h"
#include "Presets/PresetSerializer.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace fmlume
{
namespace
{
// Preset-sized files only. The user folder may contain anything (other people's preset packs),
// so: regular files only (no devices, FIFOs or sockets, which would block or never end), a size
// cap checked before and enforced while reading, and nothing is read on a stat error.
constexpr std::uintmax_t kMaxPresetFileBytes = 4u * 1024u * 1024u;
constexpr size_t kMaxFilesPerScan = 5000;

bool readFile (const std::string& path, std::string& out, std::string* why = nullptr)
{
    const auto refuse = [why] (const char* msg) { if (why) *why = msg; return false; };
    std::error_code ec;
    if (! fs::is_regular_file (path, ec) || ec) return refuse ("not a regular file");
    const auto size = fs::file_size (path, ec);
    if (ec) return refuse ("cannot read file size");
    if (size > kMaxPresetFileBytes) return refuse ("file too large to be a preset");
    std::ifstream f (path, std::ios::binary);
    if (! f) return refuse ("cannot open file");
    out.resize ((size_t) kMaxPresetFileBytes + 1);
    f.read (out.data(), (std::streamsize) out.size());
    const auto got = (size_t) f.gcount();
    if (got > kMaxPresetFileBytes) { out.clear(); return refuse ("file too large to be a preset"); }
    out.resize (got);
    return true;
}

// *.fmlume files of a directory, sorted. Iteration errors end the scan instead of throwing.
std::vector<std::string> presetFilesIn (const std::string& dir)
{
    std::vector<std::string> files;
    std::error_code ec;
    for (fs::directory_iterator it (dir, ec), end; ! ec && it != end && files.size() < kMaxFilesPerScan; it.increment (ec))
        if (it->path().extension() == ".fmlume") files.push_back (it->path().string());
    std::sort (files.begin(), files.end());
    return files;
}

// Writes through a freshly created, exclusively opened temporary file and renames it into place.
// O_EXCL | O_NOFOLLOW: a file or symlink planted under the temporary name is never written through.
bool writeFileAtomic (const std::string& path, const std::string& text)
{
    static std::atomic<unsigned> counter { 0 };
    const std::string tmp = path + ".tmp." + std::to_string ((long) ::getpid()) + "." + std::to_string (counter.fetch_add (1));
    const int fd = ::open (tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0644);
    if (fd < 0) return false;
    size_t done = 0;
    bool ok = true;
    while (ok && done < text.size())
    {
        const ssize_t n = ::write (fd, text.data() + done, text.size() - done);
        if (n > 0) done += (size_t) n; else if (errno != EINTR) ok = false;
    }
    ok = ::close (fd) == 0 && ok;
    std::error_code ec;
    if (ok) fs::rename (tmp, path, ec);
    if (! ok || ec) { fs::remove (tmp, ec); return false; }
    return true;
}
std::string lower (std::string s)
{
    for (auto& c : s) c = (char) std::tolower ((unsigned char) c);
    return s;
}
PresetEntry entryFor (const Preset& p)
{
    PresetEntry e;
    e.id = p.meta.patchId;
    e.name = p.meta.displayName;
    e.author = p.meta.author;
    e.description = p.meta.description;
    e.category = p.meta.category;
    e.tags = p.meta.tags;
    return e;
}
} // namespace

std::string PresetManager::defaultUserDirectory()
{
    const char* home = std::getenv ("HOME");
    return std::string (home != nullptr ? home : ".") + "/Library/Application Support/FMLUME/Presets";
}

void PresetManager::setUserDirectory (const std::string& dir)
{
    userDir = dir;
    loadFavourites();
    rescanUser();
}

bool PresetManager::addFactoryPreset (const std::string& jsonText, std::string* error)
{
    Preset p;
    if (! presetFromJson (jsonText, p, error)) return false;
    PresetEntry e = entryFor (p);
    e.factory = true;
    e.storeIndex = (int) factoryStore.size();
    factoryStore.push_back (std::move (p));
    factoryEntries.push_back (std::move (e));
    rebuildOrder();
    return true;
}

int PresetManager::loadFactoryDirectory (const std::string& dir, std::vector<std::string>* errors)
{
    const std::vector<std::string> files = presetFilesIn (dir);
    int count = 0;
    for (auto& f : files)
    {
        std::string text, err;
        if (readFile (f, text) && addFactoryPreset (text, &err)) ++count;
        else if (errors) errors->push_back (f + ": " + err);
    }
    return count;
}

void PresetManager::rescanUser (std::vector<std::string>* errors)
{
    userEntries.clear();
    std::error_code ec;
    if (! userDir.empty() && fs::is_directory (userDir, ec))
    {
        for (auto& f : presetFilesIn (userDir))
        {
            std::string text, err;
            Preset p;
            if (readFile (f, text) && presetFromJson (text, p, &err))
            {
                PresetEntry e = entryFor (p);
                e.path = f;
                if (e.name.empty()) e.name = fs::path (f).stem().string();
                // A file dropped into the folder by hand may carry the id of a factory preset or of
                // another user file (an export, a Finder copy). Ids must be unique in the list, or
                // that file would pose as the other preset: give it one derived from its file name.
                const auto taken = [&] (const std::string& id) {
                    if (id.empty()) return true;
                    for (auto& o : factoryEntries) if (o.id == id) return true;
                    for (auto& o : userEntries) if (o.id == id) return true;
                    return false;
                };
                if (taken (e.id)) e.id = "user-file." + fs::path (f).stem().string();
                for (int n = 2; taken (e.id); ++n) e.id = "user-file." + fs::path (f).stem().string() + "." + std::to_string (n);
                userEntries.push_back (std::move (e));
            }
            else if (errors) errors->push_back (f + ": " + err); // broken files are skipped, never fatal
        }
    }
    rebuildOrder();
}

void PresetManager::rebuildOrder()
{
    list.clear();
    list.insert (list.end(), factoryEntries.begin(), factoryEntries.end());
    list.insert (list.end(), userEntries.begin(), userEntries.end());
    std::stable_sort (list.begin(), list.end(), [] (const PresetEntry& a, const PresetEntry& b) {
        if (a.category != b.category) return (int) a.category < (int) b.category;
        if (a.factory != b.factory) return a.factory;
        return false;
    });
}

int PresetManager::findById (const std::string& id) const
{
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].id == id) return (int) i;
    return -1;
}

std::vector<int> PresetManager::search (const std::string& text, int category, const std::string& tag, bool favouritesOnly) const
{
    std::vector<int> result;
    const std::string needle = lower (text), tagL = lower (tag);
    for (size_t i = 0; i < list.size(); ++i)
    {
        const auto& e = list[i];
        if (category >= 0 && (int) e.category != category) continue;
        if (favouritesOnly && ! isFavourite (e.id)) continue;
        if (! tagL.empty())
        {
            bool has = false;
            for (auto& t : e.tags) has = has || lower (t) == tagL;
            if (! has) continue;
        }
        if (! needle.empty())
        {
            std::string hay = lower (e.name) + " " + lower (e.author) + " " + lower (e.description) + " " + lower (categoryName (e.category));
            for (auto& t : e.tags) hay += " " + lower (t);
            // every whitespace-separated word must match
            std::istringstream words (needle);
            std::string w;
            bool all = true;
            while (words >> w) all = all && hay.find (w) != std::string::npos;
            if (! all) continue;
        }
        result.push_back ((int) i);
    }
    return result;
}

std::vector<std::string> PresetManager::allTags() const
{
    std::set<std::string> tags;
    for (auto& e : list) for (auto& t : e.tags) tags.insert (t);
    return { tags.begin(), tags.end() };
}

bool PresetManager::load (int index, Preset& out, std::string* error) const
{
    if (index < 0 || index >= (int) list.size()) { if (error) *error = "preset index out of range"; return false; }
    const auto& e = list[(size_t) index];
    if (e.factory) { out = factoryStore[(size_t) e.storeIndex]; return true; }
    if (! importFrom (e.path, out, error)) return false;
    out.meta.patchId = e.id; // the list's id (see rescanUser: unique even if the file's own id is not)
    return true;
}

std::string PresetManager::sanitiseFileName (const std::string& name)
{
    std::string s;
    for (const char c : name)
    {
        const unsigned char u = (unsigned char) c;
        if (u < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') s.push_back ('_');
        else s.push_back (c);
    }
    while (! s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
    while (! s.empty() && (s.front() == ' ' || s.front() == '.')) s.erase (s.begin());
    if (s.empty()) s = "Untitled";
    if (s.size() > 120) s.resize (120);
    return s;
}

std::string PresetManager::userPathFor (const std::string& name) const { return userDir + "/" + sanitiseFileName (name) + ".fmlume"; }

bool PresetManager::userPresetExists (const std::string& name) const
{
    std::error_code ec;
    return ! userDir.empty() && fs::exists (userPathFor (name), ec);
}

bool PresetManager::saveUser (Preset preset, const std::string& name, bool overwrite, std::string* error)
{
    if (userDir.empty()) { if (error) *error = "no user preset directory configured"; return false; }
    std::error_code ec;
    fs::create_directories (userDir, ec);
    const std::string path = userPathFor (name);
    const bool exists = fs::exists (path, ec);
    if (exists && ! overwrite) { if (error) *error = "a preset named '" + name + "' already exists"; return false; }
    preset.meta.displayName = name;
    // A user copy of a factory preset (or a new name) gets its own identity.
    Preset previous;
    std::string prevText;
    if (exists && readFile (path, prevText) && presetFromJson (prevText, previous)) preset.meta.patchId = previous.meta.patchId;
    else preset.meta.patchId = makePatchId();
    if (preset.meta.createdAt.empty() || ! exists) preset.meta.createdAt = nowIso8601();
    preset.meta.modifiedAt = nowIso8601();
    if (! exportTo (path, preset, error)) return false;
    rescanUser();
    return true;
}

bool PresetManager::deleteUser (int index, std::string* error)
{
    if (index < 0 || index >= (int) list.size() || list[(size_t) index].factory)
    {
        if (error) *error = "only user presets can be deleted";
        return false;
    }
    std::error_code ec;
    fs::remove (list[(size_t) index].path, ec);
    if (ec) { if (error) *error = ec.message(); return false; }
    rescanUser();
    return true;
}

bool PresetManager::exportTo (const std::string& path, const Preset& preset, std::string* error)
{
    std::string text;
    if (! presetToJson (preset, text, error)) return false;
    if (! writeFileAtomic (path, text)) { if (error) *error = "cannot write " + path; return false; }
    return true;
}

bool PresetManager::importFrom (const std::string& path, Preset& out, std::string* error)
{
    std::string text, why;
    if (! readFile (path, text, &why)) { if (error) *error = why + ": " + path; return false; }
    return presetFromJson (text, out, error);
}

bool PresetManager::importIntoUser (const std::string& path, Preset& out, std::string* error)
{
    Preset p;
    if (! importFrom (path, p, error)) return false;
    const std::string base = p.meta.displayName.empty() ? fs::path (path).stem().string() : p.meta.displayName;
    std::string name = base;
    for (int n = 2; userPresetExists (name); ++n) name = base + " " + std::to_string (n);
    if (! saveUser (p, name, false, error)) return false;
    for (auto& e : userEntries)
        if (e.name == name) return load (findById (e.id), out, error);
    if (error) *error = "the imported preset could not be read back";
    return false;
}

void PresetManager::setFavourite (const std::string& id, bool on)
{
    if (on) favourites.insert (id); else favourites.erase (id);
    saveFavourites();
}

void PresetManager::loadFavourites()
{
    favourites.clear();
    std::string text;
    Json j;
    if (userDir.empty() || ! readFile (userDir + "/favorites.json", text) || ! Json::parse (text, j) || ! j.isArray()) return;
    for (auto& v : j.items) if (v.isString()) favourites.insert (v.string);
}

void PresetManager::saveFavourites() const
{
    if (userDir.empty()) return;
    std::error_code ec;
    fs::create_directories (userDir, ec);
    Json j = Json::array();
    for (auto& f : favourites) j.push (f);
    writeFileAtomic (userDir + "/favorites.json", j.dump (2));
}
} // namespace fmlume
