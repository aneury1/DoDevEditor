#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include "dodev/settings/Json.h"
namespace dodev::gdb {
namespace fs = std::filesystem;
struct Context {
  fs::path workspace, file;
};
struct Launch {
  std::string name;
  fs::path cwd;
  std::vector<std::string> argv;
  bool needsPid = false;
  bool restartable = false;
};
Json readJsonc(const fs::path& path);
std::vector<std::string> names(const Context& context);
Launch makeLaunch(const Context& context, const std::string& name,
                  const std::string& defaultGdb, const std::string& pid = "",
                  bool run = true);
std::string defaultLaunchJson();
std::string quoteGdb(const std::string& value);
}  // namespace dodev::gdb
