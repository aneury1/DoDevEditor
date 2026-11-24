#pragma once
#include <algorithm>
#include <system_error>
#include <utility>

#include "dodev/terminal/TerminalTasks.h"

namespace dodev::terminal {
// A lightweight, workspace-scoped catalog. The runner still reloads before
// execution, so cached labels never silently select a different task.
class TaskCatalog {
  struct Stamp {
    fs::path path;
    fs::file_time_type modified{};
    std::uintmax_t size = 0;
    bool present = false;
    std::string error;
    bool operator==(const Stamp& other) const {
      return path == other.path && modified == other.modified &&
             size == other.size && present == other.present &&
             error == other.error;
    }
  };
  static Stamp inspect(const fs::path& path) {
    Stamp s;
    s.path = path;
    if (path.empty()) return s;
    std::error_code ec;
    auto status = fs::status(path, ec);
    if (ec == std::errc::no_such_file_or_directory ||
        ec == std::errc::not_a_directory)
      return s;
    if (ec) {
      s.error = ec.message();
      return s;
    }
    if (!fs::exists(status)) return s;
    s.present = true;
    if (!fs::is_regular_file(status)) {
      s.error = "Not a regular file";
      return s;
    }
    s.modified = fs::last_write_time(path, ec);
    if (!ec) s.size = fs::file_size(path, ec);
    if (ec) s.error = ec.message();
    return s;
  }
  Stamp stamp_;
  fs::path workspace_;
  bool initialized_ = false;
  std::vector<Task> tasks_;
  std::string error_;

 public:
  bool refresh(const fs::path& workspace, const fs::path& fallback,
               bool force = false) {
    auto next = inspect(
        workspace.empty() ? fs::path{} : workspace / ".vscode" / "tasks.json");
    if (!workspace.empty() && !next.present && next.error.empty() &&
        !fallback.empty()) {
      auto alt = inspect(fallback);
      if (alt.present || !alt.error.empty()) next = std::move(alt);
    }
    if (!force && initialized_ && workspace_ == workspace && next == stamp_)
      return false;
    workspace_ = workspace;
    stamp_ = std::move(next);
    initialized_ = true;
    tasks_.clear();
    error_.clear();
    if (!stamp_.error.empty())
      error_ = stamp_.path.u8string() + ": " + stamp_.error;
    else if (stamp_.present)
      tasks_ = loadTasks(stamp_.path, &error_);
    return true;
  }
  const std::vector<Task>& tasks() const { return tasks_; }
  const fs::path& path() const { return stamp_.path; }
  const std::string& error() const { return error_; }
  const fs::path& workspace() const { return workspace_; }
  int selection(const std::string& previous) const {
    auto it = std::find_if(tasks_.begin(), tasks_.end(),
                           [&](const Task& t) { return t.label == previous; });
    if (it == tasks_.end())
      it = std::find_if(tasks_.begin(), tasks_.end(),
                        [](const Task& t) { return t.defaultBuild; });
    if (it == tasks_.end()) return tasks_.empty() ? -1 : 0;
    return static_cast<int>(it - tasks_.begin());
  }
};
}  // namespace dodev::terminal
