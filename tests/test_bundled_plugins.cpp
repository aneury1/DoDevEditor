#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "dodev/plugins/PluginManager.h"
#include "dodev/plugins/PluginPaths.h"
namespace fs = std::filesystem;
void check(bool value, const std::string& message) {
  if (!value) throw std::runtime_error(message);
}
int main() {
  const fs::path buildRoot = DODEV_QR_BUILD_ROOT;
  check(fs::exists(buildRoot / "qrcode" / "plugin.json"),
        "QR manifest missing in build root");
  const fs::path fake = fs::temp_directory_path() / "dodev_bundled_qrcode_test";
  fs::remove_all(fake);
  dodev::SettingsManager settings(fake / "config");
  settings.set("plugins.enabledIds",
               dodev::Json::Array{dodev::Json("com.dodev.qrcode")});
  dodev::TemplateRegistry templates;
  dodev::ThemeManager themes;
  dodev::PluginManager manager(settings, templates, themes);
  const auto active = manager.discoverAndLoad(
      std::vector<fs::path>{fake / "missing-user", buildRoot});
  check(active == 1, "The enabled bundled QR plugin must load");
  bool foundQr = false;
  for (const auto& plugin : manager.records())
    if (plugin.manifest.id == "com.dodev.qrcode")
      foundQr = plugin.status == "Active";
  check(foundQr, "QR plugin not discovered or not active");
  check(!manager.commands().empty(), "QR plugin commands missing");
  check(
      manager.discoverAndLoad(std::vector<fs::path>{buildRoot, buildRoot}) == 1,
      "Duplicate search roots must not load twice");
  manager.shutdown();
  fs::remove_all(fake);
  check(dodev::bundledPluginDirectory() ==
            dodev::executableDirectory() / "plugins",
        "Installed bundle path must be executable-relative");
  std::cout << "PASS: bundled QR module discovered, loaded, and duplicate "
               "roots ignored\n";
}
