#pragma once
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace dodev {
class Json;
}
namespace dodev::terminal {
namespace fs = std::filesystem;
struct TaskInputOption {
  std::string label, value;
};
struct TaskInput {
  std::string id, type, description, defaultValue, command;
  bool password = false, hasDefault = false;
  std::vector<TaskInputOption> options;
};
using TaskInputs = std::map<std::string, TaskInput>;
struct TaskCancelled {};
struct Task {
  std::string label;
  std::string command;
  std::string group;
  std::vector<std::string> arguments;
  std::string cwd;
  std::string type = "shell", shell;
  std::vector<std::string> shellArgs, dependsOn;
  std::map<std::string, std::string> env;
  bool defaultBuild = false, sequential = false, background = false;
  std::string unsupported;
  std::shared_ptr<const TaskInputs> inputs;
};
struct TaskContext {
  fs::path workspace, file;
  int line = 1, column = 1;
  std::string selectedText;
  fs::path executable, startupCwd;
  std::function<std::optional<std::string>(const TaskInput&)> prompt;
  std::function<std::optional<std::string>(const std::string&)> config;
  std::function<std::optional<std::string>(const std::string&)> commandVariable;
};
struct TaskLaunch {
  std::string label;
  fs::path cwd;
  std::vector<std::string> argv;
  fs::path requiredExecutable;  // optional: check immediately before execution
};
std::shared_ptr<const TaskInputs> parseTaskInputs(const dodev::Json& root);
std::vector<Task> loadTasks(const fs::path& path, std::string* error = nullptr);
std::vector<TaskLaunch> planTasks(const std::vector<Task>& tasks,
                                  const std::string& label,
                                  const TaskContext& context);
std::string quoteArgument(const std::string& value, bool windows = false);
std::string commandLine(const Task& task, bool windows = false);
std::string launchFileCommand(const fs::path& file, bool windows = false);
std::string defaultTasksJson();
}  // namespace dodev::terminal
