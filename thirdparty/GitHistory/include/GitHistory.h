#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace dodev::git {
struct Commit {
  std::string hash, committer, date, message;
};
std::vector<Commit> history(const std::filesystem::path&, int limit,
                            const std::string& executable = "git");
std::string details(const std::filesystem::path&, const std::string& hash,
                    bool patch = false, const std::string& executable = "git");
bool validHash(const std::string&);
}  // namespace dodev::git
