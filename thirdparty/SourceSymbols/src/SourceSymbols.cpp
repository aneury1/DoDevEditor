#include "SourceSymbols.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <sstream>

namespace dodev::symbols {
namespace {
std::string lower(std::string s) {
  for (char& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string trim(const std::string& s) {
  const auto a = s.find_first_not_of(" \t\r\n");
  return a == std::string::npos
             ? ""
             : s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
std::string extOf(const std::string& path) {
  const auto p = path.find_last_of('.');
  return p == std::string::npos ? "" : lower(path.substr(p));
}
std::string signature(std::string s) {
  for (char& c : s)
    if (c == '\t' || c == '\n' || c == '\r') c = ' ';
  s = trim(s);
  if (s.size() > 280) s.resize(280);
  return s;
}
// Preserve byte offsets/newlines so navigation still points at the unsaved
// editor buffer.
std::string maskCommentsAndLiterals(std::string_view source, bool python) {
  std::string masked(source);
  enum class State {
    Normal,
    Block,
    Line,
    Single,
    Double,
    Backtick,
    TripleSingle,
    TripleDouble
  };
  State state = State::Normal;
  bool escaped = false;
  for (size_t i = 0; i < source.size(); ++i) {
    const char ch = source[i],
               next = i + 1 < source.size() ? source[i + 1] : '\0';
    auto erase = [&](size_t n) {
      for (size_t j = 0; j < n && i + j < source.size(); ++j)
        if (masked[i + j] != '\n') masked[i + j] = ' ';
    };
    if (state == State::Line) {
      if (ch == '\n')
        state = State::Normal;
      else
        erase(1);
      continue;
    }
    if (state == State::Block) {
      if (ch == '*' && next == '/') {
        erase(2);
        ++i;
        state = State::Normal;
      } else
        erase(1);
      continue;
    }
    if (state != State::Normal) {
      if ((state == State::TripleSingle || state == State::TripleDouble) &&
          i + 2 < source.size() &&
          ch == (state == State::TripleSingle ? '\'' : '"') &&
          source[i + 1] == ch && source[i + 2] == ch) {
        erase(3);
        i += 2;
        state = State::Normal;
        continue;
      }
      erase(1);
      if (escaped) {
        escaped = false;
        continue;
      }
      if (ch == '\\') {
        escaped = true;
        continue;
      }
      if ((state == State::Single && ch == '\'') ||
          (state == State::Double && ch == '"') ||
          (state == State::Backtick && ch == '`'))
        state = State::Normal;
      continue;
    }
    if (!python && ch == '/' && next == '*') {
      erase(2);
      ++i;
      state = State::Block;
      continue;
    }
    if (!python && ch == '/' && next == '/') {
      erase(2);
      ++i;
      state = State::Line;
      continue;
    }
    if (python && ch == '#') {
      erase(1);
      state = State::Line;
      continue;
    }
    if (ch == '\'' || ch == '"' || ch == '`') {
      if (python && i + 2 < source.size() && source[i + 1] == ch &&
          source[i + 2] == ch) {
        erase(3);
        i += 2;
        state = ch == '\'' ? State::TripleSingle : State::TripleDouble;
      } else {
        erase(1);
        state = ch == '\''  ? State::Single
                : ch == '"' ? State::Double
                            : State::Backtick;
      }
    }
  }
  return masked;
}
struct Navigator {
  Result& result;
  std::string_view original;
  std::set<std::string> seen;
  void add(std::string kind, std::string name, size_t offset, size_t start,
           size_t length) {
    if (name.empty() || name.size() > 200 || offset >= original.size()) return;
    const size_t line =
        1 + static_cast<size_t>(std::count(
                original.begin(),
                original.begin() + static_cast<std::ptrdiff_t>(offset), '\n'));
    const size_t last = original.rfind('\n', offset);
    const size_t column =
        offset - (last == std::string_view::npos ? 0 : last + 1) + 1;
    const std::string key = std::to_string(offset) + ":" + name;
    if (!seen.insert(key).second) return;
    if (result.rows.size() >= 15000) {
      result.truncated = true;
      return;
    }
    const auto bounded =
        std::min(length, original.size() - std::min(start, original.size()));
    result.rows.push_back(
        {std::move(kind), std::move(name),
         signature(std::string(original.substr(start, bounded))), line,
         column});
  }
};
bool keyword(const std::string& name) {
  static const std::set<std::string> skip = {
      "if",      "for",      "while",         "switch",  "catch",
      "return",  "sizeof",   "decltype",      "alignof", "new",
      "delete",  "throw",    "static_assert", "else",    "do",
      "case",    "function", "define",        "include", "import",
      "require", "print",    "assert",        "try"};
  return skip.count(name) > 0;
}
void runRegex(Navigator& n, const std::string& clean, const std::regex& pattern,
              const std::string& kind, int capture = 1) {
  for (std::sregex_iterator it(clean.begin(), clean.end(), pattern), end;
       it != end; ++it) {
    const auto m = *it;
    const auto name = m[capture].str();
    if (keyword(name)) continue;
    n.add(kind, name, static_cast<size_t>(m.position(capture)),
          static_cast<size_t>(m.position()), static_cast<size_t>(m.length()));
  }
}
}  // namespace
Result extractRegex(std::string_view path, std::string_view contents) {
  Result out;
  if (contents.size() > 4 * 1024 * 1024) {
    contents = contents.substr(0, 4 * 1024 * 1024);
    out.truncated = true;
  }
  const auto ext = extOf(std::string(path));
  const bool py = ext == ".py" || ext == ".pyw";
  const bool js = ext == ".js" || ext == ".jsx" || ext == ".ts" ||
                  ext == ".tsx" || ext == ".mjs";
  const bool kt = ext == ".kt" || ext == ".kts";
  const bool cLike = ext == ".c" || ext == ".h" || ext == ".cc" ||
                     ext == ".cpp" || ext == ".cxx" || ext == ".hpp" ||
                     ext == ".hxx" || ext == ".hh" || ext == ".ipp" ||
                     ext == ".inl" || ext == ".java" || ext == ".cs" || kt;
  // Unnamed documents and unconventional extensions still get a useful lexical
  // outline.
  const bool cpp = !(py || js || kt);
  out.language = py      ? "Python"
                 : js    ? "JavaScript/TypeScript"
                 : kt    ? "Kotlin"
                 : cLike ? "C/C++/Java"
                         : "Generic C-like";
  const std::string clean = maskCommentsAndLiterals(contents, py);
  Navigator n{out, contents, {}};
  if (py) {
    static const std::regex pyType(
        R"((?:^|\n)[ \t]*class[ \t]+([A-Za-z_]\w*)\b)");
    static const std::regex pyFunc(
        R"((?:^|\n)[ \t]*(?:async[ \t]+)?def[ \t]+([A-Za-z_]\w*)[ \t]*\()");
    runRegex(n, clean, pyType, "class");
    runRegex(n, clean, pyFunc, "function");
  } else {
    // Header/macro declarations are taken from original source: <...> includes
    // are literals.
    if (cpp) {
      static const std::regex directives(
          R"((?:^|\n)[ \t]*#[ \t]*(define|include)[ \t]+([A-Za-z_]\w*|<[^>\n]+>|\"[^\"\n]+\"))");
      const std::string rawText(contents);
      for (std::sregex_iterator it(rawText.begin(), rawText.end(), directives),
           end;
           it != end; ++it) {
        const auto m = *it;
        n.add(m[1].str() == "define" ? "macro" : "include", m[2].str(),
              static_cast<size_t>(m.position(2)),
              static_cast<size_t>(m.position()),
              static_cast<size_t>(m.length()));
      }
    }
    static const std::regex types(
        R"(\b(namespace|class|struct|union|interface|enum(?:[ \t]+class)?|record)[ \t\n]+([A-Za-z_]\w*)\b)");
    for (std::sregex_iterator it(clean.begin(), clean.end(), types), end;
         it != end; ++it) {
      const auto m = *it;
      n.add(m[1].str(), m[2].str(), static_cast<size_t>(m.position(2)),
            static_cast<size_t>(m.position()), static_cast<size_t>(m.length()));
    }
    if (js) {
      static const std::regex jsFunctions(
          R"(\b(?:async\s+)?function\s+([A-Za-z_$][\w$]*)\s*\()");
      static const std::regex jsArrow(
          R"(\b(?:const|let|var)\s+([A-Za-z_$][\w$]*)\s*=\s*(?:async\s*)?(?:\([^;{}]*\)|[A-Za-z_$][\w$]*)\s*=>)");
      static const std::regex jsMethod(
          R"((?:^|[{};\n])\s*(?:async\s+)?([A-Za-z_$][\w$]*)\s*\([^;{}]*\)\s*\{)");
      runRegex(n, clean, jsFunctions, "function");
      runRegex(n, clean, jsArrow, "function");
      runRegex(n, clean, jsMethod, "method");
    } else if (kt) {
      static const std::regex ktFun(
          R"(\bfun\s+(?:[\w<>.?]+\.)?([A-Za-z_]\w*)\s*\([\s\S]{0,800}?\))");
      runRegex(n, clean, ktFun, "function");
    } else {
      // A bounded multi-line declaration search covers the prevalent C++
      // patterns: templates, qualified methods, multiline arguments, modifiers,
      // prototypes. Requires at least a return-type/modifier or qualified name
      // to avoid most calls.
      static const std::regex functions(
          R"((?:^|[;{}\n])[ \t]*(?:(?:template\s*<[^;{}]{0,350}>\s*)?)(?:(?:[\w~:<>,*&\[\]]+|\b(?:virtual|static|inline|constexpr|explicit|friend|extern)\b)\s+){1,12}([A-Za-z_~][\w~]*(?:::[A-Za-z_~][\w~]*)*)\s*\([\s\S]{0,1000}?\)\s*(?:const\s*|noexcept(?:\([^)]*\))?\s*|override\s*|final\s*|&{1,2}\s*|->\s*[\w:<>,*& ]+\s*|=\s*0\s*)*(?:\{|;))");
      // Use matching offsets on the masked source to avoid commented-out
      // declarations.
      runRegex(n, clean, functions, "function");
      static const std::regex qualifiers(
          R"((?:^|[;{}\n])\s*(?:explicit\s+)?([A-Za-z_]\w*(?:::[A-Za-z_~]\w*)+)\s*\([\s\S]{0,600}?\)\s*(?:const\s*|noexcept\s*)*(?:\{|;))");
      runRegex(n, clean, qualifiers, "method");
    }
  }
  std::stable_sort(
      out.rows.begin(), out.rows.end(), [](const Symbol& a, const Symbol& b) {
        return a.line == b.line ? a.column < b.column : a.line < b.line;
      });
  if (cLike && !kt && ext != ".java" && ext != ".cs") {
    std::vector<size_t> lines{0};
    for (size_t i = 0; i < clean.size(); ++i)
      if (clean[i] == '\n') lines.push_back(i + 1);
    static const std::regex call(R"(\b([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*\()");
    for (const auto& sym : out.rows) {
      if ((sym.kind != "function" && sym.kind != "method") ||
          sym.line > lines.size())
        continue;
      auto start = lines[sym.line - 1] + sym.column - 1;
      auto open = clean.find_first_of("(;{}", start);
      if (open == std::string::npos || clean[open] != '(') continue;
      size_t end = open + 1;
      int depth = 1;
      for (; end < clean.size() && depth; ++end) {
        if (clean[end] == '(')
          ++depth;
        else if (clean[end] == ')')
          --depth;
      }
      auto body = clean.find_first_of(";{}", end);
      if (body == std::string::npos || clean[body] != '{') continue;
      end = body + 1;
      depth = 1;
      for (; end < clean.size() && depth; ++end) {
        if (clean[end] == '{')
          ++depth;
        else if (clean[end] == '}')
          --depth;
      }
      if (depth) continue;
      for (std::sregex_iterator
               it(clean.begin() + body + 1, clean.begin() + end - 1, call),
           last;
           it != last; ++it) {
        auto name = (*it)[1].str();
        if (keyword(name)) continue;
        auto offset = body + 1 + it->position(1);
        out.calls.push_back(
            {sym.name, name, sym.line,
             static_cast<size_t>(
                 std::upper_bound(lines.begin(), lines.end(), offset) -
                 lines.begin())});
      }
    }
  }
  return out;
}
std::vector<Symbol> filter(const std::vector<Symbol>& rows,
                           std::string_view needle) {
  std::vector<Symbol> out;
  const auto q = lower(std::string(needle));
  for (const auto& row : rows)
    if (q.empty() || lower(row.name).find(q) != std::string::npos ||
        lower(row.kind).find(q) != std::string::npos)
      out.push_back(row);
  return out;
}
std::string toCsv(const std::vector<Symbol>& rows) {
  const auto escaped = [](const std::string& value) {
    std::string s = "\"";
    for (const char ch : value) {
      if (ch == '\"') s += '\"';
      s += ch;
    }
    return s + '\"';
  };
  std::string result = "Kind,Name,Line,Column,Signature\n";
  for (const auto& s : rows)
    result += escaped(s.kind) + "," + escaped(s.name) + "," +
              std::to_string(s.line) + "," + std::to_string(s.column) + "," +
              escaped(s.signature) + "\n";
  return result;
}
}  // namespace dodev::symbols
