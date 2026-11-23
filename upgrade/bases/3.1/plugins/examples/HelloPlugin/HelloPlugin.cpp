#include "dodev/sdk/dodev_plugin_api.h"
#include <stdio.h>
#include <string.h>
namespace {
void say_hello(const DodevHostAPI *host, void *) {
  const char *greeting = host->get_setting(host->context, "greeting");
  char buffer[512];
  snprintf(buffer, sizeof(buffer), "HelloPlugin command: %s",
           greeting ? greeting : "Hello");
  host->log(host->context, 1, buffer);
}
void on_document(const DodevHostAPI *host, const char *event, const char *,
                 void *) {
  host->log(host->context, 1, event ? event : "event");
}
void hello_tab(const DodevHostAPI *, const DodevUiBuilder *ui, void *) {
  ui->label(ui->context, "Hello Plugin · Raven UI contributions");
  ui->label(
      ui->context,
      "This tab was created dynamically by the host's plugin view builder.");
  ui->button(ui->context, "Run Hello command", "hello.say");
  ui->read_only_text(ui->context,
                     "# Hello Plugin\n\nC ABI version 1.\nNo direct dependency "
                     "on Raven headers is needed to use these widgets.\n");
}
void hello_panel(const DodevHostAPI *, const DodevUiBuilder *ui, void *) {
  ui->label(ui->context, "Hello Plugin · Dock panel");
  ui->button(ui->context, "Say Hello", "hello.say");
}
void goodbye() { /* Release owned resources; host destroys plugin views during
                    application shutdown. */
}
} // namespace
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI *host,
                                                     DodevPluginAPI *api) {
  if (!host || !api || host->api_version != DODEV_PLUGIN_API_VERSION ||
      host->struct_size < sizeof(DodevHostAPI))
    return 0;
  if (api->struct_size < sizeof(DodevPluginAPI))
    return 0;
  api->api_version = DODEV_PLUGIN_API_VERSION;
  api->shutdown = &goodbye;
  if (!host->register_setting(host->context, "greeting", "Hello from Raven!",
                              "Message shown by the demo command"))
    return 0;
  if (!host->register_command(host->context, "hello.say", "Say Hello",
                              &say_hello, nullptr))
    return 0;
  host->register_menu(host->context, "Tools", "HelloPlugin: Say Hello",
                      "hello.say");
  host->register_tab(host->context, "hello.tab", "Hello Plugin", &hello_tab,
                     nullptr);
  host->register_panel(host->context, "hello.panel", "Hello Tools", "Right",
                       &hello_panel, nullptr);
  host->register_template(host->context, "hello-file", "Hello language",
                          ".hello",
                          "Hello, ${PROJECT_NAME}!\nFile: ${FILE_NAME}\n");
  host->subscribe_event(host->context, "document.opened", &on_document,
                        nullptr);
  return 1;
}
