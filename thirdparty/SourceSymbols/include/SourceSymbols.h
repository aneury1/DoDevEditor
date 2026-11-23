#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace dodev::symbols {
struct Symbol {
  std::string kind;
  std::string name;
  std::string signature;
  std::size_t line = 0;   // one-based line
  std::size_t column = 1; // one-based byte column
};
struct Result {
  std::string language;
  std::vector<Symbol> rows;
  bool truncated = false;
};
// Works with unsaved editor text; deliberately bounded. This is a lexical
// outline, not a compiler AST. It handles common C/C++/Java/Kotlin/JS/TS/Python
// syntax.
Result extract(std::string_view path, std::string_view contents);
std::string toCsv(const std::vector<Symbol> &rows);
std::vector<Symbol> filter(const std::vector<Symbol> &rows,
                           std::string_view needle);
} // namespace dodev::symbols
