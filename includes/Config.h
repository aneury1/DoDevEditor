#ifndef CONFIG_H
#define CONFIG_H
#include <wx/stdpaths.h>
#include <fstream>
#include <string>
#include <json/json.h>

struct AppEditorConfig
{
    static Json::Value config;

    static void CreateDefaultConfig();

    static std::string GetLastOpenedFolder();

    static void LoadOpenedFolder();

    static void SaveOpenedFolder(const std::string &folder);
};

#endif // CONFIG_H