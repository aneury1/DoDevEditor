#include <wx/stdpaths.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <json/json.h>
#include "Config.h"

namespace
{
constexpr size_t kRecentLimit = 15;

std::vector<std::string> ReadStringArray(const Json::Value& value)
{
    std::vector<std::string> result;
    if (!value.isArray())
        return result;

    for (const auto& entry : value)
    {
        if (entry.isString() && !entry.asString().empty())
            result.push_back(entry.asString());
    }
    return result;
}

void WriteStringArray(Json::Value& target, const std::vector<std::string>& values)
{
    target = Json::Value(Json::arrayValue);
    for (const std::string& value : values)
    {
        if (!value.empty())
            target.append(value);
    }
}

void AddRecent(Json::Value& target, const std::string& path)
{
    if (path.empty())
        return;

    std::vector<std::string> values = ReadStringArray(target);
    values.erase(std::remove(values.begin(), values.end(), path), values.end());
    values.insert(values.begin(), path);
    if (values.size() > kRecentLimit)
        values.resize(kRecentLimit);
    WriteStringArray(target, values);
}

void RemoveRecent(Json::Value& target, const std::string& path)
{
    std::vector<std::string> values = ReadStringArray(target);
    values.erase(std::remove(values.begin(), values.end(), path), values.end());
    WriteStringArray(target, values);
}
}

