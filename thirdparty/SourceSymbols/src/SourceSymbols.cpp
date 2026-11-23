#include "SourceSymbols.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <string>

namespace dodev::symbols {
namespace {
std::string lower(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string trim(std::string s) {
  const auto a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos)
    return {};
  const auto b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}
std::string extension(std::string path) {
  const auto i = path.find_last_of('.');
  return i == std::string::npos ? "" : lower(path.substr(i));
}
std::string normalize(std::string s) {
  if (s.size() > 280)
    s.resize(280);
  for (char &c : s)
    if (c == '\t' || c == '\r')
      c = ' ';
  return trim(s);
}
std::string sanitize(const std::string &line, bool &inBlock, bool cpp) {
  std::string out = line;
  bool quote = false;
  char quoteCh = 0;
  bool escape = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char ch = line[i], n = i + 1 < line.size() ? line[i + 1] : '\0';
    if (inBlock) {
      out[i] = ' ';
      if (ch == '*' && n == '/') {
        out[++i] = ' ';
        inBlock = false;
      }
      continue;
    }
    if (quote) {
      out[i] = ' ';
      if (escape)
        escape = false;
      else if (ch == '\\')
        escape = true;
      else if (ch == quoteCh)
        quote = false;
      continue;
    }
    if (cpp && ch == '/' && n == '*') {
      out[i] = out[++i] = ' ';
      inBlock = true;
      continue;
    }
    if (cpp && ch == '/' && n == '/') {
      std::fill(out.begin() + static_cast<std::ptrdiff_t>(i), out.end(), ' ');
      break;
    }
    if (!cpp && ch == '#') {
      std::fill(out.begin() + static_cast<std::ptrdiff_t>(i), out.end(), ' ');
      break;
    }
    if (ch == '\"' || ch == '\'' || ch == '`') {
      quote = true;
      quoteCh = ch;
      out[i] = ' ';
    }
  }
  return out;
}
void add(Result &out, std::string kind, std::string name, std::string signature,
         std::size_t line, std::size_t column) {
  if (out.rows.size() >= 15000) {
    out.truncated = true;
    return;
  }
  out.rows.push_back({std::move(kind), std::move(name),
                      normalize(std::move(signature)), line, column});
}
} // namespace
Result extract(std::string_view path, std::string_view contents) {
  Result out;
  const std::string ext = extension(std::string(path));
  const bool py = ext == ".py", script = ext == ".js" || ext == ".jsx" ||
                                         ext == ".ts" || ext == ".tsx";
  const bool kotlin = ext == ".kt" || ext == ".kts";
  const bool c_family = ext == ".cpp" || ext == ".cc" || ext == ".cxx" ||
                        ext == ".c" || ext == ".h" || ext == ".hpp" ||
                        ext == ".hxx" || ext == ".java" || kotlin;
  out.language = py         ? "Python"
                 : script   ? "JavaScript/TypeScript"
                 : kotlin   ? "Kotlin"
                 : c_family ? "C/C++/Java"
                            : "Generic";
  if (contents.size() > 4 * 1024 * 1024) {
    contents = contents.substr(0, 4 * 1024 * 1024);
    out.truncated = true;
  }
  static const std::regex pyDef(
      R"(^\s*(?:async\s+)?def\s+([A-Za-z_]\w*)\s*\()"),
      pyClass(R"(^\s*class\s+([A-Za-z_]\w*)\b)"),
      group(
          R"(\b(namespace|class|struct|enum(?:\s+class)?|interface|union|record)\s+([A-Za-z_]\w*)\b)"),
      preproc(
          R"(^\s*#\s*(define|include)\s+([A-Za-z_][\w]*|[<\"][^>\"]+[>\"]))"),
      kotlinFun(R"(\bfun\s+(?:[A-Za-z_]\w*\.)?([A-Za-z_]\w*)\s*\()"),
      scriptFun(R"(\b(?:async\s+)?function\s+([A-Za-z_$][\w$]*)\s*\()"),
      scriptAssign(
          R"(\b(?:const|let|var)\s+([A-Za-z_$][\w$]*)\s*=\s*(?:async\s*)?(?:\([^;=]*\)|[A-Za-z_$][\w$]*)\s*=>)"),
      cFunction(
          R"((?:^|[;{}]\s*|\s)([A-Za-z_~][\w:~]*)\s*\([^;{}]*\)\s*(?:const\b|noexcept\b|override\b|final\b|->\s*[\w:<>*&]+\s*)*\s*(?:\{|;|$))");
  bool block = false;
  std::size_t lineNo = 0, pos = 0;
  while (pos < contents.size() && lineNo < 200000) {
    const auto end = contents.find('\n', pos);
    const auto len =
        (end == std::string_view::npos ? contents.size() : end) - pos;
    const auto raw =
        std::string(contents.substr(pos, std::min<std::size_t>(len, 4096)));
    ++lineNo;
    const auto line = sanitize(raw, block, !py);
    std::smatch m;
    const auto capture = [&](const std::regex &pattern, const std::string &kind,
                             int groupIndex = 1) -> bool {
      if (!std::regex_search(line, m, pattern))
        return false;
      auto name = m[groupIndex].str();
      if (name == "if" || name == "for" || name == "while" ||
          name == "switch" || name == "catch" || name == "sizeof" ||
          name == "return")
        return false;
      add(out, kind, name, raw, lineNo,
          static_cast<std::size_t>(m.position(groupIndex)) + 1);
      return true;
    };
    if (py) {
      if (!capture(pyClass, "class"))
        capture(pyDef, "function");
    } else {
      if (!capture(group, "type", 2)) {
        if (raw.find("#") != std::string::npos && c_family) {
          if (std::regex_search(raw, m, preproc))
            add(out, m[1] == "define" ? "macro" : "include", m[2], raw, lineNo,
                static_cast<std::size_t>(m.position(2)) + 1);
        } else if (kotlin)
          capture(kotlinFun, "function");
        else if (script) {
          if (!capture(scriptFun, "function"))
            capture(scriptAssign, "function");
        } else if (c_family) {
          if (line.find('(') != std::string::npos) {
            capture(cFunction, "function");
          }
        }
      }
    }
    if (end == std::string_view::npos)
      break;
    pos = end + 1;
  }
  if (lineNo >= 200000)
    out.truncated = true;
  return out;
}
std::vector<Symbol> filter(const std::vector<Symbol> &rows,
                           std::string_view needle) {
  std::vector<Symbol> found;
  const std::string query = lower(std::string(needle));
  for (const auto &row : rows)
    if (query.empty() || lower(row.name).find(query) != std::string::npos ||
        lower(row.kind).find(query) != std::string::npos)
      found.push_back(row);
  return found;
}
std::string toCsv(const std::vector<Symbol> &rows) {
  auto field = [](const std::string &value) {
    std::string result = "\"";
    for (char c : value) {
      if (c == '\"')
        result += '\"';
      result += c;
    }
    return result + '\"';
  };
  std::string out = "Kind,Name,Line,Column,Signature\n";
  for (const auto &row : rows)
    out += field(row.kind) + "," + field(row.name) + "," +
           std::to_string(row.line) + "," + std::to_string(row.column) + "," +
           field(row.signature) + "\n";
  return out;
}
} // namespace dodev::symbols
