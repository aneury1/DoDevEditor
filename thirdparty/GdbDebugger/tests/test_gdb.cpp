#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "GdbBreakpoints.h"
#include "GdbConfig.h"
#include "TerminalSession.h"
#include "dodev/debug/GdbPrompt.h"
#include "dodev/plugins/PluginManager.h"
#define CHECK(x)                            \
  do {                                      \
    if (!(x)) throw std::runtime_error(#x); \
  } while (0)
using namespace dodev;
using namespace dodev::gdb;
int main() {
  try {
    auto root =
        fs::temp_directory_path() /
        ("dodev-gdb-test-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / ".vscode");
    Context c{root, root / "main test.cpp"};
    auto file = root / ".vscode" / "launch.json";
    {
      std::ofstream f(file);
      f << R"({// comment
      "version":"0.2.0","configurations":[{"name":"Launch","type":"cppdbg","request":"launch","MIMode":"gdb","program":"${workspaceFolder}/bin/my app","args":["two words","a'b","https://example.com"],"cwd":"${workspaceFolder}","environment":[{"name":"MODE","value":"test"}],"stopAtEntry":true,"preLaunchTask":"${defaultBuildTask}","setupCommands":[{"text":"-enable-pretty-printing","ignoreFailures":true}],},
      {"name":"Attach","type":"cppdbg","request":"attach","processId":"${command:pickProcess}"},],})";
    }
    {
      std::ofstream f(root / ".vscode" / "tasks.json");
      f << R"({"version":"2.0.0","tasks":[{"label":"Configure","type":"process","command":"echo","args":["${config:build.mode}"]},{"label":"Build","type":"cppbuild","command":"g++","args":["-g","${file}"],"dependsOn":"Configure","group":{"kind":"build","isDefault":true},"options":{"cwd":"${workspaceFolder}"}}]})";
    }
    {
      std::ofstream f(root / ".vscode" / "settings.json");
      f << R"({"build.mode":"Debug"})";
    }
    CHECK(names(c).size() == 2);
    CHECK(toggleBreakpoint(c, 7));
    CHECK(loadBreakpoints(c).at(0).line == 7);
    auto l = makeLaunch(c, "Launch", "gdb-test");
    CHECK(l.argv[0] == "gdb-test");
    CHECK(l.cwd == root);
    CHECK(l.restartable);
    auto boot = l.argv.back();
    CHECK(boot.find("subprocess.run") != std::string::npos);
    CHECK(boot.find("Configure") < boot.find("preLaunchTask: Build"));
    CHECK(boot.find("Debug") != std::string::npos);
    CHECK(boot.find("my app") != std::string::npos);
    CHECK(boot.find("https://example.com") != std::string::npos);
    CHECK(boot.find("MODE=test") != std::string::npos);
    CHECK(boot.find("start") != std::string::npos);
    CHECK(boot.find("main test.cpp") != std::string::npos);
    CHECK(boot.find("subprocess.run") < boot.find("file "));
    CHECK(!toggleBreakpoint(c, 7));
    CHECK(loadBreakpoints(c).empty());
    CHECK(makeLaunch(c, "Attach", "gdb").needsPid);
    auto a = makeLaunch(c, "Attach", "gdb", "42");
    CHECK(!a.needsPid && !a.restartable);
    CHECK(a.argv.back().find("attach 42") != std::string::npos);
    bool rejected = false;
    try {
      makeLaunch(c, "Attach", "gdb", "42;quit");
    } catch (...) {
      rejected = true;
    }
    CHECK(rejected);
    CHECK(gdbPromptReady("hello\r\n(dodev-gdb) \x1b[?2004h"));
    CHECK(!gdbPromptReady("hello (dodev-gdb) "));
    CHECK(!gdbPromptReady("(dodev-gdb) next"));
    SettingsManager settings(root / "settings");
    CHECK(settings.load());
    ThemeManager themes;
    TemplateRegistry templates;
    PluginManager pm(settings, templates, themes);
    int launched = 0;
    std::string sent;
    PluginManager::HostHooks h;
    h.debugContext = [&](auto& w, auto& f, int& line) {
      w = root.u8string();
      f = c.file.u8string();
      line = 8;
      return true;
    };
    h.debugLaunch = [&](const auto&, const auto& args) {
      ++launched;
      CHECK(args.front() == "gdb");
      return true;
    };
    h.debugControl = [&](const auto& cmd) {
      if (cmd == "@state") return launched ? 1 : 0;
      sent = cmd;
      return 1;
    };
    h.debugOpenFile = [](const auto&) { return true; };
    h.debugStatus = [](const auto&) {};
    h.promptInput = [](const auto&, const auto&, const auto&, auto) {
      return true;
    };
    pm.setHostHooks(std::move(h));
    CHECK(pm.discoverAndLoad(DODEV_GDB_TEST_ROOT) == 0);
    pm.setEnabled("com.dodev.gdb", true);
    CHECK(pm.discoverAndLoad(DODEV_GDB_TEST_ROOT) == 1);
    CHECK(pm.execute("gdb.start"));
    CHECK(launched == 1);
    CHECK(pm.execute("gdb.next"));
    CHECK(sent == "next");
    CHECK(pm.execute("gdb.breakpoint"));
    CHECK(sent.find("break ") == 0);
    CHECK(loadBreakpoints(c).at(0).line == 8);
    CHECK(pm.execute("gdb.stop"));
    CHECK(sent == "@stop");
    pm.shutdown();
#ifndef _WIN32
    TerminalSession terminal;
    std::string error;
    CHECK(terminal.start(root, &error, "",
                         {"/bin/echo", "arg with spaces; $HOME"}));
    std::string output;
    for (int i = 0; i < 100; ++i) {
      output += terminal.takeOutput();
      if (!terminal.running()) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    output += terminal.takeOutput();
    CHECK(output.find("arg with spaces; $HOME") != std::string::npos);
    terminal.stop();
#endif
    fs::remove_all(root);
    std::cout << "GDB JSONC, profiles, preLaunchTask, breakpoints, prompt, "
                 "plugin and direct PTY OK\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
