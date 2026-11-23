#include "dodev/plugins/PluginManager.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <vector>
namespace fs=std::filesystem;
using namespace dodev;
namespace {void label(void* ptr,const char* text){static_cast<std::vector<std::string>*>(ptr)->push_back(text?text:"");}
void button(void* ptr,const char* title,const char*){static_cast<std::vector<std::string>*>(ptr)->push_back(title?title:"");}
void text(void* ptr,const char* content){static_cast<std::vector<std::string>*>(ptr)->push_back(content?content:"");}}
int main(){
    const auto root=fs::temp_directory_path()/("dodev_plugin_test_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"plugins"/"hello");
    try{
        for(const auto& entry:fs::directory_iterator(fs::path(DODEV_TEST_PLUGIN_DIR)/"hello"))
            if(entry.is_regular_file())fs::copy_file(entry.path(),root/"plugins"/"hello"/entry.path().filename(),fs::copy_options::overwrite_existing);
        SettingsManager settings(root);settings.load();TemplateRegistry templates;ThemeManager themes;
        PluginManager manager(settings,templates,themes);
        assert(manager.discoverAndLoad(root/"plugins")==0);assert(manager.records().size()==1);
        assert(manager.records()[0].status.find("Disabled")!=std::string::npos);
        manager.setEnabled("com.dodev.hello",true);assert(settings.save());
        assert(manager.discoverAndLoad(root/"plugins")==1);
        assert(manager.commands().size()==1);assert(manager.menus().size()==1);
        assert(manager.views().size()==2);assert(manager.settingSpecs().size()==1);
        assert(templates.find("hello-file"));
        assert(manager.execute("hello.say"));assert(manager.updatePluginSetting("com.dodev.hello","greeting","From CTest"));
        assert(manager.pluginSetting("com.dodev.hello","greeting")=="From CTest");
        manager.emit("document.opened",R"({"path":"/tmp/example.cpp"})");
        std::vector<std::string> strings;const DodevUiBuilder builder{sizeof(DodevUiBuilder),&strings,&label,&button,&text};
        manager.buildView(manager.views()[0],builder);
        assert(!strings.empty());
        manager.setEnabled("com.dodev.hello",false);assert(!manager.isEnabled("com.dodev.hello"));
        manager.shutdown();manager.shutdown();assert(manager.commands().empty());assert(manager.views().empty());
    }catch(...){fs::remove_all(root);throw;}
    fs::remove_all(root);std::cout<<"Native plugin integration tests passed\n";
}
