#include "dodev/plugins/PluginManager.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using namespace dodev;
namespace {
void copyPlugin(const fs::path& source,const fs::path& destination){
    fs::create_directories(destination);
    for(const auto& file:fs::directory_iterator(source))if(file.is_regular_file())
        fs::copy_file(file.path(),destination/file.path().filename(),fs::copy_options::overwrite_existing);
}
}
int main(){
    const auto root=fs::temp_directory_path()/("dodev_tool_plugins_"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try{
        copyPlugin(fs::path(DODEV_TEST_PLUGIN_DIR)/"clangformat",root/"plugins"/"clangformat");
        copyPlugin(fs::path(DODEV_TEST_PLUGIN_DIR)/"docker",root/"plugins"/"docker");
        SettingsManager settings(root);assert(settings.load());
        TemplateRegistry templates;ThemeManager themes;PluginManager manager(settings,templates,themes);
        std::string content="int  example  (  ) { return   1; }\n";
        std::vector<std::string> terminalCommands;
        std::string promptTitle;
        int promptCount=0;
        PluginManager::HostHooks hooks;
        hooks.activeDocument=[&](std::string& filename,std::string& document){filename="example.cpp";document=content;return true;};
        hooks.replaceDocument=[&](const std::string& document){content=document;return true;};
        hooks.terminalCommand=[&](const std::string& command){terminalCommands.push_back(command);return true;};
        hooks.promptInput=[&](const std::string& title,const std::string&,const std::string&,
                            std::function<void(const std::string&)> callback){
            ++promptCount;promptTitle=title;
            if(title.find("clang-format")!=std::string::npos)callback("Google");
            else callback("ps -a");
            return true;
        };
        manager.setHostHooks(std::move(hooks));
        assert(manager.discoverAndLoad(root/"plugins")==0);
        manager.setEnabled("com.dodev.clangformat",true);
        manager.setEnabled("com.dodev.docker",true);
        assert(manager.discoverAndLoad(root/"plugins")==2);
        assert(manager.commands().size()>=10);
        assert(manager.execute("docker.ps"));
        assert(!terminalCommands.empty());
        assert(terminalCommands.back().find("ps")!=std::string::npos);
        assert(manager.execute("docker.compose_up"));
        assert(terminalCommands.back().find("compose")!=std::string::npos);
        assert(manager.execute("docker.prompt"));
        assert(promptCount==1&&promptTitle=="Docker CLI Prompt");
        assert(terminalCommands.back().find("ps")!=std::string::npos);
        assert(manager.execute("clangformat.style_prompt"));
        assert(manager.pluginSetting("com.dodev.clangformat","style")=="Google");
#ifndef _WIN32
        // Substitute a fake clang-format executable to test the buffer roundtrip.
        const auto fake=root/"fake-clang-format";
        {std::ofstream f(fake);f<<"#!/bin/sh\nfor a in \"$@\"; do case \"$a\" in *.cpp) printf 'int formatted() { return 2; }\\n' > \"$a\" ;; esac; done\n";}
        fs::permissions(fake,fs::perms::owner_exec|fs::perms::owner_read|fs::perms::owner_write);
        assert(manager.updatePluginSetting("com.dodev.clangformat","executable",fake.string()));
        assert(manager.execute("clangformat.apply"));
        assert(content=="int formatted() { return 2; }\n");
#endif
        assert(manager.settingSpecs().size()>=8);
        assert(manager.views().size()==2);
        manager.shutdown();
        fs::remove_all(root);
        std::cout<<"clang-format and Docker plugin integration passed\n";
    }catch(...){fs::remove_all(root);throw;}
}
