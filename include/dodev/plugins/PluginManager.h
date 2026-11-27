#pragma once
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "dodev/sdk/dodev_plugin_api.h"
#include "dodev/settings/SettingsManager.h"
#include "dodev/templates/TemplateRegistry.h"
#include "dodev/themes/ThemeManager.h"
namespace dodev {
struct PluginManifest {
  std::string id, name, version, description, library;
  int apiVersion = 0;
  std::filesystem::path basePath;
};
struct PluginRecord {
  PluginManifest manifest;
  std::string status;
  bool enabled = false;
};
struct PluginCommand {
  std::string id, title, owner;
  DodevCommandFn callback = nullptr;
  void* userData = nullptr;
};
struct PluginMenuItem {
  std::string location, title, command, owner;
};
struct PluginView {
  std::string id, title, owner, side;
  bool isPanel = false;
  DodevViewFn factory = nullptr;
  void* userData = nullptr;
};
// Presentation location, independent of any particular plugin or GUI toolkit.
enum class PluginViewLocation { Editor, Output, Sidebar };
inline PluginViewLocation pluginViewLocation(const PluginView& view) {
  if (!view.isPanel || (view.side == "Tab" || view.side == "Editor"))
    return PluginViewLocation::Editor;
  if ((view.side == "Bottom" || view.side == "Output"))
    return PluginViewLocation::Output;
  return PluginViewLocation::Sidebar;
}
// Workspace panels are installed lazily when a project is opened.
inline bool isWorkspacePluginView(const PluginView& view) {
  return view.isPanel && view.side == "Workspace";
}
struct PluginSetting {
  std::string owner, key, description, defaultValue;
  std::string label, type = "text";
  int minimum = 0, maximum = 100;
  std::vector<std::pair<std::string, std::string>> choices;  // value, label
  bool accepts(const std::string& value) const;
};
struct PluginSettingsAction {
  std::string owner, title, command;
};
class PluginManager {
 public:
  struct HostHooks {
    std::function<bool(const std::string&, DodevWorkFn, void*)> runJob;
    std::function<bool(std::string& path, std::string& contents)>
        activeDocument;
    std::function<bool(const std::string& contents)> replaceDocument;
    std::function<bool(const std::string& command)> terminalCommand;
    std::function<bool(const std::string&, const std::string&)>
        openWorkspaceConfig;
    std::function<bool(const std::string&, const std::string&,
                       std::function<void(const std::string&)>)>
        promptSecret;
    std::function<bool(const std::string&, const std::string&,
                       const std::string&, const std::string&)>
        showTextView;
    std::function<bool(const std::string&, const std::string&,
                       const std::string&, const std::string&)>
        showHtmlView;
    std::function<bool(const std::string&, const std::string&, const void*,
                       size_t)>
        showImageView;
    std::function<bool(const std::string&, const std::string&,
                       const DodevTreeNode*, size_t)>
        showTreeView;
    std::function<bool(const std::string&, const std::string&,
                       const std::string&)>
        showMediaView;
    std::function<bool(const std::string&, bool,
                       std::function<void(const std::string&)>)>
        selectPath;
    std::function<bool()> openSerialMonitor;
    std::function<bool()> openSourceSymbols;
    std::function<bool()> openRemoteJournal;
    std::function<bool()> openAiAssistant;
    std::function<bool(std::string&, std::string&, int&)> debugContext;
    std::function<bool(const std::string&, const std::vector<std::string>&)>
        debugLaunch;
    std::function<int(const std::string&)> debugControl;
    std::function<bool(const std::string&)> debugOpenFile;
    std::function<void(const std::string&)> debugStatus;
    std::function<bool(const std::string& title, const std::string& label,
                       const std::string& initial,
                       std::function<void(const std::string&)>)>
        promptInput;
  };
  void setHostHooks(HostHooks hooks) { hooks_ = std::move(hooks); }
  PluginManager(SettingsManager& settings, TemplateRegistry& templates,
                ThemeManager& themes);
  ~PluginManager();
  void shutdown();  // invoke before the Raven runtime shuts down
  PluginManager(const PluginManager&) = delete;
  PluginManager& operator=(const PluginManager&) = delete;
  size_t discoverAndLoad(const std::filesystem::path& dir);
  size_t discoverAndLoad(const std::vector<std::filesystem::path>& directories);
  bool execute(const std::string& commandId);
  bool supportsPreview(const std::filesystem::path&) const;
  bool previewFile(const std::filesystem::path&,
                   const std::string* source = nullptr);
  void buildView(const PluginView& view, const DodevUiBuilder& builder);
  void emit(const std::string& event, const std::string& payload = "{}");
  bool isEnabled(const std::string& id) const;
  void setEnabled(const std::string& id,
                  bool enabled);  // applies on next restart
  const std::vector<PluginRecord>& records() const { return records_; }
  const std::vector<PluginCommand>& commands() const { return commands_; }
  const std::vector<PluginMenuItem>& menus() const { return menus_; }
  const std::vector<PluginView>& views() const { return views_; }
  const std::vector<PluginSetting>& settingSpecs() const {
    return settingsSpecs_;
  }
  const std::vector<PluginSettingsAction>& settingsActions() const {
    return settingsActions_;
  }
  bool updatePluginSettings(const std::string& owner,
                            const std::map<std::string, std::string>& values);
  std::string pluginSetting(const std::string& owner,
                            const std::string& key) const;
  Json pluginSettingJson(const std::string& owner, const std::string& key);
  bool updatePluginSettingJson(const std::string& owner, const std::string& key,
                               const Json& value);
  bool updatePluginSetting(const std::string& owner, const std::string& key,
                           const std::string& value);
  const std::vector<std::string>& diagnostics() const { return diagnostics_; }
  static bool readManifest(const std::filesystem::path& folder,
                           PluginManifest& out, std::string* error = nullptr);

