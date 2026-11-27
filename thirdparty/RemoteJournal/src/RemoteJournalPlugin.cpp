#include "dodev/sdk/PluginSettings.h"
namespace {
// The plugin defines the workspace profile schema; the host only renders and
// saves it.
constexpr const char* defaultProfiles =
    R"JSON({"profiles":[{"name":"Development server","host":"","user":"","port":22,"unit":"","lines":200,"authentication":"key","follow":true,"sudoNonInteractive":false,"filterSetFile":""}]})JSON";
void configureWorkspace(const DodevHostAPI* host, void*) {
  if (DODEV_HOST_HAS(host, open_workspace_config))
    host->open_workspace_config(host->context, defaultProfiles);
}
void workspaceView(const DodevHostAPI*, const DodevUiBuilder* ui, void*) {
  ui->label(ui->context,
            "Journal Logs — workspace SSH profiles. Save changes, then "
            "connect. Each capture opens in its own editor tab. Never store "
            "passwords in profiles.");
  if (ui->struct_size >= offsetof(DodevUiBuilder, workspace_profiles) +
                             sizeof(ui->workspace_profiles) &&
      ui->workspace_profiles)
    ui->workspace_profiles(ui->context, defaultProfiles);
  else
    ui->button(ui->context, "Configure workspace profiles...",
               "remotejournal.workspace.configure");
  ui->button(ui->context, "Connect / Start capture...", "remotejournal.open");
}

void filtersView(const DodevHostAPI*, const DodevUiBuilder* ui, void*) {
  ui->label(
      ui->context,
      "Manage independent capture filters and reusable workspace rule sets.");
}
void open(const DodevHostAPI* host, void*) {
  if (DODEV_HOST_HAS(host, open_remote_journal))
    host->open_remote_journal(host->context);
  else if (host && host->log)
    host->log(host->context, 2, "Remote Journal needs DoDevEditor 3.6 host");
}
}  // namespace
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI* host,
                                                     DodevPluginAPI* plugin) {
  if (!host || !plugin || host->api_version != DODEV_PLUGIN_API_VERSION ||
      !DODEV_HOST_HAS(host, open_remote_journal))
    return 0;
  if (!host->register_command(host->context, "remotejournal.open",
                              "Remote System Journal", &open, nullptr))
    return 0;
  host->register_menu(host->context, "Tools", "Remote Journal (SSH)...",
                      "plugin.view.remotejournal.workspace");
  dodev::sdk::setting(host, "host", "", "Remote host/IP (SSH)",
                      R"({"label":"SSH host"})");
  dodev::sdk::setting(host, "user", "",
                      "Remote SSH user (key or password in wxWidgets)",
                      R"({"label":"SSH username"})");
  dodev::sdk::setting(host, "port", "22", "SSH port",
                      R"({"type":"int","min":1,"max":65535})");
  host->register_setting(host->context, "unit", "", "Systemd unit (optional)");
  dodev::sdk::setting(host, "lines", "200", "Initial journal lines",
                      R"({"type":"int","min":1,"max":10000})");
  dodev::sdk::setting(host, "follow", "true", "Follow live journal logs",
                      R"({"type":"bool"})");
  dodev::sdk::setting(host, "sudoNonInteractive", "false",
                      "Use sudo -n, only when configured on remote server",
                      R"({"type":"bool"})");
  dodev::sdk::setting(host, "filterMode", "all", "all | fixed | regex",
                      R"({"type":"choice","choices":["all","fixed","regex"]})");
  host->register_setting(host->context, "filterText", "",
                         "Log text/regex filter");
  if (!dodev::sdk::setting(
          host, "authentication", "key",
          "Password mode prompts at connection time; passwords are never "
          "stored.",
          R"({"label":"Authentication","type":"choice","choices":[{"value":"key","label":"SSH key / agent"},{"value":"password","label":"SSH password prompt"}]})"))
    return 0;
  if (!dodev::sdk::setting(host, "payload", "", "Initial payload substring.",
                           R"({"label":"Payload filter"})"))
    return 0;
  if (!dodev::sdk::setting(host, "service", "",
                           "Initial systemd unit substring (local filter).",
                           R"({"label":"Service filter"})"))
    return 0;
  if (!dodev::sdk::setting(host, "fromUtc", "",
                           "Initial inclusive UTC start (blank = unbounded).",
                           R"({"label":"From UTC"})"))
    return 0;
  if (!dodev::sdk::setting(host, "toUtc", "",
                           "Initial inclusive UTC end (blank = unbounded).",
                           R"({"label":"To UTC"})"))
    return 0;
  if (!dodev::sdk::setting(
          host, "priority", "-1",
          "Initial exact priority: -1 all, 0 emergency through 7 debug, 8 "
          "diagnostics.",
          R"({"label":"Priority","type":"choice","choices":[{"value":"-1","label":"All"},{"value":"0","label":"Emergency"},{"value":"1","label":"Alert"},{"value":"2","label":"Critical"},{"value":"3","label":"Error"},{"value":"4","label":"Warning"},{"value":"5","label":"Notice"},{"value":"6","label":"Info"},{"value":"7","label":"Debug"},{"value":"8","label":"Unknown / SSH"}]})"))
    return 0;
  if (!dodev::sdk::setting(host, "caseSensitive", "false",
                           "Match text with case sensitivity.",
                           R"({"type":"bool","label":"Case sensitive"})"))
    return 0;
  if (!dodev::sdk::setting(host, "autoScroll", "true",
                           "Automatically scroll to new records.",
                           R"({"type":"bool","label":"Auto-scroll"})"))
    return 0;
  host->register_command(host->context, "remotejournal.workspace.configure",
                         "Configure RemoteJournal workspace profiles",
                         &configureWorkspace, nullptr);
  host->register_panel(host->context, "remotejournal.workspace", "Journal Logs",
                       "Workspace", &workspaceView, nullptr);
  host->register_panel(host->context, "remotejournal.filters",
                       "Journal Filters", "Workspace", &filtersView, nullptr);
  host->register_menu(host->context, "Options",
                      "RemoteJournal: Workspace profiles...",
                      "plugin.view.remotejournal.workspace");
  dodev::sdk::setting(
      host, "applicationId", "",
      "Initial application ID (APP_ID / SYSLOG_IDENTIFIER / process).",
      R"({"label":"Application ID"})");
  dodev::sdk::setting(host, "payloadRegex", "false",
                      "Interpret payload filter as a regular expression.",
                      R"({"type":"bool"})");
  dodev::sdk::setting(
      host, "filterSetFile", "",
      "Optional JSON rule set; relative paths resolve within the workspace.",
      R"({"label":"Default filter set file"})");
  dodev::sdk::setting(
      host, "tableForeground", "",
      "Table foreground color #RRGGBB; blank uses editor theme.",
      R"({"label":"Table foreground"})");
  dodev::sdk::setting(
      host, "tableBackground", "",
      "Table background color #RRGGBB; blank uses editor theme.",
      R"({"label":"Table background"})");
  plugin->api_version = DODEV_PLUGIN_API_VERSION;
  return 1;
}
