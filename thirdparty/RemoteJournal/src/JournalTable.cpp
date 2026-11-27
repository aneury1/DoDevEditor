#include "JournalTable.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "JournalRules.h"
#include "dodev/settings/Json.h"

namespace dodev::journal {
namespace {
std::string lower(std::string s) {
  for (auto& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string field(const Json& value) {
  if (value.isString()) return value.string();
  if (value.isNumber()) return value.dump(0);
  if (value.isArray())
    return value.dump(
        0);  // binary or multi-valued field, retain representation
  return {};
}
std::optional<std::uint64_t> parseMicros(const std::string& s) {
  if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos)
    return {};
  try {
    return std::stoull(s);
  } catch (...) {
    return {};
  }
}
// Parse a UTC civil date without depending on the machine timezone or time_t
// width. Accepted: YYYY-MM-DD, YYYY-MM-DD HH:MM:SS[.ffffff][Z] (T also
// accepted).
bool bound(std::string s, bool upper, std::optional<std::uint64_t>& out) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    out.reset();
    return true;
  }
  s = s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
  if (s.back() == 'Z') s.pop_back();
  const bool dateOnly = s.size() == 10;
  if (!dateOnly && (s.size() < 19 || s.size() > 26)) return false;
  auto digits = [&](std::size_t pos, std::size_t n, int& val) {
    val = 0;
    if (pos + n > s.size()) return false;
    for (std::size_t i = pos; i < pos + n; ++i) {
      if (s[i] < '0' || s[i] > '9') return false;
      val = val * 10 + s[i] - '0';
    }
    return true;
  };
  int y, m, d, h = 0, min = 0, sec = 0;
  if (!digits(0, 4, y) || !digits(5, 2, m) || !digits(8, 2, d) || s[4] != '-' ||
      s[7] != '-' || y < 1970 || m < 1 || m > 12)
    return false;
  auto leap = [](int a) {
    return a % 4 == 0 && (a % 100 != 0 || a % 400 == 0);
  };
  const int md[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  auto monthDays = [&](int mo) {
    return md[mo - 1] + (mo == 2 && leap(y) ? 1 : 0);
  };
  if (d < 1 || d > monthDays(m)) return false;
  if (!dateOnly && ((s[10] != ' ' && s[10] != 'T') || s[13] != ':' ||
                    s[16] != ':' || !digits(11, 2, h) || !digits(14, 2, min) ||
                    !digits(17, 2, sec) || h > 23 || min > 59 || sec > 59))
    return false;
  int fraction = 0;
  std::size_t precision = 0;
  if (s.size() > 19) {
    precision = s.size() - 20;
    if (s[19] != '.' || precision == 0 || precision > 6 ||
        !digits(20, precision, fraction))
      return false;
  }
  std::uint64_t days = 0;
  for (int yr = 1970; yr < y; ++yr) days += leap(yr) ? 366 : 365;
  for (int mo = 1; mo < m; ++mo) days += monthDays(mo);
  days += d - 1;
  int scale = 1;
  for (std::size_t i = precision; i < 6; ++i) scale *= 10;
  std::uint64_t n =
      (days * 86400 + h * 3600 + min * 60 + sec) * 1000000 + fraction * scale;
  if (upper)
    n += dateOnly ? 86400ULL * 1000000 - 1
                  : static_cast<std::uint64_t>(scale - 1);
  out = n;
  return true;
}
std::string timestamp(const std::string& micros) {
  if (micros.empty() ||
      micros.find_first_not_of("0123456789") != std::string::npos)
    return micros;
  try {
    auto n = std::stoull(micros);
    auto sec = static_cast<std::time_t>(n / 1000000);
    std::tm tm{};
#ifdef _WIN32
    if (gmtime_s(&tm, &sec) != 0) return micros;
#else
    if (!gmtime_r(&sec, &tm)) return micros;
#endif
    std::ostringstream s;
    s << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
      << std::setw(6) << n % 1000000 << 'Z';
    return s.str();
  } catch (...) {
    return micros;
  }
}
std::string csv(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '\"') out += '\"';
    out += c;
  }
  return out + '\"';
}
}  // namespace
std::string Record::level() const {
  static const char* names[] = {"Emergency", "Alert",  "Critical", "Error",
                                "Warning",   "Notice", "Info",     "Debug"};
  return diagnostic                      ? "SSH / diagnostic"
         : priority >= 0 && priority < 8 ? names[priority]
                                         : "Unknown";
}
std::string Record::column(int i) const {
  switch (i) {
    case 0:
      return timestamp;
    case 1:
      return level();
    case 2:
      return host;
    case 3:
      return unit;
    case 4:
      return process;
    case 5:
      return pid;
    case 6:
      return message;
    case 7:
      return applicationId;
    default:
      return {};
  }
}
std::string Record::text() const {
  if (diagnostic) return message;
  std::string out;
  for (int i = 0; i < 6; ++i) {
    auto s = column(i);
    if (!s.empty()) {
      if (!out.empty()) out += ' ';
      out += s;
    }
  }
  if (!applicationId.empty() && applicationId != process)
    out += " [" + applicationId + "]";
  if (!out.empty()) out += " | ";
  return out + message;
}
Record parseRecord(std::size_t id, const std::string& line) {
  Record r;
  r.id = id;
  r.raw = line;
  Json j;
  if (!Json::parse(line, j) || !j.isObject() ||
      (!j.find("MESSAGE") && !j.find("__REALTIME_TIMESTAMP"))) {
    r.diagnostic = true;
    r.message = line;
    return r;
  }
  const auto& obj = static_cast<const Json&>(j);
  auto micros = field(obj["__REALTIME_TIMESTAMP"]);
  r.timestampMicros = parseMicros(micros);
  r.timestamp = timestamp(micros);
  r.host = field(obj["_HOSTNAME"]);
  r.unit = field(obj["_SYSTEMD_UNIT"]);
  if (r.unit.empty()) r.unit = field(obj["_SYSTEMD_USER_UNIT"]);
  r.process = field(obj["SYSLOG_IDENTIFIER"]);
  if (r.process.empty()) r.process = field(obj["_COMM"]);
  r.applicationId = field(obj["APP_ID"]);
  if (r.applicationId.empty()) r.applicationId = field(obj["APPLICATION_ID"]);
  if (r.applicationId.empty()) r.applicationId = r.process;
  r.pid = field(obj["_PID"]);
  if (r.pid.empty()) r.pid = field(obj["SYSLOG_PID"]);
  r.message = field(obj["MESSAGE"]);
  if (!obj["MESSAGE"].isString() && r.message.empty())
    r.message = "[message unavailable or omitted by journalctl]";
  auto p = field(obj["PRIORITY"]);
  if (p.size() == 1 && p[0] >= '0' && p[0] <= '7') r.priority = p[0] - '0';
  return r;
}
bool TimeRange::parse(const std::string& fromText, const std::string& toText,
                      TimeRange& result, std::string& error) {
  TimeRange range;
  if (!bound(fromText, false, range.from)) {
    error =
        "Invalid From (UTC): use YYYY-MM-DD or YYYY-MM-DD HH:MM:SS[.ffffff]";
    return false;
  }
  if (!bound(toText, true, range.to)) {
    error = "Invalid To (UTC): use YYYY-MM-DD or YYYY-MM-DD HH:MM:SS[.ffffff]";
    return false;
  }
  if (range.from && range.to && *range.from > *range.to) {
    error = "From must not be later than To";
    return false;
  }
  result = range;
  error.clear();
  return true;
}
bool TimeRange::accepts(const Record& r) const {
  if (!from && !to) return true;
  if (!r.timestampMicros) return false;
  return (!from || *r.timestampMicros >= *from) &&
         (!to || *r.timestampMicros <= *to);
}
bool TableFilter::accepts(const Record& r) const {
  if (priority == 8 && r.priority != -1) return false;
  if (priority >= 0 && priority < 8 && r.priority != priority) return false;
  auto contains = [&](const std::string& hay, const std::string& needle) {
    return needle.empty() ||
           (caseSensitive
                ? hay.find(needle) != std::string::npos
                : lower(hay).find(lower(needle)) != std::string::npos);
  };
  return time.accepts(r) &&
         (payloadRegex ? std::regex_search(r.message, *payloadRegex)
                       : contains(r.message, payload)) &&
         contains(r.unit, service) &&
         contains(r.applicationId, applicationId) &&
         (regex ? std::regex_search(r.text(), *regex)
                : (text.empty() || contains(r.text(), text)));
}
void Table::appendVisible(std::size_t first) {
  for (std::size_t i = first; i < rows_.size(); ++i)
    if (filter_.accepts(rows_[i]) && (!rules_ || rules_->accepts(rows_[i]))) {
      visible_.push_back(i);
      markers_.push_back(rules_ ? rules_->marker(rows_[i]) : nullptr);
    }
}
void Table::rebuild() {
  visible_.clear();
  markers_.clear();
  appendVisible(0);
}
void Table::rules(std::shared_ptr<const RuleSet> value) {
  rules_ = std::move(value);
  rebuild();
}
const RuleSpec* Table::marker(std::size_t i) const {
  return i < markers_.size() ? markers_[i] : nullptr;
}
void Table::ingest(const RowBatch& b) {
  if (b.received < cursor_) clear();
  const auto oldest = b.dropped + 1;
  auto first = std::lower_bound(
      rows_.begin(), rows_.end(), oldest,
      [](const Record& r, std::size_t id) { return r.id < id; });
  const auto removed = static_cast<std::size_t>(first - rows_.begin());
  if (removed) {
    rows_.erase(rows_.begin(), first);
    auto visibleFirst =
        std::lower_bound(visible_.begin(), visible_.end(), removed);
    const auto count =
        static_cast<std::size_t>(visibleFirst - visible_.begin());
    visible_.erase(visible_.begin(), visibleFirst);
    markers_.erase(markers_.begin(), markers_.begin() + count);
    for (auto& index : visible_) index -= removed;
  }
  // Streaming only evaluates newly arrived rows. Edits to rules rebuild the
  // full table.
  const auto previousSize = rows_.size();
  auto id = b.firstId;
  for (const auto& s : b.lines) {
    if (id > cursor_) rows_.push_back(parseRecord(id, s));
    ++id;
  }
  cursor_ = b.received;
  appendVisible(previousSize);
}
void Table::clear() {
  rows_.clear();
  visible_.clear();
  markers_.clear();
  cursor_ = 0;
}
void Table::filter(TableFilter f) {
  filter_ = std::move(f);
  rebuild();
}
std::string Table::exportText(bool filtered) const {
  std::string out;
  if (filtered) {
    for (auto i : visible_) out += rows_[i].text() + '\n';
  } else
    for (const auto& r : rows_) out += r.text() + '\n';
  return out;
}
std::string Table::exportCsv(bool filtered) const {
  std::string out =
      "Timestamp (UTC),Priority,Host,Unit,Process,PID,Message,Application "
      "ID\r\n";
  auto add = [&](const Record& r) {
    for (int i = 0; i < 8; ++i) {
      if (i) out += ',';
      out += csv(r.column(i));
    }
    out += "\r\n";
  };
  if (filtered) {
    for (auto i : visible_) add(rows_[i]);
  } else
    for (const auto& r : rows_) add(r);
  return out;
}
}  // namespace dodev::journal
