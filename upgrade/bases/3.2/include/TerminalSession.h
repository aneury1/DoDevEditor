#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#else
#include <sys/types.h>
#endif

namespace dodev {
namespace fs = std::filesystem;

class TerminalSession {
 public:
  TerminalSession() = default;
  ~TerminalSession();
  TerminalSession(const TerminalSession&) = delete;
  TerminalSession& operator=(const TerminalSession&) = delete;

  // Starts a long-lived real pseudoterminal: forkpty on POSIX, ConPTY on
  // Windows 10+.
  bool start(const fs::path& cwd, std::string* error = nullptr,
             const std::string& shell = {});
  bool sendLine(const std::string& line);
  bool sendBytes(
      const std::string& bytes);  // Raw keyboard and ANSI escape sequences.
  bool resize(int columns, int rows);
  bool interrupt();  // Sends ETX (Ctrl+C) to the live pseudo-console.
  void stop();
  bool running() const noexcept { return running_.load(); }
  int exitCode() const noexcept { return exit_code_.load(); }
  std::string takeOutput();
  static const char* backend() noexcept;

 private:
  void readerLoop();
  void append(const char* data, std::size_t count);
  std::mutex output_mutex_;
  std::string output_;
  std::thread reader_;
  std::atomic<bool> running_{false};
  std::atomic<int> exit_code_{-1};
  std::atomic<bool> stopping_{false};
#ifdef _WIN32
  HANDLE process_ = nullptr;
  HPCON pseudoconsole_ = nullptr;
  HANDLE input_ = nullptr;
  HANDLE output_read_ = nullptr;
  DWORD pid_ = 0;
#else
  int pty_ = -1;
  pid_t pid_ = -1;
#endif
};
}  // namespace dodev
