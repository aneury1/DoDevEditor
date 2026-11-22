#include "dodev/plugins/PluginManager.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace dodev {
namespace {
std::string fileContents(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  return in ? std::string(std::istreambuf_iterator<char>(in),
                          std::istreambuf_iterator<char>())
            : "";
}
bool simpleId(const std::string& id) {
  return SettingsManager::validPluginId(id);
}
bool safeLibraryPath(const std::string& name) {
  if (name.empty() || name.size() > 256) return false;
  std::filesystem::path p(name);
  if (p.is_absolute()) return false;
  for (const auto& component : p)
    if (component == ".." || component == ".") return false;
  return true;
}
#ifdef _WIN32
using Library = HMODULE;
Library openLibrary(const std::filesystem::path& path) {
  return LoadLibraryW(path.wstring().c_str());
}
void* symbol(Library handle, const char* name) {
  return reinterpret_cast<void*>(GetProcAddress(handle, name));
}
void closeLibrary(Library h) {
  if (h) FreeLibrary(h);
}
std::string dynamicError() {
  return "LoadLibrary/GetProcAddress failed: " + std::to_string(GetLastError());
}
#else
using Library = void*;
Library openLibrary(const std::filesystem::path& path) {
  return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
}
void* symbol(Library handle, const char* name) { return dlsym(handle, name); }
void closeLibrary(Library h) {
  if (h) dlclose(h);
}
std::string dynamicError() {
  const char* e = dlerror();
  return e ? e : "dlopen failed";
}
#endif
std::string nativeKey() {
#ifdef _WIN32
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#else
  return "linux";
#endif
}
}  // namespace
struct PluginManager::Instance {
  PluginManager* owner = nullptr;
  PluginManifest manifest;
  Library library = nullptr;
  DodevHostAPI host{};
  DodevPluginAPI plugin{};
  Json settings = Json::object();
  std::string scratch;
  std::string scratchPath, scratchContents;
  ~Instance() {
    if (plugin.shutdown) {
      try {
        plugin.shutdown();
      } catch (...) {
      }
    }
    closeLibrary(library);
  }
};
PluginManager::PluginManager(SettingsManager& s, TemplateRegistry& t,
                             ThemeManager& themes)
    : settings_(s), templates_(t), themes_(themes) {}
