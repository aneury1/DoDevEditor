#include "dodev/launch/ProcessLaunch.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "dodev/launch/LaunchArgs.h"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <shellapi.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

namespace fs = std::filesystem;

namespace {
fs::path selfExecutable(const char* argv0) {
#ifdef _WIN32
  std::wstring path(32768, L'\0');
  const DWORD length =
      GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  if (length && length < path.size()) {
    path.resize(length);
    return fs::path(path);
  }
#elif defined(__linux__)
  std::error_code ec;
  const auto path = fs::read_symlink("/proc/self/exe", ec);
  if (!ec) return path;
#endif
  const fs::path raw = fs::u8path(argv0 ? argv0 : "");
  if (raw.has_parent_path()) return fs::absolute(raw);
  // Only reached on systems without /proc/self/exe and where argv[0] has no
  // directory.
  return fs::current_path() / raw;
}

#ifndef _WIN32
fs::path logPath() {
  const char* xdg = getenv("XDG_STATE_HOME");
  const char* home = getenv("HOME");
  fs::path base;
  if (xdg && *xdg && fs::path(xdg).is_absolute())
    base = fs::path(xdg);
  else if (home && *home)
    base = fs::path(home) / ".local/state";
  else
    base = fs::temp_directory_path();
  return base / "DoDevEditor" / "launcher.log";
}
void redirectStdio() {
  const int input = open("/dev/null", O_RDONLY);
  if (input >= 0) {
    dup2(input, STDIN_FILENO);
    if (input > STDERR_FILENO) close(input);
  }
  fs::path log;
  try {
    log = logPath();
    std::error_code ec;
    fs::create_directories(log.parent_path(), ec);
  } catch (...) {
  }
  int output = -1;
  if (!log.empty())
    output = open(log.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0600);
  if (output < 0) output = open("/dev/null", O_WRONLY);
  if (output >= 0) {
    dup2(output, STDOUT_FILENO);
    dup2(output, STDERR_FILENO);
    if (output > STDERR_FILENO) close(output);
  }
}
int launchPosix(const fs::path& executable,
                const std::vector<std::string>& args, bool foreground) {
  std::string binary = executable.string();
  std::vector<std::string> strings{binary};
  strings.insert(strings.end(), args.begin(), args.end());
  std::vector<char*> values;
  for (auto& item : strings) values.push_back(item.data());
  values.push_back(nullptr);
  if (foreground) {
    execv(binary.c_str(), values.data());
    std::cerr << "Could not start " << binary << ": " << strerror(errno)
              << '\n';
    return 127;
  }
  int errorPipe[2];
  if (pipe(errorPipe) != 0) {
    std::cerr << "pipe: " << strerror(errno) << '\n';
    return 1;
  }
  // If execv succeeds, CLOEXEC closes the writer and the parent immediately
  // returns.
  const int flags = fcntl(errorPipe[1], F_GETFD);
  if (flags < 0 || fcntl(errorPipe[1], F_SETFD, flags | FD_CLOEXEC) < 0) {
    close(errorPipe[0]);
    close(errorPipe[1]);
    return 1;
  }
  const pid_t child = fork();
  if (child < 0) {
    std::cerr << "fork: " << strerror(errno) << '\n';
    close(errorPipe[0]);
    close(errorPipe[1]);
    return 1;
  }
  if (child == 0) {
    close(errorPipe[0]);
    if (setsid() < 0) {
      int e = errno;
      (void)write(errorPipe[1], &e, sizeof(e));
      _exit(127);
    }
    signal(SIGHUP, SIG_IGN);
    const pid_t grandchild = fork();
    if (grandchild < 0) {
      int e = errno;
      (void)write(errorPipe[1], &e, sizeof(e));
      _exit(127);
    }
    if (grandchild > 0) _exit(0);
    redirectStdio();
    execv(binary.c_str(), values.data());
    const int e = errno;
    (void)write(errorPipe[1], &e, sizeof(e));
    _exit(127);
  }
  close(errorPipe[1]);
  int childError = 0;
  ssize_t count;
  do {
    count = read(errorPipe[0], &childError, sizeof(childError));
  } while (count < 0 && errno == EINTR);
  close(errorPipe[0]);
  int status = 0;
  while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
  }
  if (count > 0) {
    std::cerr << "Failed to launch DoDevEditor: " << strerror(childError)
              << '\n';
    return 127;
  }
  if (count < 0) {
    std::cerr << "launcher: " << strerror(errno) << '\n';
    return 1;
  }
  return 0;
}
#else
std::wstring quoteWin(const std::wstring& input) {
  // Correct quoting for CreateProcessW (including trailing backslashes).
  std::wstring result = L"\"";
  unsigned backslashes = 0;
  for (const wchar_t c : input) {
    if (c == L'\\') {
      ++backslashes;
      continue;
    }
    if (c == L'\"') {
      result.append(backslashes * 2 + 1, L'\\');
      result.push_back(L'\"');
      backslashes = 0;
      continue;
    }
    result.append(backslashes, L'\\');
    backslashes = 0;
    result.push_back(c);
  }
  result.append(backslashes * 2, L'\\');
  result.push_back(L'\"');
  return result;
}
int launchWindows(const fs::path& executable,
                  const std::vector<std::string>& args, bool foreground) {
  std::wstring command = quoteWin(executable.wstring());
  for (const auto& item : args) {
    command.push_back(L' ');
    command += quoteWin(fs::u8path(item).wstring());
  }
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  DWORD flags = foreground ? 0 : (DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP);
  if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr,
                      FALSE, flags, nullptr, nullptr, &startup, &process)) {
    std::cerr << "Could not launch DoDevEditor (Win32 error " << GetLastError()
              << ")\n";
    return 127;
  }
  CloseHandle(process.hThread);
  if (foreground) {
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD result = 1;
    GetExitCodeProcess(process.hProcess, &result);
    CloseHandle(process.hProcess);
    return static_cast<int>(result);
  }
  CloseHandle(process.hProcess);
  return 0;
}
#endif
}  // namespace

namespace dodev::launch {
int startEditor(int argc, char** argv, bool direct,
                int (*runGui)(int, char**)) {
  const auto request = parse(argc, const_cast<const char* const*>(argv), true);
  if (!request.error.empty()) {
    std::cerr << request.error << "\n" << usage();
    return 2;
  }
  if (request.help) {
    std::cout << usage();
    return 0;
  }
  if (request.version) {
    std::cout << "DoDevEditor " DODEV_VERSION "\n";
    return 0;
  }
  std::vector<std::string> paths;
  for (const auto& path : request.paths) paths.push_back(path.u8string());
  if (direct && (request.foreground || request.wait)) {
    std::vector<std::string> args{argv[0], "--"};
    args.insert(args.end(), paths.begin(), paths.end());
    std::vector<char*> values;
    for (auto& arg : args) values.push_back(arg.data());
    values.push_back(nullptr);
    return runGui(static_cast<int>(args.size()), values.data());
  }
  const auto self = selfExecutable(argv[0]);
#ifdef _WIN32
  const auto editor = direct ? self : self.parent_path() / "DoDevEditor.exe";
#else
  const auto editor = direct ? self : self.parent_path() / "DoDevEditor";
#endif
  if (!fs::exists(editor)) {
    std::cerr << "Editor executable not found: " << editor << "\n";
    return 127;
  }
  paths.insert(paths.begin(), {"--foreground", "--"});
#ifdef _WIN32
  return launchWindows(editor, paths, request.foreground || request.wait);
#else
  return launchPosix(editor, paths, request.foreground || request.wait);
#endif
}
}  // namespace dodev::launch
