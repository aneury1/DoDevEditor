#include "dodev/plugins/PluginManager.h"
#include <stdexcept>
#define REQUIRE(condition)                                                     \
  do {                                                                         \
    if (!(condition))                                                          \
      throw std::runtime_error(#condition);                                    \
  } while (0)
#include <chrono>
#include <filesystem>
#include <iostream>
#include <vector>
namespace fs = std::filesystem;
using namespace dodev;
namespace {
void label(void *ptr, const char *text) {
  static_cast<std::vector<std::string> *>(ptr)->push_back(text ? text : "");
}
void button(void *ptr, const char *title, const char *) {
  static_cast<std::vector<std::string> *>(ptr)->push_back(title ? title : "");
}
void text(void *ptr, const char *content) {
  static_cast<std::vector<std::string> *>(ptr)->push_back(content ? content
                                                                  : "");
}
} // namespace
int main() {
  const auto root =
      fs::temp_directory_path() /
      ("dodev_plugin_test_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root / "plugins" / "hello");
  try {
    for (const auto &entry :
         fs::directory_iterator(fs::path(DODEV_TEST_PLUGIN_DIR) / "hello"))
      if (entry.is_regular_file())
        fs::copy_file(entry.path(),
                      root / "plugins" / "hello" / entry.path().filename(),
                      fs::copy_options::overwrite_existing);
    SettingsManager settings(root);
    settings.load();
    TemplateRegistry templates;
    ThemeManager themes;
    PluginManager manager(settings, templates, themes);
    REQUIRE(manager.discoverAndLoad(root / "plugins") == 0);
    REQUIRE(manager.records().size() == 1);
    REQUIRE(manager.records()[0].status.find("Disabled") != std::string::npos);
    manager.setEnabled("com.dodev.hello", true);
    REQUIRE(settings.save());
    REQUIRE(manager.discoverAndLoad(root / "plugins") == 1);
    REQUIRE(manager.commands().size() == 1);
    REQUIRE(manager.menus().size() == 1);
    REQUIRE(manager.views().size() == 2);
    REQUIRE(manager.settingSpecs().size() == 1);
    REQUIRE(templates.find("hello-file"));
    REQUIRE(manager.execute("hello.say"));
    REQUIRE(manager.updatePluginSetting("com.dodev.hello", "greeting",
                                        "From CTest"));
    REQUIRE(manager.pluginSetting("com.dodev.hello", "greeting") ==
            "From CTest");
    manager.emit("document.opened", R"({"path":"/tmp/example.cpp"})");
    std::vector<std::string> strings;
    const DodevUiBuilder builder{sizeof(DodevUiBuilder), &strings, &label,
                                 &button, &text};
    manager.buildView(manager.views()[0], builder);
    REQUIRE(!strings.empty());
    manager.setEnabled("com.dodev.hello", false);
    REQUIRE(!manager.isEnabled("com.dodev.hello"));
    manager.shutdown();
    manager.shutdown();
    REQUIRE(manager.commands().empty());
    REQUIRE(manager.views().empty());
  } catch (...) {
    fs::remove_all(root);
    throw;
  }
  fs::remove_all(root);
  std::cout << "Native plugin integration tests passed\n";
}
