#include <string>

#include "dodev/sdk/PluginSettings.h"
namespace {
void configureWorkspace(const DodevHostAPI* host, void*) {
  if (DODEV_HOST_HAS(host, open_workspace_config))
    host->open_workspace_config(
        host->context,
        R"JSON({"profiles":[{"name":"Device A","port":"/dev/ttyUSB0","baud":115200,"dataBits":8,"stopBits":1,"parity":"N"},{"name":"Device B","port":"/dev/ttyUSB1","baud":115200}]})JSON");
}
void workspaceView(const DodevHostAPI*, const DodevUiBuilder* ui, void*) {
  ui->label(ui->context,
            "Workspace profiles: configure and save JSON, then select profiles "
            "to connect. No connections start automatically.");
  ui->button(ui->context, "Configure workspace profiles...",
             "serialmonitor.workspace.configure");
  ui->button(ui->context, "Open capture / connection...", "serialmonitor.open");
}

void openMonitor(const DodevHostAPI* host, void*) {
  if (!DODEV_HOST_HAS(host, open_serial_monitor)) {
    if (host && host->log)
      host->log(host->context, 2,
                "Serial Monitor requires DoDevEditor 3.5 host integration");
    return;
  }
  if (!host->open_serial_monitor(host->context) && host->log)
    host->log(host->context, 2, "Could not open Serial Monitor dialog");
}
void view(const DodevHostAPI*, const DodevUiBuilder* ui, void*) {
  if (!ui) return;
  ui->label(ui->context,
            "Serial Monitor | Real-time serial logs and filtering");
  ui->button(ui->context, "Configure / start serial capture...",
             "serialmonitor.open");
  ui->label(ui->context,
            "Output appears in TASKS next to the integrated terminal.");
}
void shutdown() {}
}  // namespace
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI* host,
                                                     DodevPluginAPI* plugin) {
  if (!host || !plugin || host->api_version != DODEV_PLUGIN_API_VERSION)
    return 0;
  if (!DODEV_HOST_HAS(host, open_serial_monitor)) return 0;
  if (!host->register_command(host->context, "serialmonitor.open",
                              "Monitor serie...", &openMonitor, nullptr))
    return 0;
  if (!host->register_menu(host->context, "Tools", "Monitor serie...",
                           "serialmonitor.open"))
    return 0;
  host->register_tab(host->context, "serialmonitor.help", "Serial Monitor",
                     &view, nullptr);
  host->register_setting(host->context, "port", "",
                         "Serial device path or COM port");
  dodev::sdk::setting(host, "baud", "115200", "Baud rate",
                      R"({"type":"int","min":50,"max":4000000})");
  dodev::sdk::setting(host, "dataBits", "8", "Data bits: 7 or 8",
                      R"({"type":"choice","choices":["7","8"]})");
  dodev::sdk::setting(host, "stopBits", "1", "Stop bits: 1 or 2",
                      R"({"type":"choice","choices":["1","2"]})");
  dodev::sdk::setting(host, "parity", "N", "Parity: N, E, O",
                      R"({"type":"choice","choices":["N","E","O"]})");
  dodev::sdk::setting(host, "filterMode", "all", "all | fixed | regex",
                      R"({"type":"choice","choices":["all","fixed","regex"]})");
  host->register_setting(host->context, "filterText", "",
                         "Fixed text or regular expression");
  dodev::sdk::setting(host, "caseSensitive", "false", "Case-sensitive filter",
                      R"({"type":"bool"})");
  dodev::sdk::setting(host, "showOnlyMatches", "true",
                      "Show only matching log lines", R"({"type":"bool"})");
  dodev::sdk::setting(host, "captureAsAdministrator", "false",
                      "Use the privileged Polkit helper for every capture "
                      "(Linux; explicit consent).",
                      R"({"type":"bool"})");
  dodev::sdk::setting(host, "allowPolkit", "false",
                      "Request Polkit authorization after permission denied",
                      R"({"type":"bool"})");
  host->register_command(host->context, "serialmonitor.workspace.configure",
                         "Configure SerialMonitor workspace profiles",
                         &configureWorkspace, nullptr);
  host->register_panel(host->context, "serialmonitor.workspace",
                       "SerialMonitor profiles", "Workspace", &workspaceView,
                       nullptr);
  host->register_menu(host->context, "Options",
                      "SerialMonitor: Workspace profiles...",
                      "plugin.view.serialmonitor.workspace");
  plugin->api_version = DODEV_PLUGIN_API_VERSION;
  if (plugin->struct_size >=
      offsetof(DodevPluginAPI, shutdown) + sizeof(plugin->shutdown))
    plugin->shutdown = &shutdown;
  if (host->log)
    host->log(host->context, 1,
              "Serial Monitor available in Herramientas > Monitor serie...");
  return 1;
}
