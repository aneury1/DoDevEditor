#include "SymbolCache.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <random>
#include <set>

#include "dodev/settings/Json.h"
namespace dodev::symbols {
namespace fs = std::filesystem;
namespace {
std::string read(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return in ? std::string(std::istreambuf_iterator<char>(in), {})
            : std::string{};
}
bool supported(const fs::path& p) {
  auto e = p.extension().string();
  for (auto& c : e)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  static const std::set<std::string> ex = {
      ".h",   ".hpp", ".hh", ".hxx", ".c",   ".cc",  ".cpp",  ".cxx", ".ipp",
      ".inl", ".py",  ".js", ".ts",  ".tsx", ".jsx", ".java", ".kt",  ".cs"};
  return ex.count(e) != 0;
}
}  // namespace
WorkspaceIndex indexWorkspace(const fs::path& root, const Options& options) {
  WorkspaceIndex out;
  out.root = root;
  const auto cache = root / ".dodev" / "symbols-cache.json";
  const auto key = std::to_string(static_cast<int>(options.mode)) + ":" +
                   options.clangExecutable + ":" +
                   std::to_string(options.timeoutMs) + ":" +
                   options.captureKinds;
  Json old;
  if (!Json::parse(read(cache), old) || old["version"].integer() != 2 ||
      old["options"].string() != key)
    old = Json::object();
  Json next = Json::object();
  next["version"] = 2;
  next["options"] = key;
  next["files"] = Json::object();
  std::error_code ec;
  for (fs::recursive_directory_iterator
           it(root, fs::directory_options::skip_permission_denied, ec),
       end;
       it != end; it.increment(ec)) {
    if (ec) {
      out.diagnostic = ec.message();
      break;
    }
    auto p = it->path();
    std::error_code e;
    if (it->is_directory(e)) {
      auto n = p.filename().string();
      if (n == ".git" || n == ".dodev" || n == "build" || n == "node_modules" ||
          n == "dist" || n == ".cache" || n == ".venv" ||
          n.rfind("cmake-build-", 0) == 0)
        it.disable_recursion_pending();
      continue;
    }
    if (!supported(p) || it->is_symlink(e) || !it->is_regular_file(e) || e)
      continue;
    auto size = it->file_size(e);
    if (e || size > 4 * 1024 * 1024) {
      ++out.skipped;
      continue;
    }
    auto stamp = fs::last_write_time(p, e);
    if (e) {
      ++out.skipped;
      continue;
    }
    auto rel = p.lexically_relative(root).generic_u8string();
    auto time = std::to_string(stamp.time_since_epoch().count());
    const auto& prev = static_cast<const Json&>(old)["files"][rel];
    Json entry;
    if (prev["mtime"].string() == time &&
        prev["size"].string() == std::to_string(size) &&
        prev["rows"].isArray() && prev["calls"].isArray()) {
      entry = prev;
      ++out.hits;
    } else {
      std::ifstream in(p, std::ios::binary);
      if (!in) {
        ++out.skipped;
        continue;
      }
      std::string text((std::istreambuf_iterator<char>(in)), {});
      if (in.bad() || text.find('\0') != std::string::npos) {
        ++out.skipped;
        continue;
      }
      auto result = extract(p.u8string(), text, options);
      ++out.parsed;
      Json::Array rows;
      for (const auto& s : result.rows) {
        Json j = Json::object();
        j["kind"] = s.kind;
        j["name"] = s.name;
        j["signature"] = s.signature;
        j["line"] = static_cast<double>(s.line);
        j["column"] = static_cast<double>(s.column);
        rows.push_back(std::move(j));
      }
      entry = Json::object();
      entry["mtime"] = time;
      entry["size"] = std::to_string(size);
      entry["rows"] = std::move(rows);
      Json::Array calls;
      for (const auto& c : result.calls) {
        Json j = Json::object();
        j["caller"] = c.caller;
        j["callee"] = c.callee;
        j["callerLine"] = static_cast<double>(c.callerLine);
        j["line"] = static_cast<double>(c.line);
        calls.push_back(std::move(j));
      }
      entry["calls"] = std::move(calls);
      entry["parser"] = result.parser;
      entry["diagnostic"] = result.diagnostics;
    }
    if (out.diagnostic.empty() && !entry["diagnostic"].string().empty())
      out.diagnostic = entry["diagnostic"].string();
    for (const auto& j : entry["rows"].asArray()) {
      auto line = j["line"].integer(), col = j["column"].integer();
      if (line < 1 || col < 1) continue;
      out.rows.push_back(
          {j["kind"].string(), j["name"].string(), j["signature"].string(),
           static_cast<std::size_t>(line), static_cast<std::size_t>(col)});
      out.paths.push_back(p);
    }
    for (const auto& j : entry["calls"].asArray()) {
      auto a = j["callerLine"].integer(), b = j["line"].integer();
      if (a > 0 && b > 0)
        out.calls.push_back({{j["caller"].string(), j["callee"].string(),
                              static_cast<size_t>(a), static_cast<size_t>(b)},
                             p});
    }
    next["files"][rel] = std::move(entry);
  }
  if (ec) {
    out.diagnostic = ec.message();
    return out;
  }
  if (next.dump(0) != old.dump(0)) {
    fs::create_directories(cache.parent_path(), ec);
    auto tmp = cache;
    tmp += ".tmp." +
           std::to_string(
               std::chrono::steady_clock::now().time_since_epoch().count()) +
           "." + std::to_string(std::random_device{}());
    if (!ec) {
      std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
      f << next.dump(0);
      f.close();
      if (f)
        fs::rename(tmp, cache, ec);
      else
        ec = std::make_error_code(std::errc::io_error);
    }
    if (ec) {
      out.diagnostic = "Cache write failed: " + ec.message();
      std::error_code ignored;
      fs::remove(tmp, ignored);
    }
  }
  return out;
}
}  // namespace dodev::symbols
