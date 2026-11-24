#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace dodev::search {
namespace fs = std::filesystem;
struct Options {
  std::string query;
  std::string include;  // comma-separated glob patterns; empty = all text files
  std::string
      exclude;  // comma-separated globs, plus default skipped directories
  bool regex = false;
  bool caseSensitive = false;
  bool wholeWord = false;
  std::size_t maxFiles = 2000;
  std::size_t maxHits = 10000;
  std::size_t maxFileBytes = 2 * 1024 * 1024;
};
struct Match {
  fs::path path;
  std::size_t line = 1;
  std::size_t column = 1;
  std::size_t byteOffset = 0;
  std::size_t matchLength = 0;
  std::string snippet;
};
struct Results {
  std::vector<Match> matches;
  std::size_t filesScanned = 0;
  bool truncated = false;
  std::string error;
};
using Overlays = std::map<fs::path, std::string>;
// Files on disk plus optional unsaved editor buffers. No file changes during
// scan.
Results scan(const fs::path& root, const Options& options,
             const Overlays& overlays = {});
// Preview/replacement helpers; exceptions (including bad regex) become error
// messages.
bool replaceText(std::string_view text, const Options& options,
                 std::string_view replacement, std::string& result,
                 std::size_t& count, std::string* error = nullptr);
// Atomic on-disk update; refuses to follow symbolic links or overwrite a
// changed file.
bool replaceFile(const fs::path& file, const Options& options,
                 std::string_view replacement, std::size_t& count,
                 std::string* error = nullptr);
}  // namespace dodev::search
