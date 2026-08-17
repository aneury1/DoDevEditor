#ifndef CONFIG_H
#define CONFIG_H

#include <wx/stdpaths.h>
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

    // Dependency-free manual C/C++/Kotlin symbol and call parsing. These
    // settings are harmless when the feature is compiled out.
    static ManualSymbolRuntimeConfig GetManualSymbolRuntimeConfig();
    static void SetManualSymbolRuntimeConfig(const ManualSymbolRuntimeConfig& settings);

    static void Save();
};

#endif // CONFIG_H
