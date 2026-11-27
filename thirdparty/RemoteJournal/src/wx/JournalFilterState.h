#pragma once
#include <filesystem>
#include <fstream>
#include <sstream>

#include "JournalRules.h"
namespace wxui {
// Value-only state: captures own it; Workspace controls edit a copy.
struct JournalFilterState {
  std::string text, payload, service, application, from, to;
  int priority = -1;
  bool caseSensitive = false, regex = false, payloadRegex = false;
  dodev::journal::RuleSet rules;
  bool compile(dodev::journal::TableFilter& output, std::string& error) const {
    dodev::journal::TableFilter f;
    f.text = text;
    f.payload = payload;
    f.service = service;
    f.applicationId = application;
    f.priority = priority;
    f.caseSensitive = caseSensitive;
    if (!dodev::journal::TimeRange::parse(from, to, f.time, error))
      return false;
    const auto flags = caseSensitive
                           ? std::regex::ECMAScript
                           : std::regex::ECMAScript | std::regex::icase;
    try {
      if (regex && !text.empty()) f.regex.emplace(text, flags);
      if (payloadRegex && !payload.empty())
        f.payloadRegex.emplace(payload, flags);
    } catch (const std::regex_error&) {
      error = "Invalid regular expression";
      return false;
    }
    output = std::move(f);
    error.clear();
    return true;
  }
};
inline std::filesystem::path journalDefaultFilters(
    const std::filesystem::path& workspace) {
  return workspace / ".dodev" / "plugins" / "com.dodev.remotejournal" /
         "filters.json";
}
inline bool readJournalRules(const std::filesystem::path& path,
                             dodev::journal::RuleSet& rules,
                             std::string& error) {
  std::error_code ec;
  auto size = std::filesystem::file_size(path, ec);
  if (ec || size > 4 * 1024 * 1024) {
    error = "Cannot read filter file (maximum 4 MiB)";
    return false;
  }
  std::ifstream f(path, std::ios::binary);
  std::ostringstream content;
  content << f.rdbuf();
  if (!f) {
    error = "Cannot read filter file";
    return false;
  }
  return dodev::journal::RuleSet::decode(content.str(), rules, error);
}
}  // namespace wxui
