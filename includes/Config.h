#ifndef CONFIG_H
#define CONFIG_H

#include <wx/stdpaths.h>
#include <cstddef>
#include <fstream>
#include <string>
#include <vector>
#include <json/json.h>


struct ManualSymbolRuntimeConfig
{
    bool enabled = true;
    bool cEnabled = true;
    bool cppEnabled = true;
    bool kotlinEnabled = true;
    bool symbolsEnabled = true;
    bool callsEnabled = true;
    bool typesEnabled = true;
    bool functionsEnabled = true;
    bool variablesEnabled = true;
    bool macrosEnabled = true;
    bool autoParse = true;
};


struct EditorViewRuntimeConfig
{
    bool whitespaceVisible = false;
    bool eolVisible = false;
    bool controlCharactersVisible = false;
};

struct PluginRuntimeConfig
{
    // Empty means <DoDevEditor executable>/plugins. Relative paths are resolved
    // against the executable directory by MainFrame.
    std::string directory;

    // Built-in HTTP/WebSocket Editor Server plugin settings. The host exposes these to
    // the plugin as DODEV_HTTP_* environment overrides when plugins load.
    std::string httpBindAddress = "0.0.0.0";
    int httpPort = 9934;
    std::string httpAuthToken;
    size_t httpMaxTextBytes = 4u * 1024u * 1024u;

    // Native library filenames that are discovered but intentionally not
    // loaded. Entries are basenames such as dodev_http_editor_server.so or
    // dodev_http_editor_server.dll so the preference survives folder moves.
    std::vector<std::string> disabledPlugins;
};

struct AppEditorConfig
{
    static Json::Value config;

    static void CreateDefaultConfig();

    // Backward-compatible single-folder accessors.
    static std::string GetLastOpenedFolder();
    static void LoadOpenedFolder();
    static void SaveOpenedFolder(const std::string& folder);

    // Workspace session persistence. An unsaved multi-root workspace is stored
    // as a list of folders; a saved workspace additionally stores its file.
    static std::vector<std::string> GetLastWorkspaceFolders();
    static std::string GetLastWorkspaceFile();
    static void SaveWorkspaceSession(const std::vector<std::string>& folders,
                                     const std::string& workspaceFile);

    // JSON-driven editor theme selection. Theme definitions live in config/themes.
    static std::string GetEditorTheme();
    static void SetEditorTheme(const std::string& themeId);

    // Editor visualization of otherwise non-printable characters.
    static EditorViewRuntimeConfig GetEditorViewRuntimeConfig();
    static void SetEditorViewRuntimeConfig(const EditorViewRuntimeConfig& settings);

    // Recent navigation history shown in the File menu. Newest entries are
    // first and each list is capped to a small, fixed number of entries.
    static std::vector<std::string> GetRecentFiles();
    static std::vector<std::string> GetRecentFolders();
    static void AddRecentFile(const std::string& path);
    static void AddRecentFolder(const std::string& path);
    static void RemoveRecentFile(const std::string& path);
    static void RemoveRecentFolder(const std::string& path);
    static void ClearRecentFiles();
    static void ClearRecentFolders();

    // Runtime plugin discovery and built-in HTTP/WebSocket Editor Server settings.
    // These values are harmless when plugin support is compiled out.
    static PluginRuntimeConfig GetPluginRuntimeConfig();
    static void SetPluginRuntimeConfig(const PluginRuntimeConfig& settings);

    // Dependency-free manual C/C++/Kotlin symbol and call parsing. These
    // settings are harmless when the feature is compiled out.
    static ManualSymbolRuntimeConfig GetManualSymbolRuntimeConfig();
    static void SetManualSymbolRuntimeConfig(const ManualSymbolRuntimeConfig& settings);

    static void Save();
};

#endif // CONFIG_H
