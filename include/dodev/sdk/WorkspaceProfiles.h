#pragma once
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "dodev/settings/Json.h"
namespace dodev::sdk {
inline std::filesystem::path workspaceProfilePath(
    const std::filesystem::path& root, const std::string& owner) {
  if (root.empty() || owner.empty() || owner == "." || owner == ".." ||
      owner.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUV"
                              "WXYZ0123456789._-") != std::string::npos)
    throw std::runtime_error("Invalid workspace or plugin ID");
  return root / ".dodev" / "plugins" / owner / "profiles.json";
}
inline Json parseWorkspaceProfiles(const std::string& text) {
  Json json;
  std::string err;
  if (text.size() > 1024 * 1024 || !Json::parse(text, json, &err) ||
      !json.isObject() || !json["profiles"].isArray())
    throw std::runtime_error("Invalid workspace profiles: " + err);
  auto profiles = json["profiles"].asArray();
  if (profiles.size() > 100)
    throw std::runtime_error("At most 100 profiles per plugin");
  for (auto& profile : profiles)
    if (!profile.isObject() || profile["name"].string().empty())
      throw std::runtime_error("Every profile needs a name");
  return json;
}
inline Json::Array workspaceProfiles(const std::filesystem::path& root,
                                     const std::string& owner) {
  if (root.empty()) return {};
  auto path = workspaceProfilePath(root, owner);
  if (!std::filesystem::exists(path)) return {};
  if (!std::filesystem::is_regular_file(path) ||
      std::filesystem::file_size(path) > 1024 * 1024)
    throw std::runtime_error("Invalid or oversized workspace profiles file");
  std::ifstream in(path, std::ios::binary);
  std::string text((std::istreambuf_iterator<char>(in)), {});
  if (!in) throw std::runtime_error("Cannot read workspace profiles");
  return parseWorkspaceProfiles(text)["profiles"].asArray();
}
}  // namespace dodev::sdk
