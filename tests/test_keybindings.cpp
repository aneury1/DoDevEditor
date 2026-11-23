#include "dodev/keybindings/KeybindingManager.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace dodev;
int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("dodev-keybindings-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  SettingsManager settings(directory);
  settings.load();
  keys::KeybindingManager bindings(settings);
  bindings.load();
  assert(bindings.get("navigation.go_to_line") == "Ctrl+G");
  assert(bindings.get("navigation.quick_open") == "Ctrl+P");
  assert(bindings.get("navigation.command_palette") == "Ctrl+Shift+P");
  assert(keys::normalizeShortcut(" shift + ctrl + p ") == "Ctrl+Shift+P");
  assert(keys::normalizeShortcut("ctrl+alt+up") == "Ctrl+Alt+Up");
  assert(keys::normalizeShortcut("F3") == "F3");
  assert(keys::normalizeShortcut("ctrl+ctrl+P").empty());
  assert(keys::normalizeShortcut("P").empty());
  assert(keys::normalizeShortcut("Ctrl+wrong").empty());
  std::string error;
  assert(!bindings.set("navigation.go_to_line", "Ctrl+P", &error));
  assert(error.find("navigation.quick_open") != std::string::npos);
  assert(!bindings.set("navigation.go_to_line", "Q", &error));
  assert(bindings.set("navigation.go_to_line", "Ctrl+Alt+G", &error));
  assert(bindings.set("edit.redo", "", &error));
  assert(bindings.save(&error));
  assert(std::filesystem::exists(directory / "keybindings.json"));
  keys::KeybindingManager reloaded(settings);
  reloaded.load();
  assert(reloaded.get("navigation.go_to_line") == "Ctrl+Alt+G");
  assert(reloaded.get("edit.redo").empty());
  reloaded.resetAll();
  assert(reloaded.get("navigation.go_to_line") == "Ctrl+G");
  assert(reloaded.save(&error));
  // A swap must be validated after all user overrides are applied, not one by
  // one.
  {
    std::ofstream out(directory / "keybindings.json");
    out << R"([{"key":"Ctrl+P","command":"navigation.go_to_line"},{"key":"Ctrl+G","command":"navigation.quick_open"},{"key":"Ctrl+Alt+1","command":"com.acme.disabledPlugin"}])";
  }
  keys::KeybindingManager swapped(settings);
  swapped.load();
  assert(swapped.get("navigation.go_to_line") == "Ctrl+P");
  assert(swapped.get("navigation.quick_open") == "Ctrl+G");
  assert(swapped.save(&error));
  {
    std::ifstream in(directory / "keybindings.json");
    std::string content{std::istreambuf_iterator<char>(in),
                        std::istreambuf_iterator<char>()};
    assert(content.find("com.acme.disabledPlugin") != std::string::npos);
  }
  {
    std::ofstream file(directory / "keybindings.json", std::ios::trunc);
    file << "{bad json";
  }
  keys::KeybindingManager invalid(settings);
  invalid.load();
  assert(invalid.get("navigation.go_to_line") == "Ctrl+G");
  assert(!invalid.maySave());
  assert(!invalid.save(&error));
  {
    std::ifstream file(directory / "keybindings.json");
    std::string s;
    std::getline(file, s);
    assert(s == "{bad json");
  }
  std::filesystem::remove_all(directory);
  std::cout << "PASS: default keys, parser, conflict detection, JSON "
               "persistence, fail-open and malformed preservation\n";
}