void AppEditorConfig::CreateDefaultConfig()
{
    Json::Value defaultConfig(Json::objectValue);
    defaultConfig["last_opened_folder"] = "";
    defaultConfig["last_workspace_file"] = "";
    defaultConfig["last_workspace_folders"] = Json::Value(Json::arrayValue);
    defaultConfig["editor_theme"] = "vscode";
    defaultConfig["recent_files"] = Json::Value(Json::arrayValue);
    defaultConfig["recent_folders"] = Json::Value(Json::arrayValue);
    defaultConfig["editor_view"]["whitespace"] = false;
    defaultConfig["editor_view"]["eol"] = false;
    defaultConfig["editor_view"]["control_characters"] = false;
    defaultConfig["plugins"]["directory"] = "";
    defaultConfig["plugins"]["http_editor_server"]["bind_address"] = "0.0.0.0";
    defaultConfig["plugins"]["http_editor_server"]["port"] = 9934;
    defaultConfig["plugins"]["http_editor_server"]["auth_token"] = "";
    defaultConfig["plugins"]["http_editor_server"]["max_text_bytes"] = Json::UInt64(4u * 1024u * 1024u);
    defaultConfig["plugins"]["disabled"] = Json::Value(Json::arrayValue);
    Json::Value manual(Json::objectValue);
    manual["enabled"] = true;
    manual["auto_parse"] = true;
    manual["languages"]["c"] = true;
    manual["languages"]["cpp"] = true;
    manual["languages"]["kotlin"] = true;
    manual["features"]["symbols"] = true;
    manual["features"]["calls"] = true;
    manual["features"]["types"] = true;
    manual["features"]["functions"] = true;
    manual["features"]["variables"] = true;
    manual["features"]["macros"] = true;
    defaultConfig["manual_symbol_parser"] = manual;
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

EditorViewRuntimeConfig AppEditorConfig::GetEditorViewRuntimeConfig()
{
    EditorViewRuntimeConfig settings;
    const Json::Value& root = config["editor_view"];
    if (!root.isObject())
        return settings;
    settings.whitespaceVisible = root.get("whitespace", false).asBool();
    settings.eolVisible = root.get("eol", false).asBool();
    settings.controlCharactersVisible = root.get("control_characters", false).asBool();
    return settings;
}

void AppEditorConfig::SetEditorViewRuntimeConfig(const EditorViewRuntimeConfig& settings)
{
    config["editor_view"]["whitespace"] = settings.whitespaceVisible;
    config["editor_view"]["eol"] = settings.eolVisible;
    config["editor_view"]["control_characters"] = settings.controlCharactersVisible;
    Save();
}

std::vector<std::string> AppEditorConfig::GetRecentFiles()
{
    return ReadStringArray(config["recent_files"]);
}

std::vector<std::string> AppEditorConfig::GetRecentFolders()
{
    return ReadStringArray(config["recent_folders"]);
}

void AppEditorConfig::AddRecentFile(const std::string& path)
{
    AddRecent(config["recent_files"], path);
    Save();
}

void AppEditorConfig::AddRecentFolder(const std::string& path)
{
    AddRecent(config["recent_folders"], path);
    Save();
}

void AppEditorConfig::RemoveRecentFile(const std::string& path)
{
    RemoveRecent(config["recent_files"], path);
    Save();
}

void AppEditorConfig::RemoveRecentFolder(const std::string& path)
{
    RemoveRecent(config["recent_folders"], path);
    Save();
}

void AppEditorConfig::ClearRecentFiles()
{
    config["recent_files"] = Json::Value(Json::arrayValue);
    Save();
}

void AppEditorConfig::ClearRecentFolders()
{
    config["recent_folders"] = Json::Value(Json::arrayValue);
    Save();
}


PluginRuntimeConfig AppEditorConfig::GetPluginRuntimeConfig()
{
    PluginRuntimeConfig settings;
    const Json::Value& root = config["plugins"];
    if (!root.isObject())
        return settings;

    settings.directory = root.get("directory", "").asString();
    const Json::Value& http = root["http_editor_server"];
    if (http.isObject())
    {
        settings.httpBindAddress = http.get("bind_address", "0.0.0.0").asString();
        if (settings.httpBindAddress.empty())
            settings.httpBindAddress = "0.0.0.0";
        const int port = http.get("port", 9934).asInt();
        settings.httpPort = (port >= 1 && port <= 65535) ? port : 9934;
        settings.httpAuthToken = http.get("auth_token", "").asString();
        const Json::UInt64 maxBytes = http.get("max_text_bytes", Json::UInt64(4u * 1024u * 1024u)).asUInt64();
        settings.httpMaxTextBytes = maxBytes > 0 ? static_cast<size_t>(maxBytes) : 4u * 1024u * 1024u;
    }

    const Json::Value& disabled = root["disabled"];
    if (disabled.isArray())
    {
        for (const auto& value : disabled)
        {
            if (value.isString() && !value.asString().empty())
                settings.disabledPlugins.push_back(value.asString());
        }
    }
    return settings;
}

void AppEditorConfig::SetPluginRuntimeConfig(const PluginRuntimeConfig& settings)
{
    Json::Value root(Json::objectValue);
    root["directory"] = settings.directory;
    root["http_editor_server"]["bind_address"] = settings.httpBindAddress.empty() ? "0.0.0.0" : settings.httpBindAddress;
    root["http_editor_server"]["port"] = (settings.httpPort >= 1 && settings.httpPort <= 65535) ? settings.httpPort : 9934;
    root["http_editor_server"]["auth_token"] = settings.httpAuthToken;
    root["http_editor_server"]["max_text_bytes"] = Json::UInt64(settings.httpMaxTextBytes > 0 ? settings.httpMaxTextBytes : 4u * 1024u * 1024u);
    root["disabled"] = Json::Value(Json::arrayValue);
    for (const std::string& plugin : settings.disabledPlugins)
    {
        if (!plugin.empty())
            root["disabled"].append(plugin);
    }
    config["plugins"] = root;
    Save();
}

ManualSymbolRuntimeConfig AppEditorConfig::GetManualSymbolRuntimeConfig()
{
    ManualSymbolRuntimeConfig settings;
    const Json::Value& root = config["manual_symbol_parser"];
    if (!root.isObject())
        return settings;

    settings.enabled = root.get("enabled", true).asBool();
    settings.autoParse = root.get("auto_parse", true).asBool();
    const Json::Value& languages = root["languages"];
    settings.cEnabled = languages.get("c", true).asBool();
    settings.cppEnabled = languages.get("cpp", true).asBool();
    settings.kotlinEnabled = languages.get("kotlin", true).asBool();
    const Json::Value& features = root["features"];
    settings.symbolsEnabled = features.get("symbols", true).asBool();
    settings.callsEnabled = features.get("calls", true).asBool();
    settings.typesEnabled = features.get("types", true).asBool();
    settings.functionsEnabled = features.get("functions", true).asBool();
    settings.variablesEnabled = features.get("variables", true).asBool();
    settings.macrosEnabled = features.get("macros", true).asBool();
    return settings;
}

void AppEditorConfig::SetManualSymbolRuntimeConfig(const ManualSymbolRuntimeConfig& settings)
{
    Json::Value root(Json::objectValue);
    root["enabled"] = settings.enabled;
    root["auto_parse"] = settings.autoParse;
    root["languages"]["c"] = settings.cEnabled;
    root["languages"]["cpp"] = settings.cppEnabled;
    root["languages"]["kotlin"] = settings.kotlinEnabled;
    root["features"]["symbols"] = settings.symbolsEnabled;
    root["features"]["calls"] = settings.callsEnabled;
    root["features"]["types"] = settings.typesEnabled;
    root["features"]["functions"] = settings.functionsEnabled;
    root["features"]["variables"] = settings.variablesEnabled;
    root["features"]["macros"] = settings.macrosEnabled;
    config["manual_symbol_parser"] = root;
    Save();
}

void AppEditorConfig::Save()
{
    std::ofstream file("config.json", std::ios::trunc);
    if (!file.is_open())
        return;
    file << config.toStyledString();
}
