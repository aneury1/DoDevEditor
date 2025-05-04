#include <vector>
#include <string>
#include "plugins.h"

std::vector<Plugin> loadPlugins(const std::string& folderPath) {
    std::vector<Plugin> plugins;

    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (!entry.is_regular_file()) continue;

        std::string path = entry.path().string();
        if (path.find(LIB_EXTENSION) == std::string::npos) continue;

        DYNLIB_HANDLE handle = LOAD_LIBRARY(path.c_str());
        if (!handle) {
            std::cerr << "Failed to load: " << path << std::endl;
            continue;
        }

        auto func = reinterpret_cast<PluginFunc>(GET_PROC_ADDRESS(handle, "registerPlugin"));
        if (!func) {
            std::cerr << "No registerPlugin in: " << path << std::endl;
            CLOSE_LIBRARY(handle);
            continue;
        }

        auto func2 = reinterpret_cast<PluginFunc>(GET_PROC_ADDRESS(handle, "registerStructPlugin"));
        
        if(func2){
            func2();
        }
        std::cout << "Loaded: " << path << std::endl;
    }

    return plugins;
}
