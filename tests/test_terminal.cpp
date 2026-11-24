#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "TerminalScreen.h"
#include "TerminalSession.h"
using namespace dodev;
namespace {
void expect(bool v, const char* error) {
  if (!v) throw std::runtime_error(error);
}
}  // namespace
int main() {
  try {
    TerminalScreen grid(20, 5);
    grid.feed("Hola\r\nMundo");
    expect(grid.lineText(0) == "Hola", "text first row");
    expect(grid.lineText(1) == "Mundo", "text second row");
    grid.feed("\x1b[1;1H\x1b[31mR\x1b[0m");
    expect(grid.cell(0, 0).glyph == "R", "cursor position");
    expect(grid.cell(0, 0).foreground.r > grid.cell(0, 0).foreground.g,
           "red ANSI");
    grid.feed("\x1b[?1049hAlternate\x1b[?1049l");
    expect(grid.lineText(0).substr(0, 1) == "R", "alternate screen restore");
    grid.feed("\x1b[2J\x1b[1;1H");
    expect(grid.lineText(0).empty(), "clear ANSI");
    grid.feed("\x1b[38;2;11;33;55mQ");
    expect(grid.cell(0, 0).foreground.g == 33, "RGB ANSI");
    grid.resize(45, 12);
    expect(grid.columns() == 45 && grid.rows() == 12, "resize");
    grid.clear();
    expect(grid.lineText(0).empty(), "clear");
    grid.resize(20, 5);
    grid.feed("alpha\r\nbeta\r\ngamma\r\ndelta\r\nepsilon\r\nzeta");
    expect(grid.fullText().find("alpha") != std::string::npos,
           "export contains scrollback");
    expect(grid.fullText().find("zeta") != std::string::npos,
           "export contains viewport");
#ifndef _WIN32
    TerminalSession term;
    std::string error;
    expect(term.start(std::filesystem::current_path(), &error),
           "start POSIX PTY");
    expect(term.resize(95, 25), "PTY resize");
    expect(term.sendLine("printf 'DODEV_PTY_READY\\n'"), "send input");
    bool ready = false;
    std::string output;
    for (int i = 0; i < 120; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
      output += term.takeOutput();
      if (output.find("DODEV_PTY_READY") != std::string::npos) {
        ready = true;
        break;
      }
    }
    expect(ready, "PTY shell output received");
    expect(term.sendBytes("pwd\n"), "PTY raw input");
    term.interrupt();
    term.stop();
    expect(term.start(std::filesystem::current_path(), &error, "/bin/sh",
                      {"/bin/sh", "-c", "printf '%s' \"$1\"; exit 7", "task",
                       "task argument with spaces"}),
           "launch task process");
    output.clear();
    for (int i = 0; i < 120 && term.running(); ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
      output += term.takeOutput();
    }
    output += term.takeOutput();
    expect(!term.running(), "task process ended");
    expect(term.exitCode() == 7, "task exit code");
    expect(output.find("task argument with spaces") != std::string::npos,
           "task argument boundaries");
    term.stop();
#endif
    std::cout << "terminal VT100 and PTY OK\n";
  } catch (const std::exception& e) {
    std::cerr << "TEST FAILED: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
