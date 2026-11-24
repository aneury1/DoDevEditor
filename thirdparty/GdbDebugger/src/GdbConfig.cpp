#include "GdbConfig.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "GdbBreakpoints.h"
#include "dodev/terminal/TerminalTasks.h"
namespace dodev::gdb {
namespace {
[[noreturn]] void fail(const std::string& s) { throw std::runtime_error(s); }
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
        fail("Unterminated JSONC comment");
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
std::string safe(std::string s) {
  if (s.find('\0') != std::string::npos ||
      s.find_first_of("\r\n") != std::string::npos)
    fail("NUL/newline not supported in debugger arguments");
  return s;
}
std::string expand(const std::string& input, const Context& c) {
  std::string out;
  size_t pos = 0;
  while (pos < input.size()) {
    auto a = input.find("${", pos);
    if (a == std::string::npos) {
      out += input.substr(pos);
      break;
    }
    out += input.substr(pos, a - pos);
    auto b = input.find('}', a + 2);
    if (b == std::string::npos) fail("Unclosed variable: " + input);
    auto key = input.substr(a + 2, b - a - 2);
    if (key == "workspaceFolder" || key == "workspaceRoot" || key == "cwd")
      out += c.workspace.u8string();
    else if (key == "workspaceFolderBasename")
      out += c.workspace.filename().u8string();
    else if (key == "pathSeparator" || key == "/")
      out += static_cast<char>(fs::path::preferred_separator);
    else if (key.rfind("env:", 0) == 0) {
      const char* value = std::getenv(key.substr(4).c_str());
      if (value) out += value;
    } else if (key == "file" || key == "fileDirname" || key == "fileBasename" ||
               key == "fileBasenameNoExtension" || key == "fileExtname" ||
               key == "relativeFile" || key == "relativeFileDirname") {
      if (c.file.empty()) fail("Open a saved source file for ${" + key + "}");
      if (key == "file")
        out += c.file.u8string();
      else if (key == "fileDirname")
        out += c.file.parent_path().u8string();
      else if (key == "fileBasename")
        out += c.file.filename().u8string();
      else if (key == "fileBasenameNoExtension")
        out += c.file.stem().u8string();
      else if (key == "fileExtname")
        out += c.file.extension().u8string();
      else {
        auto p = c.file.lexically_relative(c.workspace);
        if (key == "relativeFileDirname") p = p.parent_path();
        out += p.u8string();
      }
    } else
      fail("Unsupported VSCode variable: ${" + key + "}");
    pos = b + 1;
  }
  return safe(out);
}
std::string str(const Json& j, const char* key,
                const std::string& fallback = "") {
  auto p = j.find(key);
  if (!p) return fallback;
  if (!p->isString()) fail(std::string(key) + " must be a string");
  return p->string();
}
bool boolean(const Json& j, const char* key, bool fallback = false) {
  auto p = j.find(key);
  if (!p) return fallback;
  if (!p->isBool()) fail(std::string(key) + " must be a boolean");
  return p->boolean();
}
fs::path path(const std::string& s, const fs::path& root) {
  auto p = fs::u8path(s);
  return (p.is_absolute() ? p : root / p).lexically_normal();
}
std::vector<std::string> strings(const Json& j, const char* key,
                                 const Context& ctx) {
  std::vector<std::string> out;
  auto a = j.find(key);
  if (!a) return out;
  if (!a->isArray()) fail(std::string(key) + " must be an array");
  for (auto& v : a->asArray()) {
    if (!v.isString()) fail(std::string(key) + " entries must be strings");
    out.push_back(expand(v.string(), ctx));
  }
  return out;
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
Json withPlatform(Json j) {
  if (auto p = j.find(platform())) {
    if (!p->isObject()) fail("Platform override must be an object");
    Json copy = *p;
    j.merge(copy);
  }
  return j;
}
Json configs(const Context& ctx) {
  auto root = readJsonc(ctx.workspace / ".vscode" / "launch.json");
  if (!root["configurations"].isArray())
    fail("launch.json needs configurations[]");
  return root["configurations"];
}
std::string py(const std::string& s) {
  return Json(s).dump(0);
}  // JSON string escapes are valid Python string literal escapes here.
std::string pyList(const std::vector<std::string>& v) {
  std::string s = "[";
  for (const auto& a : v) {
    if (s.size() > 1) s += ", ";
    s += py(a);
  }
  return s + "]";
}
std::string shellQuote(const std::string& v) {
#ifdef _WIN32
  // Shell tasks are intentionally restricted on Windows; process tasks are
  // portable.
  (void)v;
  fail("Windows shell task arguments are not supported; use type: process");
#else
  std::string s = "'";
  for (char c : v) {
    if (c == '\'')
      s += "'\\''";
    else
      s += c;
  }
  return s + "'";
#endif
}
void addTask(std::string& code, const Context& ctx, const std::string& label) {
  namespace tt = dodev::terminal;
  std::string error;
  auto tasks = tt::loadTasks(ctx.workspace / ".vscode" / "tasks.json", &error);
  if (!error.empty()) fail(error);
  auto name = label;
  if (name == "${defaultBuildTask}") {
    auto it = std::find_if(tasks.begin(), tasks.end(),
                           [](const auto& t) { return t.defaultBuild; });
    if (it == tasks.end()) fail("No default build task");
    name = it->label;
  }
  tt::TaskContext tc;
  tc.workspace = ctx.workspace;
  tc.file = ctx.file;
  tc.line = ctx.line;
  auto plan = tt::planTasks(tasks, name, tc);
  code += "import subprocess, os\n";
  for (const auto& step : plan) {
    code += "print(" + py("[DoDev GDB] preLaunchTask: " + step.label) +
            ", flush=True)\n";
    code += "subprocess.run(" + pyList(step.argv) +
            ", cwd=" + py(step.cwd.u8string()) + ", check=True)\n";
  }
}
void exec(std::string& code, const std::string& command) {
  code += "gdb.execute(" + py(command) + ")\n";
}
}  // namespace
Json readJsonc(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) fail("Cannot open " + p.u8string());
  std::string raw;
  char buf[8192];
  while (in.read(buf, sizeof(buf)) || in.gcount()) {
    raw.append(buf, static_cast<size_t>(in.gcount()));
    if (raw.size() > 1024 * 1024) fail("Configuration exceeds 1 MiB");
  }
  if (raw.rfind("\xef\xbb\xbf", 0) == 0) raw.erase(0, 3);
  Json j;
  std::string err;
  if (!Json::parse(stripComments(raw), j, &err) || !j.isObject())
    fail(p.u8string() + ": " + err);
  return j;
}
std::string quoteGdb(const std::string& s) { return Json(safe(s)).dump(0); }
std::vector<std::string> names(const Context& ctx) {
  std::vector<std::string> result;
  auto list = configs(ctx);
  for (auto& raw : list.asArray()) {
    auto j = withPlatform(raw);
    if (str(j, "type") == "cppdbg" && str(j, "MIMode", "gdb") == "gdb") {
      auto n = str(j, "name");
      if (n.empty()) fail("GDB configuration has no name");
      result.push_back(n);
    }
  }
  return result;
}
Launch makeLaunch(const Context& ctx, const std::string& name,
                  const std::string& defaultGdb, const std::string& pid,
                  bool run) {
  Json j;
  bool found = false;
  auto list = configs(ctx);
  for (auto& raw : list.asArray()) {
    auto v = withPlatform(raw);
    if (str(v, "type") == "cppdbg" && str(v, "MIMode", "gdb") == "gdb" &&
        (name.empty() || str(v, "name") == name)) {
      if (found && !name.empty())
        fail("Duplicate GDB configuration name: " + name);
      j = v;
      found = true;
      if (name.empty()) break;
    }
  }
  if (!found) fail("No matching cppdbg/GDB configuration");
  Launch l;
  l.name = str(j, "name");
  auto request = str(j, "request");
  if (request != "launch" && request != "attach")
    fail("request must be launch or attach");
  if (boolean(j, "externalConsole"))
    fail(
        "externalConsole=true is not supported; use the integrated debug "
        "terminal");
  for (auto key :
       {"pipeTransport", "debugServerPath", "customLaunchSetupCommands",
        "launchCompleteCommand", "postDebugTask", "visualizerFile",
        "coreDumpPath", "logging"})
    if (j.find(key)) fail(std::string("Unsupported debugger setting: ") + key);
  l.cwd = path(expand(str(j, "cwd", "${workspaceFolder}"), ctx), ctx.workspace);
  if (!fs::is_directory(l.cwd))
    fail("Debugger cwd does not exist: " + l.cwd.u8string());
  auto debugger = expand(
      str(j, "miDebuggerPath", defaultGdb.empty() ? "gdb" : defaultGdb), ctx);
  if (debugger.empty()) fail("Empty GDB path");
  if (debugger.find('/') != std::string::npos ||
      debugger.find('\\') != std::string::npos)
    debugger = path(debugger, ctx.workspace).u8string();
  l.argv = {debugger, "--quiet", "--nx"};
  auto debuggerArgs = str(j, "miDebuggerArgs");
  if (!debuggerArgs.empty())
    fail("miDebuggerArgs is not supported (interactive CLI, not MI adapter)");
  auto program = expand(str(j, "program"), ctx);
  if (request == "launch" && program.empty()) fail("program is required");
  if (!program.empty()) program = path(program, ctx.workspace).u8string();
  std::string attachPid = pid;
  if (request == "attach" && attachPid.empty()) {
    const auto* p = j.find("processId");
    if (p && p->isNumber()) {
      auto n = p->integer();
      if (n > 0 && p->number() == n) attachPid = std::to_string(n);
    } else if (p && p->isString())
      attachPid = p->string();
    if (attachPid.empty() || attachPid == "${command:pickProcess}") {
      l.needsPid = true;
      return l;
    }
  }
  if (request == "attach" &&
      (attachPid.empty() || attachPid.size() > 10 ||
       attachPid.find_first_not_of("0123456789") != std::string::npos ||
       attachPid.find_first_not_of('0') == std::string::npos))
    fail("Invalid process ID");
  auto server = expand(str(j, "miDebuggerServerAddress"), ctx);
  if (!server.empty() && request == "attach")
    fail("Remote attach is not supported; use remote launch");
  auto args = strings(j, "args", ctx);
  auto task = str(j, "preLaunchTask");
  l.restartable = request == "launch" && server.empty();
  // Keep GDB as the actual PTY process. Python inside GDB sequences setup
  // atomically: a failed build or required setup command prevents automatic
  // launch.
  std::string code = "import gdb\n";
  if (!task.empty()) addTask(code, ctx, task);
  if (!program.empty()) exec(code, "file " + quoteGdb(program));
  if (!args.empty()) {
    std::string cmd = "set args";
    for (auto& a : args) {
#ifdef _WIN32
      cmd += " " + quoteGdb(a);
#else
      cmd += " " + shellQuote(a);
#endif
    }
    exec(code, cmd);
  }
  if (auto env = j.find("environment")) {
    if (!env->isArray()) fail("environment must be an array");
    for (auto& e : env->asArray()) {
      auto key = str(e, "name"), value = expand(str(e, "value"), ctx);
      if (key.empty() ||
          !(std::isalpha(static_cast<unsigned char>(key[0])) ||
            key[0] == '_') ||
          key.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrst"
                                "uvwxyz0123456789_") != std::string::npos)
        fail("Invalid environment name: " + key);
      exec(code, "set environment " + key + "=" + value);
    }
  }
  if (auto mapping = j.find("sourceFileMap")) {
    if (!mapping->isObject()) fail("sourceFileMap must be an object");
    for (auto& pair : mapping->asObject()) {
      if (!pair.second.isString()) fail("sourceFileMap values must be strings");
      exec(code, "set substitute-path " + quoteGdb(expand(pair.first, ctx)) +
                     " " + quoteGdb(expand(pair.second.string(), ctx)));
    }
  }
  if (auto setup = j.find("setupCommands")) {
    if (!setup->isArray()) fail("setupCommands must be an array");
    for (auto& e : setup->asArray()) {
      auto cmd = expand(str(e, "text"), ctx);
      if (cmd.empty()) fail("Empty setup command");
      if (cmd[0] == '-') cmd = "interpreter-exec mi " + quoteGdb(cmd);
      if (boolean(e, "ignoreFailures")) {
        code += "try:\n    gdb.execute(" + py(cmd) +
                ")\nexcept gdb.error as e:\n    print('[DoDev GDB] ignored "
                "setup error:', e)\n";
      } else
        exec(code, cmd);
    }
  }
  exec(code, "set breakpoint pending on");
  for (const auto& b : loadBreakpoints(ctx)) {
    auto command = "break " + breakpointLocation(fs::u8path(b.file), b.line);
    code += "try:\n    gdb.execute(" + py(command) +
            ")\nexcept gdb.error as e:\n    print('[DoDev GDB] breakpoint "
            "error:', e)\n";
  }
  if (!server.empty()) {
    if (server.find_first_of(" \t") != std::string::npos)
      fail("Invalid debugger server address");
    exec(code, "target remote " + server);
    if (run && !boolean(j, "stopAtConnect") && !boolean(j, "stopAtEntry"))
      exec(code, "continue");
  } else if (request == "attach")
    exec(code, "attach " + attachPid);
  else if (run)
    exec(code, boolean(j, "stopAtEntry") ? "start" : "run");
  l.argv.insert(l.argv.end(),
                {"-ex", "set pagination off", "-ex", "set confirm off", "-ex",
                 "set style enabled off", "-ex", "set prompt (dodev-gdb) ",
                 "-ex", "python exec(" + py(code) + ")"});
  return l;
}
std::string defaultLaunchJson() {
  return R"({
    "version": "0.2.0",
    "configurations": [
        {
            "name": "C++: GDB Launch",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/app",
            "args": [],
            "stopAtEntry": true,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "miDebuggerPath": "gdb",
            "setupCommands": [
                {"description": "Enable pretty-printing", "text": "-enable-pretty-printing", "ignoreFailures": true}
            ]
        },
        {
            "name": "C++: GDB Attach",
            "type": "cppdbg",
            "request": "attach",
            "program": "${workspaceFolder}/build/app",
            "processId": "${command:pickProcess}",
            "cwd": "${workspaceFolder}",
            "MIMode": "gdb",
            "miDebuggerPath": "gdb"
        }
    ]
})";
}
}  // namespace dodev::gdb