PluginManager::~PluginManager() { shutdown(); }
void PluginManager::shutdown() {
  // Callbacks and user data must never be called after their module is
  // unloaded.
  subscriptions_.clear();
  views_.clear();
  commands_.clear();
  menus_.clear();
  settingsSpecs_.clear();
  instances_.clear();
}
PluginManager::Instance* PluginManager::instance(void* context) {
  return static_cast<Instance*>(context);
}
bool PluginManager::readManifest(const std::filesystem::path& folder,
                                 PluginManifest& out, std::string* error) {
  const auto file = folder / "plugin.json";
  std::error_code ec;
  if (!std::filesystem::is_regular_file(file, ec)) {
    if (error) *error = "Missing plugin.json";
    return false;
  }
  if (std::filesystem::file_size(file, ec) > 128 * 1024) {
    if (error) *error = "Manifest too large";
    return false;
  }
  Json data;
  std::string parseErr;
  if (!Json::parse(fileContents(file), data, &parseErr) || !data.isObject()) {
    if (error) *error = "Invalid plugin manifest: " + parseErr;
    return false;
  }
  PluginManifest manifest;
  manifest.id = data["id"].string();
  manifest.name = data["name"].string(manifest.id);
  manifest.version = data["version"].string("0.0.0");
  manifest.description = data["description"].string();
  manifest.apiVersion = data["apiVersion"].integer();
  manifest.library =
      data["entry"][nativeKey()].string(data["library"].string());
  manifest.basePath = folder;
  if (!simpleId(manifest.id) ||
      manifest.apiVersion != static_cast<int>(DODEV_PLUGIN_API_VERSION) ||
      !safeLibraryPath(manifest.library)) {
    if (error) *error = "Invalid plugin id, API version or native library path";
    return false;
  }
  out = std::move(manifest);
  return true;
}
bool PluginManager::isEnabled(const std::string& id) const {
  if (!settings_.getBool("plugins.enabled", true) ||
      settings_.getBool("plugins.safeMode", false))
    return false;
  for (const auto& blocked : settings_.get("plugins.disabled").asArray())
    if (blocked.string() == id) return false;
  for (const auto& allowed : settings_.get("plugins.enabledIds").asArray())
    if (allowed.string() == id) return true;
  return false;  // native plugins require explicit opt-in
}
void PluginManager::setEnabled(const std::string& id, bool enabled) {
  if (!simpleId(id)) return;
  Json::Array list;
  for (const auto& item : settings_.get("plugins.enabledIds").asArray())
    if (item.isString() && item.string() != id)
      list.emplace_back(item.string());
  if (enabled) list.emplace_back(id);
  settings_.set("plugins.enabledIds", Json(std::move(list)));
  Json::Array denied;
  for (const auto& item : settings_.get("plugins.disabled").asArray())
    if (item.string() != id) denied.push_back(item);
  settings_.set("plugins.disabled", Json(std::move(denied)));
}
size_t PluginManager::discoverAndLoad(const std::filesystem::path& dir) {
  records_.clear();
  diagnostics_.clear();
  std::error_code ec;
  if (dir.empty() || !std::filesystem::is_directory(dir, ec)) return 0;
  size_t count = 0;
  for (const auto& path : std::filesystem::directory_iterator(dir, ec)) {
    if (!path.is_directory()) continue;
    if (!std::filesystem::exists(path.path() / "plugin.json")) continue;
    PluginManifest manifest;
    std::string error;
    PluginRecord record;
    if (!readManifest(path.path(), manifest, &error)) {
      record.manifest.id = path.path().filename().string();
      record.manifest.name = record.manifest.id;
      record.status = "Invalid: " + error;
      diagnostics_.push_back(record.manifest.id + ": " + error);
      records_.push_back(std::move(record));
      continue;
    }
    record.manifest = manifest;
    record.enabled = isEnabled(manifest.id);
    if (!record.enabled)
      record.status = "Disabled (enable in Settings; restart to load)";
    else if (activate(manifest, &error)) {
      record.status = "Active";
      ++count;
    } else {
      record.status = "Failed: " + error;
      diagnostics_.push_back(manifest.id + ": " + error);
    }
    records_.push_back(std::move(record));
  }
  return count;
}
bool PluginManager::activate(PluginManifest manifest, std::string* error) {
  for (const auto& entry : instances_)
    if (entry->manifest.id == manifest.id) {
      if (error) *error = "Duplicate plugin id";
      return false;
    }
  const auto libraryFile = manifest.basePath / manifest.library;
  if (!std::filesystem::is_regular_file(libraryFile)) {
    if (error) *error = "Library missing: " + libraryFile.string();
    return false;
  }
  auto entry = std::make_unique<Instance>();
  entry->owner = this;
  entry->manifest = std::move(manifest);
  entry->settings = settings_.loadPluginSettings(entry->manifest.id);
  entry->library = openLibrary(libraryFile);
  if (!entry->library) {
    if (error) *error = dynamicError();
    return false;
  }
  auto init = reinterpret_cast<DodevPluginInitFn>(
      symbol(entry->library, "dodev_plugin_init"));
  if (!init) {
    if (error) *error = "Missing dodev_plugin_init symbol";
    return false;
  }
  entry->host = {sizeof(DodevHostAPI),
                 DODEV_PLUGIN_API_VERSION,
                 entry.get(),
                 &hostLog,
                 &hostCommand,
                 &hostMenu,
                 &hostTab,
                 &hostPanel,
                 &hostTemplate,
                 &hostTheme,
                 &hostSetting,
                 &hostGetSetting,
                 &hostSetSetting,
                 &hostEvent,
                 &hostActiveDocument,
                 &hostReplaceDocument,
                 &hostTerminalCommand,
                 &hostPromptInput};
  entry->plugin = {sizeof(DodevPluginAPI), DODEV_PLUGIN_API_VERSION, nullptr};
  const size_t commandsBefore = commands_.size(), menusBefore = menus_.size(),
               viewsBefore = views_.size(),
               settingsBefore = settingsSpecs_.size(),
               eventsBefore = subscriptions_.size();
  int ok = 0;
  try {
    ok = init(&entry->host, &entry->plugin);
  } catch (...) {
    ok = 0;
  }
  if (!ok || entry->plugin.api_version != DODEV_PLUGIN_API_VERSION) {
    commands_.resize(commandsBefore);
    menus_.resize(menusBefore);
    views_.resize(viewsBefore);
    settingsSpecs_.resize(settingsBefore);
    subscriptions_.resize(eventsBefore);
    if (error) *error = "Plugin initialization failed or API version mismatch";
    return false;
  }
  instances_.push_back(std::move(entry));
  return true;
}
void PluginManager::hostLog(void* context, int severity, const char* message) {
  auto* plugin = instance(context);
  std::string line =
      "[plugin " + plugin->manifest.id + "] " + (message ? message : "");
  plugin->owner->diagnostics_.push_back(line);
  if (severity >= 2) std::cerr << line << '\n';
}
int PluginManager::hostCommand(void* ctx, const char* id, const char* title,
                               DodevCommandFn cb, void* data) {
  auto* p = instance(ctx);
  if (!id || !simpleId(id) || !cb) return 0;
  for (const auto& existing : p->owner->commands_)
    if (existing.id == id) return 0;
  p->owner->commands_.push_back(
      {id, title ? title : id, p->manifest.id, cb, data});
  return 1;
}
int PluginManager::hostMenu(void* ctx, const char* location, const char* title,
                            const char* command) {
  auto* p = instance(ctx);
  if (!title || !*title || !command || !*command) return 0;
  const std::string loc = location ? location : "Plugins";
  if (loc != "File" && loc != "Edit" && loc != "View" && loc != "Tools" &&
      loc != "Plugins")
    return 0;
  p->owner->menus_.push_back({loc, title, command, p->manifest.id});
  return 1;
}
int PluginManager::hostTab(void* ctx, const char* id, const char* title,
                           DodevViewFn factory, void* data) {
  auto* p = instance(ctx);
  if (!id || !simpleId(id) || !title) return 0;
  for (const auto& v : p->owner->views_)
    if (v.id == id) return 0;
  p->owner->views_.push_back(
      {id, title, p->manifest.id, "", false, factory, data});
  return 1;
}
int PluginManager::hostPanel(void* ctx, const char* id, const char* title,
                             const char* side, DodevViewFn factory,
                             void* data) {
  auto* p = instance(ctx);
  if (!id || !simpleId(id) || !title) return 0;
  const std::string loc = side ? side : "Right";
  if (loc != "Left" && loc != "Right" && loc != "Bottom" && loc != "Tab")
    return 0;
  for (const auto& v : p->owner->views_)
    if (v.id == id) return 0;
  p->owner->views_.push_back(
      {id, title, p->manifest.id, loc, true, factory, data});
  return 1;
}
int PluginManager::hostTemplate(void* ctx, const char* id, const char* name,
                                const char* ext, const char* contents) {
  auto* p = instance(ctx);
  if (!id || !name || !ext || !contents) return 0;
  p->owner->templates_.registerTemplate(
      {id, name, ext, contents, p->manifest.id});
  return 1;
}
int PluginManager::hostTheme(void* ctx, const char* xml) {
  auto* p = instance(ctx);
  if (!xml) return 0;
  ThemeDefinition theme;
  std::string error;
  if (!ThemeDefinition::fromXml(xml, theme, &error)) return 0;
  theme.source = "plugin:" + p->manifest.id;
  p->owner->themes_.add(std::move(theme));
  return 1;
}
int PluginManager::hostSetting(void* ctx, const char* key, const char* def,
                               const char* desc) {
  auto* p = instance(ctx);
  if (!key || !simpleId(key) || !def) return 0;
  for (const auto& setting : p->owner->settingsSpecs_)
    if (setting.owner == p->manifest.id && setting.key == key) return 0;
  p->owner->settingsSpecs_.push_back(
      {p->manifest.id, key, desc ? desc : "", def});
  if (!p->settings.find(key)) p->settings[key] = def;
  return 1;
}
const char* PluginManager::hostGetSetting(void* ctx, const char* key) {
  auto* p = instance(ctx);
  if (!key || !simpleId(key)) return "";
  p->scratch = p->settings[key].string();
  return p->scratch.c_str();
}
int PluginManager::hostSetSetting(void* ctx, const char* key,
                                  const char* value) {
  auto* p = instance(ctx);
  if (!key || !simpleId(key) || !value) return 0;
  p->settings[key] = value;
  std::string error;
  return p->owner->settings_.savePluginSettings(p->manifest.id, p->settings,
                                                &error)
             ? 1
             : 0;
}
int PluginManager::hostEvent(void* ctx, const char* name, DodevEventFn cb,
                             void* data) {
  auto* p = instance(ctx);
  if (!name || !simpleId(name) || !cb) return 0;
  p->owner->subscriptions_.push_back({p, name, cb, data});
  return 1;
}
int PluginManager::hostActiveDocument(void* ctx, const char** path,
                                      const char** contents) {
  auto* p = instance(ctx);
  if (!path || !contents || !p->owner->hooks_.activeDocument) return 0;
  p->scratchPath.clear();
  p->scratchContents.clear();
  if (!p->owner->hooks_.activeDocument(p->scratchPath, p->scratchContents))
    return 0;
  *path = p->scratchPath.c_str();
  *contents = p->scratchContents.c_str();
  return 1;
}
int PluginManager::hostReplaceDocument(void* ctx, const char* contents) {
  auto* p = instance(ctx);
  return contents && p->owner->hooks_.replaceDocument &&
                 p->owner->hooks_.replaceDocument(contents)
             ? 1
             : 0;
}
int PluginManager::hostTerminalCommand(void* ctx, const char* line) {
  auto* p = instance(ctx);
  return line && p->owner->hooks_.terminalCommand &&
                 p->owner->hooks_.terminalCommand(line)
             ? 1
             : 0;
}
int PluginManager::hostPromptInput(void* ctx, const char* title,
                                   const char* label, const char* initial,
                                   DodevPromptFn callback, void* user_data) {
  auto* p = instance(ctx);
  if (!callback || !p->owner->hooks_.promptInput) return 0;
  const DodevHostAPI* host = &p->host;
  return p->owner->hooks_.promptInput(
             title ? title : "Plugin input", label ? label : "Enter value",
             initial ? initial : "",
             [host, callback, user_data](const std::string& value) {
               callback(host, value.c_str(), user_data);
             })
             ? 1
             : 0;
}
bool PluginManager::execute(const std::string& id) {
  for (const auto& c : commands_)
    if (c.id == id) {
      for (const auto& p : instances_)
        if (p->manifest.id == c.owner) {
          try {
            c.callback(&p->host, c.userData);
            return true;
          } catch (...) {
            diagnostics_.push_back("Plugin command crashed with exception: " +
                                   id);
            return false;
          }
        }
    }
  return false;
}
void PluginManager::buildView(const PluginView& view,
                              const DodevUiBuilder& builder) {
  if (!view.factory) return;
  for (const auto& plugin : instances_)
    if (plugin->manifest.id == view.owner) {
      try {
        view.factory(&plugin->host, &builder, view.userData);
      } catch (...) {
        diagnostics_.push_back("Exception in plugin view: " + view.id);
      }
      return;
    }
}
void PluginManager::emit(const std::string& event, const std::string& payload) {
  for (const auto& s : subscriptions_)
    if (s.event == event) {
      try {
        s.callback(&s.owner->host, event.c_str(), payload.c_str(), s.userData);
      } catch (...) {
        diagnostics_.push_back("Exception in event: " + event);
      }
    }
}
std::string PluginManager::pluginSetting(const std::string& owner,
                                         const std::string& key) const {
  for (const auto& p : instances_)
    if (p->manifest.id == owner) return p->settings[key].string();
  return {};
}
bool PluginManager::updatePluginSetting(const std::string& owner,
                                        const std::string& key,
                                        const std::string& value) {
  if (!simpleId(owner) || !simpleId(key)) return false;
  for (const auto& p : instances_)
    if (p->manifest.id == owner) {
      p->settings[key] = value;
      return settings_.savePluginSettings(owner, p->settings);
    }
  Json j = settings_.loadPluginSettings(owner);
  j[key] = value;
  return settings_.savePluginSettings(owner, j);
}
}  // namespace dodev
