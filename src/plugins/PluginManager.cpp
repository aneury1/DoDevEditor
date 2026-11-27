#include "dodev/plugins/PluginManager.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
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
  bool migratedSettings = false;
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
  previews_.clear();
  subscriptions_.clear();
  views_.clear();
  commands_.clear();
  menus_.clear();
  settingsSpecs_.clear();
  settingsActions_.clear();
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
  return discoverAndLoad(std::vector<std::filesystem::path>{dir});
}
size_t PluginManager::discoverAndLoad(
    const std::vector<std::filesystem::path>& directories) {
  // Directories are in priority order (user override, then installed bundle).
  // Never clear plugin registrations between roots; duplicate IDs are skipped.
  shutdown();  // A rescan must never retain stale plugin handles or duplicate
               // IDs.
  records_.clear();
  diagnostics_.clear();
  std::set<std::string> searchedRoots;
  std::set<std::string> seenIds;
  size_t count = 0;
  for (const auto& dir : directories) {
    if (dir.empty()) continue;
    std::error_code ec;
    const auto canonical = std::filesystem::weakly_canonical(dir, ec);
    const auto key = (ec ? dir.lexically_normal() : canonical).u8string();
    if (!searchedRoots.insert(key).second) continue;
    std::clog << "[DoDevEditor][Plugins] Searching: " << dir.string() << '\n';
    if (!std::filesystem::is_directory(dir, ec)) {
      std::clog << "[DoDevEditor][Plugins] Directory not present (optional): "
                << dir.string() << '\n';
      continue;
    }
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
        std::cerr << "[DoDevEditor][Plugins] Invalid manifest "
                  << path.path().string() << ": " << error << '\n';
        records_.push_back(std::move(record));
        continue;
      }
      if (!seenIds.insert(manifest.id).second) {
        std::clog << "[DoDevEditor][Plugins] Skipping duplicate " << manifest.id
                  << " in " << manifest.basePath.string() << '\n';
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
      std::clog << "[DoDevEditor][Plugins] " << manifest.id << " : "
                << record.status << " | manifest="
                << (manifest.basePath / "plugin.json").string() << " | module="
                << (manifest.basePath / manifest.library).string() << '\n';
      records_.push_back(std::move(record));
    }
    if (ec) {
      const std::string message = "Error searching plugin directory " +
                                  dir.string() + ": " + ec.message();
      diagnostics_.push_back(message);
      std::cerr << "[DoDevEditor][Plugins] " << message << '\n';
    }
  }
  std::clog << "[DoDevEditor][Plugins] Activated " << count
            << " plugin(s) from " << searchedRoots.size() << " location(s)\n";
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
                 &hostPromptInput,
                 &hostOpenSerialMonitor,
                 &hostOpenSourceSymbols,
                 &hostOpenRemoteJournal,
                 &hostOpenAiAssistant,
                 &hostDebugContext,
                 &hostDebugLaunch,
                 &hostDebugControl,
                 &hostDebugOpenFile,
                 &hostDebugStatus,
                 &hostSettingEx,
                 &hostSettingsAction,
                 &hostWorkspaceConfig,
                 &hostPromptSecret,
                 &hostShowTextView,
                 &hostSelectPath,
                 &hostRunJob,
                 &hostShowHtmlView,
                 &hostRegisterPreview,
                 &hostImageView,
                 &hostTreeView,
                 &hostMediaView};
  entry->plugin = {sizeof(DodevPluginAPI), DODEV_PLUGIN_API_VERSION, nullptr};
  const size_t commandsBefore = commands_.size(), menusBefore = menus_.size(),
               viewsBefore = views_.size(),
               settingsBefore = settingsSpecs_.size(),
               eventsBefore = subscriptions_.size(),
               actionsBefore = settingsActions_.size(),
               previewsBefore = previews_.size();
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
    settingsActions_.resize(actionsBefore);
    previews_.resize(previewsBefore);
    if (error) *error = "Plugin initialization failed or API version mismatch";
    return false;
  }
  if (entry->migratedSettings &&
      !settings_.savePluginSettings(entry->manifest.id, entry->settings))
    diagnostics_.push_back("Could not persist migrated settings for " +
                           entry->manifest.id);
  instances_.push_back(std::move(entry));
  return true;
}
void PluginManager::hostLog(void* context, int severity, const char* message) {
  auto* plugin = instance(context);
  std::string line =
      "[plugin " + plugin->manifest.id + "] " + (message ? message : "");
  plugin->owner->diagnostics_.push_back(line);
  if (severity >= 2)
    std::cerr << line << '\n';
  else
    std::clog << line << '\n';
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
      loc != "Plugins" && loc != "Run" && loc != "Options")
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
  if (loc != "Left" && loc != "Right" && loc != "Bottom" && loc != "Tab" &&
      loc != "Workspace" && loc != "Editor" && loc != "Output")
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
bool PluginSetting::accepts(const std::string& value) const {
  if (type == "bool") return value == "true" || value == "false";
  if (type == "choice")
    return std::any_of(choices.begin(), choices.end(),
                       [&](const auto& c) { return c.first == value; });
  if (type == "int") {
    int n = 0;
    auto r = std::from_chars(value.data(), value.data() + value.size(), n);
    return r.ec == std::errc{} && r.ptr == value.data() + value.size() &&
           n >= minimum && n <= maximum;
  }
  return true;
}
int PluginManager::hostSetting(void* ctx, const char* key, const char* def,
                               const char* desc) {
  return hostSettingEx(ctx, key, def, desc, "{}");
}
int PluginManager::hostSettingEx(void* ctx, const char* key, const char* def,
                                 const char* desc, const char* schema) {
  auto* p = instance(ctx);
  if (!key || !simpleId(key) || !def || !schema) return 0;
  for (const auto& setting : p->owner->settingsSpecs_)
    if (setting.owner == p->manifest.id && setting.key == key) return 0;
  Json j;
  if (!Json::parse(schema, j) || !j.isObject()) return 0;
  PluginSetting sp;
  sp.owner = p->manifest.id;
  sp.key = key;
  sp.description = desc ? desc : "";
  sp.defaultValue = def;
  sp.label = j["label"].string(key);
  sp.type = j["type"].string("text");
  if (sp.type != "text" && sp.type != "multiline" && sp.type != "bool" &&
      sp.type != "int" && sp.type != "choice")
    return 0;
  sp.minimum = j["min"].integer(0);
  sp.maximum = j["max"].integer(100);
  if (sp.minimum > sp.maximum) return 0;
  for (const auto& c : j["choices"].asArray()) {
    auto value = c.isString() ? c.string() : c["value"].string();
    auto label = c.isString() ? value : c["label"].string(value);
    if ((!c.isString() && !c["value"].isString()) ||
        std::any_of(sp.choices.begin(), sp.choices.end(),
                    [&](const auto& x) { return x.first == value; }))
      return 0;
    sp.choices.emplace_back(value, label);
  }
  if (!sp.accepts(def)) return 0;
  const auto* stored = p->settings.find(key);
  if (!stored) {
    auto legacy = j["legacyKey"].string();
    auto value = legacy.empty() ? std::string(def)
                                : p->owner->settings_.getString(legacy, def);
    p->settings[key] = sp.accepts(value) ? value : std::string(def);
    if (!legacy.empty() && !p->owner->settings_.get(legacy).isString())
      legacy.clear();
    if (!legacy.empty()) p->migratedSettings = true;
  } else if (!stored->isString() || !sp.accepts(stored->string()))
    p->settings[key] = def;
  p->owner->settingsSpecs_.push_back(std::move(sp));
  return 1;
}
int PluginManager::hostSettingsAction(void* ctx, const char* title,
                                      const char* command) {
  auto* p = instance(ctx);
  if (!title || !*title || !command) return 0;
  for (const auto& c : p->owner->commands_)
    if (c.owner == p->manifest.id && c.id == command) {
      p->owner->settingsActions_.push_back({p->manifest.id, title, command});
      return 1;
    }
  return 0;
}
const char* PluginManager::hostGetSetting(void* ctx, const char* key) {
  auto* p = instance(ctx);
  if (!key || !simpleId(key)) return "";
  auto* value = p->settings.find(key);
  p->scratch = value ? value->string() : std::string{};
  return p->scratch.c_str();
}
int PluginManager::hostSetSetting(void* ctx, const char* key,
                                  const char* value) {
  auto* p = instance(ctx);
  if (!key || !simpleId(key) || !value) return 0;
  for (const auto& sp : p->owner->settingsSpecs_)
    if (sp.owner == p->manifest.id && sp.key == key && !sp.accepts(value))
      return 0;
  auto revised = p->settings;
  revised[key] = value;
  if (!p->owner->settings_.savePluginSettings(p->manifest.id, revised))
    return 0;
  p->settings = std::move(revised);
  return 1;
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
int PluginManager::hostWorkspaceConfig(void* ctx, const char* json) {
  auto* p = instance(ctx);
  auto& hook = p->owner->hooks_.openWorkspaceConfig;
  return json && hook && hook(p->manifest.id, json) ? 1 : 0;
}
int PluginManager::hostPromptSecret(void* ctx, const char* title,
                                    const char* label, DodevPromptFn callback,
                                    void* data) {
  auto* p = instance(ctx);
  auto& hook = p->owner->hooks_.promptSecret;
  return callback && hook &&
                 hook(title ? title : "Password", label ? label : "Password",
                      [p, callback, data](const std::string& value) {
                        callback(&p->host, value.c_str(), data);
                      })
             ? 1
             : 0;
}
int PluginManager::hostShowTextView(void* ctx, const char* id,
                                    const char* title, const char* side,
                                    const char* text) {
  auto* p = instance(ctx);
  auto& hook = p->owner->hooks_.showTextView;
  return id && simpleId(id) && title && side && text && hook &&
                 hook(p->manifest.id + ":" + id, title, side, text)
             ? 1
             : 0;
}
int PluginManager::hostShowHtmlView(void* ctx, const char* id,
                                    const char* title, const char* side,
                                    const char* html) {
  auto* p = instance(ctx);
  auto& hook = p->owner->hooks_.showHtmlView;
  return id && simpleId(id) && title && side && html && hook &&
                 hook(p->manifest.id + ":" + id, title, side, html)
             ? 1
             : 0;
}
static std::string previewExtension(std::string ext) {
  for (auto& c : ext)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return ext;
}
int PluginManager::hostRegisterPreview(void* ctx, const char* extensions,
                                       DodevFilePreviewFn fn, void* data) {
  auto* p = instance(ctx);
  if (!extensions || !fn) return 0;
  std::istringstream input(extensions);
  std::string ext;
  std::vector<std::string> valid;
  while (std::getline(input, ext, ',')) {
    ext = previewExtension(ext);
    if (ext != "*" &&
        (ext.size() < 2 || ext.size() > 32 || ext.front() != '.' ||
         ext.find_first_not_of(".abcdefghijklmnopqrstuvwxyz0123456789_-") !=
             std::string::npos))
      return 0;
    valid.push_back(ext);
  }
  if (valid.empty()) return 0;
  for (auto& x : valid) p->owner->previews_.push_back({p, x, fn, data});
  return 1;
}
bool PluginManager::supportsPreview(const std::filesystem::path& path) const {
  auto ext = previewExtension(path.extension().u8string());
  for (auto& p : previews_)
    if (p.extension == ext) return true;
  return false;
}
bool PluginManager::previewFile(const std::filesystem::path& path,
                                const std::string* source) {
  auto ext = previewExtension(path.extension().u8string());
  auto name = path.u8string();
  for (auto match : {ext, std::string("*")})
    for (auto& p : previews_)
      if (p.extension == match) {
        try {
          return p.callback(&p.owner->host, name.c_str(),
                            source ? source->data() : nullptr,
                            source ? source->size() : 0, p.data) != 0;
        } catch (...) {
          diagnostics_.push_back("Preview failed: " + name);
          return false;
        }
      }
  return false;
}
int PluginManager::hostImageView(void* ctx, const char* id, const char* title,
                                 const void* bytes, size_t size) {
  auto* p = instance(ctx);
  auto& h = p->owner->hooks_.showImageView;
  return id && simpleId(id) && title && bytes && size &&
         size <= 64u * 1024 * 1024 && h &&
         h(p->manifest.id + ":" + id, title, bytes, size);
}
int PluginManager::hostTreeView(void* ctx, const char* id, const char* title,
                                const DodevTreeNode* nodes, size_t count) {
  auto* p = instance(ctx);
  auto& h = p->owner->hooks_.showTreeView;
  if (!id || !simpleId(id) || !title || !nodes || !count || count > 10000 ||
      nodes[0].depth != 0 || !h)
    return 0;
  for (size_t i = 0; i < count; ++i)
    if (!nodes[i].label || nodes[i].depth > 100 ||
        (i && nodes[i].depth > nodes[i - 1].depth + 1))
      return 0;
  return h(p->manifest.id + ":" + id, title, nodes, count);
}
int PluginManager::hostMediaView(void* ctx, const char* id, const char* title,
                                 const char* path) {
  auto* p = instance(ctx);
  auto& h = p->owner->hooks_.showMediaView;
  return id && simpleId(id) && title && path && h &&
         h(p->manifest.id + ":" + id, title, path);
}
int PluginManager::hostRunJob(void* ctx, const char* title, DodevWorkFn work,
                              void* data) {
  auto* p = instance(ctx);
  auto& hook = p->owner->hooks_.runJob;
  return work && hook && hook(title ? title : "Plugin operation", work, data)
             ? 1
             : 0;
}
int PluginManager::hostSelectPath(void* ctx, const char* title, int directory,
                                  DodevPromptFn callback, void* data) {
  auto* p = instance(ctx);
  auto& hook = p->owner->hooks_.selectPath;
  return callback && hook &&
                 hook(title ? title : "Select path", directory != 0,
                      [p, callback, data](const std::string& value) {
                        callback(&p->host, value.c_str(), data);
                      })
             ? 1
             : 0;
}
int PluginManager::hostOpenSerialMonitor(void* ctx) {
  auto* p = instance(ctx);
  return p->owner->hooks_.openSerialMonitor &&
                 p->owner->hooks_.openSerialMonitor()
             ? 1
             : 0;
}

int PluginManager::hostOpenSourceSymbols(void* ctx) {
  auto* p = instance(ctx);
  return p->owner->hooks_.openSourceSymbols &&
                 p->owner->hooks_.openSourceSymbols()
             ? 1
             : 0;
}
int PluginManager::hostOpenRemoteJournal(void* ctx) {
  auto* p = instance(ctx);
  return p->owner->hooks_.openRemoteJournal &&
                 p->owner->hooks_.openRemoteJournal()
             ? 1
             : 0;
}
int PluginManager::hostOpenAiAssistant(void* ctx) {
  auto* p = instance(ctx);
  return p->owner->hooks_.openAiAssistant && p->owner->hooks_.openAiAssistant()
             ? 1
             : 0;
}
int PluginManager::hostDebugContext(void* ctx, const char** workspace,
                                    const char** file, int* line) {
  auto* p = instance(ctx);
  if (!workspace || !file || !line || !p->owner->hooks_.debugContext) return 0;
  if (!p->owner->hooks_.debugContext(p->scratchPath, p->scratchContents, *line))
    return 0;
  *workspace = p->scratchPath.c_str();
  *file = p->scratchContents.c_str();
  return 1;
}
int PluginManager::hostDebugLaunch(void* ctx, const char* cwd,
                                   const char* const* argv, size_t argc) {
  auto* p = instance(ctx);
  if (!cwd || !argv || argc == 0 || argc > 256 || !p->owner->hooks_.debugLaunch)
    return 0;
  std::vector<std::string> args;
  for (size_t i = 0; i < argc; ++i) {
    if (!argv[i]) return 0;
    args.emplace_back(argv[i]);
  }
  return p->owner->hooks_.debugLaunch(cwd, args) ? 1 : 0;
}
int PluginManager::hostDebugControl(void* ctx, const char* command) {
  auto* p = instance(ctx);
  return command && p->owner->hooks_.debugControl
             ? p->owner->hooks_.debugControl(command)
             : 0;
}
int PluginManager::hostDebugOpenFile(void* ctx, const char* path) {
  auto* p = instance(ctx);
  return path && p->owner->hooks_.debugOpenFile &&
                 p->owner->hooks_.debugOpenFile(path)
             ? 1
             : 0;
}
void PluginManager::hostDebugStatus(void* ctx, const char* message) {
  auto* p = instance(ctx);
  if (message && p->owner->hooks_.debugStatus)
    p->owner->hooks_.debugStatus(message);
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
    if (p->manifest.id == owner) {
      auto* v = p->settings.find(key);
      return v ? v->string() : std::string{};
    }
  return {};
}
Json PluginManager::pluginSettingJson(const std::string& owner,
                                      const std::string& key) {
  for (const auto& p : instances_)
    if (p->manifest.id == owner) {
      const auto* value = p->settings.find(key);
      return value ? *value : Json{};
    }
  const auto stored = settings_.loadPluginSettings(owner);
  const auto* value = stored.find(key);
  return value ? *value : Json{};
}
bool PluginManager::updatePluginSettingJson(const std::string& owner,
                                            const std::string& key,
                                            const Json& value) {
  if (!simpleId(owner) || !simpleId(key)) return false;
  for (const auto& sp : settingsSpecs_)
    if (sp.owner == owner && sp.key == key &&
        (!value.isString() || !sp.accepts(value.string())))
      return false;
  for (const auto& p : instances_)
    if (p->manifest.id == owner) {
      auto revised = p->settings;
      revised[key] = value;
      if (!settings_.savePluginSettings(owner, revised)) return false;
      p->settings = std::move(revised);
      return true;
    }
  Json j = settings_.loadPluginSettings(owner);
  j[key] = value;
  return settings_.savePluginSettings(owner, j);
}
bool PluginManager::updatePluginSetting(const std::string& owner,
                                        const std::string& key,
                                        const std::string& value) {
  return updatePluginSettingJson(owner, key, Json(value));
}
bool PluginManager::updatePluginSettings(
    const std::string& owner,
    const std::map<std::string, std::string>& values) {
  for (const auto& p : instances_)
    if (p->manifest.id == owner) {
      auto revised = p->settings;
      for (const auto& v : values) {
        auto spec = std::find_if(
            settingsSpecs_.begin(), settingsSpecs_.end(), [&](const auto& sp) {
              return sp.owner == owner && sp.key == v.first;
            });
        if (spec == settingsSpecs_.end() || !spec->accepts(v.second))
          return false;
        revised[v.first] = v.second;
      }
      if (!settings_.savePluginSettings(owner, revised)) return false;
      p->settings = std::move(revised);
      return true;
    }
  return false;
}
}  // namespace dodev
