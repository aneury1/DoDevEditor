#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace dodev {
// A compact VT screen model, independent of Raven and of the process transport.
// Main GUI thread owns the screen. The PTY reader only queues bytes.
struct TerminalColor {
  std::uint8_t r = 226, g = 232, b = 240;
};
struct TerminalCell {
  std::string glyph = " ";
  TerminalColor foreground{226, 232, 240};
  TerminalColor background{10, 15, 26};
  bool bold = false;
};
class TerminalScreen {
public:
  explicit TerminalScreen(int columns = 100, int rows = 28);
  void feed(std::string_view bytes);
  void resize(int columns, int rows);
  void scroll(int lines); // positive = toward older output
  void toBottom();
  void clear();
  void setScrollbackLimit(std::size_t lines);
  int columns() const { return cols_; }
  int rows() const { return rows_; }
  int scrollOffset() const { return scroll_offset_; }
  int cursorX() const { return x_; }
  int cursorY() const { return y_; }
  bool cursorVisible() const { return cursor_visible_ && scroll_offset_ == 0; }
  const TerminalCell &cell(int row, int col) const;
  std::string lineText(int row) const;
  std::string visibleText() const;
  bool isAlternate() const { return alternate_; }

private:
  using Line = std::vector<TerminalCell>;
  enum class Parse { Normal, Esc, Csi, Osc, OscEsc };
  int cols_, rows_, x_ = 0, y_ = 0, save_x_ = 0, save_y_ = 0;
  int scroll_top_ = 0, scroll_bottom_ = 0, scroll_offset_ = 0;
  std::deque<Line> history_;
  std::size_t max_history_ = 3000;
  std::vector<Line> main_, alt_;
  bool alternate_ = false, cursor_visible_ = true;
  bool wrapping_ = false;
  Parse state_ = Parse::Normal;
  std::string params_;
  std::string pending_utf8_;
  int utf8_remaining_ = 0;
  TerminalCell style_{};
  TerminalCell blank_{};
  Line blankLine() const;
  std::vector<Line> &buffer();
  const std::vector<Line> &buffer() const;
  void put(std::string_view glyph);
  void newline();
  void index();
  void reverseIndex();
  void carriageReturn();
  void csi(char final);
  void sgr(const std::vector<int> &args);
  void eraseDisplay(int how);
  void eraseLine(int how);
  void scrollUp(int amount);
  void scrollDown(int amount);
  void switchAlternate(bool on);
  void reset();
  static TerminalColor palette(int number);
};
} // namespace dodev
