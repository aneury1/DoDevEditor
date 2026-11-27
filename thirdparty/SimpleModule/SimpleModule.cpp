// A minimal, self-contained DoDevEditor native plugin.
// No wxWidgets headers, editor internals, or direct access to settings files.
#include <string>

#include "dodev/sdk/PluginSettings.h"

namespace {
// All configuration belongs to this plugin. The host scopes these keys by
// the manifest's plugin ID and generates the Settings page automatically.
constexpr char kEditorId[] = "simplemodule.editor";
constexpr char kOutputId[] = "simplemodule.output";
constexpr char kCommandId[] = "simplemodule.sayHello";

// 1. A normal command callback. Settings are read at execution time.
void sayHello(const DodevHostAPI* host, void*) {
  const char* value = host->get_setting(host->context, "message");
  // Copy host-owned strings before calling other SDK functions.
  const std::string message = value ? value : "Hello from Simple Module!";
  host->log(host->context, 0, message.c_str());
  // Use the generic status hook when available (despite its legacy name).
  if (DODEV_HOST_HAS(host, debug_status))
    host->debug_status(host->context, message.c_str());
}

// 2. Editor-tab factory. The host builds and owns the native controls.
// The builder and its context are valid only during this callback.
void editorView(const DodevHostAPI* host, const DodevUiBuilder* ui, void*) {
  ui->label(ui->context, "Simple Module — Editor tab");
  const char* value = host->get_setting(host->context, "message");
  const std::string text = value ? value : "Hello from Simple Module!";
  ui->read_only_text(ui->context, text.c_str());
  // A button invokes a registered command or a host view action.
  ui->button(ui->context, "Say hello", kCommandId);
  ui->button(ui->context, "Open output panel",
             "plugin.view.simplemodule.output");
}

// 3. Output-panel factory. A selectable read-only text control is sufficient
// for this example; a live log stream would need a separate update API.
void outputView(const DodevHostAPI* host, const DodevUiBuilder* ui, void*) {
  ui->label(ui->context, "Simple Module — Output panel");
  const char* value = host->get_setting(host->context, "message");
  const std::string message = value ? value : "Hello from Simple Module!";
  const char* setting = host->get_setting(host->context, "level");
  const std::string level = setting ? setting : "info";
  const std::string prefix = level == "error"     ? "[ERROR] "
                             : level == "warning" ? "[WARNING] "
                                                  : "[INFO] ";
  const std::string output =
      "[INFO] Plugin output view opened.\n" + prefix + message;
  ui->read_only_text(ui->context, output.c_str());
  ui->button(ui->context, "Open editor tab", "plugin.view.simplemodule.editor");
}

// 4. Plugin shutdown. No resources are retained in this simple example.
void shutdown() {}
}  // namespace

// Required exported entry point. The host calls this once at activation.
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI* host,
                                                     DodevPluginAPI* api) {
  if (!host || !api || api->struct_size < sizeof(DodevPluginAPI) ||
      host->api_version != DODEV_PLUGIN_API_VERSION ||
      !DODEV_HOST_HAS(host, register_setting_ex))
    return 0;

  api->api_version = DODEV_PLUGIN_API_VERSION;
  api->shutdown = &shutdown;
  const auto ctx = host->context;

  // 5. Settings page. No settings dialog code is required in the editor.
  // Failure to register any field or view aborts plugin activation.
  if (!dodev::sdk::setting(host, "message", "Hello from Simple Module!",
                           "Message displayed when a new view opens.",
                           R"({"label":"Greeting message","type":"text"})"))
    return 0;
  if (!dodev::sdk::setting(
          host, "level", "info", "Example output severity label.",
          R"({"label":"Severity","type":"choice","choices":["info","warning","error"]})"))
    return 0;

  // 6. Register commands first, then register views and menu items.
  if (!host->register_command(ctx, kCommandId, "Simple Module: Say hello",
                              &sayHello, nullptr))
    return 0;
  if (!host->register_tab(ctx, kEditorId, "Simple Module", &editorView,
                          nullptr))
    return 0;
  if (!host->register_panel(ctx, kOutputId, "Simple Module Output", "Bottom",
                            &outputView, nullptr))
    return 0;

  // 7. Add entries to the top-level Options menu. The plugin owns labels
  // and action IDs. The generic host knows how to resolve plugin.view.*.
  if (!host->register_menu(ctx, "Options", "Simple Module: Say hello",
                           kCommandId))
    return 0;
  if (!host->register_menu(ctx, "Options", "Simple Module: Open editor tab",
                           "plugin.view.simplemodule.editor"))
    return 0;
  if (!host->register_menu(ctx, "Options", "Simple Module: Open output panel",
                           "plugin.view.simplemodule.output"))
    return 0;
  return 1;
}
