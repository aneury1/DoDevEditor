#pragma once
#include <cstddef>
#include <string>
#include <string_view>

namespace dodev::editor {
// A deterministic, Raven-independent transformation. All offsets are UTF-8
// bytes, matching Rtext_editor_get_selection. Edits are applied with
// Rtext_editor_insert_text so Raven's native undo/redo history is retained.
struct Result {
  bool handled = false;
  bool changed = false;
  std::size_t replaceBegin = 0, replaceEnd = 0;
  std::string replacement;
  std::size_t selectBegin = 0, selectEnd = 0;
  std::string status;
};
struct Options {
  unsigned tabSize = 4;
  bool useSpaces = true;
  std::string lineComment = "//";
  std::string blockStart = "/*", blockEnd = "*/";
};
Result execute(std::string_view action, const std::string &text,
               std::size_t anchor, std::size_t caret,
               const Options &options = {});
// UTF-8 byte-aware position helpers shared by the UI and unit tests.
std::size_t lineStart(const std::string &text, std::size_t index);
std::size_t lineEnd(const std::string &text, std::size_t index);
std::size_t previousCharacter(const std::string &text, std::size_t index);
std::size_t nextCharacter(const std::string &text, std::size_t index);
} // namespace dodev::editor
