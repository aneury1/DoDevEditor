#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>

namespace dodev::serial {
enum class FilterMode { All, Fixed, Regex };
struct PortOptions {
  std::string port;
  int baud = 115200;
  int dataBits = 8;
  int stopBits = 1;
  char parity = 'N';
  // Explicit consent is required; only used following a permission-denied
  // error.
  bool allowPolkit = false;
  std::string privilegedHelper;
};
struct Snapshot {
  std::string output;
  std::string status;
  std::size_t received = 0;
  std::size_t dropped = 0;
  bool running = false;
};

// Threaded capture, never accesses Raven UI from a worker thread. Captures up
// to kMaxCapturedLines decoded lines; the oldest are discarded when the limit
// is hit.
class CaptureSession {
public:
  static constexpr std::size_t kMaxCapturedLines = 20000;
  CaptureSession() = default;
  ~CaptureSession();
  CaptureSession(const CaptureSession &) = delete;
  CaptureSession &operator=(const CaptureSession &) = delete;
  bool start(const PortOptions &options, std::string *error = nullptr);
  void stop();
  bool setFilter(FilterMode mode, const std::string &pattern,
                 bool caseSensitive, std::string *error = nullptr);
  Snapshot snapshot(bool onlyMatching = true,
                    std::size_t maxLines = 2500) const;
  std::string exportText(bool onlyMatching = true) const;
  void clear();
  bool running() const { return running_.load(); }
  bool sendBytes(const std::string &bytes,
                 std::string *error = nullptr); // raw interactive serial TX
  static bool validatePort(const std::string &port,
                           std::string *error = nullptr);
  static bool validateOptions(const PortOptions &options,
                              std::string *error = nullptr);
  static std::vector<std::string> availablePorts();
  // Used by the small privileged helper; writes raw bytes to stdout.
  static int runReaderToStdout(const PortOptions &options,
                               std::string *error = nullptr);

private:
  void worker(PortOptions options);
  void appendBytes(const unsigned char *data, std::size_t size);
  void appendLineUnlocked(std::string line);
  void setStatus(std::string status);
  bool matchingUnlocked(const std::string &line) const;
  void readDirect(const PortOptions &options);
#ifndef _WIN32
  void readViaPolkit(const PortOptions &options);
  std::atomic<int> helperPid_{-1};
#endif
  mutable std::mutex mutex_;
  std::thread thread_;
  std::atomic<bool> requestedStop_{false};
  std::atomic<bool> running_{false};
  mutable std::mutex txMutex_;
#ifdef _WIN32
  void *activeHandle_ = nullptr;
#else
  int activeFd_ = -1;
  int polkitInputFd_ = -1;
#endif
  std::deque<std::string> lines_;
  std::string pending_;
  std::string status_ = "Idle";
  std::size_t received_ = 0, dropped_ = 0;
  FilterMode filterMode_ = FilterMode::All;
  std::string pattern_;
  bool caseSensitive_ = false;
  std::regex regex_;
};
} // namespace dodev::serial
