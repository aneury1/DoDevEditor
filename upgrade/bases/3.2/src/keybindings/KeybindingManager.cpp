#include "dodev/keybindings/KeybindingManager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
namespace dodev::keys {
namespace {
std::string strip(const std::string& text) {
  auto b = text.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return {};
  auto e = text.find_last_not_of(" \t\r\n");
  return text.substr(b, e - b + 1);
}
std::string up(std::string s) {
  for (char& c : s)
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
std::string keyName(const std::string& candidate) {
  const std::string n = up(strip(candidate));
  if (n.size() == 1 &&
      ((n[0] >= 'A' && n[0] <= 'Z') || (n[0] >= '0' && n[0] <= '9')))
    return n;
  static const std::map<std::string, std::string> alias = {
      {"`", "Grave"},
      {"GRAVE", "Grave"},
      {"BACKTICK", "Grave"},
      {"/", "Slash"},
      {"SLASH", "Slash"},
      {"DELETE", "Delete"},
      {"DEL", "Delete"},
      {"SPACE", "Space"},
      {"UP", "Up"},
      {"DOWN", "Down"},
      {"LEFT", "Left"},
      {"RIGHT", "Right"},
      {"HOME", "Home"},
      {"END", "End"},
      {"TAB", "Tab"},
      {"ENTER", "Enter"},
      {"ESCAPE", "Escape"},
      {"ESC", "Escape"},
      {"BACKSPACE", "Backspace"},
      {"PAGEUP", "PageUp"},
      {"PAGEDOWN", "PageDown"},
      {"INSERT", "Insert"},
      {"MINUS", "Minus"},
      {"-", "Minus"},
      {"EQUAL", "Equal"},
      {"=", "Equal"},
      {"SEMICOLON", "Semicolon"},
      {";", "Semicolon"},
      {"COMMA", "Comma"},
      {",", "Comma"},
      {"PERIOD", "Period"},
      {".", "Period"},
      {"BACKSLASH", "Backslash"},
      {"[", "LeftBracket"},
      {"]", "RightBracket"},
      {"F1", "F1"},
      {"F2", "F2"},
      {"F3", "F3"},
      {"F4", "F4"},
      {"F5", "F5"},
      {"F6", "F6"},
      {"F7", "F7"},
      {"F8", "F8"},
      {"F9", "F9"},
      {"F10", "F10"},
      {"F11", "F11"},
      {"F12", "F12"}};
  const auto it = alias.find(n);
  return it == alias.end() ? "" : it->second;
}
}  // namespace
Shortcut parseShortcut(const std::string& source) {
  Shortcut s;
  const auto raw = strip(source);
  if (raw.empty()) return s;
  std::istringstream segments(raw);
  std::string piece;
  bool keyFound = false;
  while (std::getline(segments, piece, '+')) {
    const auto x = up(strip(piece));
    if (x.empty()) return {};
    unsigned mod = 0;
    if (x == "CTRL" || x == "CONTROL")
      mod = Ctrl;
    else if (x == "SHIFT")
      mod = Shift;
    else if (x == "ALT" || x == "OPTION")
      mod = Alt;
    else if (x == "SUPER" || x == "META" || x == "CMD" || x == "COMMAND")
      mod = Super;
    if (mod) {
      if (keyFound || (s.modifiers & mod)) return {};
      s.modifiers |= mod;
    } else {
      if (keyFound) return {};
      s.key = keyName(x);
      if (s.key.empty()) return {};
      keyFound = true;
    }
  }
  // Block bare printable keys: those would interfere with normal text entry.
  s.valid = keyFound && (s.modifiers != 0 || s.key.size() > 1);
  return s.valid ? s : Shortcut{};
}
std::string formatShortcut(const Shortcut& k) {
  if (!k.valid) return {};
  std::string s;
  if (k.modifiers & Ctrl) s += "Ctrl+";
  if (k.modifiers & Shift) s += "Shift+";
  if (k.modifiers & Alt) s += "Alt+";
  if (k.modifiers & Super) s += "Super+";
  return s + k.key;
}
std::string normalizeShortcut(const std::string& source) {
  return formatShortcut(parseShortcut(source));
}
std::map<std::string, std::string> KeybindingManager::defaults() {
  return {
      {"app.about", ""},
      {"designer.close", ""},
      {"designer.preview", ""},
      {"designer.save_json", ""},
      {"designer.save_xml", ""},
      {"edit.duplicate", ""},
      {"edit.indent", ""},
      {"edit.insert_above", ""},
      {"edit.insert_below", ""},
      {"edit.join_lines", ""},
      {"edit.lowercase", ""},
      {"edit.outdent", ""},
      {"edit.sort_asc", ""},
      {"edit.sort_desc", ""},
      {"edit.titlecase", ""},
      {"edit.trim_whitespace", ""},
      {"edit.uppercase", ""},
      {"folder.refresh", ""},
      {"git.details", ""},
      {"git.refresh", ""},
      {"keybindings.apply", ""},
      {"keybindings.reset", ""},
      {"keybindings.reset_all", ""},
      {"keybindings.unbind", ""},
      {"navigation.goto_close", ""},
      {"navigation.goto_submit", ""},
      {"navigation.quick_accept", ""},
      {"navigation.quick_close", ""},
      {"panel.git", ""},
      {"panel.output", ""},
      {"plugin.disable", ""},
      {"plugin.enable", ""},
      {"plugin.rescan", ""},
      {"plugin.setting.save", ""},
      {"search.hide", ""},
      {"search.replace", ""},
      {"search.replace_all", ""},
      {"selection.cursor_down", ""},
      {"selection.cursor_up", ""},
      {"selection.line_end", ""},
      {"selection.line_start", ""},
      {"selection.prev_occurrence", ""},
      {"selection.swap_anchor", ""},
      {"selection.to_end", ""},
      {"selection.to_start", ""},
      {"selection.word", ""},
      {"settings.apply", ""},
      {"settings.browse_editor_font", ""},
      {"settings.browse_plugins", ""},
      {"settings.browse_templates", ""},
      {"settings.browse_terminal_font", ""},
      {"settings.browse_themes", ""},
      {"settings.browse_ui_font", ""},
      {"settings.cancel", ""},
      {"settings.defaults", ""},
      {"template.cancel", ""},
      {"template.create", ""},
      {"terminal.clear", ""},
      {"terminal.close", ""},
      {"terminal.configure_build", ""},
      {"terminal.configure_tasks", ""},
      {"terminal.copy", ""},
      {"terminal.interrupt", ""},
      {"terminal.next", ""},
      {"terminal.paste", ""},
      {"terminal.previous", ""},
      {"terminal.rename", ""},
      {"terminal.restart", ""},
      {"terminal.run_file", ""},
      {"terminal.run_selected", ""},
      {"theme.designer", ""},
      {"theme.export", ""},
      {"theme.import", ""},
      {"tool.csv", ""},
      {"tool.data_server", ""},
      {"tool.dlt", ""},
      {"file.new", "Ctrl+N"},
      {"template.show", "Ctrl+Alt+N"},
      {"file.open", "Ctrl+O"},
      {"folder.open", "Ctrl+Shift+O"},
      {"file.save", "Ctrl+S"},
      {"file.save_as", "Ctrl+Shift+S"},
      {"file.close", "Ctrl+W"},
      {"app.exit", "Ctrl+Q"},
      {"edit.undo", "Ctrl+Z"},
      {"edit.redo", "Ctrl+Y"},
      {"edit.cut", "Ctrl+X"},
      {"edit.copy", "Ctrl+C"},
      {"edit.paste", "Ctrl+V"},
      {"edit.delete", "Delete"},
      {"search.show", "Ctrl+F"},
      {"search.next", "F3"},
      {"search.prev", "Shift+F3"},
      {"search.replace_show", "Ctrl+H"},
      {"edit.find_workspace", "Ctrl+Shift+F"},
      {"edit.toggle_line_comment", "Ctrl+/"},
      {"edit.toggle_block_comment", "Shift+Alt+A"},
      {"selection.all", "Ctrl+A"},
      {"selection.line", "Ctrl+L"},
      {"selection.expand", "Shift+Alt+Right"},
      {"selection.shrink", "Shift+Alt+Left"},
      {"selection.next_occurrence", "Ctrl+D"},
      {"edit.copy_line_up", "Shift+Alt+Up"},
      {"edit.copy_line_down", "Shift+Alt+Down"},
      {"edit.move_line_up", "Alt+Up"},
      {"edit.move_line_down", "Alt+Down"},
      {"terminal.new", "Ctrl+Shift+Grave"},
      {"terminal.split", "Ctrl+Shift+5"},
      {"terminal.run_build", "Ctrl+Shift+B"},
      {"theme.next", "Ctrl+Shift+T"},
      {"settings.show", "Ctrl+Alt+S"},
      {"navigation.quick_open", "Ctrl+P"},
      {"navigation.go_to_line", "Ctrl+G"},
      {"navigation.command_palette", "Ctrl+Shift+P"},
      {"keybindings.show", "Ctrl+K"},
      {"terminal.focus", "Ctrl+Grave"},
      {"terminal.run_task", "Ctrl+Shift+R"},
      {"panel.explorer", "Ctrl+Shift+E"},
      {"edit.delete_line", "Ctrl+Shift+K"},
  };
}
KeybindingManager::KeybindingManager(SettingsManager& settings)
    : settings_(settings), defaults_(defaults()), bindings_(defaults_) {}
bool KeybindingManager::hasCommand(const std::string& command) const {
  return defaults_.count(command) > 0;
}
void KeybindingManager::registerCommand(const std::string& command,
                                        const std::string& key) {
  if (command.empty() || command.size() > 128 || hasCommand(command)) return;
  const auto normalized = normalizeShortcut(key);
  defaults_[command] = normalized;
  bindings_[command] = normalized;
}
void KeybindingManager::load() {
  bindings_ = defaults_;
  dormantOverrides_.clear();
  diagnostics_.clear();
  malformed_ = false;
  const auto file = settings_.directory() / "keybindings.json";
  std::error_code ec;
  if (!std::filesystem::exists(file, ec)) return;
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    malformed_ = true;
    diagnostics_.push_back("Cannot read keybindings.json");
    return;
  }
  std::string source{std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>()};
  Json j;
  std::string error;
  if (source.size() > 1024 * 1024 || !Json::parse(source, j, &error) ||
      !j.isArray()) {
    malformed_ = true;
    diagnostics_.push_back("Invalid keybindings.json: " + error);
    return;
  }
  auto candidate = bindings_;
  for (const auto& entry : j.asArray()) {
    const auto command = entry["command"].string(), key = entry["key"].string();
    if (!entry.isObject() || !entry["command"].isString() ||
        !entry["key"].isString()) {
      diagnostics_.push_back("Ignoring malformed keybinding entry");
      continue;
    }
    const auto normalized = key.empty() ? "" : normalizeShortcut(key);
    if (!key.empty() && normalized.empty()) {
      diagnostics_.push_back("Invalid shortcut for " + command + ": " + key);
      continue;
    }
    if (!hasCommand(command)) {
      // Preserve disabled plugin keybindings without registering their actions.
      if (SettingsManager::validPluginId(command))
        dormantOverrides_[command] = normalized;
      diagnostics_.push_back("Inactive/unknown shortcut command: " + command);
      continue;
    }
    candidate[command] = normalized;
  }
  std::map<std::string, std::string> assigned;
  bool collision = false;
  for (const auto& kv : candidate) {
    if (kv.second.empty()) continue;
    const auto other = assigned.find(kv.second);
    if (other != assigned.end()) {
      collision = true;
      diagnostics_.push_back("Conflict: " + kv.second + " for " +
                             other->second + " and " + kv.first);
    } else
      assigned[kv.second] = kv.first;
  }
  if (collision) {
    // Fail open: a conflicting configuration cannot hide built-in shortcuts.
    diagnostics_.push_back(
        "Conflicting overrides ignored; using safe defaults");
    bindings_ = defaults_;
  } else
    bindings_ = std::move(candidate);
}
bool KeybindingManager::set(const std::string& command, const std::string& key,
                            std::string* error) {
  if (!hasCommand(command)) {
    if (error) *error = "Unknown command";
    return false;
  }
  const auto normalized = key.empty() ? "" : normalizeShortcut(key);
  if (!key.empty() && normalized.empty()) {
    if (error)
      *error = "Use one valid shortcut, e.g. Ctrl+G, Ctrl+Shift+P or F3";
    return false;
  }
  if (!normalized.empty())
    for (const auto& pair : bindings_)
      if (pair.first != command && pair.second == normalized) {
        if (error) *error = "Shortcut already assigned to " + pair.first;
        return false;
      }
  bindings_[command] = normalized;
  if (error) error->clear();
  return true;
}
void KeybindingManager::reset(const std::string& command) {
  if (hasCommand(command)) bindings_[command] = defaults_.at(command);
}
void KeybindingManager::resetAll() { bindings_ = defaults_; }
std::string KeybindingManager::get(const std::string& command) const {
  const auto it = bindings_.find(command);
  return it == bindings_.end() ? "" : it->second;
}
std::vector<Binding> KeybindingManager::all() const {
  std::vector<Binding> out;
  for (const auto& kv : bindings_) {
    const auto it = defaults_.find(kv.first);
    out.push_back({kv.first, kv.second,
                   it == defaults_.end() || it->second != kv.second});
  }
  return out;
}
bool KeybindingManager::save(std::string* error) const {
  if (malformed_) {
    if (error)
      *error =
          "Refusing to overwrite invalid keybindings.json; repair or rename "
          "the file";
    return false;
  }
  Json::Array changes;
  for (const auto& kv : dormantOverrides_) {
    Json obj = Json::object();
    obj["key"] = kv.second;
    obj["command"] = kv.first;
    changes.emplace_back(std::move(obj));
  }
  for (const auto& kv : bindings_)
    if (kv.second != defaults_.at(kv.first)) {
      Json obj = Json::object();
      obj["key"] = kv.second;
      obj["command"] = kv.first;
      changes.emplace_back(std::move(obj));
    }
  return SettingsManager::writeAtomic(
      settings_.directory() / "keybindings.json",
      Json(std::move(changes)).dump(2) + "\n", error);
}
}  // namespace dodev::keys
