#include "QRMatrix.h"
#include "dodev/sdk/dodev_plugin_api.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#ifdef _WIN32
#include <shellapi.h>
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;
enum class Output { Png, Svg };
const Output kPng = Output::Png, kSvg = Output::Svg;
std::string lastOutput;

void logMsg(const DodevHostAPI *host, int level, const std::string &msg) {
  if (host && host->log)
    host->log(host->context, level, msg.c_str());
}
std::string setting(const DodevHostAPI *host, const char *key,
                    const char *fallback) {
  if (!host || !host->get_setting)
    return fallback;
  const char *value = host->get_setting(host->context, key);
  return value && *value ? std::string(value) : std::string(fallback);
}
int numericSetting(const DodevHostAPI *host, const char *name, int fallback,
                   int minimum, int maximum) {
  const auto raw = setting(host, name, "");
  if (raw.empty() || raw.size() > 10)
    return fallback;
  try {
    size_t parsed = 0;
    int result = std::stoi(raw, &parsed);
    return parsed == raw.size() && result >= minimum && result <= maximum
               ? result
               : fallback;
  } catch (...) {
    return fallback;
  }
}
std::string lowercase(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
dodevqr::ErrorCorrection eccSetting(const DodevHostAPI *host) {
  const auto raw = lowercase(setting(host, "errorCorrection", "medium"));
  if (raw == "low" || raw == "l")
    return dodevqr::ErrorCorrection::Low;
  if (raw == "high" || raw == "h")
    return dodevqr::ErrorCorrection::High;
  if (raw == "quartile" || raw == "q")
    return dodevqr::ErrorCorrection::Quartile;
  return dodevqr::ErrorCorrection::Medium;
}
std::string bytesString(const std::vector<uint8_t> &bytes) {
  return std::string(reinterpret_cast<const char *>(bytes.data()),
                     bytes.size());
}
fs::path initialOutputPath(const DodevHostAPI *host, const std::string &docPath,
                           Output format) {
  std::error_code ec;
  const std::string custom = setting(host, "outputDirectory", "");
  fs::path source = fs::u8path(docPath);
  fs::path directory;
  if (!custom.empty())
    directory = fs::u8path(custom);
  else if (!docPath.empty() && source.has_parent_path())
    directory = source.parent_path();
  else
    directory = fs::current_path(ec);
  if (directory.empty() || ec)
    directory = fs::temp_directory_path(ec);
  std::string title = docPath.empty() ? "untitled" : source.stem().u8string();
  if (title.empty())
    title = "untitled";
  return directory /
         fs::u8path(title + (format == Output::Png ? ".qr.png" : ".qr.svg"));
}
// Writes using a same-directory temporary file and renames on success. Existing
// destinations are left intact unless overwriteExisting is explicitly enabled.
void writeFile(const fs::path &destination, const std::string &contents,
               bool overwrite) {
  std::error_code ec;
  if (!fs::is_directory(destination.parent_path(), ec))
    throw std::runtime_error("Output directory does not exist");
  if (!overwrite && fs::exists(destination, ec))
    throw std::runtime_error(
        "Output already exists (change name or enable overwriteExisting)");
  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path temporary = destination;
  temporary += ".tmp-" + std::to_string(stamp);
  struct Cleanup {
    fs::path path;
    ~Cleanup() {
      std::error_code e;
      fs::remove(path, e);
    }
  } cleanup{temporary};
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out)
      throw std::runtime_error("Cannot create output file");
    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    out.flush();
    if (!out)
      throw std::runtime_error("Failed to write complete QR image");
  }
  if (overwrite && fs::exists(destination, ec)) {
#ifdef _WIN32
    // std::filesystem::rename() cannot replace an existing file on Windows.
    fs::remove(destination, ec);
    if (ec)
      throw std::runtime_error("Cannot replace existing output file");
#endif
  }
  fs::rename(temporary, destination, ec);
  if (ec)
    throw std::runtime_error("Cannot move generated image to destination: " +
                             ec.message());
}
void generate(const DodevHostAPI *host, const char *textPath, Output format) {
  if (!DODEV_HOST_HAS(host, get_active_document)) {
    logMsg(host, 2, "QR plugin requires DoDevEditor 3.3 document access");
    return;
  }
  const char *docPath = nullptr, *contents = nullptr;
  if (!host->get_active_document(host->context, &docPath, &contents) ||
      !contents) {
    logMsg(host, 2, "Open a text document before generating a QR code");
    return;
  }
  try {
    const std::string text(
        contents); // Important: copy before further host callbacks.
    if (text.empty())
      throw std::invalid_argument("Active document is empty");
    fs::path output = fs::u8path(textPath ? textPath : "");
    if (output.empty())
      throw std::invalid_argument("No output path selected");
    if (output.extension() != (format == Output::Png ? ".png" : ".svg")) {
      throw std::invalid_argument(
          "Output must have a " +
          std::string(format == Output::Png ? ".png" : ".svg") + " extension");
    }
    if (!output.is_absolute())
      output = fs::absolute(output);
    const auto matrix = dodevqr::encode(text, eccSetting(host));
    const int border = numericSetting(host, "quietZone", 4, 4, 16);
    std::string image;
    if (format == Output::Png) {
      const auto png = dodevqr::toPng(
          matrix, numericSetting(host, "pixelsPerModule", 8, 2, 32), border);
      image = bytesString(png);
    } else
      image = dodevqr::toSvg(matrix, border);
    const bool overwrite =
        lowercase(setting(host, "overwriteExisting", "false")) == "true";
    writeFile(output, image, overwrite);
    lastOutput = output.u8string();
    logMsg(host, 1,
           "QR code created: " + lastOutput + " (version " +
               std::to_string(matrix.version) + ", " +
               std::to_string(text.size()) + " UTF-8 bytes; offline)");
  } catch (const std::exception &e) {
    logMsg(host, 2, std::string("QR export failed: ") + e.what());
  }
}
void onOutputPath(const DodevHostAPI *host, const char *entered, void *data) {
  if (!entered || !*entered)
    return;
  const auto fmt = *static_cast<const Output *>(data);
  generate(host, entered, fmt);
}
void generateWithPrompt(const DodevHostAPI *host, Output fmt) {
  if (!DODEV_HOST_HAS(host, get_active_document)) {
    logMsg(host, 2, "Upgrade to DoDevEditor 3.3 for document integration");
    return;
  }
  const char *path = nullptr, *contents = nullptr;
  if (!host->get_active_document(host->context, &path, &contents) ||
      !contents || !*contents) {
    logMsg(host, 2, "Open a non-empty text document first");
    return;
  }
  const auto proposed =
      initialOutputPath(host, path ? path : "", fmt).u8string();
  if (!DODEV_HOST_HAS(host, prompt_input)) {
    // SDK supports non-interactive fallback to a predictably named file.
    generate(host, proposed.c_str(), fmt);
    return;
  }
  const char *title =
      fmt == Output::Png ? "Export QR Code to PNG" : "Export QR Code to SVG";
  const auto *marker = fmt == Output::Png ? &kPng : &kSvg;
  if (!host->prompt_input(host->context, title,
                          "Save image to path:", proposed.c_str(),
                          &onOutputPath, const_cast<Output *>(marker)))
    logMsg(host, 2, "Unable to open QR image export prompt");
}
void onPng(const DodevHostAPI *host, void *) {
  generateWithPrompt(host, Output::Png);
}
void onSvg(const DodevHostAPI *host, void *) {
  generateWithPrompt(host, Output::Svg);
}
void onPreferred(const DodevHostAPI *host, void *) {
  auto s = lowercase(setting(host, "preferredFormat", "png"));
  generateWithPrompt(host, s == "svg" ? Output::Svg : Output::Png);
}
void openLast(const DodevHostAPI *host, void *) {
  if (lastOutput.empty()) {
    logMsg(host, 2, "No QR export yet. Generate an image first.");
    return;
  }
  std::error_code ec;
  if (!fs::exists(fs::u8path(lastOutput), ec)) {
    logMsg(host, 2, "Previously generated QR image no longer exists");
    return;
  }
#ifdef _WIN32
  auto path = fs::u8path(lastOutput).wstring();
  if (reinterpret_cast<intptr_t>(ShellExecuteW(nullptr, L"open", path.c_str(),
                                               nullptr, nullptr,
                                               SW_SHOWNORMAL)) <= 32)
    logMsg(host, 2, "Could not open the image with the default application");
#else
  // Double-fork reaps the intermediate child; no shell and no injection.
  pid_t child = fork();
  if (child < 0) {
    logMsg(host, 2, "Could not start image viewer");
    return;
  }
  if (child == 0) {
    pid_t grand = fork();
    if (grand == 0) {
#if defined(__APPLE__)
      execlp("open", "open", lastOutput.c_str(), static_cast<char *>(nullptr));
#else
      execlp("xdg-open", "xdg-open", lastOutput.c_str(),
             static_cast<char *>(nullptr));
#endif
      _exit(127);
    }
    _exit(grand < 0 ? 1 : 0);
  }
  int status = 0;
  if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status) ||
      WEXITSTATUS(status) != 0)
    logMsg(host, 2, "Could not start image viewer");
