#pragma once
#include <string>
namespace dodev {
// The CLI prompt is intentionally distinct from a normal shell prompt. Ignore
// complete ANSI CSI sequences emitted by readline around the prompt.
inline bool gdbPromptReady(const std::string& raw) {
  std::string text;
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] == '\x1b' && i + 1 < raw.size() && raw[i + 1] == '[') {
      i += 2;
      while (i < raw.size() && !(raw[i] >= '@' && raw[i] <= '~')) ++i;
      if (i == raw.size()) return false;
      continue;
    }
    text += raw[i];
  }
  const auto last = text.find_last_not_of(" \r\n\t");
  if (last == std::string::npos) return false;
  const std::string marker = "(dodev-gdb)";
  if (last + 1 < marker.size() ||
      text.compare(last + 1 - marker.size(), marker.size(), marker) != 0)
    return false;
  const auto begin = last + 1 - marker.size();
  return begin == 0 || text[begin - 1] == '\n' || text[begin - 1] == '\r';
}
}  // namespace dodev
