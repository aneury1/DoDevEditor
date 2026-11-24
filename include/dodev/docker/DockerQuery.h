#pragma once
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

#include "TerminalSession.h"
#include "dodev/docker/DockerTools.h"
namespace dodev::docker {
// Read-only commands are bounded and cancellable. Long operations use the IDE
// PTY tabs.
inline std::string capture(
    const Args& argv, const std::filesystem::path& cwd,
    std::atomic<bool>* cancel = nullptr,
    std::chrono::milliseconds timeout = std::chrono::seconds(20)) {
  if (argv.empty()) throw std::runtime_error("Empty Docker command");
  TerminalSession session;
  std::string err, result;
  if (!session.start(cwd, &err, argv.front(), argv))
    throw std::runtime_error(err);
  auto deadline = std::chrono::steady_clock::now() + timeout;
  auto append = [&] {
    result += session.takeOutput();
    if (result.size() > 16 * 1024 * 1024)
      throw std::runtime_error(
          "Docker output exceeded 16 MiB; use terminal instead");
  };
  while (session.running()) {
    append();
    if ((cancel && cancel->load()) ||
        std::chrono::steady_clock::now() > deadline) {
      session.stop();
      throw std::runtime_error(cancel && cancel->load()
                                   ? "Docker query cancelled"
                                   : "Docker query timed out");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
  append();
  if (session.exitCode() != 0)
    throw std::runtime_error(
        "Docker exit " + std::to_string(session.exitCode()) + ":\n" + result);
  return result;
}
}  // namespace dodev::docker
