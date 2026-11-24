#include "dodev/settings/SettingsManager.h"
#include "dodev/themes/ThemeManager.h"
#include "dodev/templates/TemplateRegistry.h"
#include "EmbeddedUi.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
using namespace dodev;
namespace fs=std::filesystem;
int main(){
    const auto dir=fs::temp_directory_path()/("dodev_3_test_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    try{
        SettingsManager settings(dir);assert(settings.load());assert(settings.getInt("editor.fontSize")==14);assert(!fs::exists(settings.file()));
        Json src;std::string err;assert(Json::parse(R"({"hello":"\\u0041","array":[1,2,true,null],"obj":{"word":"\u00E1"}})",src,&err));
        assert(src["array"].asArray().size()==4);assert(src["obj"]["word"].string()=="á");
        assert(!Json::parse("{\"x\":12,}",src,&err));assert(!Json::parse("{\"duplicate\":1,\"duplicate\":2}",src,&err));
        settings.set("editor.fontSize",18);settings.set("appearance.theme","VS Code Dark Modern");assert(settings.save(&err));
        SettingsManager restored(dir);assert(restored.load());assert(restored.getInt("editor.fontSize")==18);
        assert(restored.getString("appearance.theme")=="VS Code Dark Modern");
        assert(restored.savePluginSettings("com.dodev.hello",Json::Object{{"greeting","Hi"}}));
        assert(restored.loadPluginSettings("com.dodev.hello")["greeting"].string()=="Hi");
        assert(!SettingsManager::validPluginId("../../bad"));
        fs::remove(restored.file());{std::ofstream out(dir/"settings.ini");out<<"theme=Music Daylight\nfolder=/tmp/myproject\n";}
        SettingsManager legacy(dir);assert(legacy.load());assert(legacy.getString("appearance.theme")=="Music Daylight");
        {std::ofstream out(dir/"settings.json");out<<"{ malformed ";}
        SettingsManager invalid(dir);assert(!invalid.load());assert(!invalid.maySave());assert(!invalid.save(&err));
        assert(fs::exists(dir/"settings.json"));
        // Verify every embedded Raven theme can be registered without its GUI runtime.
        ThemeManager embedded;
        for (const auto& original : dodev::ui::kThemes) {
            ThemeDefinition builtIn;std::string issue;
            assert(ThemeDefinition::fromXml(std::string(original.xml),builtIn,&issue));
            assert(builtIn.name == original.name);
            embedded.add(std::move(builtIn));
        }
        assert(embedded.names().size() == dodev::ui::kThemes.size());
        ThemeDefinition theme;theme.name="Test Azure";theme.accent="#5678FE";assert(theme.valid());
        ThemeManager themes;themes.add(theme);assert(themes.exportFile("Test Azure",dir/"themes"/"test.json",&err));
        assert(themes.exportFile("Test Azure",dir/"themes"/"test.xml",&err));
        ThemeManager imported;assert(imported.importDirectory(dir/"themes")==2);assert(imported.find("Test Azure"));
        ThemeDefinition fromXml;assert(ThemeDefinition::fromXml(theme.toXml(),fromXml));assert(fromXml.accent=="#5678FE");
        theme.accent="red";assert(!theme.valid());
        TemplateRegistry templates;const auto data=templates.renderForExtension(".h",SettingsManager::defaultSettings()["templates"]["associations"],{{"FILE_NAME","Example.h"},{"CLASS_NAME","Example"},{"PROJECT_NAME","My IDE"}});
        assert(data.find("class Example")!=std::string::npos);assert(data.find("Example.h")!=std::string::npos);
        fs::create_directories(dir/"customTemplates");
        {std::ofstream f(dir/"customTemplates"/"templates.json");f<<R"({"templates":[{"id":"sql-test","name":"SQL","extension":".sql","file":"sql.tpl"}]})";}
        {std::ofstream f(dir/"customTemplates"/"sql.tpl");f<<"SELECT '${PROJECT_NAME}';\n";}
        assert(templates.loadDirectory(dir/"customTemplates"));assert(templates.find("sql-test")!=nullptr);
        assert(templates.render(*templates.find("sql-test"),{{"PROJECT_NAME","Raven"}})=="SELECT 'Raven';\n");
    }catch(...){fs::remove_all(dir);throw;}
    fs::remove_all(dir);std::cout<<"Settings, JSON, themes and template tests passed\n";
}
