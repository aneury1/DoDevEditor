#include "dodev/sdk/dodev_plugin_api.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace {
namespace fs = std::filesystem;
void logMessage(const DodevHostAPI *host, int severity,
                const std::string &msg) {
  if (host->log)
    host->log(host->context, severity, msg.c_str());
}
std::string setting(const DodevHostAPI *host, const char *name,
                    const char *fallback) {
  const char *value = host->get_setting(host->context, name);
  return value && *value ? value : fallback;
}
std::string quote(const std::string &arg) {
#ifdef _WIN32
  std::string quoted = "\"";
  for (char c : arg) {
    if (c == '\"')
      quoted += "\\\"";
    else if (c == '%' || c == '!' || c == '^' || c == '&' || c == '|' ||
             c == '<' || c == '>')
      return {};
    else
      quoted += c;
  }
  return quoted + '"';
#else
  std::string quoted = "'";
  for (char c : arg) {
    if (c == '\'')
      quoted += "'\\''";
    else
      quoted += c;
  }
  return quoted + '\'';
#endif
}
void formatDocument(const DodevHostAPI *host, void *) {
  if (!DODEV_HOST_HAS(host, get_active_document) ||
      !DODEV_HOST_HAS(host, replace_active_document)) {
    logMessage(host, 2,
               "Formatting requires DoDevEditor Plugin SDK v1 document bridge");
    return;
  }
  const char *path = nullptr, *text = nullptr;
  if (!host->get_active_document(host->context, &path, &text) || !text) {
    logMessage(host, 2, "Open a writable text document before formatting");
    return;
  }
  const std::string original(text);
  if (original.size() > 16 * 1024 * 1024) {
    logMessage(host, 2, "Document is larger than 16 MiB formatting limit");
    return;
  }
  std::string executable = setting(host, "executable", "clang-format");
  std::string style = setting(host, "style", "LLVM");
  if (executable.empty() || executable.size() > 1024 || style.empty() ||
      style.size() > 2048 || executable.find('\n') != std::string::npos ||
      style.find('\n') != std::string::npos) {
    logMessage(host, 2, "Invalid clang-format executable or style setting");
    return;
  }
  std::error_code ec;
  const fs::path folder = fs::temp_directory_path(ec);
  if (ec) {
    logMessage(host, 2, "Cannot locate temporary directory");
    return;
  }
  std::string extension = path ? fs::path(path).extension().string() : ".cpp";
  if (extension.empty() || extension.size() > 12)
    extension = ".cpp";
  for (char c : extension)
    if (!(c == '.' || c == '_' || (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
      extension = ".cpp";
      break;
    }
  const auto timestamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path temporary =
      folder / ("dodev-clangformat-" + std::to_string(timestamp) + extension);
  struct Cleaner {
    fs::path file;
    ~Cleaner() {
      std::error_code ignored;
      fs::remove(file, ignored);
    }
  } cleaner{temporary};
  {
    std::ofstream file(temporary, std::ios::binary);
    if (!file) {
      logMessage(host, 2, "Cannot create formatting scratch file");
      return;
    }
    file.write(original.data(), static_cast<std::streamsize>(original.size()));
    if (!file) {
      logMessage(host, 2, "Cannot write scratch file");
      return;
    }
  }
  // The executable and its arguments are escaped independently. The source file
  // is not modified.
  const auto command = quote(executable) + " --style=" + quote(style) + " -i " +
                       quote(temporary.string());
  if (command.empty() || std::system(command.c_str()) != 0) {
    logMessage(host, 2,
               "clang-format failed; check executable and style in Settings > "
               "Plugins");
    return;
  }
  std::ifstream file(temporary, std::ios::binary);
  if (!file) {
    logMessage(host, 2, "clang-format did not return an output file");
    return;
  }
  const std::string formatted(std::istreambuf_iterator<char>{file}, {});
  if (formatted.size() > 32 * 1024 * 1024) {
    logMessage(host, 2, "Formatted file unexpectedly large");
    return;
  }
  if (formatted == original) {
    logMessage(host, 1, "Document is already formatted");
    return;
  }
  if (!host->replace_active_document(host->context, formatted.c_str())) {
    logMessage(
        host, 2,
        "Cannot apply formatted result to editor (document may be read-only)");
    return;
  }
  logMessage(
      host, 1,
      "Applied clang-format to active buffer. Save the document when ready.");
}
void onStyle(const DodevHostAPI *host, const char *value, void *) {
  if (!value || !*value)
    return;
  if (host->set_setting(host->context, "style", value))
    logMessage(host, 1, std::string("clang-format style changed to ") + value);
}
void configure(const DodevHostAPI *host, void *) {
  if (!DODEV_HOST_HAS(host, prompt_input)) {
    logMessage(host, 2, "Input prompt is unavailable");
    return;
  }
  const auto current = setting(host, "style", "LLVM");
  host->prompt_input(
      host->context, "clang-format Settings",
      "Style (LLVM, Google, Mozilla, Chromium, Microsoft, file, file:/path)",
      current.c_str(), &onStyle, nullptr);
}
void formatTab(const DodevHostAPI *, const DodevUiBuilder *ui, void *) {
  ui->label(ui->context,
            "clang-format · C / C++ / Objective-C / JavaScript formatting");
  ui->label(
      ui->context,
      "Formats the active in-memory editor buffer without saving to disk.");
  ui->button(ui->context, "Format active document", "clangformat.apply");
  ui->button(ui->context, "Choose formatting style...",
             "clangformat.style_prompt");
  ui->read_only_text(
      ui->context, "Settings > Plugins: executable, style.\nRequires a local "
                   "clang-format binary; network access is not required.\nUndo "
                   "uses the editor's native transaction history.");
}
} // namespace
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI *host,
                                                     DodevPluginAPI *api) {
  if (!host || !api || host->api_version != DODEV_PLUGIN_API_VERSION ||
      host->struct_size < offsetof(DodevHostAPI, subscribe_event) +
                              sizeof(host->subscribe_event) ||
      api->struct_size < sizeof(DodevPluginAPI))
    return 0;
  api->api_version = DODEV_PLUGIN_API_VERSION;
  api->shutdown = nullptr;
  if (!host->register_setting(host->context, "executable", "clang-format",
                              "Path or command name of clang-format"))
    return 0;
  if (!host->register_setting(host->context, "style", "LLVM",
                              "Preset or file:/path/to/.clang-format"))
    return 0;
  if (!host->register_command(host->context, "clangformat.apply",
                              "Format Active Document", &formatDocument,
                              nullptr))
    return 0;
  if (!host->register_command(host->context, "clangformat.style_prompt",
                              "Configure clang-format Style...", &configure,
                              nullptr))
    return 0;
  host->register_menu(host->context, "Edit", "Format with clang-format",
                      "clangformat.apply");
  host->register_menu(host->context, "Tools", "clang-format Style...",
                      "clangformat.style_prompt");
  host->register_tab(host->context, "clangformat.help", "clang-format Tools",
                     &formatTab, nullptr);
  return 1;
}
