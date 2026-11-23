#pragma once
#include "dodev/settings/SettingsManager.h"
#include "dodev/templates/TemplateRegistry.h"
#include "dodev/themes/ThemeManager.h"
#include "dodev/sdk/dodev_plugin_api.h"
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace dodev {
struct PluginManifest {std::string id,name,version,description,library;int apiVersion=0;std::filesystem::path basePath;};
struct PluginRecord {PluginManifest manifest;std::string status;bool enabled=false;};
struct PluginCommand {std::string id,title,owner;DodevCommandFn callback=nullptr;void* userData=nullptr;};
struct PluginMenuItem {std::string location,title,command,owner;};
struct PluginView {std::string id,title,owner,side;bool isPanel=false;DodevViewFn factory=nullptr;void* userData=nullptr;};
struct PluginSetting {std::string owner,key,description,defaultValue;};
class PluginManager {
public:
    struct HostHooks {
        std::function<bool(std::string& path,std::string& contents)> activeDocument;
        std::function<bool(const std::string& contents)> replaceDocument;
        std::function<bool(const std::string& command)> terminalCommand;
        std::function<bool()> openSerialMonitor;
        std::function<bool()> openSourceSymbols;
        std::function<bool()> openRemoteJournal;
        std::function<bool()> openAiAssistant;
        std::function<bool(const std::string& title,const std::string& label,const std::string& initial,
                           std::function<void(const std::string&)>)> promptInput;
    };
    void setHostHooks(HostHooks hooks){hooks_=std::move(hooks);}
    PluginManager(SettingsManager& settings,TemplateRegistry& templates,ThemeManager& themes);
    ~PluginManager();
    void shutdown(); // invoke before the Raven runtime shuts down
    PluginManager(const PluginManager&)=delete;
    PluginManager& operator=(const PluginManager&)=delete;
    size_t discoverAndLoad(const std::filesystem::path& dir);
    size_t discoverAndLoad(const std::vector<std::filesystem::path>& directories);
    bool execute(const std::string& commandId);
    void buildView(const PluginView& view,const DodevUiBuilder& builder);
    void emit(const std::string& event,const std::string& payload="{}");
    bool isEnabled(const std::string& id)const;
    void setEnabled(const std::string& id,bool enabled); // applies on next restart
    const std::vector<PluginRecord>& records()const{return records_;}
    const std::vector<PluginCommand>& commands()const{return commands_;}
    const std::vector<PluginMenuItem>& menus()const{return menus_;}
    const std::vector<PluginView>& views()const{return views_;}
    const std::vector<PluginSetting>& settingSpecs()const{return settingsSpecs_;}
    std::string pluginSetting(const std::string& owner,const std::string& key)const;
    Json pluginSettingJson(const std::string& owner,const std::string& key);
    bool updatePluginSettingJson(const std::string& owner,const std::string& key,const Json& value);
    bool updatePluginSetting(const std::string& owner,const std::string& key,const std::string& value);
    const std::vector<std::string>& diagnostics()const{return diagnostics_;}
    static bool readManifest(const std::filesystem::path& folder,PluginManifest& out,std::string* error=nullptr);
private:
    struct Instance;
    struct Subscription{Instance* owner;std::string event;DodevEventFn callback;void* userData;};
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
    std::vector<Subscription> subscriptions_;
    std::vector<std::string> diagnostics_;
    bool activate(PluginManifest manifest,std::string* error);
    static Instance* instance(void* context);
    static void hostLog(void*,int,const char*);
    static int hostCommand(void*,const char*,const char*,DodevCommandFn,void*);
    static int hostMenu(void*,const char*,const char*,const char*);
    static int hostTab(void*,const char*,const char*,DodevViewFn,void*);
    static int hostPanel(void*,const char*,const char*,const char*,DodevViewFn,void*);
    static int hostTemplate(void*,const char*,const char*,const char*,const char*);
    static int hostTheme(void*,const char*);
    static int hostSetting(void*,const char*,const char*,const char*);
    static const char* hostGetSetting(void*,const char*);
    static int hostSetSetting(void*,const char*,const char*);
    static int hostEvent(void*,const char*,DodevEventFn,void*);
    static int hostActiveDocument(void*,const char**,const char**);
    static int hostReplaceDocument(void*,const char*);
    static int hostTerminalCommand(void*,const char*);
    static int hostPromptInput(void*,const char*,const char*,const char*,DodevPromptFn,void*);
    static int hostOpenSerialMonitor(void*);
    static int hostOpenSourceSymbols(void*);
    static int hostOpenRemoteJournal(void*);
    static int hostOpenAiAssistant(void*);
};
}