 private:
  struct Instance;
  struct Subscription {
    Instance* owner;
    std::string event;
    DodevEventFn callback;
    void* userData;
  };
  HostHooks hooks_;
  SettingsManager& settings_;
  TemplateRegistry& templates_;
  ThemeManager& themes_;
  std::vector<std::unique_ptr<Instance>> instances_;
  std::vector<PluginRecord> records_;
  std::vector<PluginCommand> commands_;
  std::vector<PluginMenuItem> menus_;
  std::vector<PluginView> views_;
  std::vector<PluginSetting> settingsSpecs_;
  std::vector<PluginSettingsAction> settingsActions_;
  std::vector<Subscription> subscriptions_;
  struct FilePreview {
    Instance* owner;
    std::string extension;
    DodevFilePreviewFn callback;
    void* data;
  };
  std::vector<FilePreview> previews_;
  std::vector<std::string> diagnostics_;
  bool activate(PluginManifest manifest, std::string* error);
  static Instance* instance(void* context);
  static void hostLog(void*, int, const char*);
  static int hostCommand(void*, const char*, const char*, DodevCommandFn,
                         void*);
  static int hostMenu(void*, const char*, const char*, const char*);
  static int hostTab(void*, const char*, const char*, DodevViewFn, void*);
  static int hostPanel(void*, const char*, const char*, const char*,
                       DodevViewFn, void*);
  static int hostTemplate(void*, const char*, const char*, const char*,
                          const char*);
  static int hostTheme(void*, const char*);
  static int hostSettingEx(void*, const char*, const char*, const char*,
                           const char*);
  static int hostSettingsAction(void*, const char*, const char*);
  static int hostSetting(void*, const char*, const char*, const char*);
  static const char* hostGetSetting(void*, const char*);
  static int hostSetSetting(void*, const char*, const char*);
  static int hostEvent(void*, const char*, DodevEventFn, void*);
  static int hostActiveDocument(void*, const char**, const char**);
  static int hostReplaceDocument(void*, const char*);
  static int hostTerminalCommand(void*, const char*);
  static int hostPromptInput(void*, const char*, const char*, const char*,
                             DodevPromptFn, void*);
  static int hostWorkspaceConfig(void*, const char*);
  static int hostPromptSecret(void*, const char*, const char*, DodevPromptFn,
                              void*);
  static int hostShowTextView(void*, const char*, const char*, const char*,
                              const char*);
  static int hostShowHtmlView(void*, const char*, const char*, const char*,
                              const char*);
  static int hostRegisterPreview(void*, const char*, DodevFilePreviewFn, void*);
  static int hostImageView(void*, const char*, const char*, const void*,
                           size_t);
  static int hostTreeView(void*, const char*, const char*, const DodevTreeNode*,
                          size_t);
  static int hostMediaView(void*, const char*, const char*, const char*);
  static int hostRunJob(void*, const char*, DodevWorkFn, void*);
  static int hostSelectPath(void*, const char*, int, DodevPromptFn, void*);
  static int hostOpenSerialMonitor(void*);
  static int hostOpenSourceSymbols(void*);
  static int hostOpenRemoteJournal(void*);
  static int hostOpenAiAssistant(void*);
  static int hostDebugContext(void*, const char**, const char**, int*);
  static int hostDebugLaunch(void*, const char*, const char* const*, size_t);
  static int hostDebugControl(void*, const char*);
  static int hostDebugOpenFile(void*, const char*);
  static void hostDebugStatus(void*, const char*);
};
}  // namespace dodev
