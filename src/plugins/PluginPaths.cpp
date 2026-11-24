#include "dodev/plugins/PluginPaths.h"

#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif
namespace dodev {
std::filesystem::path executableDirectory() {
#if defined(_WIN32)
  std::vector<wchar_t> buffer(512);
  for (;;) {
    DWORD count = GetModuleFileNameW(nullptr, buffer.data(),
                                     static_cast<DWORD>(buffer.size()));
    if (count == 0) return std::filesystem::current_path();
    if (count < buffer.size())
      return std::filesystem::path(buffer.data()).parent_path();
    if (buffer.size() > 32768) return std::filesystem::current_path();
    buffer.resize(buffer.size() * 2);
  }
#elif defined(__APPLE__)
  uint32_t length = 0;
  _NSGetExecutablePath(nullptr, &length);
  if (length == 0) return std::filesystem::current_path();
  std::vector<char> buffer(length + 1, 0);
  if (_NSGetExecutablePath(buffer.data(), &length) == 0) {
    std::error_code ec;
    auto real = std::filesystem::weakly_canonical(buffer.data(), ec);
    return (ec ? std::filesystem::path(buffer.data()) : real).parent_path();
  }
  return std::filesystem::current_path();
#else
  std::vector<char> buffer(512);
  for (;;) {
    const auto n = readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (n < 0) return std::filesystem::current_path();
    if (static_cast<size_t>(n) < buffer.size())
      return std::filesystem::path(
                 std::string(buffer.data(), static_cast<size_t>(n)))
          .parent_path();
    if (buffer.size() > 1024 * 1024) return std::filesystem::current_path();
    buffer.resize(buffer.size() * 2);
  }
#endif
}
std::filesystem::path bundledPluginDirectory() {
  return executableDirectory() / "plugins";
}
}  // namespace dodev
