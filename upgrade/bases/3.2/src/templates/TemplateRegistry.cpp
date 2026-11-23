#include "dodev/templates/TemplateRegistry.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
namespace dodev {
namespace {
std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}
std::string read(const std::filesystem::path &p) {
  std::ifstream in(p, std::ios::binary);
  return in ? std::string(std::istreambuf_iterator<char>(in),
                          std::istreambuf_iterator<char>())
            : "";
}
} // namespace
TemplateRegistry::TemplateRegistry() {
  registerTemplate({"cpp17", "C++17 source", ".cpp", R"(#include <iostream>

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    std::cout << "${FILE_NAME}\\n";
    return 0;
}
)",
                    "built-in"});
  registerTemplate({"cpp-header", "C++ header", ".h", R"(#pragma once

// ${FILE_NAME}
// Project: ${PROJECT_NAME}

class ${CLASS_NAME} {
public:
    ${CLASS_NAME}() = default;
    ~${CLASS_NAME}() = default;
};
)",
                    "built-in"});
  registerTemplate({"python", "Python script", ".py", R"(#!/usr/bin/env python3
"""${FILE_NAME} · ${PROJECT_NAME}"""


def main():
    pass


if __name__ == "__main__":
    main()
)",
                    "built-in"});
  registerTemplate({"json", "JSON file", ".json",
                    "{\n  \"name\": \"${PROJECT_NAME}\"\n}\n", "built-in"});
  registerTemplate({"xml", "XML document", ".xml",
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<root/>\n",
                    "built-in"});
  registerTemplate(
      {"cmake", "CMakeLists", ".txt",
       "cmake_minimum_required(VERSION 3.21)\nproject(${PROJECT_NAME} "
       "LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 17)\n",
       "built-in"});
}
void TemplateRegistry::registerTemplate(FileTemplate value) {
  if (value.id.empty() || value.id.size() > 128 ||
      value.body.size() > 1024 * 1024)
    return;
  value.extension = lower(value.extension);
  templates_[value.id] = std::move(value);
}
bool TemplateRegistry::loadDirectory(const std::filesystem::path &dir,
                                     std::vector<std::string> *warnings) {
  std::error_code ec;
  if (dir.empty() || !std::filesystem::is_directory(dir, ec))
    return true;
  const auto manifest = dir / "templates.json";
  if (!std::filesystem::exists(manifest, ec))
    return true;
  Json data;
  std::string error;
  if (!Json::parse(read(manifest), data, &error) || !data.isObject() ||
      !data["templates"].isArray()) {
    if (warnings)
      warnings->push_back("Bad templates.json: " + error);
    return false;
  }
  bool ok = true;
  for (const auto &item : data["templates"].asArray()) {
    const auto id = item["id"].string();
    const auto file = item["file"].string();
    const auto ext = item["extension"].string();
    if (!item.isObject() || id.empty() || file.empty() ||
        std::filesystem::path(file).is_absolute() ||
        file.find("..") != std::string::npos || file.size() > 256) {
      ok = false;
      continue;
    }
    const auto path = dir / file;
    auto body = read(path);
    if (body.size() > 1024 * 1024) {
      ok = false;
      continue;
    }
    if (body.empty() && std::filesystem::file_size(path, ec) > 0) {
      ok = false;
      continue;
    }
    registerTemplate({id, item["name"].string(id), ext, body, path.string()});
  }
  if (!ok && warnings)
    warnings->push_back(
        "Some templates were skipped (missing or invalid files)");
  return ok;
}
const FileTemplate *TemplateRegistry::find(const std::string &id) const {
  auto it = templates_.find(id);
  return it == templates_.end() ? nullptr : &it->second;
}
std::vector<FileTemplate> TemplateRegistry::all() const {
  std::vector<FileTemplate> out;
  for (const auto &item : templates_)
    out.push_back(item.second);
  return out;
}
std::string TemplateRegistry::associatedId(const std::string &extension,
                                           const Json &associations) const {
  auto ext = lower(extension);
  if (!ext.empty() && ext[0] != '.')
    ext = '.' + ext;
  if (const Json *associated = associations.find(ext);
      associated && associated->isString() && find(associated->string()))
    return associated->string();
  for (const auto &t : templates_)
    if (t.second.extension == ext)
      return t.first;
  return {};
}
std::string TemplateRegistry::render(
    const FileTemplate &value,
    const std::map<std::string, std::string> &variables) const {
  std::string out = value.body;
  for (const auto &pair : variables) {
    const auto token = "${" + pair.first + "}";
    size_t from = 0;
    while ((from = out.find(token, from)) != std::string::npos) {
      out.replace(from, token.size(), pair.second);
      from += pair.second.size();
    }
  }
  return out;
}
std::string TemplateRegistry::renderForExtension(
    const std::string &extension, const Json &associations,
    const std::map<std::string, std::string> &variables) const {
  if (const auto *value = find(associatedId(extension, associations)))
    return render(*value, variables);
  return {};
}
} // namespace dodev
