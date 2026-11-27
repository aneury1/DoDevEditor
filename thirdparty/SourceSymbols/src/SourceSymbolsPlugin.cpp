#include "dodev/sdk/PluginSettings.h"
namespace {
void open(const DodevHostAPI* host, void*) {
  if (DODEV_HOST_HAS(host, open_source_symbols))
    host->open_source_symbols(host->context);
  else if (host && host->log)
    host->log(host->context, 2, "Symbols requires DoDevEditor 3.6 or later");
}
void help(const DodevHostAPI*, const DodevUiBuilder* ui, void*) {
  if (!ui) return;
  ui->label(ui->context,
            "Clang-first AST symbols with automatic regex "
            "fallback; works with unsaved edits.");
  ui->button(ui->context, "Extract symbols from editor", "sourcesymbols.open");
  ui->label(ui->context,
            "Results appear in Tasks · Symbols, with search, "
            "navigation and CSV export.");
}
}  // namespace
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI* host,
                                                     DodevPluginAPI* plugin) {
  if (!host || !plugin || host->api_version != DODEV_PLUGIN_API_VERSION ||
      !DODEV_HOST_HAS(host, open_source_symbols))
    return 0;
  if (!host->register_command(host->context, "sourcesymbols.open",
                              "Symbols from Active File", &open, nullptr))
    return 0;
  host->register_menu(host->context, "Tools", "Symbols / Outline...",
                      "sourcesymbols.open");
  host->register_tab(host->context, "sourcesymbols.help", "Source Symbols",
                     &help, nullptr);
  dodev::sdk::setting(host, "showIncludes", "true",
                      "Include #include directives and #define macros",
                      R"({"type":"bool"})");
  dodev::sdk::setting(
      host, "parserMode", "auto", "Symbol parser: auto, clang or regex",
      R"({"type":"choice","choices":["auto","clang","regex"]})");
  host->register_setting(
      host->context, "clangExecutable", "",
      "Optional Clang binary path (empty finds clang++ on PATH)");
  dodev::sdk::setting(host, "timeoutMs", "2500",
                      "Clang AST parsing timeout in milliseconds",
                      R"({"type":"int","min":100,"max":60000})");
  dodev::sdk::setting(
      host, "captureKinds", "",
      "Comma-separated kinds; blank captures all. "
      "function,method,class,struct,namespace,enum,enumerator,field,variable,"
      "type,concept,macro,include,interface,trait",
      R"({"label":"Capture symbol kinds","type":"text"})");
  plugin->api_version = DODEV_PLUGIN_API_VERSION;
  return 1;
}
