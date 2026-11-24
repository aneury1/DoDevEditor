#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "dodev/terminal/TerminalTasks.h"
int main() {
  using namespace dodev::terminal;
  auto p =
      std::filesystem::temp_directory_path() / "dodev_tasks_integration.json";
  {
    std::ofstream o(p);
    o << defaultTasksJson();
  }
  std::string err;
  auto tasks = loadTasks(p, &err);
  assert(err.empty());
  assert(tasks.size() == 3);
  assert(tasks[1].group == "build");
  assert(tasks[1].defaultBuild);
  assert(commandLine(tasks[1]).find("cmake --build") == 0);
  assert(quoteArgument("my 'file'.cpp") == "'my '\\''file'\\''.cpp'");
  assert(launchFileCommand("test.py") == "python3 'test.py'");
  assert(launchFileCommand("test.cpp").empty());
  auto root = std::filesystem::temp_directory_path();
  TaskContext ctx{root, root / "my file.cpp", 7, 4, "selection"};
  {
    std::ofstream o(p);
    o << R"JSON({
 // VSCode comments and trailing commas
 "version":"2.0.0",
 "options":{"cwd":"${workspaceFolder}"},
 "tasks":[
  {"label":"configure","type":"process","command":"cmake","args":["-S","${workspaceFolder}","-B","build",],},
  {"label":"compile","type":"cppbuild","command":"g++","args":["${file}","-o","${fileBasenameNoExtension}"]},
  {"label":"build","type":"shell","command":"echo","args":["https://example.org/a/*b*/","${lineNumber}","${selectedText}"],"dependsOn":["configure","compile"],"dependsOrder":"sequence","group":{"kind":"build","isDefault":true},"options":{"env":{"MODE":"debug"}},"linux":{"command":"printf"}},
  {"label":"compound","dependsOn":"build"},
  {"label":"cycle","dependsOn":"cycle"},
  {"label":"parallel","dependsOn":["configure","compile"]},
  {"label":"extension","type":"cmake"},
  {"label":"bad variable","command":"${command:cmake.foo}"},
  {"label":"background","command":"sleep 2","isBackground":true},
 ],
 })JSON";
  }
  tasks = loadTasks(p, &err);
  assert(err.empty());
  assert(tasks.size() == 9);
  assert(tasks[2].defaultBuild);
  auto plan = planTasks(tasks, "compile", ctx);
  assert(plan.size() == 1);
  assert(plan[0].argv[1] == ctx.file.u8string());
  assert(plan[0].argv[3] == "my file");
  assert(plan[0].cwd == root);
#ifndef _WIN32
  plan = planTasks(tasks, "compound", ctx);
  assert(plan.size() == 3);
  assert(plan[0].label == "configure");
  assert(plan[1].label == "compile");
  assert(plan[2].argv[0] == "env");
  assert(plan[2].argv[2] == "MODE=debug");
  assert(plan[2].argv.back().find("https://example.org/a/*b*/") !=
         std::string::npos);
  assert(plan[2].argv.back().find("'7' 'selection'") != std::string::npos);
#ifdef __linux__
  assert(plan[2].argv.back().find("'printf'") == 0);
#endif
#endif
  for (auto name :
       {"cycle", "parallel", "extension", "bad variable", "background"}) {
    bool failed = false;
    try {
      planTasks(tasks, name, ctx);
    } catch (const std::runtime_error&) {
      failed = true;
    }
    assert(failed);
  }
  {
    std::ofstream o(p);
    o << "{ /* unterminated";
  }
  assert(loadTasks(p, &err).empty());
  assert(!err.empty());
  std::filesystem::remove(p);
  assert(loadTasks(p).empty());
  std::cout << "terminal tasks passed\n";
}
