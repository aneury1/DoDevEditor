#include <filesystem>
#include <stdexcept>
#include <string>

#include "dodev/plugins/PluginManager.h"
#define CHECK(x)                            \
  do {                                      \
    if (!(x)) throw std::runtime_error(#x); \
  } while (0)
int main() {
  namespace fs = std::filesystem;
  const auto dir = fs::temp_directory_path() / "dodev_new_plugins_tests";
  fs::remove_all(dir);
  dodev::SettingsManager settings(dir);
  CHECK(settings.load());
  settings.set("remoteJournal.tabLocation", "output");
  CHECK(settings.save());
  dodev::ThemeManager themes;
  dodev::TemplateRegistry templates;
  dodev::PluginManager manager(settings, templates, themes);
  int openedSymbols = 0, openedJournal = 0;
  dodev::PluginManager::HostHooks hooks;
  hooks.openSourceSymbols = [&] {
    ++openedSymbols;
    return true;
  };
  hooks.openRemoteJournal = [&] {
    ++openedJournal;
    return true;
  };
  manager.setHostHooks(std::move(hooks));
  CHECK(manager.discoverAndLoad(DODEV_NEW_PLUGIN_ROOT) == 0);
  manager.setEnabled("com.dodev.sourcesymbols", true);
  manager.setEnabled("com.dodev.remotejournal", true);
  CHECK(manager.discoverAndLoad(DODEV_NEW_PLUGIN_ROOT) == 2);
  CHECK(manager.execute("sourcesymbols.open"));
  CHECK(manager.execute("remotejournal.open"));
  CHECK(openedSymbols == 1 && openedJournal == 1);
  const std::string journal = "com.dodev.remotejournal";
  // Capture placement is no longer configurable: old preferences must not
  // change it.
  for (const auto& spec : manager.settingSpecs())
    CHECK(spec.owner != journal || spec.key != "tabLocation");
  CHECK(!manager.updatePluginSetting(journal, "port", "65536"));
  CHECK(!manager.updatePluginSettings(
      journal, {{"port", "invalid"}, {"payload", "not saved"}}));
  CHECK(manager.pluginSetting(journal, "payload").empty());
  CHECK(manager.updatePluginSettings(
      journal, {{"port", "2222"}, {"payload", "timeout"}}));
  CHECK(manager.pluginSetting(journal, "port") == "2222");
  CHECK(settings.loadPluginSettings(journal)["payload"].string() == "timeout");
  bool typed = false;
  for (const auto& s : manager.settingSpecs())
    if (s.owner == journal && s.key == "authentication")
      typed = s.type == "choice" && s.choices.size() == 2;
  CHECK(typed);
  int menuCount = 0;
  for (const auto& item : manager.menus())
    if (item.location == "Tools") ++menuCount;
  CHECK(menuCount == 2);
  manager.shutdown();
  CHECK(manager.settingSpecs().empty());
  CHECK(manager.settingsActions().empty());
  CHECK(manager.discoverAndLoad(DODEV_NEW_PLUGIN_ROOT) == 2);
  CHECK(manager.pluginSetting(journal, "port") == "2222");
  manager.shutdown();
  manager.setEnabled(journal, false);
  CHECK(manager.discoverAndLoad(DODEV_NEW_PLUGIN_ROOT) == 1);
  for (const auto& s : manager.settingSpecs()) CHECK(s.owner != journal);
  manager.shutdown();
  fs::remove_all(dir);
  return 0;
}
