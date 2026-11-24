#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace dodev::launch {

struct Request {
  std::vector<std::filesystem::path> paths;
  bool help = false;
  bool version = false;
  bool foreground = false;
  bool wait = false;
  std::string error;
};

// Parse arguments without performing any graphical initialization. Only the
// launcher accepts process-control flags. Relative paths are made absolute
// against the invoking process's working directory.
Request parse(int argc, const char* const argv[], bool launcher);
std::string usage();

}  // namespace dodev::launch
