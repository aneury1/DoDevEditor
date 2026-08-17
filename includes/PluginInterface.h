#pragma once

#include "plugin/DoDevPluginAPI.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class PluginLoader
{
public:
    void* Load(const std::string& path);
    void* GetSymbol(void* handle, const std::string& name);
    void Unload(void* handle);
    const std::string& LastError() const { return m_lastError; }
private:
    std::string m_lastError;
};

class PluginManager
{
public:
    struct HostBindings
    {
        std::function<void(DoDevLogLevel, const std::string&)> log;
        std::function<void*(DoDevPanelId)> getPanel;
        std::function<bool(DoDevPanelId)> focusPanel;

        std::function<int()> tabCount;
        std::function<int()> activeTabIndex;
        std::function<void*(int)> tabAt;
        std::function<void*()> activeTab;
        std::function<bool(int)> selectTab;
        std::function<bool(int)> closeTab;
        std::function<std::string(int)> tabTitle;
        std::function<std::string(int)> tabPath;

        std::function<void*()> activeEditor;
        std::function<bool(void*)> isEditor;
        std::function<std::string(void*)> editorGetText;
        std::function<bool(void*, const std::string&)> editorSetText;
        std::function<bool(void*, size_t, const std::string&)> editorInsertText;
        std::function<std::string(void*)> editorGetPath;
        std::function<size_t(void*)> editorGetCaret;
        std::function<bool(void*, size_t)> editorSetCaret;
        std::function<bool(void*, size_t&, size_t&)> editorGetSelection;
        std::function<bool(void*, const std::string&)> editorReplaceSelection;

        std::function<bool(const std::string&)> openFile;
        std::function<void*(const std::string&, const std::string&)> newEditorTab;
        std::function<void(const std::string&)> setStatus;

        std::function<int(const std::string&, const std::string&, const std::string&, std::function<void()>)> addMenuItem;
        std::function<bool(int)> removeMenuItem;

        std::function<void*(DoDevPanelLocation, const std::string&, const std::string&, bool)> createTextPanel;
        std::function<bool(void*, const std::string&)> textPanelSetText;
        std::function<bool(void*)> removePanel;
        std::function<void*(DoDevPanelLocation, const std::string&, DoDevWindowFactory, void*, bool)> addCustomPanel;
    };

    struct PluginSummary
    {
        std::string id;
        std::string name;
        std::string version;
        std::string description;
        std::string path;
    };

    explicit PluginManager(HostBindings bindings);
    ~PluginManager();

    bool LoadPlugin(const std::string& path, std::string* error = nullptr);
    size_t LoadDirectory(const std::string& directory, std::vector<std::string>* errors = nullptr,
                         const std::vector<std::string>& disabledFiles = {});
    void UnloadAll();
    bool ReloadDirectory(const std::string& directory, std::vector<std::string>* errors = nullptr,
                         const std::vector<std::string>& disabledFiles = {});

    void DispatchEvent(DoDevEventType type, void* object, int index, const std::string& pathUtf8 = std::string());
    std::vector<PluginSummary> GetPlugins() const;

private:
    struct PluginRecord;
    struct ApiContext;

    HostBindings m_bindings;
    PluginLoader m_loader;
    std::vector<std::unique_ptr<PluginRecord>> m_plugins;

    static PluginManager* ManagerFrom(void* context);
    static PluginRecord* RecordFrom(void* context);
    static size_t CopyUtf8(const std::string& value, char* out, size_t outSize);

    static void ApiLog(void*, DoDevLogLevel, const char*);
    static DoDevHandle ApiGetPanel(void*, DoDevPanelId);
    static int ApiFocusPanel(void*, DoDevPanelId);
    static int ApiTabCount(void*);
    static int ApiActiveTabIndex(void*);
    static DoDevHandle ApiTabAt(void*, int);
    static DoDevHandle ApiActiveTab(void*);
    static int ApiSelectTab(void*, int);
    static int ApiCloseTab(void*, int);
    static size_t ApiTabTitle(void*, int, char*, size_t);
    static size_t ApiTabPath(void*, int, char*, size_t);
    static DoDevHandle ApiActiveEditor(void*);
    static int ApiIsEditor(void*, DoDevHandle);
    static size_t ApiEditorGetText(void*, DoDevHandle, char*, size_t);
    static int ApiEditorSetText(void*, DoDevHandle, const char*);
    static int ApiEditorInsertText(void*, DoDevHandle, size_t, const char*);
    static size_t ApiEditorGetPath(void*, DoDevHandle, char*, size_t);
    static size_t ApiEditorGetCaret(void*, DoDevHandle);
    static int ApiEditorSetCaret(void*, DoDevHandle, size_t);
    static int ApiEditorGetSelection(void*, DoDevHandle, size_t*, size_t*);
    static int ApiEditorReplaceSelection(void*, DoDevHandle, const char*);
    static int ApiOpenFile(void*, const char*);
    static DoDevHandle ApiNewEditorTab(void*, const char*, const char*);
    static int ApiSetStatus(void*, const char*);
    static int ApiAddMenuItem(void*, const char*, const char*, const char*, DoDevMenuCallback, void*);
    static int ApiRemoveMenuItem(void*, int);
    static DoDevHandle ApiCreateTextPanel(void*, DoDevPanelLocation, const char*, const char*, int);
    static int ApiTextPanelSetText(void*, DoDevHandle, const char*);
    static int ApiRemovePanel(void*, DoDevHandle);
    static DoDevHandle ApiAddCustomPanel(void*, DoDevPanelLocation, const char*, DoDevWindowFactory, void*, int);

    void FillHostApi(PluginRecord& record);
    void CleanupOwnedObjects(PluginRecord& record);
};
