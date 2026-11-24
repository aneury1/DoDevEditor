#pragma once
#include "SymbolCache.h"
namespace dodev::symbols {
struct CallNode {
  std::string name, status;
  std::filesystem::path path;
  size_t line = 0, depth = 0;
};
std::vector<CallNode> hierarchy(const WorkspaceIndex&, const std::string&,
                                const std::filesystem::path&, size_t, bool,
                                size_t maxDepth = 5);
}  // namespace dodev::symbols
