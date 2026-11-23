#pragma once
#include "dodev/settings/Json.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>
namespace dodev {
struct ThemeDefinition {
    std::string name="Custom Theme",base="Dark",accent="#7C9CFF";
    std::string window="#10131D",panel="#1A2030",control="#29334A",input="#131B29",text="#EDF2FF",separator="#344159";
    std::string keyword="#91A7FF",stringColor="#A6DCAC",comment="#8998AF";
    int cornerRadius=12;
    std::string source;
    std::string originalXml; // preserves native Raven classes on XML round-trips
    bool valid(std::string* error=nullptr)const;
    std::string toXml()const;
    Json toJson()const;
    static bool fromJson(const Json& json,ThemeDefinition& out,std::string* error=nullptr);
    static bool fromXml(const std::string& xml,ThemeDefinition& out,std::string* error=nullptr);
};
class ThemeManager {
public:
    void add(ThemeDefinition theme);
    const ThemeDefinition* find(const std::string& name)const;
    std::vector<std::string> names()const;
    bool importFile(const std::filesystem::path& path,std::string* error=nullptr);
    size_t importDirectory(const std::filesystem::path& directory,std::vector<std::string>* warnings=nullptr);
    bool exportFile(const ThemeDefinition& theme,const std::filesystem::path& file,std::string* error=nullptr)const;
    bool exportFile(const std::string& themeName,const std::filesystem::path& file,std::string* error=nullptr)const;
private:
    std::map<std::string,ThemeDefinition> themes_;
};
}
