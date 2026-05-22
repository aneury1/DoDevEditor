
#include <wx/stdpaths.h>
#include <fstream>
#include <iostream>
#include <string>
#include <json/json.h>
#include "Config.h"

void AppEditorConfig::CreateDefaultConfig()
{
    Json::Value defaultConfig;
    defaultConfig["last_opened_folder"] = wxStandardPaths::Get().GetDocumentsDir().ToStdString();
    std::ofstream file("config.json");
    if (file.is_open())
    {
        file.close();
        return;
    }
    file << defaultConfig.toStyledString(); // Pretty print with 4 spaces indentation
    file.close();
}

std::string AppEditorConfig::GetLastOpenedFolder()
{
    return config["last_opened_folder"].asString();
}

void AppEditorConfig::LoadOpenedFolder()
{
    std::ifstream file("config.json");
    if (!file.is_open())
        return;

    Json::CharReaderBuilder builder;
    std::string errors;
    bool ok = Json::parseFromStream(
        builder,
        file,
        &config,
        &errors);

    if (!ok)
    {
        std::cerr << errors << std::endl;
    }
}

void AppEditorConfig::SaveOpenedFolder(const std::string &folder)
{
    config["last_opened_folder"] = folder;

    std::ofstream file("config.json");
    if (!file.is_open())
        return;
    file << config.toStyledString(); // Pretty print with 4 spaces indentation
    file.close();
}

