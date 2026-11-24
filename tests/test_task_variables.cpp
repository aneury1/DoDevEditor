#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "dodev/terminal/TerminalTasks.h"
#define CHECK(x)                            \
  do {                                      \
    if (!(x)) throw std::runtime_error(#x); \
  } while (0)
using namespace dodev::terminal;
int main() {
  auto root =
      fs::temp_directory_path() /
      ("dodev variables " +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    fs::create_directories(root / ".vscode");
    fs::create_directories(root / "src");
    {
      std::ofstream f(root / ".vscode/settings.json");
      f << R"({// workspace settings
 "build.jobs":8,"build":{"directory":"build out","enabled":true},"dodev.taskCommandVariables":{"cmake.launchTargetPath":"build out/app"},})";
    }
    auto path = root / ".vscode/tasks.json";
    {
      std::ofstream f(path);
      f << R"JSON({"version":"2.0.0","inputs":[
 {"id":"mode","type":"pickString","default":"Debug","options":["Debug",{"label":"Optimized","value":"Release"}]},
 {"id":"target","type":"promptString","default":"my app","description":"Target"},
 {"id":"exe","type":"command","command":"cmake.launchTargetPath"},
 {"id":"masked","type":"promptString","password":true}],
 "tasks":[
 {"label":"prepare","type":"process","command":"echo","args":["${input:mode}","${input:target}","${config:build.jobs}","${config:build.enabled}"]},
 {"label":"build","type":"process","command":"echo","args":["${input:mode}","${input:target}","${input:exe}","${command:cmake.launchTargetPath}","${config:build.directory}","${fileDirnameBasename}","${fileWorkspaceFolder}","${execPath}","${cwd}","${defaultBuildTask}","${config:user.name}"],"dependsOn":"prepare","group":{"kind":"build","isDefault":true}},
 {"label":"shell","type":"shell","command":"echo","args":["${input:target}"]},
 {"label":"raw","type":"shell","command":"echo ${input:target}"},
 {"label":"unknown","type":"process","command":"echo","args":["${input:unknown}"]},
 {"label":"missingSetting","type":"process","command":"echo","args":["${config:does.not.exist}"]},
 {"label":"masked","type":"process","command":"echo","args":["${input:masked}"]}
 ]})JSON";
    }
    std::string err;
    auto tasks = loadTasks(path, &err);
    CHECK(err.empty());
    CHECK(tasks.size() == 7);
    TaskContext c;
    c.workspace = root;
    c.file = root / "src/main.cpp";
    c.executable = root / "DoDevEditor";
    c.startupCwd = root / "src";
    int prompts = 0;
    c.prompt = [&](const TaskInput& i) -> std::optional<std::string> {
      ++prompts;
      if (i.id == "mode") {
        CHECK(i.defaultValue == "Debug");
        CHECK(i.options.at(1).label == "Optimized");
        return "Release";
      }
      if (i.id == "masked") {
        CHECK(i.password);
        return "example";
      }
      CHECK(i.defaultValue == "my app");
      return "a b; $(echo unexpected) '";
    };
    c.config = [](const auto& k) -> std::optional<std::string> {
      if (k == "user.name") return "demo";
      return std::nullopt;
    };
    auto plan = planTasks(tasks, "build", c);
    CHECK(plan.size() == 2);
    CHECK(prompts == 2);
    CHECK(plan[0].argv[1] == "Release");
    CHECK(plan[0].argv[3] == "8");
    CHECK(plan[0].argv[4] == "true");
    auto& a = plan[1].argv;
    CHECK(a[2] == plan[0].argv[2]);
    CHECK(a[3] == "build out/app");
    CHECK(a[4] == a[3]);
    CHECK(a[5] == "build out");
    CHECK(a[6] == "src");
    CHECK(a[7] == root.u8string());
    CHECK(a[8] == c.executable.u8string());
    CHECK(a[9] == c.startupCwd.u8string());
    CHECK(a[10] == "build");
    CHECK(a[11] == "demo");
#ifndef _WIN32
    auto shell = planTasks(tasks, "shell", c);
    CHECK(shell[0].argv.back() == "'echo' " + quoteArgument(a[2]));
    auto raw = planTasks(tasks, "raw", c);
    CHECK(raw[0].argv.back() == "echo " + quoteArgument(a[2]));
#endif
    auto masked = planTasks(tasks, "masked", c);
    CHECK(masked[0].argv[1] == "example");
    for (auto name : {"unknown", "missingSetting"}) {
      bool failed = false;
      try {
        planTasks(tasks, name, c);
      } catch (const std::runtime_error&) {
        failed = true;
      }
      CHECK(failed);
    }
    c.prompt = [](const auto&) -> std::optional<std::string> {
      return std::nullopt;
    };
    bool cancelled = false;
    try {
      planTasks(tasks, "build", c);
    } catch (const TaskCancelled&) {
      cancelled = true;
    }
    CHECK(cancelled);
    c.prompt = [](const auto&) -> std::optional<std::string> {
      return "invalid option";
    };
    bool invalid = false;
    try {
      planTasks(tasks, "build", c);
    } catch (const std::runtime_error&) {
      invalid = true;
    }
    CHECK(invalid);
    {
      std::ofstream f(path);
      f << R"({"tasks":[],"inputs":[{"id":"x","type":"pickString","options":["a"],"default":"b"}]})";
    }
    CHECK(loadTasks(path, &err).empty());
    CHECK(!err.empty());
    fs::remove_all(root);
    std::cout << "task input/config/command variables passed\n";
    return 0;
  } catch (const std::exception& e) {
    fs::remove_all(root);
    std::cerr << e.what() << '\n';
    return 1;
  }
}
