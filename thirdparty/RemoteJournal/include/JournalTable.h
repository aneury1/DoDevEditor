#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "RemoteJournal.h"

namespace dodev::journal {
// Structured journal rows, independent of the desktop toolkit.
struct Record {
  std::size_t id = 0;
  int priority = -1;
  std::string timestamp, host, unit, process, pid, message, raw, applicationId;
  bool diagnostic = false;
  std::optional<std::uint64_t> timestampMicros;
  std::string level() const;
  std::string text() const;
  std::string column(int index) const;
};
Record parseRecord(std::size_t id, const std::string& line);
// Inclusive UTC bounds. Empty fields are unbounded. Date-only and second-only
// upper bounds include the entire specified day or second.
struct TimeRange {
  std::optional<std::uint64_t> from, to;
  static bool parse(const std::string& fromText, const std::string& toText,
                    TimeRange& result, std::string& error);
  bool accepts(const Record& record) const;
};
struct TableFilter {
  std::string text;
  bool caseSensitive = false;
  int priority =
      -1;  // -1 all, 0..7 exact journal priority, 8 diagnostics/unknown
  std::string payload, service, applicationId;
  std::optional<std::regex> payloadRegex;
  TimeRange time;
  std::optional<std::regex> regex;  // optional general-text regex
  bool accepts(const Record&) const;
};
struct RuleSet;
struct RuleSpec;
class Table {
 public:
  void ingest(const RowBatch& batch);
  void clear();
  void filter(TableFilter value);
  void rules(std::shared_ptr<const RuleSet> value);
  const RuleSpec* marker(std::size_t visibleIndex) const;
  const std::vector<Record>& rows() const { return rows_; }
  const std::vector<std::size_t>& visible() const { return visible_; }
  const Record& at(std::size_t i) const { return rows_.at(visible_.at(i)); }
  std::size_t cursor() const { return cursor_; }
  std::string exportText(bool filtered) const;
  std::string exportCsv(bool filtered) const;

 private:
  void rebuild();
  void appendVisible(std::size_t first);
  std::vector<Record> rows_;
  std::vector<std::size_t> visible_;
  TableFilter filter_;
  std::shared_ptr<const RuleSet> rules_;
  std::vector<const RuleSpec*> markers_;
  std::size_t cursor_ = 0;
};
}  // namespace dodev::journal
