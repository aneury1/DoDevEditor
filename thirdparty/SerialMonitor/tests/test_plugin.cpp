#include "dodev/plugins/PluginManager.h"
#include <stdexcept>
#define REQUIRE(cond) do { if(!(cond)) throw std::runtime_error(#cond); } while(0)
#include <chrono>
#include <filesystem>
#include <iostream>
namespace fs=std::filesystem;
int main(){
    const auto root=fs::temp_directory_path()/("dodev_serial_test_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"plugins"/"serial-monitor");
    try {
        for(const auto& f:fs::directory_iterator(DODEV_SERIAL_PLUGIN_DIR))if(f.is_regular_file())
            fs::copy_file(f.path(),root/"plugins"/"serial-monitor"/f.path().filename(),fs::copy_options::overwrite_existing);
        dodev::SettingsManager settings(root);REQUIRE(settings.load());
        dodev::TemplateRegistry templates;dodev::ThemeManager themes;
        dodev::PluginManager mgr(settings,templates,themes);
        REQUIRE(mgr.discoverAndLoad(root/"plugins")==0);
        mgr.setEnabled("com.dodev.serialmonitor",true);
        int opened=0;dodev::PluginManager::HostHooks hooks;
        hooks.openSerialMonitor=[&]{++opened;return true;};mgr.setHostHooks(std::move(hooks));
        REQUIRE(mgr.discoverAndLoad(root/"plugins")==1);
        REQUIRE(mgr.execute("serialmonitor.open"));REQUIRE(opened==1);
        bool tools=false;for(const auto& menu:mgr.menus())if(menu.location=="Tools"&&menu.command=="serialmonitor.open")tools=true;
        REQUIRE(tools);REQUIRE(mgr.settingSpecs().size()>=5);
        mgr.shutdown();fs::remove_all(root);
    }catch(...){fs::remove_all(root);throw;}
    std::cout<<"serial plugin UI hook and menu registration passed\n";
}
