#include <wx/stdpaths.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <json/json.h>
#include "Config.h"

void AppEditorConfig::CreateDefaultConfig()
{
    Json::Value defaultConfig(Json::objectValue);
    defaultConfig["last_opened_folder"] = "";
    defaultConfig["last_workspace_file"] = "";
    defaultConfig["last_workspace_folders"] = Json::Value(Json::arrayValue);
    defaultConfig["editor_theme"] = "vscode";
    config = defaultConfig;
    Save();
}

std::string AppEditorConfig::GetLastOpenedFolder()
{
    return config.get("last_opened_folder", "").asString();
}

std::vector<std::string> AppEditorConfig::GetLastWorkspaceFolders()
{
    std::vector<std::string> result;
    const Json::Value& folders = config["last_workspace_folders"];
    if (folders.isArray())
    {
        for (const auto& folder : folders)
        {
            if (folder.isString() && !folder.asString().empty())
                result.push_back(folder.asString());
        }
    }
    return result;
}

std::string AppEditorConfig::GetLastWorkspaceFile()
{
    return config.get("last_workspace_file", "").asString();
}

void AppEditorConfig::LoadOpenedFolder()
{
    std::ifstream file("config.json");
    if (!file.is_open())
    {
        config = Json::Value(Json::objectValue);
        return;
    }

    Json::CharReaderBuilder builder;
    std::string errors;
    const bool ok = Json::parseFromStream(builder, file, &config, &errors);

    if (!ok)
    {
        std::cerr << errors << std::endl;
        config = Json::Value(Json::objectValue);
    }
}

void AppEditorConfig::SaveOpenedFolder(const std::string& folder)
{
    config["last_opened_folder"] = folder;
    config["last_workspace_file"] = "";
    config["last_workspace_folders"] = Json::Value(Json::arrayValue);
    if (!folder.empty())
        config["last_workspace_folders"].append(folder);
    Save();
}

void AppEditorConfig::SaveWorkspaceSession(const std::vector<std::string>& folders,
                                           const std::string& workspaceFile)
{
    config["last_workspace_folders"] = Json::Value(Json::arrayValue);
    for (const std::string& folder : folders)
    {
        if (!folder.empty())
            config["last_workspace_folders"].append(folder);
    }
    config["last_workspace_file"] = workspaceFile;
    config["last_opened_folder"] = folders.size() == 1 ? folders.front() : "";
    Save();
}

std::string AppEditorConfig::GetEditorTheme()
{
    const std::string theme = config.get("editor_theme", "vscode").asString();
    return theme.empty() ? std::string("vscode") : theme;
}

void AppEditorConfig::SetEditorTheme(const std::string& themeId)
{
    config["editor_theme"] = themeId.empty() ? "vscode" : themeId;
    Save();
}

void AppEditorConfig::Save()
{
    std::ofstream file("config.json", std::ios::trunc);
    if (!file.is_open())
        return;
    file << config.toStyledString();
}
