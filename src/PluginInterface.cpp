#include "PluginInterface.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <sstream>

void* PluginLoader::Load(const std::string& path)
{
    m_lastError.clear();
#ifdef _WIN32
    HMODULE handle = LoadLibraryA(path.c_str());
    if (!handle)
        m_lastError = "LoadLibrary failed with error " + std::to_string(static_cast<unsigned long>(GetLastError()));
    return reinterpret_cast<void*>(handle);
#else
    dlerror();
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle)
    {
        const char* error = dlerror();
        m_lastError = error ? error : "dlopen failed";
    }
    return handle;
#endif
}

void* PluginLoader::GetSymbol(void* handle, const std::string& name)
{
    m_lastError.clear();
    if (!handle)
        return nullptr;
#ifdef _WIN32
    FARPROC symbol = GetProcAddress(reinterpret_cast<HMODULE>(handle), name.c_str());
    if (!symbol)
        m_lastError = "GetProcAddress failed for " + name;
    return reinterpret_cast<void*>(symbol);
#else
    dlerror();
    void* symbol = dlsym(handle, name.c_str());
    if (const char* error = dlerror())
    {
        m_lastError = error;
        return nullptr;
    }
    return symbol;
#endif
}

void PluginLoader::Unload(void* handle)
{
    if (!handle)
        return;
#ifdef _WIN32
    FreeLibrary(reinterpret_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

struct PluginManager::ApiContext
{
    PluginManager* manager = nullptr;
    PluginRecord* record = nullptr;
};

struct PluginManager::PluginRecord
{
    void* library = nullptr;
    DoDevPluginLoadFn loadFn = nullptr;
    DoDevPluginUnloadFn unloadFn = nullptr;
    DoDevPluginEventFn eventFn = nullptr;
    void* pluginState = nullptr;
    PluginSummary summary;
    ApiContext apiContext;
    DoDevHostApi api{};
    std::vector<int> menuTokens;
    std::vector<void*> panels;
};

PluginManager::PluginManager(HostBindings bindings)
    : m_bindings(std::move(bindings))
{
}

PluginManager::~PluginManager()
{
    UnloadAll();
}

PluginManager* PluginManager::ManagerFrom(void* context)
{
    auto* api = static_cast<ApiContext*>(context);
    return api ? api->manager : nullptr;
}

PluginManager::PluginRecord* PluginManager::RecordFrom(void* context)
{
    auto* api = static_cast<ApiContext*>(context);
    return api ? api->record : nullptr;
}

size_t PluginManager::CopyUtf8(const std::string& value, char* out, size_t outSize)
{
    const size_t required = value.size() + 1;
    if (out && outSize > 0)
    {
        const size_t count = std::min(value.size(), outSize - 1);
        if (count > 0)
            std::memcpy(out, value.data(), count);
        out[count] = '\0';
    }
    return required;
}

void PluginManager::FillHostApi(PluginRecord& record)
{
    record.apiContext.manager = this;
    record.apiContext.record = &record;

    DoDevHostApi& api = record.api;
    std::memset(&api, 0, sizeof(api));
    api.abi_version = DODEV_PLUGIN_ABI_VERSION;
    api.size = static_cast<uint32_t>(sizeof(api));
    api.context = &record.apiContext;
    api.log = &ApiLog;
    api.get_panel = &ApiGetPanel;
    api.focus_panel = &ApiFocusPanel;
    api.tab_count = &ApiTabCount;
    api.active_tab_index = &ApiActiveTabIndex;
    api.tab_at = &ApiTabAt;
    api.active_tab = &ApiActiveTab;
    api.select_tab = &ApiSelectTab;
    api.close_tab = &ApiCloseTab;
    api.tab_title = &ApiTabTitle;
    api.tab_path = &ApiTabPath;
    api.active_editor = &ApiActiveEditor;
    api.is_editor = &ApiIsEditor;
    api.editor_get_text = &ApiEditorGetText;
    api.editor_set_text = &ApiEditorSetText;
    api.editor_insert_text = &ApiEditorInsertText;
    api.editor_get_path = &ApiEditorGetPath;
    api.editor_get_caret = &ApiEditorGetCaret;
    api.editor_set_caret = &ApiEditorSetCaret;
    api.editor_get_selection = &ApiEditorGetSelection;
    api.editor_replace_selection = &ApiEditorReplaceSelection;
    api.open_file = &ApiOpenFile;
    api.new_editor_tab = &ApiNewEditorTab;
    api.set_status = &ApiSetStatus;
    api.add_menu_item = &ApiAddMenuItem;
    api.remove_menu_item = &ApiRemoveMenuItem;
    api.create_text_panel = &ApiCreateTextPanel;
    api.text_panel_set_text = &ApiTextPanelSetText;
    api.remove_panel = &ApiRemovePanel;
    api.add_custom_panel = &ApiAddCustomPanel;
}

bool PluginManager::LoadPlugin(const std::string& path, std::string* error)
{
    auto record = std::make_unique<PluginRecord>();
    record->summary.path = path;
    record->library = m_loader.Load(path);
    if (!record->library)
    {
        if (error)
            *error = m_loader.LastError();
        return false;
    }

    record->loadFn = reinterpret_cast<DoDevPluginLoadFn>(m_loader.GetSymbol(record->library, "dodev_plugin_load"));
    if (!record->loadFn)
    {
        if (error)
            *error = "Missing required export dodev_plugin_load: " + m_loader.LastError();
        m_loader.Unload(record->library);
        return false;
    }

    record->unloadFn = reinterpret_cast<DoDevPluginUnloadFn>(m_loader.GetSymbol(record->library, "dodev_plugin_unload"));
    if (!record->unloadFn)
    {
        if (error)
            *error = "Missing required export dodev_plugin_unload: " + m_loader.LastError();
        m_loader.Unload(record->library);
        return false;
    }

    /* Optional event export. */
    record->eventFn = reinterpret_cast<DoDevPluginEventFn>(m_loader.GetSymbol(record->library, "dodev_plugin_on_event"));
    FillHostApi(*record);

    DoDevPluginInfo info{};
    info.abi_version = DODEV_PLUGIN_ABI_VERSION;
    info.size = static_cast<uint32_t>(sizeof(info));

    const int loaded = record->loadFn(&record->api, &info, &record->pluginState);
    if (!loaded)
    {
        CleanupOwnedObjects(*record);
        if (record->pluginState && record->unloadFn)
            record->unloadFn(record->pluginState);
        m_loader.Unload(record->library);
        if (error)
            *error = "Plugin rejected loading";
        return false;
    }

    if (info.abi_version != DODEV_PLUGIN_ABI_VERSION)
    {
        CleanupOwnedObjects(*record);
        if (record->unloadFn)
            record->unloadFn(record->pluginState);
        m_loader.Unload(record->library);
        if (error)
            *error = "Plugin ABI mismatch";
        return false;
    }

    record->summary.id = info.id ? info.id : std::filesystem::path(path).stem().string();
    record->summary.name = info.name ? info.name : record->summary.id;
    record->summary.version = info.version ? info.version : "";
    record->summary.description = info.description ? info.description : "";

    if (record->summary.id.empty())
        record->summary.id = std::filesystem::path(path).stem().string();

    for (const auto& existing : m_plugins)
    {
        if (existing->summary.id == record->summary.id)
        {
            CleanupOwnedObjects(*record);
            if (record->unloadFn)
                record->unloadFn(record->pluginState);
            m_loader.Unload(record->library);
            if (error)
                *error = "A plugin with id '" + record->summary.id + "' is already loaded";
            return false;
        }
    }

    if (m_bindings.log)
        m_bindings.log(DODEV_LOG_INFO, "Loaded plugin: " + record->summary.name + " (" + path + ")");
    m_plugins.push_back(std::move(record));
    return true;
}

size_t PluginManager::LoadDirectory(const std::string& directory, std::vector<std::string>* errors,
                                    const std::vector<std::string>& disabledFiles)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(directory, ec);

    std::vector<fs::path> files;
    if (fs::exists(directory, ec))
    {
        for (const auto& entry : fs::directory_iterator(directory, ec))
        {
            if (!entry.is_regular_file())
                continue;
            const std::string ext = entry.path().extension().string();
#ifdef _WIN32
            if (ext == ".dll")
#else
            if (ext == ".so" || ext == ".dylib")
#endif
                files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    auto normalizeName = [](std::string name)
    {
#ifdef _WIN32
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
#endif
        return name;
    };

    std::vector<std::string> disabled;
    disabled.reserve(disabledFiles.size());
    for (const std::string& value : disabledFiles)
        disabled.push_back(normalizeName(fs::path(value).filename().string()));

    size_t loaded = 0;
    for (const fs::path& file : files)
    {
        const std::string basename = normalizeName(file.filename().string());
        if (std::find(disabled.begin(), disabled.end(), basename) != disabled.end())
        {
            if (m_bindings.log)
                m_bindings.log(DODEV_LOG_INFO, "Plugin disabled by settings: " + file.filename().string());
            continue;
        }

        std::string err;
        if (LoadPlugin(file.string(), &err))
            ++loaded;
        else if (errors)
            errors->push_back(file.filename().string() + ": " + err);
    }
    return loaded;
}

void PluginManager::CleanupOwnedObjects(PluginRecord& record)
{
    if (m_bindings.removePanel)
    {
        const auto panels = record.panels;
        for (auto it = panels.rbegin(); it != panels.rend(); ++it)
            m_bindings.removePanel(*it);
    }
    record.panels.clear();

    if (m_bindings.removeMenuItem)
    {
        const auto tokens = record.menuTokens;
        for (int token : tokens)
            m_bindings.removeMenuItem(token);
    }
    record.menuTokens.clear();
}

void PluginManager::UnloadAll()
{
    for (auto it = m_plugins.rbegin(); it != m_plugins.rend(); ++it)
    {
        PluginRecord& record = *(*it);
        CleanupOwnedObjects(record);
        if (record.unloadFn)
            record.unloadFn(record.pluginState);
        m_loader.Unload(record.library);
    }
    m_plugins.clear();
}

bool PluginManager::ReloadDirectory(const std::string& directory, std::vector<std::string>* errors,
                                    const std::vector<std::string>& disabledFiles)
{
    UnloadAll();
    LoadDirectory(directory, errors, disabledFiles);
    return !errors || errors->empty();
}

void PluginManager::DispatchEvent(DoDevEventType type, void* object, int index, const std::string& pathUtf8)
{
    DoDevEvent event{};
    event.size = static_cast<uint32_t>(sizeof(event));
    event.type = type;
    event.object = object;
    event.index = index;
    event.path_utf8 = pathUtf8.empty() ? nullptr : pathUtf8.c_str();

    for (const auto& plugin : m_plugins)
    {
        if (plugin->eventFn)
            plugin->eventFn(plugin->pluginState, &event);
    }
}

std::vector<PluginManager::PluginSummary> PluginManager::GetPlugins() const
{
    std::vector<PluginSummary> result;
    result.reserve(m_plugins.size());
    for (const auto& plugin : m_plugins)
        result.push_back(plugin->summary);
    return result;
}

void PluginManager::ApiLog(void* context, DoDevLogLevel level, const char* text)
{
    if (auto* manager = ManagerFrom(context); manager && manager->m_bindings.log)
        manager->m_bindings.log(level, text ? text : "");
}

DoDevHandle PluginManager::ApiGetPanel(void* context, DoDevPanelId panel)
{
    if (auto* manager = ManagerFrom(context); manager && manager->m_bindings.getPanel)
        return manager->m_bindings.getPanel(panel);
    return nullptr;
}

int PluginManager::ApiFocusPanel(void* context, DoDevPanelId panel)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.focusPanel && manager->m_bindings.focusPanel(panel);
}

int PluginManager::ApiTabCount(void* context)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.tabCount ? manager->m_bindings.tabCount() : 0;
}

int PluginManager::ApiActiveTabIndex(void* context)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.activeTabIndex ? manager->m_bindings.activeTabIndex() : -1;
}

