#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "dodev/plugins/PluginManager.h"
namespace fs = std::filesystem;
using namespace dodev;
void check(bool v, const char* msg) {
  if (!v) throw std::runtime_error(msg);
}
int main() {
  auto root =
      fs::temp_directory_path() /
      ("preview-sdk-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root / "plugins" / "file-preview");
  try {
    fs::copy(
        fs::path(PREVIEW_PLUGIN_DIR), root / "plugins" / "file-preview",
        fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    SettingsManager settings(root / "config");
    settings.load();
    TemplateRegistry templates;
    ThemeManager themes;
    PluginManager manager(settings, templates, themes);
    std::vector<std::string> ids, docs;
    PluginManager::HostHooks hooks;
    hooks.activeDocument = [](std::string& path, std::string& text) {
      path = "example.md";
      text = "# Unsaved content";
      return true;
    };
    auto csv = root / "data.csv";
    std::ofstream(csv) << "a,b\n1,2\n";
    hooks.selectPath = [&](const std::string&, bool directory,
                           std::function<void(const std::string&)> cb) {
      check(!directory, "file picker");
      cb(csv.u8string());
      return true;
    };
    hooks.showHtmlView = [&](const std::string& id, const std::string&,
                             const std::string& side, const std::string& text) {
      check(side == "Editor", "editor location");
      ids.push_back(id);
      docs.push_back(text);
      return true;
    };
    bool imageSeen = false, treeSeen = false;
    hooks.showImageView = [&](const std::string&, const std::string&,
                              const void* data, size_t n) {
      imageSeen = n == 3 && std::string(static_cast<const char*>(data), n) ==
                                std::string("A\0B", 3);
      return imageSeen;
    };
    hooks.showTreeView = [&](const std::string&, const std::string&,
                             const DodevTreeNode* nodes, size_t count) {
      treeSeen = count == 2 && nodes[1].depth == 1 &&
                 std::string(nodes[1].label).find("42") != std::string::npos;
      return treeSeen;
    };
    manager.setHostHooks(std::move(hooks));
    manager.setEnabled("com.dodev.filepreview", true);
    check(manager.discoverAndLoad(root / "plugins") == 1, "load plugin");
    check(manager.settingSpecs().size() == 10 && manager.menus().size() == 2,
          "owned settings and commands");
    check(
        manager.execute("preview.active") && manager.execute("preview.choose"),
        "commands");
    check(manager.supportsPreview("EXAMPLE.PNG") &&
              manager.supportsPreview("x.csv") &&
              !manager.supportsPreview("x.cpp"),
          "generic extension registration");
    std::string modified = "# Changed buffer";
    check(manager.previewFile("example.md", &modified), "registered preview");
    check(manager.previewFile("custom.unknown", &modified),
          "explicit hex fallback");
    check(ids.size() == 4 && ids[0] != ids[1], "independent tabs");
    check(docs[0].find("Unsaved content") != std::string::npos &&
              docs[1].find("<td>2</td>") != std::string::npos,
          "source and table");
    auto img = root / "test.png";
    {
      std::ofstream f(img, std::ios::binary);
      f.write("A\0B", 3);
    }
    std::string json = "{\"n\":42}";
    check(manager.previewFile(img) && imageSeen, "binary image data callback");
    check(manager.previewFile("x.json", &json) && treeSeen, "tree callback");
    manager.shutdown();
    check(!manager.supportsPreview("x.json"), "unregister preview on shutdown");
    fs::remove_all(root);
    std::cout << "Preview plugin SDK passed\n";
  } catch (const std::exception& e) {
    fs::remove_all(root);
    std::cerr << e.what() << '\n';
    return 1;
  }
}
