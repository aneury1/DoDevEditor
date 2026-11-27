#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>

#include "FilePreview.h"
#ifdef PREVIEW_SQLITE
#include <sqlite3.h>
#endif
#ifdef PREVIEW_EXPAT
#include <expat.h>
#endif
namespace preview {
std::string readBounded(const std::filesystem::path& p, size_t limit,
                        bool truncate) {
  if (!std::filesystem::is_regular_file(p))
    throw std::runtime_error("Select a regular file");
  std::ifstream in(p, std::ios::binary);
  if (!in) throw std::runtime_error("Cannot read file");
  std::string s(limit + 1, '\0');
  in.read(s.data(), static_cast<std::streamsize>(s.size()));
  s.resize(static_cast<size_t>(in.gcount()));
  if (in.bad()) throw std::runtime_error("Read failed");
  if (s.size() > limit) {
    if (!truncate) throw std::runtime_error("File exceeds preview size limit");
    s.resize(limit);
  }
  return s;
}
Result hexView(const std::string& data, bool truncated) {
  std::ostringstream o;
  o << "Offset    Hexadecimal                                       ASCII\n";
  for (size_t i = 0; i < data.size(); i += 16) {
    o << std::hex << std::setfill('0') << std::setw(8) << i << "  ";
    std::string text;
    for (size_t k = 0; k < 16; ++k) {
      if (i + k < data.size()) {
        auto c = static_cast<unsigned char>(data[i + k]);
        o << std::setw(2) << unsigned(c) << ' ';
        text += c >= 32 && c <= 126 ? char(c) : '.';
      } else
        o << "   ";
    }
    o << ' ' << text << '\n';
  }
  return {"<html><body><pre>" + escape(o.str()) + "</pre>" +
              (truncated ? "<p>Preview truncated.</p>" : "") + "</body></html>",
          truncated};
}
namespace {
// Lexical JSON tree retains exact strings and numeric literals, including
// 64-bit IDs.
struct Parser {
  const std::string& s;
  size_t p = 0;
  std::vector<Node> nodes;
  void ws() {
    while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
  }
  bool take(char c) {
    ws();
    if (p < s.size() && s[p] == c) {
      ++p;
      return true;
    }
    return false;
  }
  [[noreturn]] void fail() {
    throw std::runtime_error("Invalid JSON at byte " + std::to_string(p));
  }
  std::string string() {
    ws();
    if (p >= s.size() || s[p] != '"') fail();
    auto start = p++;
    while (p < s.size()) {
      auto c = s[p++];
      if (c == '"') return s.substr(start, p - start);
      if (static_cast<unsigned char>(c) < 32) fail();
      if (c == '\\') {
        if (p >= s.size()) fail();
        auto e = s[p++];
        if (e == 'u') {
          for (int i = 0; i < 4; i++)
            if (p >= s.size() ||
                !std::isxdigit(static_cast<unsigned char>(s[p++])))
              fail();
        } else if (std::string("\"\\/bfnrt").find(e) == std::string::npos)
          fail();
      }
    }
    fail();
  }
  void add(size_t d, std::string label) {
    if (d > 80 || nodes.size() >= 10000)
      throw std::runtime_error("Tree preview limit exceeded");
    if (label.size() > 4096) label = label.substr(0, 4096) + "...";
    nodes.push_back({d, std::move(label)});
  }
  void value(size_t d, std::string key) {
    ws();
    if (p >= s.size()) fail();
    if (take('{')) {
      add(d, key + " {object}");
      if (take('}')) return;
      do {
        auto k = string();
        if (!take(':')) fail();
        value(d + 1, k);
      } while (take(','));
      if (!take('}')) fail();
      return;
    }
    if (take('[')) {
      add(d, key + " [array]");
      if (take(']')) return;
      size_t i = 0;
      do {
        value(d + 1, "[" + std::to_string(i++) + "]");
      } while (take(','));
      if (!take(']')) fail();
      return;
    }
    if (s[p] == '"') {
      add(d, key + ": " + string());
      return;
    }
    auto start = p;
    while (p < s.size() &&
           std::string(" ,]}\r\n\t").find(s[p]) == std::string::npos)
      ++p;
    auto v = s.substr(start, p - start);
    if (v != "true" && v != "false" && v != "null") {
      size_t j = 0;
      auto digit = [&] { return j < v.size() && v[j] >= '0' && v[j] <= '9'; };
      if (j < v.size() && v[j] == '-') ++j;
      if (!digit()) fail();
      if (v[j] == '0')
        ++j;
      else
        while (digit()) ++j;
      if (j < v.size() && v[j] == '.') {
        ++j;
        if (!digit()) fail();
        while (digit()) ++j;
      }
      if (j < v.size() && (v[j] == 'e' || v[j] == 'E')) {
        ++j;
        if (j < v.size() && (v[j] == '+' || v[j] == '-')) ++j;
        if (!digit()) fail();
        while (digit()) ++j;
      }
      if (j != v.size()) fail();
    }
    add(d, key + ": " + v);
  }
};
}  // namespace
std::vector<Node> jsonTree(const std::string& s) {
  if (s.size() > MaxInput) throw std::runtime_error("JSON exceeds input limit");
  Parser p{s};
  p.value(0, "JSON");
  p.ws();
  if (p.p != s.size()) p.fail();
  return p.nodes;
}
std::vector<Node> xmlTree(const std::string& s) {
#ifdef PREVIEW_EXPAT
  if (s.size() > MaxInput) throw std::runtime_error("XML exceeds input limit");
  struct State {
    XML_Parser p;
    size_t depth = 0;
    bool rejected = false;
    std::vector<Node> nodes;
    void add(size_t d, std::string t) {
      if (d > 80 || nodes.size() >= 10000) {
        rejected = true;
        XML_StopParser(p, XML_FALSE);
        return;
      }
      nodes.push_back({d, t.substr(0, 4096)});
    }
  };
  auto parser = XML_ParserCreate(nullptr);
  if (!parser) throw std::runtime_error("Cannot create XML parser");
  std::unique_ptr<std::remove_pointer<XML_Parser>::type,
                  decltype(&XML_ParserFree)>
      guard(parser, XML_ParserFree);
  State state{parser};
  XML_SetUserData(parser, &state);
  XML_SetStartDoctypeDeclHandler(
      parser,
      [](void* v, const XML_Char*, const XML_Char*, const XML_Char*, int) {
        auto& a = *static_cast<State*>(v);
        a.rejected = true;
        XML_StopParser(a.p, XML_FALSE);
      });
  XML_SetElementHandler(
      parser,
      [](void* v, const XML_Char* name, const XML_Char** attrs) {
        auto& a = *static_cast<State*>(v);
        a.add(a.depth++, std::string("<") + name + ">");
        for (size_t i = 0; attrs && attrs[i] && !a.rejected; i += 2)
          a.add(a.depth, std::string("@") + attrs[i] + " = " + attrs[i + 1]);
      },
      [](void* v, const XML_Char*) {
        auto& a = *static_cast<State*>(v);
        if (a.depth) --a.depth;
      });
  XML_SetCharacterDataHandler(
      parser, [](void* v, const XML_Char* text, int len) {
        auto& a = *static_cast<State*>(v);
        std::string t(text, len);
        if (t.find_first_not_of(" \t\r\n") != std::string::npos)
          a.add(a.depth, t);
      });
  if (XML_Parse(parser, s.data(), static_cast<int>(s.size()), XML_TRUE) !=
      XML_STATUS_OK)
    throw std::runtime_error(state.rejected ? "XML DTD or tree limit rejected"
                                            : "Invalid XML");
  return state.nodes;
#else
  (void)s;
  throw std::runtime_error("XML tree requires Expat development libraries");
#endif
}
Result sqliteView(const std::filesystem::path& path,
                  const std::string& requested, const std::string& query) {
#ifdef PREVIEW_SQLITE
  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("Select a database file");
  sqlite3* db = nullptr;
  int rc =
      sqlite3_open_v2(path.u8string().c_str(), &db,
                      SQLITE_OPEN_READONLY | SQLITE_OPEN_PRIVATECACHE, nullptr);
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> guard(db, sqlite3_close);
  if (rc != SQLITE_OK) throw std::runtime_error("Cannot open SQLite read-only");
  sqlite3_busy_timeout(db, 1000);
  sqlite3_limit(db, SQLITE_LIMIT_LENGTH, 2 * 1024 * 1024);
  sqlite3_exec(db, "PRAGMA query_only=ON; PRAGMA trusted_schema=OFF;", nullptr,
               nullptr, nullptr);
  auto stmt = [&](const std::string& sql) {
    sqlite3_stmt* st = nullptr;
    const char* tail = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, &tail) != SQLITE_OK)
      throw std::runtime_error(sqlite3_errmsg(db));
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> result(
        st, sqlite3_finalize);
    if (!st || !sqlite3_stmt_readonly(st) ||
        std::string(tail ? tail : "").find_first_not_of(" \t\r\n") !=
            std::string::npos)
      throw std::runtime_error(
          "Only one read-only SELECT statement is allowed");
    return result;
  };
  auto text = [](sqlite3_stmt* st, int i) {
    const auto* p = sqlite3_column_text(st, i);
    return p ? std::string(reinterpret_cast<const char*>(p),
                           sqlite3_column_bytes(st, i))
             : std::string("NULL");
  };
  std::vector<std::string> names;
  auto list = stmt(
      "SELECT name,sql FROM sqlite_master WHERE type='table' AND name NOT LIKE "
      "'sqlite_%' ORDER BY name LIMIT 101");
  std::string out = "<html><body><h2>SQLite tables (read-only)</h2><ul>";
  while ((rc = sqlite3_step(list.get())) == SQLITE_ROW) {
    auto n = text(list.get(), 0);
    auto schema = text(list.get(), 1);
    auto upper = schema;
    for (auto& c : upper)
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (upper.find("CREATE VIRTUAL TABLE") != std::string::npos) continue;
    names.push_back(n);
    out += "<li>" + escape(n) + "</li>";
  }
  if (rc != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db));
  out += "</ul>";
  if (names.empty())
    return {out + "<p>No ordinary tables.</p></body></html>", false};
  auto name = requested.empty() ? names.front() : requested;
  if (query.empty() &&
      std::find(names.begin(), names.end(), name) == names.end())
    throw std::runtime_error("Unknown table: " + name);
  std::string identifier = "\"";
  for (char c : name) {
    identifier += c;
    if (c == '"') identifier += '"';
  }
  identifier += '"';  // Do not permit ATTACH, PRAGMA, extensions, writes, or
                      // arbitrary functions.
  sqlite3_set_authorizer(
      db,
      [](void*, int action, const char*, const char* second, const char*,
         const char*) -> int {
        if (action == SQLITE_SELECT || action == SQLITE_READ) return SQLITE_OK;
        if (action == SQLITE_FUNCTION) {
          static const std::set<std::string> funcs = {
              "count",   "sum",    "avg",      "min",    "max",    "length",
              "lower",   "upper",  "coalesce", "ifnull", "substr", "round",
              "abs",     "typeof", "hex",      "like",   "glob",   "trim",
              "replace", "total",  "nullif",   "instr"};
          return second && funcs.count(second) ? SQLITE_OK : SQLITE_DENY;
        }
        return SQLITE_DENY;
      },
      nullptr);
  int budget = 2000;
  sqlite3_progress_handler(
      db, 1000, [](void* v) -> int { return --*static_cast<int*>(v) <= 0; },
      &budget);
  if (query.size() > 8192)
    throw std::runtime_error("SQL query exceeds 8192 bytes");
  auto q = stmt(query.empty() ? "SELECT * FROM " + identifier + " LIMIT 201"
                              : query);
  int cols = std::min(sqlite3_column_count(q.get()), 80);
  out += "<h3>" + escape(query.empty() ? name : "Query result") +
         "</h3><table border=\"1\" cellpadding=\"4\"><tr>";
  for (int i = 0; i < cols; ++i)
    out += "<th>" + escape(sqlite3_column_name(q.get(), i)) + "</th>";
  out += "</tr>";
  size_t rows = 0;
  bool truncated = false;
  while ((rc = sqlite3_step(q.get())) == SQLITE_ROW) {
    if (rows++ == 200 || out.size() > 8 * 1024 * 1024) {
      truncated = true;
      break;
    }
    out += "<tr>";
    for (int i = 0; i < cols; ++i) {
      auto v =
          sqlite3_column_type(q.get(), i) == SQLITE_BLOB
              ? "[BLOB: " + std::to_string(sqlite3_column_bytes(q.get(), i)) +
                    " bytes]"
              : text(q.get(), i);
      if (v.size() > 4096) v = v.substr(0, 4096) + "...";
      out += "<td>" + escape(v) + "</td>";
    }
    out += "</tr>";
  }
  if (!truncated && rc != SQLITE_DONE)
    throw std::runtime_error(sqlite3_errmsg(db));
  out +=
      "</table><p>Maximum 200 rows / 80 columns; long cells "
      "shortened.</p></body></html>";
  return {out, truncated};
#else
  (void)path;
  (void)requested;
  (void)query;
  throw std::runtime_error(
      "SQLite preview requires SQLite development libraries");
#endif
}
}  // namespace preview
