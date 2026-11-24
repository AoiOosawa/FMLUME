// .fmlume preset (JSON, schemaVersion 1) and project-state serialisation (SPEC §12).
#pragma once
#include "Core/Json.h"
#include "Core/Model.h"
#include <string>
#include <vector>

namespace fmlume
{
// Serialises a preset. Refuses (returns false) when the patch does not validate: values are
// never clamped on export (SPEC §12.1).
bool presetToJson (const Preset& p, std::string& out, std::string* error = nullptr);
Json presetToJsonValue (const Preset& p);

// Parses + validates a preset. Unknown future schema versions and malformed / out-of-range
// data are rejected with a message; older schemas are migrated first.
bool presetFromJson (const std::string& text, Preset& out, std::string* error = nullptr, std::vector<std::string>* warnings = nullptr);
bool presetFromJsonValue (const Json& root, Preset& out, std::string* error = nullptr, std::vector<std::string>* warnings = nullptr);

// Complete plug-in state: canonical model + host-owned parameter values.
// `edited`: the sound differs from the preset it came from (the header's "*"), restored with the project.
bool stateToJson (const Preset& p, const HostParams& host, std::string& out, std::string* error = nullptr, bool edited = false);
bool stateFromJson (const std::string& text, Preset& out, HostParams& host, std::string* error = nullptr,
                    std::vector<std::string>* warnings = nullptr, bool* edited = nullptr);

// Stable content checksum of the sound-relevant data (used by Explorer candidates).
uint64_t patchChecksum (const PatchData& d);
bool patchDataEqual (const PatchData& a, const PatchData& b);
std::string makePatchId();
std::string nowIso8601();
} // namespace fmlume
