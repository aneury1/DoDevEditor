#pragma once
#include "dodev/settings/Json.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>
namespace dodev {
struct FileTemplate {std::string id,name,extension,body,source;};
class TemplateRegistry {
public:
    TemplateRegistry();
    void registerTemplate(FileTemplate value);
    bool loadDirectory(const std::filesystem::path& path,std::vector<std::string>* warnings=nullptr);
    const FileTemplate* find(const std::string& id)const;
    std::vector<FileTemplate> all()const;
    std::string associatedId(const std::string& extension,const Json& associations)const;
    std::string render(const FileTemplate& value,const std::map<std::string,std::string>& variables)const;
    std::string renderForExtension(const std::string& extension,const Json& associations,const std::map<std::string,std::string>& variables)const;
private:
    std::map<std::string,FileTemplate> templates_;
};
}
