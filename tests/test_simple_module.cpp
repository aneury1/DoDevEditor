#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "dodev/plugins/PluginManager.h"
using namespace dodev;
namespace fs = std::filesystem;
static void check(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
struct ViewCapture {
  std::string text;
  std::vector<std::string> actions;
};
int main() {
  const auto root =
      fs::temp_directory_path() /
      ("dodev_simple_module_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root / "plugins" / "simple-module");
  try {
    fs::copy(
        fs::path(SIMPLE_MODULE_DIR), root / "plugins" / "simple-module",
        fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    SettingsManager settings(root / "config");
    settings.load();
    TemplateRegistry templates;
    ThemeManager themes;
    PluginManager manager(settings, templates, themes);
    const std::string owner = "com.dodev.simplemodule";
    check(manager.discoverAndLoad(root / "plugins") == 0,
          "explicit opt-in required");
    manager.setEnabled(owner, true);
    check(manager.discoverAndLoad(root / "plugins") == 1, "module activation");
    check(manager.menus().size() == 3, "three menu entries");
    for (auto& menu : manager.menus())
      check(menu.location == "Options", "menu location");
    check(manager.settingSpecs().size() == 2, "plugin-owned schema");
    check(manager.updatePluginSetting(owner, "message", "Custom message"),
          "save message");
    check(manager.updatePluginSetting(owner, "level", "warning"), "save level");
    check(!manager.updatePluginSetting(owner, "level", "invalid"),
          "reject invalid choice");
    check(manager.views().size() == 2, "two registered views");
    check(pluginViewLocation(manager.views()[0]) == PluginViewLocation::Editor,
          "editor placement");
    check(pluginViewLocation(manager.views()[1]) == PluginViewLocation::Output,
          "output placement");
    ViewCapture captured;
    DodevUiBuilder builder{
        sizeof(DodevUiBuilder), &captured, [](void*, const char*) {},
        [](void* c, const char*, const char* action) {
          static_cast<ViewCapture*>(c)->actions.emplace_back(action);
        },
        [](void* c, const char* text) {
          static_cast<ViewCapture*>(c)->text = text;
        }};
    manager.buildView(manager.views()[0], builder);
    check(captured.text == "Custom message", "editor reads setting");
    check(captured.actions.size() == 2 &&
              captured.actions[1] == "plugin.view.simplemodule.output",
          "editor button actions");
    manager.buildView(manager.views()[1], builder);
    check(captured.text.find("[WARNING] Custom message") != std::string::npos,
          "output reads settings");
    check(manager.execute("simplemodule.sayHello"), "command execution");
    manager.shutdown();
    check(manager.discoverAndLoad(root / "plugins") == 1, "reload module");
    check(manager.pluginSetting(owner, "message") == "Custom message",
          "persistent value");
    manager.shutdown();
  } catch (...) {
    fs::remove_all(root);
    throw;
  }
  fs::remove_all(root);
  std::cout << "Simple Module integration passed\n";
}
