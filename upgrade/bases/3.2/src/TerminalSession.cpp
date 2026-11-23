#include "TerminalSession.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <exception>
#include <utility>

#ifndef _WIN32
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#include <vector>
#endif

namespace dodev {

TerminalSession::~TerminalSession() { stop(); }
const char *TerminalSession::backend() noexcept {
#ifdef _WIN32
  return "Windows ConPTY";
#else
  return "POSIX PTY";
#endif
}
void TerminalSession::append(const char *bytes, std::size_t size) {
  std::lock_guard<std::mutex> lk(output_mutex_);
  output_.append(bytes, size);
  // Limit memory even if GUI stops pumping for a moment.
  if (output_.size() > 2 * 1024 * 1024)
    output_.erase(0, output_.size() - 1024 * 1024);
}
std::string TerminalSession::takeOutput() {
  std::lock_guard<std::mutex> lk(output_mutex_);
  std::string chunk;
  chunk.swap(output_);
  return chunk;
}

#ifndef _WIN32
bool TerminalSession::start(const fs::path &cwd, std::string *error,
                            const std::string &shell) {
  stop();
  {
    std::lock_guard<std::mutex> lk(output_mutex_);
    output_.clear();
  }
  stopping_ = false;
  exit_code_ = -1;
  struct winsize ws{};
  ws.ws_row = 30;
  ws.ws_col = 120;
  const pid_t child = forkpty(&pty_, nullptr, nullptr, &ws);
  if (child < 0) {
    if (error)
      *error = std::string("forkpty: ") + std::strerror(errno);
    pty_ = -1;
    return false;
  }
  if (child == 0) {
    if (!cwd.empty() && chdir(cwd.c_str()) != 0)
      _exit(126);
    setenv("TERM", "xterm", 1);
    const char *preferred = getenv("SHELL");
    // A selectable profile; otherwise use the user's login shell.
    const std::string program =
        shell.empty() ? (preferred && *preferred ? preferred : "/bin/sh")
                      : shell;
    execlp(program.c_str(), program.c_str(), "-i",
           static_cast<char *>(nullptr));
    execl("/bin/sh", "sh", "-i", static_cast<char *>(nullptr));
    _exit(127);
  }
  pid_ = child;
  running_ = true;
  try {
    reader_ = std::thread(&TerminalSession::readerLoop, this);
  } catch (const std::exception &e) {
    if (error)
      *error = e.what();
    kill(-pid_, SIGKILL);
    kill(pid_, SIGKILL);
    int status = 0;
    while (waitpid(pid_, &status, 0) < 0 && errno == EINTR) {
    }
    close(pty_);
    pty_ = -1;
    pid_ = -1;
    running_ = false;
    return false;
  }
  return true;
}
void TerminalSession::readerLoop() {
  char buf[4096];
  while (!stopping_) {
    struct pollfd pf{pty_, POLLIN, 0};
    const int pol = poll(&pf, 1, 120);
    if (pol < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    if (pol == 0)
      continue;
    if (!(pf.revents & (POLLIN | POLLHUP | POLLERR)))
      continue;
    const ssize_t n = read(pty_, buf, sizeof(buf));
    if (n > 0)
      append(buf, static_cast<std::size_t>(n));
    else if (n < 0 && errno == EINTR)
      continue;
    else if (n < 0 && errno == EAGAIN)
      continue;
    else
      break; // EIO from a closed PTY is normal after shell exit.
  }
  int status = 0;
  pid_t result;
  do {
    result = waitpid(pid_, &status, 0);
  } while (result < 0 && errno == EINTR);
  if (result > 0) {
    if (WIFEXITED(status))
      exit_code_ = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
      exit_code_ = 128 + WTERMSIG(status);
  }
  running_ = false;
}
bool TerminalSession::sendLine(const std::string &line) {
  return sendBytes(line + "\n");
}
bool TerminalSession::sendBytes(const std::string &data) {
  if (!running_ || pty_ < 0)
    return false;
  std::size_t sent = 0;
  while (sent < data.size()) {
    const ssize_t n = write(pty_, data.data() + sent, data.size() - sent);
    if (n > 0)
      sent += static_cast<std::size_t>(n);
    else if (n < 0 && errno == EINTR)
      continue;
    else
      return false;
  }
  return true;
}
bool TerminalSession::interrupt() { return sendBytes("\x03"); }
bool TerminalSession::resize(int columns, int rows) {
  if (pty_ < 0 || columns < 1 || rows < 1)
    return false;
  struct winsize ws{};
  ws.ws_col = static_cast<unsigned short>(columns);
  ws.ws_row = static_cast<unsigned short>(rows);
  return ioctl(pty_, TIOCSWINSZ, &ws) == 0;
}
void TerminalSession::stop() {
  stopping_ = true;
  if (pid_ > 0 && running_) {
    // Interactive shells often ignore SIGTERM. Force-stop the PTY session
    // so closing an editor tab cannot block on waitpid() indefinitely.
    kill(-pid_, SIGKILL);
    kill(pid_, SIGKILL);
  }
  if (reader_.joinable())
    reader_.join();
  if (pty_ >= 0) {
    close(pty_);
    pty_ = -1;
  }
  pid_ = -1;
  running_ = false;
}
#else
namespace {
std::wstring wide(const fs::path &p) { return p.wstring(); }
void closeHandle(HANDLE &h) {
  if (h) {
    CloseHandle(h);
    h = nullptr;
  }
}
} // namespace
bool TerminalSession::start(const fs::path &cwd, std::string *error,
                            const std::string &shell) {
  stop();
  {
    std::lock_guard<std::mutex> lk(output_mutex_);
    output_.clear();
  }
  stopping_ = false;
  exit_code_ = -1;
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE out_write = nullptr, in_read = nullptr;
  if (!CreatePipe(&output_read_, &out_write, &sa, 0) ||
      !SetHandleInformation(output_read_, HANDLE_FLAG_INHERIT, 0) ||
      !CreatePipe(&in_read, &input_, &sa, 0) ||
      !SetHandleInformation(input_, HANDLE_FLAG_INHERIT, 0)) {
    if (error)
      *error = "No se pueden crear tuberias para ConPTY: " +
               std::to_string(GetLastError());
    closeHandle(out_write);
    closeHandle(in_read);
    closeHandle(input_);
    closeHandle(output_read_);
    return false;
  }
  const COORD size{120, 30};
  const HRESULT created =
      CreatePseudoConsole(size, in_read, out_write, 0, &pseudoconsole_);
  closeHandle(in_read);
  closeHandle(out_write);
  if (FAILED(created)) {
    if (error)
      *error = "ConPTY no disponible (requiere Windows 10 1809 o superior): " +
               std::to_string(static_cast<unsigned>(created));
    closeHandle(input_);
    closeHandle(output_read_);
    return false;
  }
  STARTUPINFOEXW si{};
  si.StartupInfo.cb = sizeof(si);
  SIZE_T attr_size = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
  std::vector<unsigned char> storage(attr_size);
  si.lpAttributeList =
      reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
  if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0,
                                         &attr_size) ||
      !UpdateProcThreadAttribute(
          si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
          pseudoconsole_, sizeof(pseudoconsole_), nullptr, nullptr)) {
    if (error)
      *error =
          "No se pudo configurar ConPTY: " + std::to_string(GetLastError());
    if (si.lpAttributeList)
      DeleteProcThreadAttributeList(si.lpAttributeList);
    ClosePseudoConsole(pseudoconsole_);
    pseudoconsole_ = nullptr;
    closeHandle(input_);
    closeHandle(output_read_);
    return false;
  }
  PROCESS_INFORMATION pi{};
  std::wstring command = shell == "cmd"    ? L"cmd.exe"
                         : shell == "pwsh" ? L"pwsh.exe -NoLogo -NoExit"
                                           : L"powershell.exe -NoLogo -NoExit";
  const auto current = wide(cwd);
  const BOOL ok = CreateProcessW(nullptr, command.data(), nullptr, nullptr,
                                 FALSE, EXTENDED_STARTUPINFO_PRESENT, nullptr,
                                 current.empty() ? nullptr : current.c_str(),
                                 &si.StartupInfo, &pi);
  DeleteProcThreadAttributeList(si.lpAttributeList);
  if (!ok) {
    if (error)
      *error = "No se pudo iniciar cmd.exe en ConPTY: " +
               std::to_string(GetLastError());
    ClosePseudoConsole(pseudoconsole_);
    pseudoconsole_ = nullptr;
    closeHandle(input_);
    closeHandle(output_read_);
    return false;
  }
  process_ = pi.hProcess;
  pid_ = pi.dwProcessId;
  CloseHandle(pi.hThread);
  running_ = true;
  try {
    reader_ = std::thread(&TerminalSession::readerLoop, this);
  } catch (const std::exception &e) {
    if (error)
      *error = e.what();
    TerminateProcess(process_, 1);
    WaitForSingleObject(process_, INFINITE);
    ClosePseudoConsole(pseudoconsole_);
    pseudoconsole_ = nullptr;
    closeHandle(process_);
    closeHandle(input_);
    closeHandle(output_read_);
    running_ = false;
    return false;
  }
  return true;
}
void TerminalSession::readerLoop() {
  char buf[4096];
  DWORD got = 0;
  while (ReadFile(output_read_, buf, sizeof(buf), &got, nullptr) && got) {
    append(buf, got);
  }
  if (process_) {
    WaitForSingleObject(process_, INFINITE);
    DWORD status = 1;
    if (GetExitCodeProcess(process_, &status))
      exit_code_ = static_cast<int>(status);
  }
  running_ = false;
}
bool TerminalSession::sendBytes(const std::string &data) {
  if (!running_ || !input_)
    return false;
  std::size_t offset = 0;
  while (offset < data.size()) {
    DWORD written = 0;
    if (!WriteFile(input_, data.data() + offset,
                   static_cast<DWORD>(
                       std::min<std::size_t>(data.size() - offset, 32768)),
                   &written, nullptr) ||
        written == 0)
      return false;
    offset += written;
  }
  return true;
}
bool TerminalSession::sendLine(const std::string &line) {
  return sendBytes(line + "\r");
}
bool TerminalSession::interrupt() { return sendBytes("\x03"); }
bool TerminalSession::resize(int columns, int rows) {
  if (!pseudoconsole_ || columns < 1 || rows < 1)
    return false;
  const COORD size{static_cast<SHORT>(columns), static_cast<SHORT>(rows)};
  return SUCCEEDED(ResizePseudoConsole(pseudoconsole_, size));
}
void TerminalSession::stop() {
  stopping_ = true;
  if (process_ && running_)
    TerminateProcess(process_, 130);
  closeHandle(input_);
  if (pseudoconsole_) {
    ClosePseudoConsole(pseudoconsole_);
    pseudoconsole_ = nullptr;
  }
  if (reader_.joinable())
    reader_.join();
  closeHandle(output_read_);
  closeHandle(process_);
  running_ = false;
  pid_ = 0;
}
#endif
} // namespace dodev