DoDevHandle PluginManager::ApiTabAt(void* context, int index)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.tabAt ? manager->m_bindings.tabAt(index) : nullptr;
}

DoDevHandle PluginManager::ApiActiveTab(void* context)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.activeTab ? manager->m_bindings.activeTab() : nullptr;
}

int PluginManager::ApiSelectTab(void* context, int index)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.selectTab && manager->m_bindings.selectTab(index);
}

int PluginManager::ApiCloseTab(void* context, int index)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.closeTab && manager->m_bindings.closeTab(index);
}

size_t PluginManager::ApiTabTitle(void* context, int index, char* out, size_t outSize)
{
    auto* manager = ManagerFrom(context);
    return CopyUtf8(manager && manager->m_bindings.tabTitle ? manager->m_bindings.tabTitle(index) : std::string(), out, outSize);
}

size_t PluginManager::ApiTabPath(void* context, int index, char* out, size_t outSize)
{
    auto* manager = ManagerFrom(context);
    return CopyUtf8(manager && manager->m_bindings.tabPath ? manager->m_bindings.tabPath(index) : std::string(), out, outSize);
}

DoDevHandle PluginManager::ApiActiveEditor(void* context)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.activeEditor ? manager->m_bindings.activeEditor() : nullptr;
}

int PluginManager::ApiIsEditor(void* context, DoDevHandle handle)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.isEditor && manager->m_bindings.isEditor(handle);
}

