#include <cerrno>
#include <chrono>
#include <fstream>
#include <random>
#include <stdexcept>
#include <thread>

#include "FilePreview.h"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif
namespace preview {
namespace {
struct Scratch {
  std::filesystem::path path;
  Scratch() {
    auto base = std::filesystem::temp_directory_path();
    std::random_device rng;
    for (int i = 0; i < 20; ++i) {
      auto p = base / ("dodev-preview-" + std::to_string(rng()) + "-" +
                       std::to_string(rng()));
      if (std::filesystem::create_directory(p)) {
        path = p;
        std::filesystem::permissions(path, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
        return;
      }
    }
    throw std::runtime_error("Cannot create temporary renderer directory");
  }
  ~Scratch() {
    std::error_code e;
    std::filesystem::remove_all(path, e);
  }
};
#ifdef _WIN32
std::wstring quote(const std::wstring& s) {
  std::wstring out = L"\"";
  size_t n = 0;
  for (auto c : s) {
    if (c == L'\\') {
      ++n;
      continue;
    }
    if (c == L'\"') {
      out.append(n * 2 + 1, L'\\');
      out += c;
    } else {
      out.append(n, L'\\');
      out += c;
    }
    n = 0;
  }
  out.append(n * 2, L'\\');
  return out + L"\"";
}
#endif
void run(const std::vector<std::string>& a) {
#ifdef _WIN32
  std::wstring command;
  for (auto& arg : a) {
    if (!command.empty()) command += L' ';
    command += quote(std::filesystem::u8path(arg).wstring());
  }
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    throw std::runtime_error("Renderer not executable: " + a.front());
  CloseHandle(pi.hThread);
  auto status = WaitForSingleObject(pi.hProcess, 30000);
  if (status != WAIT_OBJECT_0) {
    TerminateProcess(pi.hProcess, 1);
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);
    throw std::runtime_error("Renderer timeout");
  }
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hProcess);
  if (code)
    throw std::runtime_error("Renderer returned error " + std::to_string(code));
#else
  std::vector<char*> argv;
  for (auto& arg : a) argv.push_back(const_cast<char*>(arg.c_str()));
  argv.push_back(nullptr);
  pid_t pid;
  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);
  posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
  posix_spawnattr_setpgroup(&attr, 0);
  int err = posix_spawnp(&pid, argv[0], nullptr, &attr, argv.data(), environ);
  posix_spawnattr_destroy(&attr);
  if (err) throw std::runtime_error("Renderer not found: " + a.front());
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  int status = 0;
  while (true) {
    auto r = waitpid(pid, &status, WNOHANG);
    if (r == pid) break;
    if (r < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error("Renderer wait failed");
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      kill(-pid, SIGKILL);
      waitpid(pid, &status, 0);
      throw std::runtime_error("Renderer timeout");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status))
    throw std::runtime_error("Renderer failed: " + a.front());
#endif
}
}  // namespace
std::string externalImage(const std::filesystem::path& path,
                          const std::string& source, const std::string& ext,
                          const std::string& executable, int page) {
  if (executable.empty())
    throw std::runtime_error("Renderer is not configured");
  Scratch tmp;
  auto input = tmp.path / ("source" + ext), output = tmp.path / "preview.png";
  if (ext == ".pdf") {
    if (!std::filesystem::is_regular_file(path) ||
        std::filesystem::file_size(path) > 64u * 1024 * 1024)
      throw std::runtime_error("PDF size limit: 64 MiB");
    std::filesystem::copy_file(path, input);
  } else {
    if (source.size() > MaxInput)
      throw std::runtime_error("Diagram exceeds preview size limit");
    std::ofstream f(input, std::ios::binary);
    f.write(source.data(), source.size());
    if (!f) throw std::runtime_error("Cannot write renderer input");
  }
  auto in = input.u8string(), out = output.u8string();
  if (ext == ".pdf") {
    if (page < 1 || page > 10000) throw std::runtime_error("Invalid PDF page");
    auto p = std::to_string(page);
    run({executable, "-f", p, "-l", p, "-scale-to", "1600", "-singlefile",
         "-png", in, (tmp.path / "preview").u8string()});
  } else if (ext == ".svg")
    run({executable, "--width", "1600", "--keep-aspect-ratio", "-o", out, in});
  else if (ext == ".puml" || ext == ".plantuml") {
    run({executable, "-DPLANTUML_SECURITY_PROFILE=SECURE", "-tpng", "-o",
         tmp.path.u8string(), in});
    output = tmp.path / "source.png";
  } else if (ext == ".mmd" || ext == ".mermaid")
    run({executable, "-i", in, "-o", out, "-b", "white"});
  else
    throw std::runtime_error("Unsupported external renderer");
  return readBounded(output, 16 * 1024 * 1024);
}
}  // namespace preview
