#include "RemoteJournal.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <string>
#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif
namespace dodev::journal {
namespace {
std::string lower(std::string text) {
  for (char &c : text)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}
bool allowed(const std::string &text, const std::string &chars,
             std::size_t max) {
  if (text.empty() || text.size() > max || text[0] == '-')
    return false;
  for (const unsigned char ch : text)
    if (!std::isalnum(ch) &&
        chars.find(static_cast<char>(ch)) == std::string::npos)
      return false;
  return true;
}
} // namespace
bool Session::validate(const Options &o, std::string *error) {
  if (!allowed(o.host, ".-:[]_", 253)) {
    if (error)
      *error = "Invalid SSH hostname/IP";
    return false;
  }
  if (!allowed(o.user, "-._", 64)) {
    if (error)
      *error = "Invalid SSH username";
    return false;
  }
  if (o.sshPort < 1 || o.sshPort > 65535) {
    if (error)
      *error = "Invalid SSH port";
    return false;
  }
  if (!o.unit.empty() && !allowed(o.unit, "-_.@:", 160)) {
    if (error)
      *error = "Invalid systemd unit";
    return false;
  }
  if (o.lines < 1 || o.lines > 10000) {
    if (error)
      *error = "Journal lines must be 1..10000";
    return false;
  }
  return true;
}
std::vector<std::string> Session::sshArguments(const Options &o) {
  // Units and user/host are validated before we construct this; the remote
  // command is a fixed journalctl invocation, never arbitrary input.
  const auto target = o.user + "@" + o.host;
  std::string remote = (o.sudoNonInteractive ? "sudo -n " : "");
  remote +=
      "journalctl --no-pager --output=short-iso -n " + std::to_string(o.lines);
  if (!o.unit.empty())
    remote += " --unit='" + o.unit + "'";
  if (o.follow)
    remote += " --follow";
  return {"-o",  "BatchMode=yes",           "-o", "StrictHostKeyChecking=yes",
          "-o",  "ConnectTimeout=8",        "-o", "ServerAliveInterval=15",
          "-p",  std::to_string(o.sshPort), "--", target,
          remote};
}
void Session::setStatus(std::string status) {
  std::lock_guard<std::mutex> guard(mutex_);
  status_ = std::move(status);
}
void Session::append(std::string text) {
  std::lock_guard<std::mutex> guard(mutex_);
  for (char c : text) {
    if (c == '\r')
      continue;
    if (c == '\n') {
      if (lines_.size() >= kMaxLines) {
        lines_.pop_front();
        ++dropped_;
      }
      lines_.push_back(fragment_);
      fragment_.clear();
      ++received_;
    } else if (fragment_.size() < 8192)
      fragment_ += c;
  }
}
bool Session::setFilter(Filter filter, std::string pattern, bool sensitive,
                        std::string *error) {
  if (pattern.size() > 512) {
    if (error)
      *error = "Pattern exceeds 512 chars";
    return false;
  }
  std::regex regex;
  if (filter == Filter::Regex) {
    try {
      regex = std::regex(pattern, sensitive ? std::regex::ECMAScript
                                            : std::regex::ECMAScript |
                                                  std::regex::icase);
    } catch (const std::regex_error &e) {
      if (error)
        *error = e.what();
      return false;
    }
  }
  std::lock_guard<std::mutex> lock(mutex_);
  filter_ = filter;
  pattern_ = std::move(pattern);
  caseSensitive_ = sensitive;
  regex_ = std::move(regex);
  return true;
}
bool Session::matches(const std::string &line) const {
  if (filter_ == Filter::All || pattern_.empty())
    return true;
  if (filter_ == Filter::Regex)
    return std::regex_search(line, regex_);
  return caseSensitive_
             ? line.find(pattern_) != std::string::npos
             : lower(line).find(lower(pattern_)) != std::string::npos;
}
Snapshot Session::snapshot(bool onlyMatching, std::size_t maxLines) const {
  Snapshot s;
  std::lock_guard<std::mutex> lock(mutex_);
  s.status = status_;
  s.received = received_;
  s.dropped = dropped_;
  s.running = running_.load();
  std::deque<std::string> visible;
  for (const auto &line : lines_)
    if (!onlyMatching || matches(line)) {
      if (visible.size() >= maxLines)
        visible.pop_front();
      visible.push_back(line);
    }
  if (!fragment_.empty() && (!onlyMatching || matches(fragment_))) {
    if (visible.size() >= maxLines)
      visible.pop_front();
    visible.push_back(fragment_);
  }
  for (const auto &line : visible)
    s.text += line + '\n';
  return s;
}
std::string Session::exportText(bool onlyMatching) const {
  std::string s;
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &line : lines_)
    if (!onlyMatching || matches(line))
      s += line + '\n';
  if (!fragment_.empty() && (!onlyMatching || matches(fragment_)))
    s += fragment_ + '\n';
  return s;
}
void Session::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  lines_.clear();
  fragment_.clear();
  received_ = dropped_ = 0;
}
bool Session::start(const Options &options, std::string *error) {
  if (!validate(options, error))
    return false;
  stop();
  clear();
  stopping_ = false;
  running_ = true;
  setStatus("Connecting: " + options.user + "@" + options.host);
  try {
    thread_ = std::thread(&Session::worker, this, options);
  } catch (const std::exception &e) {
    running_ = false;
    if (error)
      *error = e.what();
    return false;
  }
  return true;
}
void Session::stop() {
  stopping_ = true;
#ifndef _WIN32
  const auto pid = processId_.load();
  if (pid > 0)
    kill(pid, SIGTERM);
#endif
  if (thread_.joinable())
    thread_.join();
  running_ = false;
}
void Session::worker(Options options) {
#ifndef _WIN32
  int pipes[2];
  if (pipe(pipes) != 0) {
    setStatus("SSH pipe: " + std::string(std::strerror(errno)));
    running_ = false;
    return;
  }
  auto args = sshArguments(options);
  std::vector<char *> argv;
  argv.push_back(const_cast<char *>("ssh"));
  for (auto &arg : args)
    argv.push_back(&arg[0]);
  argv.push_back(nullptr);
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, pipes[1], STDERR_FILENO);
  posix_spawn_file_actions_addclose(&actions, pipes[0]);
  posix_spawn_file_actions_addclose(&actions, pipes[1]);
  pid_t pid = -1;
  const int status =
      posix_spawnp(&pid, "ssh", &actions, nullptr, argv.data(), ::environ);
  posix_spawn_file_actions_destroy(&actions);
  close(pipes[1]);
  if (status != 0) {
    close(pipes[0]);
    setStatus("SSH spawn failed: " + std::string(std::strerror(status)));
    running_ = false;
    return;
  }
  processId_ = static_cast<int>(pid);
  fcntl(pipes[0], F_SETFL, fcntl(pipes[0], F_GETFL, 0) | O_NONBLOCK);
  setStatus("Connected via SSH / waiting for journal output");
  while (!stopping_) {
    pollfd p{};
    p.fd = pipes[0];
    p.events = POLLIN;
    const auto wait = poll(&p, 1, 150);
    if (wait < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    if (wait == 0)
      continue;
    if (p.revents & POLLIN) {
      char b[4096];
      const auto got = read(pipes[0], b, sizeof(b));
      if (got > 0)
        append(std::string(b, static_cast<std::size_t>(got)));
      else if (got == 0)
        break;
    }
    if (p.revents & (POLLHUP | POLLERR | POLLNVAL)) {
      char b[4096];
      ssize_t got = 0;
      while ((got = read(pipes[0], b, sizeof(b))) > 0)
        append(std::string(b, static_cast<std::size_t>(got)));
      break;
    }
  }
  close(pipes[0]);
  if (stopping_)
    kill(pid, SIGTERM);
  int code = 0;
  while (waitpid(pid, &code, 0) < 0 && errno == EINTR) {
  }
  processId_ = -1;
  setStatus(stopping_ ? "Stopped"
            : WIFEXITED(code) && WEXITSTATUS(code) == 0
                ? "Finished"
                : "SSH/journalctl exit status " +
                      std::to_string(WIFEXITED(code) ? WEXITSTATUS(code) : -1));
#else
  (void)options;
  setStatus("Remote journal transport currently requires POSIX/OpenSSH; "
            "Windows host support is not yet available");
#endif
  running_ = false;
}
} // namespace dodev::journal
