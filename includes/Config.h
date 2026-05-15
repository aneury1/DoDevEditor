#ifndef CONFIG_H
#define CONFIG_H
#include <wx/stdpaths.h>
#include <fstream>
#include <string>
#include <json/json.h>


struct AppEditorConfig{
    static Json::Value config;

    static void CreateDefaultConfig() {
        Json::Value defaultConfig;
        defaultConfig["last_opened_folder"] = wxStandardPaths::Get().GetDocumentsDir().ToStdString();
        std::ofstream file("config.json");
        if (file.is_open()) {
            file.close();
            return;
        }
        file << defaultConfig.toStyledString(); // Pretty print with 4 spaces indentation
        file.close();
    }

    static std::string GetLastOpenedFolder()   {
        return config["last_opened_folder"].asString();
    }

    static void LoadOpenedFolder() {
        std::ifstream file("config.json");   
        if (!file.is_open()) return;
     
        Json::CharReaderBuilder builder;
        std::string errors;
        bool ok = Json::parseFromStream(
            builder,
            file,
            &config,
            &errors
        );

        if (!ok)
        {
            std::cerr << errors << std::endl;
        } 
    }
 
   static void SaveOpenedFolder(const std::string& folder) {
        config["last_opened_folder"] = folder;
        
     
        std::ofstream file("config.json");
        if (!file.is_open()) return;
        file << config.toStyledString(); // Pretty print with 4 spaces indentation
        file.close();
    }

};


#endif // CONFIG_H