#include "dodev/settings/SettingsManager.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <tuple>
namespace dodev {
namespace {
files::path envPath(const char* key) {
  const char* value = std::getenv(key);
  return value && *value ? files::u8path(value) : files::path{};
}
std::string readFile(const files::path& file) {
  std::ifstream input(file, std::ios::binary);
  if (!input) return {};
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}
int clampInt(int n, int minimum, int maximum) {
  return std::clamp(n, minimum, maximum);
}
}  // namespace
SettingsManager::SettingsManager(files::path root)
    : root_(root.empty() ? defaultDirectory() : std::move(root)),
      data_(defaultSettings()) {}
files::path SettingsManager::defaultDirectory() {
  auto override = envPath("DODEV_CONFIG_DIR");
  if (!override.empty()) return override;
#ifdef _WIN32
  auto base = envPath("APPDATA");
  if (base.empty()) base = envPath("USERPROFILE");
  return base.empty() ? files::current_path() / "DoDevEditor"
                      : base / "DoDevEditor";
#elif defined(__APPLE__)
  auto base = envPath("HOME");
  return base.empty()
             ? files::current_path() / "DoDevEditor"
             : base / "Library" / "Application Support" / "DoDevEditor";
#else
  auto base = envPath("XDG_CONFIG_HOME");
  if (base.empty()) {
    base = envPath("HOME");
    if (!base.empty()) base /= ".config";
  }
  return base.empty() ? files::current_path() / "DoDevEditor"
                      : base / "DoDevEditor";
#endif
}
Json SettingsManager::defaultSettings() {
  Json base;
  std::string error;
  const std::string s = R"({
      "schemaVersion":1,
      "general":{"language":"en","restoreSession":true,"autoSave":false,"lastFolder":""},
      "editor":{"fontFamily":"monospace","fontFile":"","fontSize":14,"tabSize":4,"insertSpaces":true,"wordWrap":false,"lineNumbers":true},
      "appearance":{"theme":"Music Nocturne","uiFontFile":"","uiFontSize":13,"customThemesDirectory":"","preferredThemeFormat":"json"},
      "syntaxHighlighting":{"global":{},"themes":{}},
      "terminal":{"fontFamily":"monospace","fontFile":"","fontSize":14,"defaultShell":"","scrollbackLines":3000},
      "templates":{"directory":"","associations":{".cpp":"cpp17",".hpp":"cpp-header",".h":"cpp-header",".py":"python",".json":"json",".xml":"xml"}},
      "plugins":{"enabled":true,"directory":"","safeMode":false,"enabledIds":[],"disabled":[]}
    })";
  Json::parse(s, base, &error);
  return base;
}
const Json& SettingsManager::get(const std::string& dotted) const {
  const Json* cur = &data_;
  size_t pos = 0;
  do {
    const size_t end = dotted.find('.', pos);
    const auto key =
        dotted.substr(pos, end == std::string::npos ? end : end - pos);
    cur = cur->find(key);
    if (!cur) {
      static const Json null;
      return null;
    }
    if (end == std::string::npos) break;
    pos = end + 1;
  } while (pos < dotted.size());
  return *cur;
}
void SettingsManager::set(const std::string& dotted, Json value) {
  size_t start = 0;
  Json* node = &data_;
  for (;;) {
    const auto end = dotted.find('.', start);
    auto key =
        dotted.substr(start, end == std::string::npos ? end : end - start);
    if (key.empty()) return;
    if (end == std::string::npos) {
      (*node)[key] = std::move(value);
      return;
    }
    node = &(*node)[key];
    start = end + 1;
  }
}
std::string SettingsManager::getString(const std::string& key,
                                       const std::string& fallback) const {
  return get(key).string(fallback);
}
int SettingsManager::getInt(const std::string& key, int fallback) const {
  return get(key).integer(fallback);
}
bool SettingsManager::getBool(const std::string& key, bool fallback) const {
  return get(key).boolean(fallback);
}
void SettingsManager::tryLegacyIni() {
  const auto ini = root_ / "settings.ini";
  if (!files::exists(ini)) return;
  std::istringstream input(readFile(ini));
  std::string line;
  bool changed = false;
  while (std::getline(input, line)) {
    if (line.rfind("theme=", 0) == 0) {
      set("appearance.theme", line.substr(6));
      changed = true;
    } else if (line.rfind("folder=", 0) == 0) {
      set("general.lastFolder", line.substr(7));
      changed = true;
    }
  }
  if (changed)
    diagnostics_.push_back(
        "Imported legacy settings.ini into memory; next "
        "save writes settings.json");
}
void SettingsManager::validate() {
  const auto baseline = defaultSettings();
  const std::vector<std::string> strings = {"general.language",
                                            "general.lastFolder",
                                            "editor.fontFamily",
                                            "editor.fontFile",
                                            "appearance.theme",
                                            "appearance.uiFontFile",
                                            "appearance.customThemesDirectory",
                                            "appearance.preferredThemeFormat",
                                            "terminal.fontFamily",
                                            "terminal.fontFile",
                                            "terminal.defaultShell",
                                            "templates.directory",
                                            "plugins.directory"};
  for (const auto& key : strings)
    if (!get(key).isString()) {
      const auto pos = key.find('.');
      set(key, baseline[key.substr(0, pos)][key.substr(pos + 1)]);
      diagnostics_.push_back("Invalid string setting: " + key);
    }
  const std::vector<std::string> booleans = {
      "general.restoreSession", "general.autoSave",   "editor.insertSpaces",
      "editor.wordWrap",        "editor.lineNumbers", "plugins.enabled",
      "plugins.safeMode"};
  for (const auto& key : booleans)
    if (!get(key).isBool()) {
      const auto pos = key.find('.');
      set(key, baseline[key.substr(0, pos)][key.substr(pos + 1)]);
      diagnostics_.push_back("Invalid boolean setting: " + key);
    }
  for (const auto& row : std::vector<std::tuple<std::string, int, int>>{
           {"editor.fontSize", 8, 64},
           {"editor.tabSize", 1, 16},
           {"appearance.uiFontSize", 9, 36},
           {"terminal.fontSize", 8, 48},
           {"terminal.scrollbackLines", 100, 200000}}) {
    const auto& key = std::get<0>(row);
    const auto pos = key.find('.');
    const int defaultValue =
        baseline[key.substr(0, pos)][key.substr(pos + 1)].integer();
    if (!get(key).isNumber()) {
      set(key, defaultValue);
      diagnostics_.push_back("Invalid numeric setting: " + key);
    } else
      set(key, clampInt(getInt(key), std::get<1>(row), std::get<2>(row)));
  }
  if (getInt("schemaVersion") != 1) {
    diagnostics_.push_back(
        "Unsupported schemaVersion; compatible keys loaded "
        "using schema 1 defaults");
    set("schemaVersion", 1);
  }
  if (!get("templates.associations").isObject())
    set("templates.associations", baseline["templates"]["associations"]);
  for (const char* path : {"plugins.enabledIds", "plugins.disabled"})
    if (!get(path).isArray()) set(path, Json::array());
  if (getString("appearance.preferredThemeFormat") != "json" &&
      getString("appearance.preferredThemeFormat") != "xml")
    set("appearance.preferredThemeFormat", "json");
}
bool SettingsManager::load() {
  diagnostics_.clear();
  malformed_ = false;
  data_ = defaultSettings();
  std::error_code ec;
  if (!files::exists(file(), ec)) {
    tryLegacyIni();
    validate();
    return true;
  }
  const auto raw = readFile(file());
  Json parsed;
  std::string error;
  if (raw.size() > 2 * 1024 * 1024 || !Json::parse(raw, parsed, &error) ||
      !parsed.isObject()) {
    malformed_ = true;
    diagnostics_.push_back("Malformed settings.json kept untouched (" + error +
                           "); using in-memory defaults");
    return false;
  }
  data_.merge(parsed);
  validate();
  return true;
}
bool SettingsManager::writeAtomic(const files::path& destination,
                                  const std::string& contents,
                                  std::string* error) {
  try {
    files::create_directories(destination.parent_path());
    const auto temp = files::path(destination.string() + ".tmp");
    {
      std::ofstream out(temp, std::ios::binary | std::ios::trunc);
      if (!out) throw std::runtime_error("open failed");
      out << contents;
      out.flush();
      if (!out) throw std::runtime_error("write failed");
    }
#ifdef _WIN32
    // Windows std::filesystem::rename cannot overwrite an existing file.
    if (files::exists(destination)) files::remove(destination);
#endif
    files::rename(temp, destination);
    if (error) error->clear();
    return true;
  } catch (const std::exception& e) {
    if (error) *error = e.what();
    return false;
  }
}
bool SettingsManager::save(std::string* error) {
  if (malformed_) {
    if (error)
      *error =
          "Refusing to overwrite malformed settings.json; repair or "
          "rename it first";
    return false;
  }
  return writeAtomic(file(), data_.dump(2) + "\n", error);
}
bool SettingsManager::validPluginId(const std::string& id) {
  if (id.empty() || id.size() > 128) return false;
  for (char c : id)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
      return false;
  return id != "." && id != "..";
}
Json SettingsManager::loadPluginSettings(const std::string& id) {
  if (!validPluginId(id)) {
    diagnostics_.push_back("Bad plugin ID: " + id);
    return Json::object();
  }
  const auto p = root_ / "plugins" / id / "settings.json";
  if (!files::exists(p)) return Json::object();
  Json j;
  std::string error;
  if (!Json::parse(readFile(p), j, &error) || !j.isObject()) {
    diagnostics_.push_back("Plugin settings invalid: " + id + " (" + error +
                           ")");
    return Json::object();
  }
  return j;
}
bool SettingsManager::savePluginSettings(const std::string& id,
                                         const Json& object,
                                         std::string* error) {
  if (!validPluginId(id) || !object.isObject()) {
    if (error) *error = "Invalid plugin ID or settings object";
    return false;
  }
  return writeAtomic(root_ / "plugins" / id / "settings.json",
                     object.dump(2) + "\n", error);
}
}  // namespace dodev
