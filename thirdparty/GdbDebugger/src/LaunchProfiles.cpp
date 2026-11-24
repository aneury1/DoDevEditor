#include "LaunchProfiles.h"

#include <algorithm>
#include <stdexcept>
namespace dodev::gdb {
namespace {
const char* platform() {
#ifdef _WIN32
  return "windows";
#elif defined(__APPLE__)
  return "osx";
#else
  return "linux";
#endif
}
std::string text(const Json& j, const char* key, const std::string& def = {}) {
  auto v = j.find(key);
  if (!v) return def;
  if (!v->isString())
    throw std::runtime_error(std::string(key) + " must be a string");
  return v->string();
}
Json select(const Json& doc, const std::string& name) {
  if (!doc["configurations"].isArray())
    throw std::runtime_error("launch.json needs configurations[]");
  Json result;
  bool found = false;
  for (auto raw : doc["configurations"].asArray()) {
    if (!raw.isObject()) throw std::runtime_error("Invalid launch profile");
    if (auto p = raw.find(platform())) {
      if (!p->isObject()) throw std::runtime_error("Invalid platform override");
      Json copy = *p;
      raw.merge(copy);
    }
    if (text(raw, "name") != name) continue;
    if (found) throw std::runtime_error("Duplicate launch profile: " + name);
    result = raw;
    found = true;
  }
  if (!found)
    throw std::runtime_error("Launch configuration no longer exists: " + name);
  return result;
}
}  // namespace
Json appendLaunchProfile(Json doc, const std::string& name,
                         const std::string& program) {
  if (name.empty() || program.empty())
    throw std::runtime_error("Profile name and program are required");
  if (!doc.isObject())
    throw std::runtime_error("launch.json must be an object");
  if (!doc.find("version")) doc["version"] = "0.2.0";
  if (!doc.find("configurations")) doc["configurations"] = Json::array();
  if (!doc["configurations"].isArray())
    throw std::runtime_error("configurations must be an array");
  auto configs = doc["configurations"].asArray();
  for (const auto& j : configs)
    if (text(j, "name") == name)
      throw std::runtime_error("Duplicate launch profile: " + name);
  Json p = Json::object();
  p["name"] = name;
  p["type"] = "cppdbg";
  p["request"] = "launch";
  p["program"] = program;
  p["args"] = Json::array();
  p["cwd"] = "${workspaceFolder}";
  p["environment"] = Json::array();
  p["externalConsole"] = false;
  p["stopAtEntry"] = false;
  p["MIMode"] = "gdb";
  p["miDebuggerPath"] = "gdb";
  configs.push_back(std::move(p));
  doc["configurations"] = Json(std::move(configs));
  return doc;
}
std::vector<terminal::TaskLaunch> planWithoutDebug(
    const terminal::TaskContext& ctx, const std::string& name) {
  namespace tt = dodev::terminal;
  if (ctx.workspace.empty()) throw std::runtime_error("Open a workspace first");
  const auto doc = readJsonc(ctx.workspace / ".vscode" / "launch.json");
  const auto j = select(doc, name);
  if (text(j, "type") != "cppdbg" || text(j, "MIMode", "gdb") != "gdb" ||
      text(j, "request") != "launch")
    throw std::runtime_error(
        "Run Without Debugging requires a local cppdbg/GDB launch profile (not "
        "attach)");
  if (!text(j, "miDebuggerServerAddress").empty() ||
      j["externalConsole"].boolean() || j.find("pipeTransport") ||
      j.find("debugServerPath") || j.find("coreDumpPath") ||
      j.find("postDebugTask"))
    throw std::runtime_error(
        "Remote launch, external consoles, core dumps and postDebugTask are "
        "not supported by Run Without Debugging");
  tt::Task t;
  t.label = "Launch: " + name;
  t.type = "process";
  t.command = text(j, "program");
  t.cwd = text(j, "cwd", "${workspaceFolder}");
  t.inputs = tt::parseTaskInputs(doc);
  if (t.command.empty()) throw std::runtime_error("Launch program is required");
  if (auto a = j.find("args")) {
    if (!a->isArray()) throw std::runtime_error("args must be an array");
    for (const auto& v : a->asArray()) {
      if (!v.isString())
        throw std::runtime_error("args entries must be strings");
      t.arguments.push_back(v.string());
    }
  }
  if (auto env = j.find("environment")) {
    if (!env->isArray())
      throw std::runtime_error("environment must be an array");
    for (const auto& e : env->asArray()) {
      auto key = text(e, "name");
      if (key.empty() || !t.env.emplace(key, text(e, "value")).second)
        throw std::runtime_error("Invalid or duplicate environment name");
    }
  }
  std::string err;
  auto tasks = tt::loadTasks(ctx.workspace / ".vscode" / "tasks.json", &err);
  auto pre = text(j, "preLaunchTask");
  if (!pre.empty() && !err.empty()) throw std::runtime_error(err);
  if (pre == "${defaultBuildTask}") {
    auto i = std::find_if(tasks.begin(), tasks.end(),
                          [](const auto& q) { return q.defaultBuild; });
    if (i == tasks.end()) throw std::runtime_error("No default build task");
    pre = i->label;
  }
  // Resolve launch variables using the same resolver as tasks, but keep the two
  // input namespaces separate. Existing tasks are visible for
  // ${defaultBuildTask}; use a unique internal label.
  auto label = t.label;
  while (std::any_of(tasks.begin(), tasks.end(),
                     [&](const auto& q) { return q.label == t.label; }))
    t.label += "#";
  auto combined = tasks;
  combined.push_back(t);
  auto target = tt::planTasks(combined, t.label, ctx).front();
  target.label = label;
  size_t programIndex = t.env.empty() ? 0 : 2 + t.env.size();
  auto program = fs::u8path(target.argv.at(programIndex));
  if (program.empty()) throw std::runtime_error("Empty launch executable");
  if (program.is_relative()) program = ctx.workspace / program;
  target.requiredExecutable = program.lexically_normal();
  target.argv[programIndex] = target.requiredExecutable.u8string();
  std::vector<tt::TaskLaunch> plan;
  if (!pre.empty()) plan = tt::planTasks(tasks, pre, ctx);
  if (plan.empty() && !fs::is_regular_file(target.requiredExecutable))
    throw std::runtime_error("Launch executable not found: " +
                             target.requiredExecutable.u8string());
  plan.push_back(std::move(target));
  return plan;
}
}  // namespace dodev::gdb
