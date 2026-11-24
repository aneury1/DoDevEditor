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
  int menuCount = 0;
  for (const auto& item : manager.menus())
    if (item.location == "Tools") ++menuCount;
  CHECK(menuCount == 2);
  manager.shutdown();
  fs::remove_all(dir);
  return 0;
}
