#include "dodev/sdk/PluginSettings.h"
namespace {
void open(const DodevHostAPI* host, void*) {
  if (DODEV_HOST_HAS(host, open_ai_assistant))
    host->open_ai_assistant(host->context);
}
void panel(const DodevHostAPI*, const DodevUiBuilder* ui, void*) {
  if (!ui) return;
  ui->label(
      ui->context,
      "AI Assistant: chat with an OpenAI-compatible provider or local Ollama.");
  ui->button(ui->context, "Open AI Assistant", "ai.open");
  ui->label(ui->context,
            "The active file is sent only when you explicitly choose an action "
            "that includes it.");
}
}  // namespace
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI* host,
                                                     DodevPluginAPI* plugin) {
  if (!host || !plugin || host->api_version != DODEV_PLUGIN_API_VERSION ||
      !DODEV_HOST_HAS(host, open_ai_assistant))
    return 0;
  if (!host->register_command(host->context, "ai.open", "Open AI Assistant",
                              &open, nullptr))
    return 0;
  host->register_menu(host->context, "Tools", "AI Assistant...", "ai.open");
  host->register_panel(host->context, "ai.help", "AI Assistant Help", "Right",
                       &panel, nullptr);
  host->register_setting(host->context, "endpoint",
                         "http://127.0.0.1:11434/v1/chat/completions",
                         "Legacy endpoint (AI Provider Profiles supersede it)");
  host->register_setting(
      host->context, "model", "llama3.2",
      "Legacy model identifier (provider profiles override this)");
  host->register_setting(
      host->context, "activeProfileId", "local-llamacpp",
      "Selected AI provider profile (edit profiles in AI Assistant)");
  host->register_setting(
      host->context, "keyEnvironment", "OPENAI_API_KEY",
      "Environment variable holding API token (never the token itself)");
  dodev::sdk::setting(host, "timeoutSeconds", "45", "Request timeout seconds",
                      R"({"type":"int","min":1,"max":600})");
  dodev::sdk::setting(
      host, "maxContextBytes", "16000",
      "Maximum active-file bytes to include with explicit consent",
      R"({"type":"int","min":0,"max":1000000})");
  if (!dodev::sdk::settingsAction(host, "Open AI Assistant / Provider profiles",
                                  "ai.open"))
    return 0;
  plugin->api_version = DODEV_PLUGIN_API_VERSION;
  return 1;
}
