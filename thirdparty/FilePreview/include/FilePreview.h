#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace preview {
constexpr size_t MaxInput = 2 * 1024 * 1024, MaxRows = 2000, MaxColumns = 80;
struct Result {
  std::string html;
  bool truncated = false;
};
std::string escape(const std::string&);
std::string read(const std::filesystem::path&);
Result render(std::string extension, const std::string& source,
              bool csvHeader = true);
}  // namespace preview

namespace preview {
struct Node {
  size_t depth;
  std::string label;
};
std::vector<Node> jsonTree(const std::string&);
std::vector<Node> xmlTree(const std::string&);
Result hexView(const std::string&, bool truncated = false);
Result sqliteView(const std::filesystem::path&, const std::string& table = {},
                  const std::string& query = {});
std::string readBounded(const std::filesystem::path&, size_t limit,
                        bool truncate = false);
// External conversion is deliberately opt-in for trusted files only.
std::string externalImage(const std::filesystem::path&,
                          const std::string& source, const std::string& ext,
                          const std::string& executable, int page = 1);
}  // namespace preview
