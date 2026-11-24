#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "dodev/terminal/TaskCatalog.h"
using namespace dodev::terminal;
static void check(bool ok, const char* what) {
  if (!ok) throw std::runtime_error(what);
}
static void write(const fs::path& p, const std::string& body) {
  fs::create_directories(p.parent_path());
  std::ofstream out(p);
  out << body;
  if (!out) throw std::runtime_error("Cannot write fixture");
}
int main() {
  auto tmp = fs::temp_directory_path() /
             ("dodev-task-catalog-" +
              std::to_string(
                  std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup {
    fs::path p;
    ~Cleanup() {
      std::error_code ec;
      fs::remove_all(p, ec);
    }
  } cleanup{tmp};
  auto first = tmp / "first", second = tmp / "second",
       local = first / ".vscode/tasks.json",
       fallback = tmp / "global/tasks.json";
  write(fallback, R"({"tasks":[{"label":"Global","command":"echo"}]})");
  TaskCatalog c;
  check(c.refresh({}, fallback), "initial refresh");
  check(c.tasks().empty(), "no workspace cannot list global tasks");
  check(c.refresh(first, fallback), "workspace opens");
  check(c.path() == fallback && c.tasks().at(0).label == "Global",
        "global fallback");
  write(local, defaultTasksJson());
  check(c.refresh(first, fallback), "new workspace task file");
  check(c.path() == local && c.tasks().size() == 3, "workspace precedence");
  check(c.selection("") == 1, "default build selection");
  check(c.selection(c.tasks().front().label) == 0, "preserve selected label");
  check(!c.refresh(first, fallback), "unchanged file is not reparsed");
  auto old = c.tasks()[1].label;
  write(local, R"({// jsonc
 "tasks":[{"label":"New task","command":"echo",}],})");
  check(c.refresh(first, fallback), "external update");
  check(c.error().empty() && c.tasks().size() == 1 &&
            c.tasks()[0].label == "New task",
        "updated JSONC label");
  check(c.selection(old) == 0, "removed selection replaced");
  bool rejected = false;
  try {
    planTasks(c.tasks(), old, {first});
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  check(rejected, "stale label rejected by runner");
  write(local, "{ bad json");
  check(c.refresh(first, fallback), "invalid file changed");
  check(c.tasks().empty() && !c.error().empty() && c.selection("") == -1,
        "invalid file clears catalog; no fallback");
  fs::remove(local);
  check(c.refresh(first, fallback), "deletion detected");
  check(c.tasks().at(0).label == "Global", "fallback after deletion");
  check(c.refresh(second, fallback),
        "workspace change even with same fallback");
  check(c.workspace() == second, "workspace identity");
  write(second / ".vscode/tasks.json", R"({"tasks":[]})");
  check(c.refresh(second, fallback), "second workspace file");
  check(c.tasks().empty() && c.error().empty(),
        "empty workspace list takes precedence");
  check(c.refresh(second, fallback, true), "forced refresh before launch");
  std::cout << "Task catalog lifecycle passed\n";
}
