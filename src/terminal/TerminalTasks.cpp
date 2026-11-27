#include "dodev/terminal/TerminalTasks.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>

#include "dodev/settings/Json.h"
namespace dodev::terminal {
namespace {
std::string stripComments(const std::string& raw) {
  std::string out;
  bool str = false, esc = false;
  for (size_t i = 0; i < raw.size(); ++i) {
    char c = raw[i];
    if (str) {
      out += c;
      if (esc)
        esc = false;
      else if (c == '\\')
        esc = true;
      else if (c == '"')
        str = false;
      continue;
    }
    if (c == '"') {
      str = true;
      out += c;
      continue;
    }
    if (c == '/' && i + 1 < raw.size() && raw[i + 1] == '/') {
      out += ' ';
      while (i + 1 < raw.size() && raw[i + 1] != '\n') ++i;
      continue;
    }
    if (c == '/' && i + 1 < raw.size() && raw[i + 1] == '*') {
      out += ' ';
      i += 2;
      while (i + 1 < raw.size() && !(raw[i] == '*' && raw[i + 1] == '/')) {
        if (raw[i] == '\n') out += '\n';
        ++i;
      }
      if (i + 1 == raw.size() || i >= raw.size())
        throw std::runtime_error("Unterminated JSONC comment");
      ++i;
      continue;
    }
    out += c;
  }
  std::string clean;
  str = false;
  esc = false;
  for (size_t i = 0; i < out.size(); ++i) {
    char c = out[i];
    if (str) {
      clean += c;
      if (esc)
        esc = false;
      else if (c == '\\')
        esc = true;
      else if (c == '"')
        str = false;
      continue;
    }
    if (c == '"') str = true;
    if (c == ',') {
      size_t j = i + 1;
      while (j < out.size() && std::isspace(static_cast<unsigned char>(out[j])))
        ++j;
      if (j < out.size() && (out[j] == '}' || out[j] == ']')) continue;
    }
    clean += c;
  }
  return clean;
}

const char* platform() {
#ifdef _WIN32
  return "windows";
#elif defined(__APPLE__)
  return "osx";
#else
  return "linux";
#endif
}
Json readObject(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("Cannot open " + path.u8string());
  std::string raw;
  char buf[8192];
  while (in.read(buf, sizeof(buf)) || in.gcount()) {
    raw.append(buf, static_cast<size_t>(in.gcount()));
    if (raw.size() > 1024 * 1024)
      throw std::runtime_error("JSONC file exceeds 1 MiB");
  }
  if (raw.rfind("\xef\xbb\xbf", 0) == 0) raw.erase(0, 3);
  Json root;
  std::string message;
  if (!Json::parse(stripComments(raw), root, &message) || !root.isObject())
    throw std::runtime_error("Malformed " + path.u8string() + ": " + message);
  return root;
}
const Json* lookup(const Json& root, const std::string& key) {
  if (auto v = root.find(key)) return v;
  auto dot = key.find('.');
  if (dot == std::string::npos) return nullptr;
  auto p = root.find(key.substr(0, dot));
  return p ? lookup(*p, key.substr(dot + 1)) : nullptr;
}
std::string scalar(const Json& v, const std::string& key) {
  if (v.isString()) return v.string();
  if (v.isBool() || v.isNumber()) return v.dump(0);
  throw std::runtime_error(
      "Task variable is not a string, number or boolean: " + key);
}
std::shared_ptr<const TaskInputs> parseInputs(const Json& root) {
  auto result = std::make_shared<TaskInputs>();
  auto all = root.find("inputs");
  if (!all) return result;
  if (!all->isArray()) throw std::runtime_error("inputs must be an array");
  for (auto& j : all->asArray()) {
    if (!j.isObject()) throw std::runtime_error("Invalid task input");
    TaskInput i;
    i.id = j["id"].string();
    i.type = j["type"].string();
    i.description = j["description"].string(i.id);
    i.command = j["command"].string();
    i.password = j["password"].boolean();
    if (i.id.empty() || i.type.empty())
      throw std::runtime_error("Task input requires id and type");
    if (auto d = j.find("default")) {
      if (!d->isString())
        throw std::runtime_error("Task input default must be string: " + i.id);
      i.hasDefault = true;
      i.defaultValue = d->string();
    }
    if (i.type == "pickString") {
      if (!j["options"].isArray())
        throw std::runtime_error("pickString requires options: " + i.id);
      std::set<std::string> values;
      for (auto& o : j["options"].asArray()) {
        TaskInputOption opt;
        if (o.isString())
          opt = {o.string(), o.string()};
        else if (o.isObject() && o["value"].isString() && o["label"].isString())
          opt = {o["label"].string(), o["value"].string()};
        else
          throw std::runtime_error("Invalid pickString option: " + i.id);
        if (!values.insert(opt.value).second)
          throw std::runtime_error("Duplicate pickString value: " + i.id);
        i.options.push_back(std::move(opt));
      }
      if (i.options.empty() || (i.hasDefault && !values.count(i.defaultValue)))
        throw std::runtime_error("Invalid pickString options/default: " + i.id);
    }
    if (!result->emplace(i.id, std::move(i)).second)
      throw std::runtime_error("Duplicate task input id");
  }
  return result;
}
class Resolver {
  const TaskContext& c;
  const std::vector<Task>& tasks;
  std::map<std::string, std::string> cache;
  std::optional<Json> settings;
  const Json& workspaceSettings() {
    if (!settings) {
      auto p = c.workspace / ".vscode" / "settings.json";
      settings = fs::exists(p) ? readObject(p) : Json::object();
    }
    return *settings;
  }
  std::string command(const std::string& id) {
    if (auto m = workspaceSettings().find("dodev.taskCommandVariables")) {
      if (auto v = m->find(id)) return scalar(*v, id);
    }
    if (c.commandVariable) {
      if (auto v = c.commandVariable(id)) return *v;
    }
    throw std::runtime_error(
        "Unsupported VSCode command variable: " + id +
        " (set dodev.taskCommandVariables in .vscode/settings.json)");
  }
  std::string resolve(const std::string& k, const Task& task) {
    if (k == "workspaceFolder" || k == "workspaceRoot")
      return c.workspace.u8string();
    if (k == "cwd")
      return (c.startupCwd.empty() ? c.workspace : c.startupCwd).u8string();
    if (k.rfind("workspaceFolder:", 0) == 0) {
      if (k.substr(16) == c.workspace.filename().u8string())
        return c.workspace.u8string();
      throw std::runtime_error("Workspace folder not open: " + k.substr(16));
    }
    if (k == "workspaceFolderBasename")
      return c.workspace.filename().u8string();
    if (k == "pathSeparator" || k == "/")
      return std::string(1, static_cast<char>(fs::path::preferred_separator));
    if (k == "lineNumber") return std::to_string(c.line);
    if (k == "columnNumber") return std::to_string(c.column);
    if (k == "selectedText") return c.selectedText;
    if (k == "execPath") {
      if (c.executable.empty())
        throw std::runtime_error("Editor executable path unavailable");
      return c.executable.u8string();
    }
    if (k == "defaultBuildTask") {
      auto i = std::find_if(tasks.begin(), tasks.end(),
                            [](const Task& t) { return t.defaultBuild; });
      if (i == tasks.end())
        throw std::runtime_error("No default build task configured");
      return i->label;
    }
    if (k.rfind("env:", 0) == 0) {
      if (auto v = std::getenv(k.substr(4).c_str())) return v;
      return {};
    }
    if (k == "userHome") {
#ifdef _WIN32
      if (auto v = std::getenv("USERPROFILE")) return v;
#else
      if (auto v = std::getenv("HOME")) return v;
#endif
      throw std::runtime_error("Home directory unavailable");
    }
    if (k.rfind("config:", 0) == 0) {
      auto id = k.substr(7);
      if (auto v = lookup(workspaceSettings(), id)) return scalar(*v, id);
      if (c.config) {
        if (auto v = c.config(id)) return *v;
      }
      throw std::runtime_error("Task setting not found: " + id);
    }
    if (k.rfind("command:", 0) == 0) return command(k.substr(8));
    if (k.rfind("input:", 0) == 0) {
      auto id = k.substr(6);
      if (!task.inputs || !task.inputs->count(id))
        throw std::runtime_error("Unknown task input: " + id);
      auto& i = task.inputs->at(id);
      if (i.type == "command") return command(i.command);
      if (i.type != "promptString" && i.type != "pickString")
        throw std::runtime_error("Unsupported task input type: " + i.type);
      if (!c.prompt)
        throw std::runtime_error(
            "Interactive task input requires a GUI task run: " + id);
      auto value = c.prompt(i);
      if (!value) throw TaskCancelled{};
      if (i.type == "pickString" &&
          std::none_of(i.options.begin(), i.options.end(),
                       [&](const auto& o) { return o.value == *value; }))
        throw std::runtime_error("Invalid input option: " + id);
      return *value;
    }
    if (k == "file" || k == "fileDirname" || k == "fileDirnameBasename" ||
        k == "fileWorkspaceFolder" || k == "fileBasename" ||
        k == "fileBasenameNoExtension" || k == "fileExtname" ||
        k == "relativeFile" || k == "relativeFileDirname") {
      if (c.file.empty())
        throw std::runtime_error("Open a saved file for ${" + k + "}");
      if (k == "file") return c.file.u8string();
      if (k == "fileDirname") return c.file.parent_path().u8string();
      if (k == "fileDirnameBasename")
        return c.file.parent_path().filename().u8string();
      if (k == "fileBasename") return c.file.filename().u8string();
      if (k == "fileBasenameNoExtension") return c.file.stem().u8string();
      if (k == "fileExtname") return c.file.extension().u8string();
      auto p = c.file.lexically_relative(c.workspace);
      if (k == "fileWorkspaceFolder") {
        if (p.empty() || *p.begin() == "..")
          throw std::runtime_error("Active file is outside workspace");
        return c.workspace.u8string();
      }
      if (k == "relativeFileDirname") p = p.parent_path();
      return p.empty() ? "." : p.u8string();
    }
    throw std::runtime_error("Unsupported VSCode task variable: ${" + k + "}");
  }

