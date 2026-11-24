#include "CallHierarchy.h"

#include <algorithm>
#include <functional>
#include <set>
namespace dodev::symbols {
namespace {
std::string leaf(const std::string& n) {
  auto p = n.rfind("::");
  return p == std::string::npos ? n : n.substr(p + 2);
}
bool matches(const std::string& a, const std::string& b) {
  return a == b || ((a.find("::") == std::string::npos ||
                     b.find("::") == std::string::npos) &&
                    leaf(a) == leaf(b));
}
}  // namespace
std::vector<CallNode> hierarchy(const WorkspaceIndex& index,
                                const std::string& name,
                                const std::filesystem::path& path, size_t line,
                                bool incoming, size_t maxDepth) {
  std::vector<CallNode> out;
  std::set<std::string> stack;
  std::function<void(CallNode)> visit = [&](CallNode n) {
    if (out.size() >= 2000) return;
    auto key =
        n.name + "|" + n.path.generic_string() + ":" + std::to_string(n.line);
    if (stack.count(key)) {
      n.status = "recursive";
      out.push_back(n);
      return;
    }
    out.push_back(n);
    if (n.depth >= maxDepth || n.status == "ambiguous" ||
        n.status == "unresolved")
      return;
    stack.insert(key);
    for (const auto& edge : index.calls) {
      const auto& c = edge.call;
      if (incoming) {
        if (!matches(c.callee, n.name)) continue;
        visit({c.caller, "inferred caller", edge.path, c.callerLine,
               n.depth + 1});
      } else {
        if (!matches(c.caller, n.name) ||
            (!n.path.empty() &&
             (edge.path != n.path || c.callerLine != n.line)))
          continue;
        std::set<std::pair<std::filesystem::path, size_t>> targets;
        for (const auto& e : index.calls)
          if (matches(e.call.caller, c.callee))
            targets.emplace(e.path, e.call.callerLine);
        if (targets.empty())
          for (size_t i = 0; i < index.rows.size(); ++i) {
            const auto& s = index.rows[i];
            if ((s.kind == "function" || s.kind == "method") &&
                matches(s.name, c.callee))
              targets.emplace(index.paths[i], s.line);
          }
        if (targets.size() == 1)
          visit({c.callee, "inferred", targets.begin()->first,
                 targets.begin()->second, n.depth + 1});
        else
          visit({c.callee, targets.empty() ? "unresolved" : "ambiguous",
                 edge.path, c.line, n.depth + 1});
      }
    }
    stack.erase(key);
  };
  visit({name, "root", path, line, 0});
  return out;
}
}  // namespace dodev::symbols