size_t PluginManager::ApiEditorGetText(void* context, DoDevHandle editor, char* out, size_t outSize)
{
    auto* manager = ManagerFrom(context);
    return CopyUtf8(manager && manager->m_bindings.editorGetText ? manager->m_bindings.editorGetText(editor) : std::string(), out, outSize);
}

int PluginManager::ApiEditorSetText(void* context, DoDevHandle editor, const char* text)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.editorSetText && manager->m_bindings.editorSetText(editor, text ? text : "");
}

int PluginManager::ApiEditorInsertText(void* context, DoDevHandle editor, size_t position, const char* text)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.editorInsertText && manager->m_bindings.editorInsertText(editor, position, text ? text : "");
}

size_t PluginManager::ApiEditorGetPath(void* context, DoDevHandle editor, char* out, size_t outSize)
{
    auto* manager = ManagerFrom(context);
    return CopyUtf8(manager && manager->m_bindings.editorGetPath ? manager->m_bindings.editorGetPath(editor) : std::string(), out, outSize);
}

size_t PluginManager::ApiEditorGetCaret(void* context, DoDevHandle editor)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.editorGetCaret ? manager->m_bindings.editorGetCaret(editor) : 0;
}

int PluginManager::ApiEditorSetCaret(void* context, DoDevHandle editor, size_t position)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.editorSetCaret && manager->m_bindings.editorSetCaret(editor, position);
}

