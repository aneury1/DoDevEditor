#include "GitHistory.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "TerminalSession.h"
namespace dodev::git {
namespace {
// No shell: all arguments are passed literally through the existing bounded
// process runner.
std::string run(const std::filesystem::path& cwd, const std::string& exe,
                std::vector<std::string> args) {
  std::vector<std::string> argv{
      exe.empty() ? "git" : exe, "--no-pager", "-c",
      "color.ui=false",          "-c",         "core.quotePath=true"};
  argv.insert(argv.end(), args.begin(), args.end());
  TerminalSession process;
  std::string error, out;
  if (!process.start(cwd, &error, argv.front(), argv))
    throw std::runtime_error(error);
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(20);
  auto collect = [&] {
    out += process.takeOutput();
    if (out.size() > 16 * 1024 * 1024) {
      process.stop();
      throw std::runtime_error("Git output limit exceeded");
    }
  };
  while (process.running()) {
    collect();
    if (std::chrono::steady_clock::now() > deadline) {
      process.stop();
      throw std::runtime_error("Git query timed out");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  collect();
  if (process.exitCode() != 0)
    throw std::runtime_error("Git exit " + std::to_string(process.exitCode()) +
                             ":\n" + out);
  return out;
}
}  // namespace
bool validHash(const std::string& s) {
  return (s.size() == 40 || s.size() == 64) &&
         s.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos;
}
std::vector<Commit> history(const std::filesystem::path& cwd, int limit,
                            const std::string& exe) {
  auto text = run(cwd, exe,
                  {"log", "-n", std::to_string(std::clamp(limit, 1, 1000)),
                   "--format=%H%x1f%cn <%ce>%x1f%cI%x1f%s", "--"});
  std::vector<Commit> out;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::array<std::string, 4> f;
    size_t p = 0;
    bool ok = true;
    for (int i = 0; i < 3; i++) {
      auto next = line.find('\x1f', p);
      if (next == std::string::npos) {
        ok = false;
        break;
      }
      f[i] = line.substr(p, next - p);
      p = next + 1;
    }
    if (ok && validHash(f[0])) {
      f[3] = line.substr(p);
      out.push_back({f[0], f[1], f[2], f[3]});
    }
  }
  return out;
}
std::string details(const std::filesystem::path& cwd, const std::string& hash,
                    bool patch, const std::string& exe) {
  if (!validHash(hash)) throw std::runtime_error("Expected a full commit hash");
  auto out =
      run(cwd, exe,
          {"show",
           "--format=commit %H%nParents: %P%nAuthor: %an <%ae>%nAuthorDate: "
           "%aI%nCommitter: %cn <%ce>%nCommitDate: %cI%n%n%B%nChanged files:",
           "--name-status", "--no-renames", "--no-ext-diff", "--no-textconv",
           "-m", hash, "--"});
  if (patch)
    out += "\n=== Patch ===\n" +
           run(cwd, exe,
               {"show", "--format=", "--patch", "--no-renames", "--no-ext-diff",
                "--no-textconv", "-m", hash, "--"});
  return out;
}
}  // namespace dodev::git