#endif
}
void onFormatSelected(const DodevHostAPI *host, const char *value, void *) {
  if (!value)
    return;
  auto s = lowercase(value);
  if (s != "png" && s != "svg") {
    logMsg(host, 2, "Supported formats: png or svg");
    return;
  }
  if (!host->set_setting(host->context, "preferredFormat", s.c_str()))
    logMsg(host, 2, "Could not save preferred QR format");
  else
    logMsg(host, 1, "Preferred QR export format: " + s);
}
void configureFormat(const DodevHostAPI *host, void *) {
  if (!DODEV_HOST_HAS(host, prompt_input)) {
    logMsg(host, 2, "Input prompts unavailable");
    return;
  }
  const auto format = setting(host, "preferredFormat", "png");
  host->prompt_input(host->context, "QR Code Options",
                     "Preferred format (png or svg):", format.c_str(),
                     &onFormatSelected, nullptr);
}
void panel(const DodevHostAPI *, const DodevUiBuilder *ui, void *) {
  if (!ui || !ui->label || !ui->button || !ui->read_only_text)
    return;
  ui->label(ui->context, "QR Code Generator — DoDevEditor 3.3");
  ui->label(ui->context,
            "Encodes the entire active document into one offline QR code.");
  ui->button(ui->context, "Generate QR Code...", "qrcode.generate");
  ui->button(ui->context, "Save PNG...", "qrcode.png");
  ui->button(ui->context, "Save SVG...", "qrcode.svg");
  ui->button(ui->context, "Open last generated image", "qrcode.open_last");
  ui->button(ui->context, "Change preferred format...",
             "qrcode.preferred_format");
  const std::string status =
      "Last export: " +
      (lastOutput.empty() ? std::string("none") : lastOutput) +
      "\nSettings > Plugins: preferredFormat, errorCorrection, "
      "pixelsPerModule, quietZone, outputDirectory, overwriteExisting."
      "\nNo image preview widget is exposed by the current plugin SDK; use "
      "Open last generated image."
      "\nSingle QR image capacity is limited. Text is never uploaded or "
      "truncated.";
  ui->read_only_text(ui->context, status.c_str());
}
} // namespace

extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI *host,
                                                     DodevPluginAPI *plugin) {
  if (!host || !plugin || host->api_version != DODEV_PLUGIN_API_VERSION ||
      host->struct_size < offsetof(DodevHostAPI, subscribe_event) +
                              sizeof(host->subscribe_event) ||
      plugin->struct_size < sizeof(DodevPluginAPI) || !host->register_command ||
      !host->register_menu || !host->register_setting || !host->register_tab)
    return 0;
  plugin->api_version = DODEV_PLUGIN_API_VERSION;
  plugin->shutdown = nullptr;
  if (!host->register_setting(host->context, "preferredFormat", "png",
                              "QR output format: png or svg"))
    return 0;
  if (!host->register_setting(
          host->context, "errorCorrection", "medium",
          "Error correction: low, medium, quartile or high"))
    return 0;
  if (!host->register_setting(host->context, "pixelsPerModule", "8",
                              "PNG scale from 2 to 32 pixels per module"))
    return 0;
  if (!host->register_setting(host->context, "quietZone", "4",
                              "White border: 4 to 16 QR modules"))
    return 0;
  if (!host->register_setting(
          host->context, "outputDirectory", "",
          "Optional default export directory; empty uses document folder"))
    return 0;
  if (!host->register_setting(
          host->context, "overwriteExisting", "false",
          "Allow replacing existing QR image files (true/false)"))
    return 0;
  if (!host->register_command(host->context, "qrcode.generate",
                              "QR Code: Generate from Active Editor...",
                              &onPreferred, nullptr))
    return 0;
  if (!host->register_command(host->context, "qrcode.png",
                              "QR Code: Export PNG...", &onPng, nullptr))
    return 0;
  if (!host->register_command(host->context, "qrcode.svg",
                              "QR Code: Export SVG...", &onSvg, nullptr))
    return 0;
  if (!host->register_command(host->context, "qrcode.open_last",
                              "QR Code: Open Last Image", &openLast, nullptr))
    return 0;
  if (!host->register_command(host->context, "qrcode.preferred_format",
                              "QR Code: Configure Format...", &configureFormat,
                              nullptr))
    return 0;
  host->register_menu(host->context, "Tools",
                      "QR Code: Generate from Editor...", "qrcode.generate");
  host->register_menu(host->context, "Tools", "QR Code: Save PNG...",
                      "qrcode.png");
  host->register_menu(host->context, "Tools", "QR Code: Save SVG...",
                      "qrcode.svg");
  host->register_menu(host->context, "Tools", "QR Code: View Last Image",
                      "qrcode.open_last");
  host->register_tab(host->context, "qrcode.generator", "QR Code Generator",
                     &panel, nullptr);
  return 1;
}
