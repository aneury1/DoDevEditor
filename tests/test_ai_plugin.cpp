#include <filesystem>
#include <stdexcept>
#include <string>

#include "dodev/plugins/PluginManager.h"
#define REQUIRE(x)                          \
  do {                                      \
    if (!(x)) throw std::runtime_error(#x); \
  } while (0)
int main() {
  namespace fs = std::filesystem;
  const auto root = fs::temp_directory_path() / "dodev-ai-plugin-smoke";
  fs::remove_all(root);
  dodev::SettingsManager settings(root);
  REQUIRE(settings.load());
  dodev::ThemeManager themes;
  dodev::TemplateRegistry templates;
  dodev::PluginManager manager(settings, templates, themes);
  int opens = 0;
  dodev::PluginManager::HostHooks hooks;
  hooks.openAiAssistant = [&]() {
    ++opens;
    return true;
  };
  manager.setHostHooks(std::move(hooks));
  REQUIRE(manager.discoverAndLoad(DODEV_AI_TEST_ROOT) == 0);
  manager.setEnabled("com.dodev.aiassistant", true);
  REQUIRE(manager.discoverAndLoad(DODEV_AI_TEST_ROOT) == 1);
  REQUIRE(manager.execute("ai.open"));
  REQUIRE(opens == 1);
  bool foundMenu = false, foundSettings = false;
  for (const auto& item : manager.menus())
    if (item.location == "Tools" && item.command == "ai.open") foundMenu = true;
  for (const auto& item : manager.settingSpecs())
    if (item.owner == "com.dodev.aiassistant" && item.key == "model")
      foundSettings = true;
  REQUIRE(foundMenu && foundSettings);
  manager.shutdown();
  fs::remove_all(root);
  return 0;
}