int PluginManager::ApiEditorGetSelection(void* context, DoDevHandle editor, size_t* start, size_t* end)
{
    if (!start || !end)
        return 0;
    auto* manager = ManagerFrom(context);
    size_t a = 0, b = 0;
    if (!manager || !manager->m_bindings.editorGetSelection || !manager->m_bindings.editorGetSelection(editor, a, b))
        return 0;
    *start = a;
    *end = b;
    return 1;
}

int PluginManager::ApiEditorReplaceSelection(void* context, DoDevHandle editor, const char* text)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.editorReplaceSelection && manager->m_bindings.editorReplaceSelection(editor, text ? text : "");
}

int PluginManager::ApiOpenFile(void* context, const char* path)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.openFile && path && manager->m_bindings.openFile(path);
}

DoDevHandle PluginManager::ApiNewEditorTab(void* context, const char* title, const char* text)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.newEditorTab
        ? manager->m_bindings.newEditorTab(title ? title : "Plugin", text ? text : "")
        : nullptr;
}

int PluginManager::ApiSetStatus(void* context, const char* text)
{
    auto* manager = ManagerFrom(context);
    if (!manager || !manager->m_bindings.setStatus)
        return 0;
    manager->m_bindings.setStatus(text ? text : "");
    return 1;
}

