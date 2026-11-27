#include "SerialMonitor.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
extern char** environ;
#endif

namespace dodev::serial {
namespace {
std::string lower(std::string s) {
  for (auto& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string systemError(const std::string& prefix) {
  return prefix + ": " + std::strerror(errno);
}
std::string renderByte(unsigned char c) {
  if (c == '\t') return "\t";
  if (c >= 32 && c != 127) return std::string(1, static_cast<char>(c));
  static const char* hex = "0123456789ABCDEF";
  std::string s = "\\x00";
  s[2] = hex[c >> 4];
  s[3] = hex[c & 15];
  return s;
}
#ifndef _WIN32
speed_t baudValue(int baud) {
  switch (baud) {
    case 1200:
      return B1200;
    case 2400:
      return B2400;
    case 4800:
      return B4800;
    case 9600:
      return B9600;
    case 19200:
      return B19200;
    case 38400:
      return B38400;
    case 57600:
      return B57600;
    case 115200:
      return B115200;
#ifdef B230400
    case 230400:
      return B230400;
#endif
#ifdef B460800
    case 460800:
      return B460800;
#endif
    default:
      return 0;
  }
}
struct PortFd {
  int fd = -1;
  termios old{};
  bool restore = false;
  ~PortFd() {
    if (fd >= 0) {
      if (restore) tcsetattr(fd, TCSANOW, &old);
      close(fd);
    }
  }
};
int openPort(const PortOptions& options, PortFd& handle, std::string* error) {
  struct stat st{};
  if (stat(options.port.c_str(), &st) != 0) {
    if (error) *error = systemError("Cannot inspect serial port");
    return -1;
  }
  if (!S_ISCHR(st.st_mode)) {
    if (error) *error = "Not a character device: " + options.port;
    errno = ENOTTY;
    return -1;
  }
  handle.fd =
      open(options.port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
  if (handle.fd < 0) {
    if (error) *error = systemError("Cannot open serial port");
    return -1;
  }
  if (!isatty(handle.fd)) {
    if (error) *error = "Device is not a serial TTY";
    errno = ENOTTY;
    return -1;
  }
  if (tcgetattr(handle.fd, &handle.old) < 0) {
    if (error) *error = systemError("tcgetattr failed");
    return -1;
  }
  handle.restore = true;
  termios t = handle.old;
  cfmakeraw(&t);
  t.c_cflag &= ~CSIZE;
  t.c_cflag |= options.dataBits == 7 ? CS7 : CS8;
  t.c_cflag |= CLOCAL | CREAD;
  t.c_cflag &= ~(PARENB | PARODD | CSTOPB);
  if (options.stopBits == 2) t.c_cflag |= CSTOPB;
  if (options.parity == 'E') t.c_cflag |= PARENB;
  if (options.parity == 'O') t.c_cflag |= PARENB | PARODD;
  t.c_cc[VMIN] = 0;
  t.c_cc[VTIME] = 1;
  if (cfsetispeed(&t, baudValue(options.baud)) != 0 ||
      cfsetospeed(&t, baudValue(options.baud)) != 0 ||
      tcsetattr(handle.fd, TCSANOW, &t) != 0) {
    if (error) *error = systemError("Could not configure serial port");
    return -1;
  }
  return handle.fd;
}
#endif
}  // namespace
CaptureSession::~CaptureSession() { stop(); }
bool CaptureSession::validatePort(const std::string& port, std::string* error) {
  if (port.empty() || port.size() > 260 ||
      port.find('\0') != std::string::npos ||
      port.find('\n') != std::string::npos) {
    if (error) *error = "Serial port name is empty or invalid";
    return false;
  }
#ifndef _WIN32
  // The privileged helper is deliberately restricted to devices under /dev.
  if (port.rfind("/dev/", 0) != 0 || port.find("/../") != std::string::npos) {
    if (error) *error = "Linux serial port must be under /dev/";
    return false;
  }
#endif
  return true;
}
bool CaptureSession::validateOptions(const PortOptions& options,
                                     std::string* error) {
  if (!validatePort(options.port, error)) return false;
  if (options.dataBits != 7 && options.dataBits != 8) {
    if (error) *error = "Data bits must be 7 or 8";
    return false;
  }
  if (options.stopBits != 1 && options.stopBits != 2) {
    if (error) *error = "Stop bits must be 1 or 2";
    return false;
  }
  if (options.parity != 'N' && options.parity != 'E' && options.parity != 'O') {
    if (error) *error = "Parity must be N, E, or O";
    return false;
  }
#ifdef _WIN32
  const std::vector<int> supported{1200,  2400,  4800,   9600,   19200,
                                   38400, 57600, 115200, 230400, 460800};
  if (std::find(supported.begin(), supported.end(), options.baud) ==
      supported.end()) {
#else
  if (baudValue(options.baud) == 0) {
#endif
    if (error) *error = "Unsupported baud rate";
    return false;
  }
  return true;
}
bool CaptureSession::setFilter(FilterMode mode, const std::string& pattern,
                               bool sensitive, std::string* error) {
  if (pattern.size() > 2048) {
    if (error) *error = "Filter pattern too long";
    return false;
  }
  std::regex compiled;
  if (mode == FilterMode::Regex) {
    try {
      compiled = std::regex(
          pattern, sensitive ? std::regex::ECMAScript
                             : std::regex::ECMAScript | std::regex::icase);
    } catch (const std::regex_error& e) {
      if (error)
        *error = std::string("Invalid regular expression: ") + e.what();
      return false;
    }
  }
  std::lock_guard<std::mutex> lock(mutex_);
  filterMode_ = mode;
  pattern_ = pattern;
  caseSensitive_ = sensitive;
  regex_ = std::move(compiled);
  return true;
}
bool CaptureSession::matchingUnlocked(const std::string& line) const {
  if (filterMode_ == FilterMode::All || pattern_.empty()) return true;
  if (filterMode_ == FilterMode::Regex) return std::regex_search(line, regex_);
  if (caseSensitive_) return line.find(pattern_) != std::string::npos;
  return lower(line).find(lower(pattern_)) != std::string::npos;
}
void CaptureSession::appendLineUnlocked(std::string line) {
  if (lines_.size() >= kMaxCapturedLines) {
    lines_.pop_front();
    ++dropped_;
  }
  lines_.push_back(std::move(line));
  ++received_;
}
void CaptureSession::appendBytes(const unsigned char* data, std::size_t size) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (std::size_t i = 0; i < size; ++i) {
    const auto c = data[i];
    if (c == '\n') {
      appendLineUnlocked(pending_);
      pending_.clear();
      continue;
    }
    if (c == '\r') continue;
    pending_ += renderByte(c);
    if (pending_.size() > 16384) {
      appendLineUnlocked(pending_ + " [fragment]");
      pending_.clear();
    }
  }
}
void CaptureSession::setStatus(std::string status) {
  std::lock_guard<std::mutex> lock(mutex_);
  status_ = std::move(status);
}
Snapshot CaptureSession::snapshot(bool matched, std::size_t maxLines) const {
  Snapshot result;
  std::lock_guard<std::mutex> lock(mutex_);
  result.status = status_;
  result.received = received_;
  result.dropped = dropped_;
  result.running = running_.load();
  std::deque<std::string> visible;
  for (const auto& line : lines_) {
    if (!matched || matchingUnlocked(line)) {
      if (visible.size() >= maxLines) visible.pop_front();
      visible.push_back(line);
    }
  }
  if (!pending_.empty() && (!matched || matchingUnlocked(pending_))) {
    if (visible.size() >= maxLines) visible.pop_front();
    visible.push_back(pending_);
  }
  for (const auto& line : visible) {
    result.output += line;
    result.output += '\n';
  }
  return result;
}
std::string CaptureSession::exportText(bool matched) const {
  std::string output;
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& line : lines_)
    if (!matched || matchingUnlocked(line)) {
      output += line;
      output += '\n';
    }
  if (!pending_.empty() && (!matched || matchingUnlocked(pending_))) {
    output += pending_;
    output += '\n';
  }
  return output;
}
bool CaptureSession::sendBytes(const std::string& bytes, std::string* error) {
  if (bytes.empty()) return true;
  if (bytes.size() > 65536) {
    if (error) *error = "Serial TX limit is 64 KiB per send";
    return false;
  }
  std::lock_guard<std::mutex> lock(txMutex_);
#ifdef _WIN32
  auto h = reinterpret_cast<HANDLE>(activeHandle_);
  if (!h) {
    if (error) *error = "Serial port is not connected";
    return false;
  }
  DWORD done = 0;
  if (!WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &done,
                 nullptr) ||
      done != bytes.size()) {
    if (error)
      *error = "Serial WriteFile failed: " + std::to_string(GetLastError());
    return false;
  }
#else
  const int fd = activeFd_ >= 0 ? activeFd_ : polkitInputFd_;
  if (fd < 0) {
    if (error) *error = "Serial port not connected / awaiting authorization";
    return false;
  }
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const ssize_t n = ::write(fd, bytes.data() + offset, bytes.size() - offset);
    if (n > 0) {
      offset += static_cast<std::size_t>(n);
      continue;
    }
    if (n < 0 && errno == EINTR) continue;
    if (n < 0 && errno == EAGAIN) {
      pollfd p{};
      p.fd = fd;
      p.events = POLLOUT;
      if (poll(&p, 1, 200) > 0) continue;
    }
    if (error) *error = systemError("Serial TX failed");
    return false;
  }
#endif
  return true;
}
void CaptureSession::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  lines_.clear();
  pending_.clear();
  received_ = 0;
  dropped_ = 0;
}
bool CaptureSession::start(const PortOptions& options, std::string* error) {
  if (!validateOptions(options, error)) return false;
#ifdef _WIN32
  if (options.captureAsAdministrator) {
    if (error)
      *error =
          "Per-session administrator capture requires Linux Polkit; on Windows "
          "use an authorized serial device";
    return false;
  }
#endif
  if (thread_.joinable()) stop();
  requestedStop_ = false;
  running_ = true;
  setStatus("Connecting to " + options.port + " @ " +
            std::to_string(options.baud));
  try {
    thread_ = std::thread(&CaptureSession::worker, this, options);
  } catch (const std::exception& e) {
    running_ = false;
    if (error) *error = e.what();
    return false;
  }
  return true;
}
void CaptureSession::stop() {
  requestedStop_ = true;
#ifndef _WIN32
  const int pid = helperPid_.load();
  if (pid > 0) kill(pid, SIGTERM);
#endif
  if (thread_.joinable()) thread_.join();
  running_ = false;
}
void CaptureSession::worker(PortOptions options) {
  readDirect(options);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pending_.empty()) {
      appendLineUnlocked(pending_);
      pending_.clear();
    }
  }
  running_ = false;
}
#ifdef _WIN32
void CaptureSession::readDirect(const PortOptions& options) {
  std::string port = options.port;
  if (port.rfind("\\\\.\\", 0) != 0) port = "\\\\.\\" + port;
  HANDLE h = CreateFileA(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    setStatus("Serial port open failed (Windows error " +
              std::to_string(GetLastError()) + ")");
    return;
  }
  DCB dcb{};
  dcb.DCBlength = sizeof(dcb);
  if (!GetCommState(h, &dcb)) {
    setStatus("GetCommState failed");
    CloseHandle(h);
    return;
  }
  dcb.BaudRate = static_cast<DWORD>(options.baud);
  dcb.ByteSize = static_cast<BYTE>(options.dataBits);
  dcb.Parity = options.parity == 'E'   ? EVENPARITY
               : options.parity == 'O' ? ODDPARITY
                                       : NOPARITY;
  dcb.StopBits = options.stopBits == 2 ? TWOSTOPBITS : ONESTOPBIT;
  dcb.fParity = options.parity != 'N';
  if (!SetCommState(h, &dcb)) {
    setStatus("SetCommState failed");
    CloseHandle(h);
    return;
  }
  COMMTIMEOUTS t{};
  t.ReadIntervalTimeout = MAXDWORD;
  t.ReadTotalTimeoutConstant = 150;
  t.ReadTotalTimeoutMultiplier = 0;
  SetCommTimeouts(h, &t);
  {
    std::lock_guard<std::mutex> lock(txMutex_);
    activeHandle_ = h;
  }
  setStatus("Interactive serial console: " + options.port);
  while (!requestedStop_) {
    unsigned char buf[2048];
    DWORD got = 0;
    if (!ReadFile(h, buf, sizeof(buf), &got, nullptr)) {
      setStatus("ReadFile failed (Windows error " +
                std::to_string(GetLastError()) + ")");
      break;
    }
    if (got) appendBytes(buf, got);
  }
  {
    std::lock_guard<std::mutex> lock(txMutex_);
    activeHandle_ = nullptr;
    CloseHandle(h);
  }
  if (requestedStop_) setStatus("Stopped");
}
#else
void CaptureSession::readDirect(const PortOptions& options) {
  if (options.captureAsAdministrator) {
    readViaPolkit(options);
    return;
  }
  PortFd h;
  std::string error;
  const int fd = openPort(options, h, &error);
  if (fd < 0) {
    if (errno == EACCES && options.allowPolkit) {
      setStatus("Permission denied; requesting Polkit authorization");
      readViaPolkit(options);
      return;
    }
    if (errno == EACCES)
      error +=
          ". Enable Polkit authorization or request membership in the device's "
          "serial group (often dialout)";
    setStatus(error);
    return;
  }
  {
    std::lock_guard<std::mutex> lock(txMutex_);
    activeFd_ = fd;
  }
  setStatus("Interactive serial console: " + options.port);
  while (!requestedStop_) {
    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;
    const int n = poll(&pfd, 1, 150);
    if (n < 0) {
      if (errno == EINTR) continue;
      setStatus(systemError("poll failed"));
      break;
    }
    if (n == 0) continue;
    if (pfd.revents & (POLLERR | POLLNVAL)) {
      setStatus("Serial device error/disconnect");
      break;
    }
    if (pfd.revents & POLLIN) {
      unsigned char bytes[2048];
      const ssize_t got = read(fd, bytes, sizeof(bytes));
      if (got > 0)
        appendBytes(bytes, static_cast<std::size_t>(got));
      else if (got < 0 && errno != EAGAIN && errno != EINTR) {
        setStatus(systemError("read failed"));
        break;
      }
    }
    if (pfd.revents & POLLHUP) {
      setStatus("Serial device disconnected");
      break;
    }
  }
  {
    std::lock_guard<std::mutex> lock(txMutex_);
    activeFd_ = -1;
  }
  if (requestedStop_) setStatus("Stopped");
}
void CaptureSession::readViaPolkit(const PortOptions& options) {
  namespace fs = std::filesystem;
  if (options.privilegedHelper.empty() ||
      !fs::is_regular_file(fs::u8path(options.privilegedHelper))) {
    setStatus(
        "Polkit helper missing; install dodev-serial-reader-helper next to "
        "DoDevEditor");
    return;
  }
  int pipes[2], inputPipes[2];
  if (pipe(pipes) != 0) {
    setStatus(systemError("pipe failed"));
    return;
  }
  if (pipe(inputPipes) != 0) {
    close(pipes[0]);
    close(pipes[1]);
    setStatus(systemError("input pipe failed"));
    return;
  }
  // posix_spawnp avoids running C++ library code after fork in a multi-threaded
  // IDE.
  const std::string baud = std::to_string(options.baud);
  const std::string bits = std::to_string(options.dataBits);
  const std::string stops = std::to_string(options.stopBits);
  const std::string parity(1, options.parity);
  std::vector<char*> args = {
      const_cast<char*>("pkexec"),
      const_cast<char*>(options.privilegedHelper.c_str()),
      const_cast<char*>(options.port.c_str()),
      const_cast<char*>(baud.c_str()),
      const_cast<char*>(bits.c_str()),
      const_cast<char*>(stops.c_str()),
      const_cast<char*>(parity.c_str()),
      nullptr};
  posix_spawn_file_actions_t actions{};
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, pipes[1], STDERR_FILENO);
  posix_spawn_file_actions_adddup2(&actions, inputPipes[0], STDIN_FILENO);
  posix_spawn_file_actions_addclose(&actions, inputPipes[0]);
  posix_spawn_file_actions_addclose(&actions, inputPipes[1]);
  posix_spawn_file_actions_addclose(&actions, pipes[0]);
  posix_spawn_file_actions_addclose(&actions, pipes[1]);
  pid_t pid = -1;
  const int launch =
      posix_spawnp(&pid, "pkexec", &actions, nullptr, args.data(), ::environ);
  posix_spawn_file_actions_destroy(&actions);
  if (launch != 0) {
    close(inputPipes[0]);
    close(inputPipes[1]);
    close(pipes[0]);
    close(pipes[1]);
    setStatus(std::string("Cannot start pkexec: ") + std::strerror(launch));
    return;
  }
  close(pipes[1]);
  close(inputPipes[0]);
  fcntl(inputPipes[1], F_SETFL, fcntl(inputPipes[1], F_GETFL) | O_NONBLOCK);
  {
    std::lock_guard<std::mutex> lock(txMutex_);
    polkitInputFd_ = inputPipes[1];
  }
  helperPid_ = static_cast<int>(pid);
  fcntl(pipes[0], F_SETFL, fcntl(pipes[0], F_GETFL) | O_NONBLOCK);
  setStatus("Polkit helper running (authorization may be required)");
  while (!requestedStop_) {
    pollfd pfd{};
    pfd.fd = pipes[0];
    pfd.events = POLLIN;
    const int n = poll(&pfd, 1, 150);
    if (n < 0 && errno != EINTR) break;
    if (n > 0 && (pfd.revents & POLLIN)) {
      unsigned char buf[2048];
      ssize_t k = read(pipes[0], buf, sizeof(buf));
      if (k > 0)
        appendBytes(buf, static_cast<std::size_t>(k));
      else if (k == 0)
        break;
    }
    if (n > 0 && (pfd.revents & (POLLHUP | POLLERR))) break;
  }
  close(pipes[0]);
  {
    std::lock_guard<std::mutex> lock(txMutex_);
    if (polkitInputFd_ >= 0) {
      close(polkitInputFd_);
      polkitInputFd_ = -1;
    }
  }
  if (requestedStop_) kill(pid, SIGTERM);
  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
  }
  helperPid_ = -1;
  if (requestedStop_)
    setStatus("Stopped");
  else if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    setStatus("Serial helper finished");
  else
    setStatus("Polkit authentication rejected or helper exited (status " +
              std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : -1) +
              ")");
}
#endif
int CaptureSession::runReaderToStdout(const PortOptions& options,
                                      std::string* error) {
  if (!validateOptions(options, error)) return 2;
#ifndef _WIN32
  // Accept only real character devices under /dev. Never run commands or open
  // arbitrary files as root.
  char resolved[4096]{};
  if (!realpath(options.port.c_str(), resolved)) {
    if (error) *error = "Cannot resolve privileged serial device";
    return 2;
  }
  const std::string canonical(resolved);
  const auto allowed = [&](const char* prefix) {
    return canonical.rfind(prefix, 0) == 0;
  };
  if (!(allowed("/dev/ttyUSB") || allowed("/dev/ttyACM") ||
        allowed("/dev/ttyS") || allowed("/dev/ttyAMA") ||
        allowed("/dev/rfcomm"))) {
    if (error)
      *error = "Polkit helper only permits whitelisted /dev serial TTY devices";
    return 2;
  }
  PortFd handle;
  if (openPort(options, handle, error) < 0) return 3;
  // Bidirectional transport: output goes to stdout, host keyboard input comes
  // from stdin. The helper cannot execute commands and only opens TTY devices.
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  if (flags >= 0) fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
  unsigned char data[2048];
  bool inputOpen = true;
  for (;;) {
    pollfd fds[2]{};
    fds[0].fd = handle.fd;
    fds[0].events = POLLIN;
    fds[1].fd = inputOpen ? STDIN_FILENO : -1;
    fds[1].events = POLLIN;
    const int ready = poll(fds, 2, 150);
    if (ready < 0) {
      if (errno == EINTR) continue;
      return 5;
    }
    if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) return 0;
    if (fds[0].revents & POLLIN) {
      const ssize_t got = read(handle.fd, data, sizeof(data));
      if (got > 0) {
        const unsigned char* p = data;
        ssize_t left = got;
        while (left > 0) {
          const auto n = write(STDOUT_FILENO, p, static_cast<size_t>(left));
          if (n < 0) {
            if (errno == EINTR) continue;
            return 4;
          }
          p += n;
          left -= n;
        }
      } else if (got < 0 && errno != EAGAIN && errno != EINTR)
        return 5;
    }
    if (inputOpen && (fds[1].revents & POLLIN)) {
      const ssize_t got = read(STDIN_FILENO, data, sizeof(data));
      if (got == 0)
        inputOpen = false;
      else if (got > 0) {
        ssize_t written = 0;
        while (written < got) {
          const auto n = write(handle.fd, data + written,
                               static_cast<size_t>(got - written));
          if (n > 0)
            written += n;
          else if (n < 0 && errno == EINTR)
            continue;
          else if (n < 0 && errno == EAGAIN) {
            pollfd wait{};
            wait.fd = handle.fd;
            wait.events = POLLOUT;
            poll(&wait, 1, 100);
          } else
            return 5;
        }
      }
    }
    if (fds[1].revents & (POLLHUP | POLLERR)) inputOpen = false;
  }
#else
  if (error) *error = "Polkit helper is only needed on Linux/macOS";
  return 2;
#endif
}
std::vector<std::string> CaptureSession::availablePorts() {
  std::vector<std::string> result;
#ifdef _WIN32
  for (int i = 1; i <= 64; ++i) {
    std::string name = "COM" + std::to_string(i);
    char target[1024]{};
    if (QueryDosDeviceA(name.c_str(), target, sizeof(target)) != 0)
      result.push_back(name);
  }
#else
  namespace fs = std::filesystem;
  std::error_code ec;
  for (const auto& root : {"/dev", "/dev/serial/by-id"}) {
    if (!fs::is_directory(root, ec)) continue;
    for (const auto& f : fs::directory_iterator(
             root, fs::directory_options::skip_permission_denied, ec)) {
      const auto name = f.path().filename().string();
      if (std::string(root) == "/dev" &&
          !(name.rfind("ttyUSB", 0) == 0 || name.rfind("ttyACM", 0) == 0 ||
            name.rfind("ttyS", 0) == 0 || name.rfind("ttyAMA", 0) == 0 ||
            name.rfind("rfcomm", 0) == 0))
        continue;
      result.push_back(f.path().string());
    }
  }
#endif
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}
}  // namespace dodev::serial
