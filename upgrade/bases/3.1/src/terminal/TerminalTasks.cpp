#include "dodev/terminal/TerminalTasks.h"
#include "dodev/settings/Json.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
namespace dodev::terminal {
std::vector<Task> loadTasks(const fs::path &path, std::string *error) {
  if (error)
    error->clear();
  std::vector<Task> result;
  std::error_code ec;
  if (!fs::exists(path, ec))
    return result; // optional
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error)
      *error = "Cannot open " + path.string();
    return result;
  }
  std::string raw{std::istreambuf_iterator<char>(in),
                  std::istreambuf_iterator<char>()};
  if (raw.size() > 1024 * 1024) {
    if (error)
      *error = "tasks.json exceeds 1 MiB";
    return result;
  }
  Json root;
  std::string message;
  if (!Json::parse(raw, root, &message) || !root.isObject()) {
    if (error)
      *error = "Malformed tasks.json: " + message;
    return result;
  }
  const auto *array = root.find("tasks");
  if (!array || !array->isArray())
    return result;
  for (const Json &entry : array->asArray()) {
    if (!entry.isObject())
      continue;
    const std::string type = entry["type"].string("shell");
    if (type != "shell" && type != "process")
      continue;
    Task t;
    t.label = entry["label"].string();
    t.command = entry["command"].string();
    if (t.label.empty() || t.command.empty())
      continue;
    const auto &group = entry["group"];
    t.group = group.isObject() ? group["kind"].string() : group.string();
    if (const auto &options = entry["options"]; options.isObject())
      t.cwd = options["cwd"].string();
    const auto &args = entry["args"];
    if (args.isArray())
      for (const auto &item : args.asArray())
        if (item.isString())
          t.arguments.push_back(item.string());
    result.push_back(std::move(t));
    if (result.size() >= 128)
      break;
  }
  return result;
}
std::string quoteArgument(const std::string &value, bool windows) {
  if (windows) {
    std::string out = "\"";
    for (char ch : value) {
      if (ch == '"')
        out += "\\\"";
      else
        out += ch;
    }
    return out + "\"";
  }
  std::string out = "'";
  for (char ch : value) {
    if (ch == '\'')
      out += "'\\''";
    else
      out += ch;
  }
  return out + "'";
}
std::string commandLine(const Task &task, bool windows) {
  std::string result = task.command;
  for (const auto &arg : task.arguments)
    result += " " + quoteArgument(arg, windows);
  if (!task.cwd.empty()) {
    const std::string cd = windows ? "cd /d " : "cd ";
    result = cd + quoteArgument(task.cwd, windows) +
             (windows ? " && " : " && ") + result;
  }
  return result;
}
std::string launchFileCommand(const fs::path &file, bool windows) {
  const auto path = file.u8string(), ext = file.extension().u8string();
  const auto q = quoteArgument(path, windows);
  if (ext == ".py")
    return (windows ? "python " : "python3 ") + q;
  if (ext == ".js" || ext == ".mjs")
    return "node " + q;
  if (ext == ".sh")
    return "sh " + q;
  if (ext == ".ps1")
    return "pwsh -File " + q;
  if (ext == ".rb")
    return "ruby " + q;
  if (ext == ".pl")
    return "perl " + q;
  if (ext == ".bat" || ext == ".cmd" || ext == ".exe")
    return q;
  // Do not silently attempt to execute or compile an arbitrary unsupported
  // file.
  return {};
}
std::string defaultTasksJson() {
  return R"({
  "version": "2.0.0",
  "tasks": [
    {
      "label": "CMake: Configure",
      "type": "shell",
      "command": "cmake -S . -B build"
    },
    {
      "label": "CMake: Build",
      "type": "shell",
      "command": "cmake --build build -j 4",
      "group": {"kind": "build", "isDefault": true}
    },
    {
      "label": "CTest",
      "type": "shell",
      "command": "ctest --test-dir build --output-on-failure",
      "group": "test"
    }
  ]
})";
}
} // namespace dodev::terminal
