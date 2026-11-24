#include "dodev/launch/LaunchArgs.h"

#include <system_error>

namespace dodev::launch {
Request parse(int argc, const char* const argv[], bool launcher) {
  Request request;
  bool positional = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i] ? argv[i] : "";
    if (!positional && arg == "--") {
      positional = true;
      continue;
    }
    if (!positional && (arg == "-h" || arg == "--help")) {
      request.help = true;
      continue;
    }
    if (!positional && arg == "--version") {
      request.version = true;
      continue;
    }
    if (launcher && !positional && arg == "--foreground") {
      request.foreground = true;
      continue;
    }
    if (launcher && !positional && arg == "--wait") {
      request.wait = true;
      continue;
    }
    if (!positional && !arg.empty() && arg[0] == '-') {
      request.error = "Unknown argument: " + arg +
                      " (use -- to open a file whose name starts with '-')";
      return request;
    }
    if (arg.empty()) continue;
    std::error_code error;
    auto path = std::filesystem::absolute(std::filesystem::u8path(arg), error);
    if (error) {
      request.error = "Cannot resolve path: " + arg + ": " + error.message();
      return request;
    }
    path = path.lexically_normal();
    if (path.has_relative_path() && path.filename().empty())
      path = path.parent_path();
    request.paths.push_back(path);
  }
  return request;
}

std::string usage() {
  return "Usage: dodeveditor (or DoDevEditor) [--foreground | --wait] [--] "
         "[FILE | FOLDER]...\n"
         "  dodeveditor .                Open current directory (detached)\n"
         "  dodeveditor main.cpp other.h Open multiple files\n"
         "  dodeveditor --foreground .   Keep process in terminal (debugging)\n"
         "  dodeveditor --wait file.cpp  Wait until editor exits\n"
         "  dodeveditor --help           Show this help\n"
         "  dodeveditor --version        Show launcher version\n"
         "Opening a second time currently starts another editor window.\n";
}
}  // namespace dodev::launch