 public:
  Resolver(const TaskContext& ctx, const std::vector<Task>& t)
      : c(ctx), tasks(t) {}
  std::string expand(const std::string& in, const Task& task,
                     bool shellRaw = false) {
    std::string out;
    size_t pos = 0;
    char quote = 0;
    bool escaped = false;
    auto literal = [&](const std::string& text) {
      out += text;
      if (!shellRaw) return;
      for (char ch : text) {
        if (escaped) {
          escaped = false;
          continue;
        }
        if (ch == '\\' && quote != '\'') {
          escaped = true;
          continue;
        }
        if (quote) {
          if (ch == quote) quote = 0;
        } else if (ch == '\'' || ch == '"')
          quote = ch;
      }
    };
    while (pos < in.size()) {
      auto a = in.find("${", pos);
      if (a == std::string::npos) {
        literal(in.substr(pos));
        break;
      }
      literal(in.substr(pos, a - pos));
      auto b = in.find('}', a + 2);
      if (b == std::string::npos)
        throw std::runtime_error("Unclosed task variable");
      auto k = in.substr(a + 2, b - a - 2);
      auto it = cache.find(k);
      if (it == cache.end()) it = cache.emplace(k, resolve(k, task)).first;
      const auto& v = it->second;
      if (!shellRaw)
        out += v;
      else {
#ifdef _WIN32
        throw std::runtime_error(
            "Use args or a process task for variables in "
            "Windows shell commands");
#else
        if (escaped)
          throw std::runtime_error(
              "Escaped task variable in shell command; use args instead");
        if (!quote)
          out += quoteArgument(v, false);
        else
          for (char ch : v) {
            if (quote == '\'' && ch == '\'')
              out += "'\\''";
            else {
              if (quote == '"' &&
                  (ch == '"' || ch == '\\' || ch == '$' || ch == '`'))
                out += '\\';
              out += ch;
            }
          }
#endif
      }
      pos = b + 1;
    }
    if (out.find('\0') != std::string::npos)
      throw std::runtime_error("NUL in task argument");
    return out;
  }
};
std::vector<std::string> strings(const Json& j) {
  std::vector<std::string> v;
  if (j.isString())
    v.push_back(j.string());
  else if (j.isArray())
    for (auto& a : j.asArray()) {
      if (!a.isString())
        throw std::runtime_error("Expected string task argument/dependency");
      v.push_back(a.string());
    }
  return v;
}
}  // namespace
std::shared_ptr<const TaskInputs> parseTaskInputs(const Json& root) {
  return parseInputs(root);
}
std::vector<Task> loadTasks(const fs::path& path, std::string* error) {
  if (error) error->clear();
  std::vector<Task> result;
  std::error_code ec;
  if (!fs::exists(path, ec)) return result;
  try {
    auto root = readObject(path);
    auto inputs = parseInputs(root);
    if (!root["tasks"].isArray())
      throw std::runtime_error("tasks.json requires a tasks array");
    Json defaults = Json::object();
    for (auto k : {"type", "command", "args", "options", "group",
                   "problemMatcher", "presentation"})
      if (auto v = root.find(k)) defaults[k] = *v;
    if (root[platform()].isObject()) defaults.merge(root[platform()]);
    std::set<std::string> labels;
    for (const auto& entry : root["tasks"].asArray()) {
      if (!entry.isObject()) throw std::runtime_error("Invalid task entry");
      Json e = defaults;
      e.merge(entry);
      if (entry[platform()].isObject()) e.merge(entry[platform()]);
      const Json& data = e;
      Task t;
      t.inputs = inputs;
      t.label = data["label"].string();
      if (t.label.empty()) throw std::runtime_error("Task missing label");
      if (!labels.insert(t.label).second)
        throw std::runtime_error("Duplicate task: " + t.label);
      t.type = data["type"].string("shell");
      t.command = data["command"].string();
      if (t.type == "cppbuild" && !t.command.empty()) t.type = "process";
      t.dependsOn = strings(data["dependsOn"]);
      t.sequential = data["dependsOrder"].string() == "sequence";
      t.background = data["isBackground"].boolean();
      if (t.type != "shell" && t.type != "process")
        t.unsupported = "Unsupported task type: " + t.type;

      if (data.find("command") && !data["command"].isString())
        t.unsupported = "Object-form command is not supported";
      if (t.command.empty() && t.dependsOn.empty() && t.unsupported.empty())
        t.unsupported = "Task has no command";
      const auto& group = data["group"];
      t.group = group.isObject() ? group["kind"].string() : group.string();
      t.defaultBuild = t.group == "build" && group["isDefault"].boolean();
      const auto& options = data["options"];
      t.cwd = options["cwd"].string();
      t.shell = options["shell"]["executable"].string();
      t.shellArgs = strings(options["shell"]["args"]);
      for (auto& kv : options["env"].asObject()) {
        if (!kv.second.isString())
          throw std::runtime_error("Task environment values must be strings");
        t.env.emplace(kv.first, kv.second.string());
      }
      if (data["args"].isArray()) {
        for (auto& v : data["args"].asArray()) {
          if (v.isString())
            t.arguments.push_back(v.string());
          else {
            t.unsupported = "Object-form shell quoting arguments not supported";
          }
        }
      } else if (data.find("args"))
        t.unsupported = "Task args must be an array";
      result.push_back(std::move(t));
    }
  } catch (const std::exception& e) {
    if (error) *error = e.what();
    return {};
  }
  return result;
}
std::vector<TaskLaunch> planTasks(const std::vector<Task>& tasks,
                                  const std::string& label,
                                  const TaskContext& ctx) {
  Resolver resolver(ctx, tasks);
  std::vector<TaskLaunch> result;
  std::set<std::string> visiting, done;
  std::function<void(const std::string&)> visit = [&](const std::string& name) {
    if (done.count(name)) return;
    if (!visiting.insert(name).second)
      throw std::runtime_error("Cyclic task dependency: " + name);
    auto it = std::find_if(tasks.begin(), tasks.end(),
                           [&](const auto& t) { return t.label == name; });
    if (it == tasks.end()) throw std::runtime_error("Unknown task: " + name);
    const auto& t = *it;
    if (!t.unsupported.empty())
      throw std::runtime_error(t.label + ": " + t.unsupported);
    if (t.background)
      throw std::runtime_error(
          "Background task readiness/problem matchers not supported: " + name);
    if (t.dependsOn.size() > 1 && !t.sequential)
      throw std::runtime_error(
          "Parallel dependencies not supported; use dependsOrder: sequence");
    for (auto& d : t.dependsOn) visit(d);
    if (!t.command.empty()) {
      TaskLaunch l;
      l.label = t.label;
      l.cwd =
          t.cwd.empty() ? ctx.workspace : fs::u8path(resolver.expand(t.cwd, t));
      if (l.cwd.is_relative()) l.cwd = ctx.workspace / l.cwd;
      if (!fs::is_directory(l.cwd))
        throw std::runtime_error("Task working directory does not exist: " +
                                 l.cwd.u8string());
      auto cmd = resolver.expand(t.command, t,
                                 t.type == "shell" && t.arguments.empty());
      std::vector<std::string> args;
      for (auto& a : t.arguments) args.push_back(resolver.expand(a, t));
      if (t.type == "process") {
        l.argv = {cmd};
        l.argv.insert(l.argv.end(), args.begin(), args.end());
      } else {
#ifdef _WIN32
        const bool win = true;
        std::string shell =
            t.shell.empty() ? "cmd.exe" : resolver.expand(t.shell, t);
        l.argv = {shell};
        if (t.shellArgs.empty()) {
          if (!t.shell.empty() && fs::u8path(shell).stem().string() != "cmd")
            throw std::runtime_error(
                "Specify options.shell.args for custom Windows shells");
          l.argv.insert(l.argv.end(), {"/D", "/C"});
        }
#else
        const bool win = false;
        std::string shell =
            t.shell.empty() ? "/bin/sh" : resolver.expand(t.shell, t);
        l.argv = {shell};
        if (t.shellArgs.empty()) l.argv.push_back("-c");
#endif
        for (auto& a : t.shellArgs) l.argv.push_back(resolver.expand(a, t));
        if (!args.empty()) cmd = quoteArgument(cmd, win);
        for (auto& a : args) cmd += " " + quoteArgument(a, win);
        l.argv.push_back(cmd);
      }
      if (!t.env.empty()) {
#ifdef _WIN32
        throw std::runtime_error(
            "Task options.env is not supported on Windows yet");
#else
        std::vector<std::string> argv = {"env", "--"};
        for (auto& kv : t.env) {
          if (kv.first.empty() || kv.first.find('=') != std::string::npos ||
              kv.first.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid environment name");
          argv.push_back(kv.first + "=" + resolver.expand(kv.second, t));
        }
        argv.insert(argv.end(), l.argv.begin(), l.argv.end());
        l.argv = std::move(argv);
#endif
      }
      result.push_back(std::move(l));
    }
    visiting.erase(name);
    done.insert(name);
  };
  visit(label);
  if (result.empty()) throw std::runtime_error("Task plan is empty");
  return result;
}
std::string quoteArgument(const std::string& value, bool windows) {
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
std::string commandLine(const Task& task, bool windows) {
  std::string result = task.command;
  for (const auto& arg : task.arguments)
    result += " " + quoteArgument(arg, windows);
  if (!task.cwd.empty()) {
    const std::string cd = windows ? "cd /d " : "cd ";
    result = cd + quoteArgument(task.cwd, windows) +
             (windows ? " && " : " && ") + result;
  }
  return result;
}
std::string launchFileCommand(const fs::path& file, bool windows) {
  const auto path = file.u8string(), ext = file.extension().u8string();
  const auto q = quoteArgument(path, windows);
  if (ext == ".py") return (windows ? "python " : "python3 ") + q;
  if (ext == ".js" || ext == ".mjs") return "node " + q;
  if (ext == ".sh") return "sh " + q;
  if (ext == ".ps1") return "pwsh -File " + q;
  if (ext == ".rb") return "ruby " + q;
  if (ext == ".pl") return "perl " + q;
  if (ext == ".bat" || ext == ".cmd" || ext == ".exe") return q;
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
}  // namespace dodev::terminal
