#include "dodev/sdk/dodev_plugin_api.h"
namespace {
void open(const DodevHostAPI* host, void*) {
  if (DODEV_HOST_HAS(host, open_remote_journal))
    host->open_remote_journal(host->context);
  else if (host && host->log)
    host->log(host->context, 2, "Remote Journal needs DoDevEditor 3.6 host");
}
void help(const DodevHostAPI*, const DodevUiBuilder* ui, void*) {
  if (!ui) return;
  ui->label(ui->context,
            "Remote systemd journal via SSH (host keys checked, no passwords "
            "saved).");
  ui->button(ui->context, "Configure Remote Journal...", "remotejournal.open");
  ui->label(ui->context,
            "Logs appear in Tasks · Journal next to the integrated Terminal.");
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
                      "remotejournal.open");
  host->register_tab(host->context, "remotejournal.help", "Remote Journal",
                     &help, nullptr);
  host->register_setting(host->context, "host", "", "Remote host/IP (SSH)");
  host->register_setting(host->context, "user", "",
                         "Remote SSH user (key or password in wxWidgets)");
  host->register_setting(host->context, "port", "22", "SSH port");
  host->register_setting(host->context, "unit", "", "Systemd unit (optional)");
  host->register_setting(host->context, "lines", "200",
                         "Initial journal lines");
  host->register_setting(host->context, "follow", "true",
                         "Follow live journal logs");
  host->register_setting(host->context, "sudoNonInteractive", "false",
                         "Use sudo -n, only when configured on remote server");
  host->register_setting(host->context, "filterMode", "all",
                         "all | fixed | regex");
  host->register_setting(host->context, "filterText", "",
                         "Log text/regex filter");
  plugin->api_version = DODEV_PLUGIN_API_VERSION;
  return 1;
}
