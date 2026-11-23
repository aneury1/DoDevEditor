#pragma once
#include <filesystem>
namespace dodev {
// Resolve application-installed plugins alongside the executable, not cwd.
// For a normal install: <prefix>/bin/DoDevEditor -> <prefix>/bin/plugins/.
std::filesystem::path executableDirectory();
std::filesystem::path bundledPluginDirectory();
}
