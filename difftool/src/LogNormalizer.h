#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "DiffEngine.h"

namespace jld {

struct NormalizationOptions {
  bool ignoreTimestamp{true};
  bool ignoreWhitespace{false};
  bool ignoreCase{false};
};

class LogNormalizer {
 public:
  static std::vector<SourceLine> loadFile(const std::filesystem::path& path,
                                          const NormalizationOptions& options);

  static std::vector<SourceLine> fromText(const std::string& text,
                                          const NormalizationOptions& options);

  static std::string normalizeLine(std::string line,
                                   const NormalizationOptions& options);

 private:
  static std::string removeJournalTimestamp(const std::string& line);
  static std::string normalizeWhitespace(const std::string& line);
};

}  // namespace jld