int PluginManager::ApiAddMenuItem(void* context,
                                  const char* menu,
                                  const char* label,
                                  const char* shortcut,
                                  DoDevMenuCallback callback,
                                  void* userData)
{
    auto* manager = ManagerFrom(context);
    auto* record = RecordFrom(context);
    if (!manager || !record || !manager->m_bindings.addMenuItem || !callback || !menu || !label)
        return 0;

    const int token = manager->m_bindings.addMenuItem(
        menu, label, shortcut ? shortcut : "",
        [callback, userData]() { callback(userData); });
    if (token > 0)
        record->menuTokens.push_back(token);
    return token;
}

int PluginManager::ApiRemoveMenuItem(void* context, int token)
{
    auto* manager = ManagerFrom(context);
    auto* record = RecordFrom(context);
    if (!manager || !record || !manager->m_bindings.removeMenuItem)
        return 0;
    if (!manager->m_bindings.removeMenuItem(token))
        return 0;
    record->menuTokens.erase(std::remove(record->menuTokens.begin(), record->menuTokens.end(), token), record->menuTokens.end());
    return 1;
}

DoDevHandle PluginManager::ApiCreateTextPanel(void* context,
                                              DoDevPanelLocation location,
                                              const char* title,
                                              const char* text,
                                              int select)
{
    auto* manager = ManagerFrom(context);
    auto* record = RecordFrom(context);
    if (!manager || !record || !manager->m_bindings.createTextPanel)
        return nullptr;
    void* panel = manager->m_bindings.createTextPanel(location,
                                                       title ? title : "Plugin",
                                                       text ? text : "",
                                                       select != 0);
    if (panel)
        record->panels.push_back(panel);
    return panel;
}

int PluginManager::ApiTextPanelSetText(void* context, DoDevHandle panel, const char* text)
{
    auto* manager = ManagerFrom(context);
    return manager && manager->m_bindings.textPanelSetText && manager->m_bindings.textPanelSetText(panel, text ? text : "");
}

int PluginManager::ApiRemovePanel(void* context, DoDevHandle panel)
{
    auto* manager = ManagerFrom(context);
    auto* record = RecordFrom(context);
    if (!manager || !record || !manager->m_bindings.removePanel || !manager->m_bindings.removePanel(panel))
        return 0;
    record->panels.erase(std::remove(record->panels.begin(), record->panels.end(), panel), record->panels.end());
    return 1;
}

DoDevHandle PluginManager::ApiAddCustomPanel(void* context,
                                             DoDevPanelLocation location,
                                             const char* title,
                                             DoDevWindowFactory factory,
                                             void* userData,
                                             int select)
{
    auto* manager = ManagerFrom(context);
    auto* record = RecordFrom(context);
    if (!manager || !record || !manager->m_bindings.addCustomPanel || !factory)
        return nullptr;
    void* panel = manager->m_bindings.addCustomPanel(location,
                                                      title ? title : "Plugin",
                                                      factory,
                                                      userData,
                                                      select != 0);
    if (panel)
        record->panels.push_back(panel);
    return panel;
}
