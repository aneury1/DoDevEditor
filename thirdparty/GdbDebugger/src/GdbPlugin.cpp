#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "GdbBreakpoints.h"
#include "GdbConfig.h"
#include "dodev/sdk/dodev_plugin_api.h"
namespace {
using namespace dodev::gdb;
std::string setting(const DodevHostAPI* h, const char* key,
                    const char* fallback = "") {
  auto v = h->get_setting(h->context, key);
  return v && *v ? v : fallback;
}
void status(const DodevHostAPI* h, const std::string& msg) {
  if (DODEV_HOST_HAS(h, debug_status))
    h->debug_status(h->context, msg.c_str());
  else if (h->log)
    h->log(h->context, 1, msg.c_str());
}
Context context(const DodevHostAPI* h, int* outLine = nullptr) {
  const char* w = nullptr;
  const char* f = nullptr;
  int line = 0;
  if (!h->debug_context(h->context, &w, &f, &line) || !w || !*w)
    throw std::runtime_error("Open a workspace folder first");
  if (outLine) *outLine = line;
  return {fs::u8path(w), fs::u8path(f ? f : ""), line > 0 ? line : 1};
}
template <class F>
void guard(const DodevHostAPI* h, F fn) {
  try {
    fn();
  } catch (const std::exception& e) {
    status(h, std::string("GDB error: ") + e.what());
  }
}
struct Pending {
  Context ctx;
  std::string name;
  bool run;
};
std::optional<Pending> pending;
bool restartable = false;
fs::path activeWorkspace;
int control(const DodevHostAPI* h, const std::string& cmd) {
  return h->debug_control(h->context, cmd.c_str());
}
void send(const DodevHostAPI* h, const std::string& cmd) {
  int result = control(h, cmd);
  if (result == 0)
    status(h, "GDB: no active debug session");
  else if (result < 0)
    status(h,
           "GDB: busy; pause or finish console input before issuing commands");
}
void launch(const DodevHostAPI* h, const Context& ctx, const std::string& name,
            bool run, const std::string& pid = "");
void onPid(const DodevHostAPI* h, const char* input, void*) {
  guard(h, [&] {
    if (!pending) return;
    auto request = *pending;
    pending.reset();
    if (!input || !*input) return;
    launch(h, request.ctx, request.name, request.run, input);
  });
}
void launch(const DodevHostAPI* h, const Context& ctx, const std::string& name,
            bool run, const std::string& pid) {
  if (control(h, "@state") != 0)
    throw std::runtime_error("Stop the current debug session first");
  auto cfg =
      makeLaunch(ctx, name, setting(h, "miDebuggerPath", "gdb"), pid, run);
  if (cfg.needsPid) {
    pending = Pending{ctx, cfg.name, run};
    if (!h->prompt_input(
            h->context, "GDB: Attach to process",
            "Enter the PID of your process (e.g. from ps or Task Manager)", "",
            onPid, nullptr)) {
      pending.reset();
      throw std::runtime_error("Cannot open PID prompt");
    }
    return;
  }
  std::vector<const char*> argv;
  for (const auto& arg : cfg.argv) argv.push_back(arg.c_str());
  auto cwd = cfg.cwd.u8string();
  if (!h->debug_launch(h->context, cwd.c_str(), argv.data(), argv.size()))
    throw std::runtime_error("Cannot start debug terminal");
  restartable = cfg.restartable;
  activeWorkspace = ctx.workspace;
  status(h, "GDB: " + cfg.name + " (debug terminal)");
}
std::string selected(const DodevHostAPI* h, const Context& ctx) {
  auto key = setting(h, "configuration");
  auto options = names(ctx);
  if (options.empty())
    throw std::runtime_error("No cppdbg/GDB profile in .vscode/launch.json");
  for (auto& n : options)
    if (n == key) return n;
  return options.front();
}
void configure(const DodevHostAPI* h, void*) {
  guard(h, [&] {
    auto c = context(h);
    auto file = c.workspace / ".vscode" / "launch.json";
    if (!fs::exists(file)) {
      fs::create_directories(file.parent_path());
      std::ofstream out(file, std::ios::binary);
      out << defaultLaunchJson() << '\n';
      out.close();
      if (!out) throw std::runtime_error("Cannot create launch.json");
    }
    if (!h->debug_open_file(h->context, file.u8string().c_str()))
      throw std::runtime_error("Cannot open launch.json");
    status(h,
           "GDB: edit program, cwd and debugger path; save before launching");
  });
}
void selectDone(const DodevHostAPI* h, const char* answer, void*) {
  guard(h, [&] {
    if (!answer || !*answer) return;
    auto options = names(context(h));
    for (auto& n : options)
      if (n == answer) {
        if (!h->set_setting(h->context, "configuration", answer))
          throw std::runtime_error("Could not save configuration selection");
        status(h, "GDB configuration: " + n);
        return;
      }
    throw std::runtime_error("Unknown configuration name; use the exact name");
  });
}
void select(const DodevHostAPI* h, void*) {
  guard(h, [&] {
    auto c = context(h);
    auto options = names(c);
    if (options.empty()) throw std::runtime_error("No GDB configurations");
    std::string label = "Choose an exact launch.json configuration name:\n";
    for (auto& n : options) label += " • " + n + "\n";
    auto def = selected(h, c);
    if (!h->prompt_input(h->context, "GDB configurations", label.c_str(),
                         def.c_str(), selectDone, nullptr))
      throw std::runtime_error("Cannot show configuration picker");
  });
}
void start(const DodevHostAPI* h, void*) {
  guard(h, [&] {
    if (control(h, "@state") != 0) {
      send(h, restartable ? "python gdb.execute('continue' if "
                            "gdb.selected_inferior().pid else 'run')"
                          : "continue");
      return;
    }
    auto c = context(h);
    if (!fs::exists(c.workspace / ".vscode" / "launch.json")) {
      configure(h, nullptr);
      return;
    }
    launch(h, c, selected(h, c), true);
  });
}
void prepare(const DodevHostAPI* h, void*) {
  guard(h, [&] {
    auto c = context(h);
    launch(h, c, selected(h, c), false);
  });
}
void restart(const DodevHostAPI* h, void*) {
  guard(h, [&] {
    if (control(h, "@state") == 0) {
      start(h, nullptr);
      return;
    }
    if (!restartable)
      throw std::runtime_error(
          "Restart is available only for local launch profiles");
    send(h, "run");
  });
}
void breakpoint(const DodevHostAPI* h, void*) {
  guard(h, [&] {
    int line = 0;
    auto c = context(h, &line);
    bool add = toggleBreakpoint(c, line);
    auto location = breakpointLocation(c.file, line);
    std::string msg =
        std::string(add ? "Breakpoint added: " : "Breakpoint removed: ") +
        c.file.filename().u8string() + ":" + std::to_string(line);
    if (control(h, "@state") && activeWorkspace == c.workspace) {
      auto result =
          control(h, std::string(add ? "break " : "clear ") + location);
      if (result != 1)
        msg += " (saved; applied next launch)";
      else
        msg += " (sent to GDB)";
    } else
      msg += " (saved for next launch)";
    status(h, msg);
  });
}
void commandDone(const DodevHostAPI* h, const char* cmd, void*) {
  guard(h, [&] {
    if (cmd && *cmd) send(h, cmd);
  });
}
void prompt(const DodevHostAPI* h, void*) {
  h->prompt_input(h->context, "GDB console command",
                  "GDB CLI command (e.g. print variable, info locals)", "",
                  commandDone, nullptr);
}
void evalDone(const DodevHostAPI* h, const char* expr, void*) {
  guard(h, [&] {
    if (expr && *expr) send(h, std::string("print ") + expr);
  });
}
void evaluate(const DodevHostAPI* h, void*) {
  h->prompt_input(h->context, "GDB: Evaluate expression", "C/C++ expression",
                  "", evalDone, nullptr);
}
struct Action {
  const char* id;
  const char* title;
  DodevCommandFn cb;
  const char* cmd;
};
void simple(const DodevHostAPI* h, void* value) {
  guard(h, [&] { send(h, static_cast<const char*>(value)); });
}
void panelView(const DodevHostAPI*, const DodevUiBuilder* ui, void*) {
  ui->label(ui->context, "Run and Debug — GDB");
  ui->button(ui->context, "Open / create launch.json", "gdb.configure");
  ui->button(ui->context, "Select configuration", "gdb.select");
  ui->button(ui->context, "Start / Continue (F5)", "gdb.start");
  ui->button(ui->context, "Pause", "gdb.pause");
  ui->button(ui->context, "Stop (Shift+F5)", "gdb.stop");
  ui->button(ui->context, "Restart (Ctrl+Shift+F5)", "gdb.restart");
  ui->button(ui->context, "Step over (F10)", "gdb.next");
  ui->button(ui->context, "Step into (F11)", "gdb.step");
  ui->button(ui->context, "Step out (Shift+F11)", "gdb.finish");
  ui->button(ui->context, "Toggle source breakpoint (F9)", "gdb.breakpoint");
  ui->button(ui->context, "Breakpoints", "gdb.breakpoints");
  ui->button(ui->context, "Call stack", "gdb.stack");
  ui->button(ui->context, "Local variables", "gdb.locals");
  ui->button(ui->context, "Threads", "gdb.threads");
  ui->button(ui->context, "Evaluate expression", "gdb.evaluate");
  ui->button(ui->context, "GDB command", "gdb.command");
  ui->read_only_text(
      ui->context,
      "Results and program I/O appear in the separate GDB Debug terminal. "
      "Pause before inspecting variables. Breakpoints are saved in "
      ".vscode/dodev-breakpoints.json. Use the editor cursor for F9. This is a "
      "GDB CLI integration, not the VSCode MI adapter.");
}
}  // namespace
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI* h,
                                                     DodevPluginAPI* p) {
  if (!h || !p || p->struct_size < sizeof(DodevPluginAPI) ||
      h->api_version != DODEV_PLUGIN_API_VERSION ||
      !DODEV_HOST_HAS(h, debug_context) || !DODEV_HOST_HAS(h, debug_launch) ||
      !DODEV_HOST_HAS(h, debug_control) ||
      !DODEV_HOST_HAS(h, debug_open_file) || !DODEV_HOST_HAS(h, prompt_input))
    return 0;
  p->api_version = DODEV_PLUGIN_API_VERSION;
  p->shutdown = []() {
    pending.reset();
    restartable = false;
    activeWorkspace.clear();
  };
  if (!h->register_setting(
          h->context, "miDebuggerPath", "gdb",
          "Default debugger when launch.json omits miDebuggerPath") ||
      !h->register_setting(h->context, "configuration", "",
                           "Selected launch.json profile name"))
    return 0;
  const Action actions[] = {
      {"gdb.configure", "GDB: Open launch.json", configure, nullptr},
      {"gdb.select", "GDB: Select Configuration", select, nullptr},
      {"gdb.start", "Start Debugging / Continue", start, nullptr},
      {"gdb.prepare", "GDB: Prepare without Running", prepare, nullptr},
      {"gdb.pause", "Pause", simple, "@pause"},
      {"gdb.stop", "Stop Debugging", simple, "@stop"},
      {"gdb.restart", "Restart Debugging", restart, nullptr},
      {"gdb.next", "Step Over", simple, "next"},
      {"gdb.step", "Step Into", simple, "step"},
      {"gdb.finish", "Step Out", simple, "finish"},
      {"gdb.breakpoint", "Toggle Breakpoint", breakpoint, nullptr},
      {"gdb.breakpoints", "List Breakpoints", simple, "info breakpoints"},
      {"gdb.stack", "Call Stack", simple, "backtrace"},
      {"gdb.locals", "Local Variables", simple, "info locals"},
      {"gdb.threads", "Threads", simple, "info threads"},
      {"gdb.evaluate", "Evaluate Expression", evaluate, nullptr},
      {"gdb.command", "GDB Command...", prompt, nullptr}};
  for (auto& a : actions) {
    if (!h->register_command(h->context, a.id, a.title, a.cb,
                             const_cast<char*>(a.cmd)))
      return 0;
    h->register_menu(h->context, "Run", a.title, a.id);
  }
  return h->register_panel(h->context, "gdb.debug", "Run and Debug", "Left",
                           panelView, nullptr)
             ? 1
             : 0;
}
