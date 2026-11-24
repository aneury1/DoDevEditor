#pragma once
#include <filesystem>
#include <string>

namespace dodev::explorer {
namespace fs = std::filesystem;
struct Result { bool ok=false; std::string error; fs::path path; };
// Entry names, not paths. No separators, dot entries, or traversal allowed.
bool validEntryName(const std::string& name);
Result createEntry(const fs::path& parent,const std::string& name,bool directory);
Result renameEntry(const fs::path& source,const std::string& newName);
Result removeEntry(const fs::path& entry,const fs::path& protectedWorkspace);
}
