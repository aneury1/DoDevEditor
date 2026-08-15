#ifndef CONFIG_H
#define CONFIG_H

#include <wx/stdpaths.h>
#include <fstream>
#include <string>
#include <vector>
#include <json/json.h>

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

    static void Save();
};

#endif // CONFIG_H
