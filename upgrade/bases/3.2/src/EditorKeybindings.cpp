#include "EditorApp.h"
#include "EmbeddedNavigationUi.h"
#include <algorithm>
#include <cctype>
#include <limits>
#include <set>
#include <sstream>
#include <system_error>
namespace dodev {
namespace {
Rid widget(Rid win, const char *name) {
  return win == R_INVALID_ID ? R_INVALID_ID : Rwidget_find(win, name);
}
std::string textInput(Rid win, const char *name) {
  const auto w = widget(win, name);
  if (w == R_INVALID_ID)
    return {};
  const char *s = Rtext_input_get_text(w);
  return s ? s : "";
}
void setLabel(Rid win, const char *name, const std::string &value) {
  const auto w = widget(win, name);
  if (w != R_INVALID_ID)
    Rlabel_set_text(w, value.c_str());
}
std::string lower(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string trim(const std::string &s) {
  const auto a = s.find_first_not_of(" \r\t\n");
  if (a == std::string::npos)
    return {};
  auto b = s.find_last_not_of(" \r\t\n");
  return s.substr(a, b - a + 1);
}
RKey ravenKey(const std::string &key) {
  if (key.size() == 1) {
    const auto c = key[0];
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
      return static_cast<RKey>(c);
  }
  static const std::map<std::string, RKey> keys = {
      {"Grave", R_KEY_GRAVE},
      {"Slash", R_KEY_SLASH},
      {"Space", R_KEY_SPACE},
      {"Delete", R_KEY_DELETE},
      {"Up", R_KEY_UP},
      {"Down", R_KEY_DOWN},
      {"Left", R_KEY_LEFT},
      {"Right", R_KEY_RIGHT},
      {"Home", R_KEY_HOME},
      {"End", R_KEY_END},
      {"Tab", R_KEY_TAB},
      {"Enter", R_KEY_ENTER},
      {"Escape", R_KEY_ESCAPE},
      {"Backspace", R_KEY_BACKSPACE},
      {"PageUp", R_KEY_PAGE_UP},
      {"PageDown", R_KEY_PAGE_DOWN},
      {"Insert", R_KEY_INSERT},
      {"Minus", R_KEY_MINUS},
      {"Equal", R_KEY_EQUAL},
      {"Comma", R_KEY_COMMA},
      {"Period", R_KEY_PERIOD},
      {"Semicolon", R_KEY_SEMICOLON},
      {"Backslash", R_KEY_BACKSLASH},
      {"LeftBracket", R_KEY_LEFT_BRACKET},
      {"RightBracket", R_KEY_RIGHT_BRACKET},
      {"F1", R_KEY_F1},
      {"F2", R_KEY_F2},
      {"F3", R_KEY_F3},
      {"F4", R_KEY_F4},
      {"F5", R_KEY_F5},
      {"F6", R_KEY_F6},
      {"F7", R_KEY_F7},
      {"F8", R_KEY_F8},
      {"F9", R_KEY_F9},
      {"F10", R_KEY_F10},
      {"F11", R_KEY_F11},
      {"F12", R_KEY_F12}};
  auto it = keys.find(key);
  return it == keys.end() ? R_KEY_UNKNOWN : it->second;
}
Ruint ravenMods(unsigned m) {
  Ruint flags = 0;
  if (m & keys::Ctrl)
    flags |= R_MOD_CTRL;
  if (m & keys::Shift)
    flags |= R_MOD_SHIFT;
  if (m & keys::Alt)
    flags |= R_MOD_ALT;
  if (m & keys::Super)
    flags |= R_MOD_SUPER;
  return flags;
}
} // namespace
void EditorApp::installKeybindings() {
  if (window_ == R_INVALID_ID)
    return;
  for (const auto &shortcut : installedShortcuts_) {
    const auto key = ravenKey(shortcut.key);
    if (key != R_KEY_UNKNOWN)
      Rwindow_unregister_shortcut(window_, key, ravenMods(shortcut.modifiers));
  }
  installedShortcuts_.clear();
  for (const auto &binding : keybindings_.all()) {
    if (binding.key.empty())
      continue;
    const auto parsed = keys::parseShortcut(binding.key);
    const auto key = ravenKey(parsed.key);
    if (!parsed.valid || key == R_KEY_UNKNOWN)
      continue;
    Rwindow_register_shortcut(window_, key, ravenMods(parsed.modifiers),
                              binding.command.c_str());
    installedShortcuts_.push_back(parsed);
  }
  refreshShortcutLabels();
}
void EditorApp::refreshShortcutLabels() {
  if (window_ == R_INVALID_ID)
    return;
  for (const auto &b : keybindings_.all()) {
    std::string item = "kb_" + b.command;
    std::replace(item.begin(), item.end(), '.', '_');
    auto id = Rwidget_find(window_, item.c_str());
    if (id == R_INVALID_ID)
      continue;
    auto it = originalMenuLabels_.find(b.command);
    if (it == originalMenuLabels_.end()) {
      const char *label = Rmenu_item_get_text(id);
      it = originalMenuLabels_.emplace(b.command, label ? label : "").first;
    }
    std::string display = it->second;
    if (!b.key.empty())
      display += "    " + b.key;
    Rmenu_item_set_text(id, display.c_str());
  }
}
void EditorApp::openGoToLine() {
  auto *doc = active();
  if (!doc || doc->editor == R_INVALID_ID) {
    setStatus("Open a text file to use Go to Line");
    return;
  }
  if (goToLineWindow_ == R_INVALID_ID || !Rwindow_is_valid(goToLineWindow_))
    goToLineWindow_ = Rwindow_create_from_xml(ui::kGoToLineWindow.data(),
                                              ui::kGoToLineWindow.size());
  if (goToLineWindow_ == R_INVALID_ID) {
    setStatus("Cannot open Go to Line");
    return;
  }
  int line = 0, col = 0;
  Rtext_editor_get_cursor(doc->editor, &line, &col);
  const auto position =
      std::to_string(line + 1) + ":" + std::to_string(col + 1);
  Rtext_input_set_text(widget(goToLineWindow_, "goto_position"),
                       position.c_str());
  Rtext_input_set_committed_callback(widget(goToLineWindow_, "goto_position"),
                                     &EditorApp::goToLineCommitCb, this);
  Rwindow_show(goToLineWindow_);
  Rwidget_focus(widget(goToLineWindow_, "goto_position"));
  Rtext_input_select_all(widget(goToLineWindow_, "goto_position"));
}
void EditorApp::goToLineCommitCb(Rid, const char *, void *user) {
  static_cast<EditorApp *>(user)->goToLine();
}
void EditorApp::goToLine() {
  auto *doc = active();
  if (!doc || doc->editor == R_INVALID_ID)
    return;
  const auto v = trim(textInput(goToLineWindow_, "goto_position"));
  const auto delim = v.find(':');
  auto parsePositive = [](const std::string &text, int &number) -> bool {
    if (text.empty() || text.size() > 9)
      return false;
    for (unsigned char c : text)
      if (!std::isdigit(c))
        return false;
    try {
      number = std::stoi(text);
      return number >= 1;
    } catch (...) {
      return false;
    }
  };
  int line = 0, col = 1;
  if (!parsePositive(v.substr(0, delim), line) ||
      (delim != std::string::npos &&
       !parsePositive(v.substr(delim + 1), col))) {
    setLabel(goToLineWindow_, "goto_status", "Invalid position. Example: 42:8");
    return;
  }
  const int count = Rtext_editor_get_line_count(doc->editor);
  line = std::clamp(line, 1, std::max(count, 1));
  const char *content = Rtext_editor_get_line(doc->editor, line - 1);
  const int length =
      content ? static_cast<int>(std::string(content).size()) : 0;
  col = std::clamp(col - 1, 0, length);
  Rtext_editor_set_cursor(doc->editor, line - 1, col);
  Rtext_editor_scroll_to_line(doc->editor, line - 1);
  Rwidget_focus(doc->editor);
  if (Rwindow_is_valid(goToLineWindow_))
    Rwindow_destroy(goToLineWindow_);
  goToLineWindow_ = R_INVALID_ID;
  setStatus("Go to line " + std::to_string(line) + ", column " +
            std::to_string(col + 1));
}
void EditorApp::openQuickOpen(bool commands) {
  commandPalette_ = commands;
  quickCandidates_.clear();
  quickCommands_.clear();
  if (commands) {
    for (const auto &b : keybindings_.all())
      quickCommands_.push_back(b.command);
  } else {
    std::set<fs::path> unique;
    for (const auto &doc : docs_)
      if (!doc->path.empty())
        unique.insert(doc->path);
    std::error_code ec;
    if (!folder_.empty() && fs::is_directory(folder_, ec)) {
      std::size_t scanned = 0;
      for (auto it = fs::recursive_directory_iterator(
               folder_, fs::directory_options::skip_permission_denied, ec);
           !ec && it != fs::recursive_directory_iterator() && scanned < 20000 &&
           unique.size() < 3000;
           it.increment(ec)) {
        if (it->is_directory(ec)) {
          const auto name = it->path().filename().string();
          if (name == ".git" || name == "build" || name == "node_modules" ||
              name == ".cache" || name == ".venv" || name == ".idea")
            it.disable_recursion_pending();
          continue;
        }
        ++scanned;
        if (it->is_regular_file(ec))
          unique.insert(it->path());
      }
    }
    quickCandidates_.assign(unique.begin(), unique.end());
  }
  if (quickOpenWindow_ == R_INVALID_ID || !Rwindow_is_valid(quickOpenWindow_))
    quickOpenWindow_ = Rwindow_create_from_xml(ui::kQuickOpenWindow.data(),
                                               ui::kQuickOpenWindow.size());
  if (quickOpenWindow_ == R_INVALID_ID) {
    setStatus("Cannot open Quick Open");
    return;
  }
  Rtext_input_set_text(widget(quickOpenWindow_, "quick_query"), "");
  Rtext_input_set_placeholder(widget(quickOpenWindow_, "quick_query"),
                              commands ? "Search commands..."
                                       : "Search file names...");
  Rtext_input_set_changed_callback(widget(quickOpenWindow_, "quick_query"),
                                   &EditorApp::quickFilterCb, this);
  Rtext_input_set_committed_callback(widget(quickOpenWindow_, "quick_query"),
                                     &EditorApp::quickCommitCb, this);
  Rlist_view_set_activated_callback(widget(quickOpenWindow_, "quick_list"),
                                    &EditorApp::quickSelectCb, this);
  refreshQuickOpen();
  Rwindow_show(quickOpenWindow_);
  Rwidget_focus(widget(quickOpenWindow_, "quick_query"));
}
void EditorApp::quickFilterCb(Rid, const char *, void *user) {
  static_cast<EditorApp *>(user)->refreshQuickOpen();
}
void EditorApp::quickCommitCb(Rid, const char *, void *user) {
  static_cast<EditorApp *>(user)->chooseQuickOpen();
}
void EditorApp::quickSelectCb(Rid, int, void *user) {
  static_cast<EditorApp *>(user)->chooseQuickOpen();
}
void EditorApp::refreshQuickOpen() {
  if (quickOpenWindow_ == R_INVALID_ID)
    return;
  const auto query = lower(trim(textInput(quickOpenWindow_, "quick_query")));
  const auto list = widget(quickOpenWindow_, "quick_list");
  Rlist_view_clear_items(list);
  quickMatches_.clear();
  if (commandPalette_) {
    std::size_t count = 0;
    for (const auto &command : quickCommands_) {
      if (lower(command).find(query) == std::string::npos)
        continue;
      std::string display = command;
      const auto key = keybindings_.get(command);
      if (!key.empty())
        display += "    " + key;
      Rlist_view_add_item(list, display.c_str());
      ++count;
      if (count >= 250)
        break;
    }
    setLabel(quickOpenWindow_, "quick_status",
             std::to_string(count) + " commands");
  } else {
    for (const auto &path : quickCandidates_) {
      const auto file = lower(path.filename().string());
      const auto full = lower(path.string());
      if (!query.empty() && file.find(query) == std::string::npos &&
          full.find(query) == std::string::npos)
        continue;
      std::error_code ec;
      const auto relative =
          folder_.empty() ? path : fs::relative(path, folder_, ec);
      Rlist_view_add_item(list,
                          (ec ? path.string() : relative.string()).c_str());
      quickMatches_.push_back(path);
      if (quickMatches_.size() >= 250)
        break;
    }
    setLabel(quickOpenWindow_, "quick_status",
             std::to_string(quickMatches_.size()) + " files (max 250 shown)");
  }
  if (Rlist_view_get_item_count(list) > 0)
    Rlist_view_set_selected(list, 0);
}
void EditorApp::chooseQuickOpen() {
  if (quickOpenWindow_ == R_INVALID_ID)
    return;
  const auto list = widget(quickOpenWindow_, "quick_list");
  const int idx = Rlist_view_get_selected(list);
  if (idx < 0)
    return;
  std::string action;
  fs::path path;
  if (commandPalette_) {
    const char *row = Rlist_view_get_item_text(list, idx);
    if (!row)
      return;
    action = std::string(row);
    auto space = action.find(' ');
    if (space != std::string::npos)
      action.resize(space);
  } else {
    if (idx >= static_cast<int>(quickMatches_.size()))
      return;
    path = quickMatches_[static_cast<size_t>(idx)];
  }
  if (Rwindow_is_valid(quickOpenWindow_))
    Rwindow_destroy(quickOpenWindow_);
  quickOpenWindow_ = R_INVALID_ID;
  if (!action.empty())
    handleAction(action);
  else if (!path.empty())
    opened(path);
}
void EditorApp::keybindingFilterCb(Rid, const char *, void *user) {
  static_cast<EditorApp *>(user)->refreshKeybindingRows();
}
void EditorApp::keybindingSelectCb(Rid, int, void *user) {
  static_cast<EditorApp *>(user)->chooseKeybinding();
}
void EditorApp::refreshKeybindingRows() {
  if (settingsWindow_ == R_INVALID_ID || !Rwindow_is_valid(settingsWindow_))
    return;
  const auto list = widget(settingsWindow_, "kb_command_list");
  if (list == R_INVALID_ID)
    return;
  const auto filter = lower(trim(textInput(settingsWindow_, "kb_filter")));
  keybindingRows_.clear();
  Rlist_view_clear_items(list);
  for (const auto &binding : keybindings_.all()) {
    const auto search = lower(binding.command + " " + binding.key);
    if (!filter.empty() && search.find(filter) == std::string::npos)
      continue;
    std::string name = binding.command;
    if (binding.customized)
      name += "  *";
    name += "    " + (binding.key.empty() ? "(unbound)" : binding.key);
    Rlist_view_add_item(list, name.c_str());
    keybindingRows_.push_back(binding.command);
  }
  if (!keybindingRows_.empty()) {
    Rlist_view_set_selected(list, 0);
    chooseKeybinding();
  }
}
void EditorApp::chooseKeybinding() {
  if (settingsWindow_ == R_INVALID_ID || !Rwindow_is_valid(settingsWindow_))
    return;
  const auto list = widget(settingsWindow_, "kb_command_list");
  const int idx = Rlist_view_get_selected(list);
  if (idx < 0 || idx >= static_cast<int>(keybindingRows_.size()))
    return;
  Rtext_input_set_text(
      widget(settingsWindow_, "kb_shortcut"),
      keybindings_.get(keybindingRows_[static_cast<size_t>(idx)]).c_str());
}
int EditorApp::keybindingEventCb(Rid, const REvent *event, void *user) {
  auto *app = static_cast<EditorApp *>(user);
  if (!app->recordingBinding_ || !event || event->type != R_EVENT_KEY_DOWN)
    return R_FALSE;
  const auto key = event->key;
  if (key == R_KEY_ESCAPE) {
    app->recordingBinding_ = false;
    setLabel(app->settingsWindow_, "kb_status", "Recording cancelled");
    return R_TRUE;
  }
  if (key == R_KEY_LEFT_CTRL || key == R_KEY_RIGHT_CTRL ||
      key == R_KEY_LEFT_SHIFT || key == R_KEY_RIGHT_SHIFT ||
      key == R_KEY_LEFT_ALT || key == R_KEY_RIGHT_ALT ||
      key == R_KEY_LEFT_SUPER || key == R_KEY_RIGHT_SUPER)
    return R_TRUE;
  std::string name;
  if ((key >= R_KEY_A && key <= R_KEY_Z) || (key >= R_KEY_0 && key <= R_KEY_9))
    name = std::string(1, static_cast<char>(key));
  else if (key >= R_KEY_F1 && key <= R_KEY_F12)
    name = "F" + std::to_string(static_cast<int>(key - R_KEY_F1 + 1));
  else {
    static const std::map<RKey, std::string> special = {
        {R_KEY_GRAVE, "Grave"},
        {R_KEY_SLASH, "Slash"},
        {R_KEY_SPACE, "Space"},
        {R_KEY_DELETE, "Delete"},
        {R_KEY_UP, "Up"},
        {R_KEY_DOWN, "Down"},
        {R_KEY_LEFT, "Left"},
        {R_KEY_RIGHT, "Right"},
        {R_KEY_HOME, "Home"},
        {R_KEY_END, "End"},
        {R_KEY_TAB, "Tab"},
        {R_KEY_ENTER, "Enter"},
        {R_KEY_BACKSPACE, "Backspace"},
        {R_KEY_PAGE_UP, "PageUp"},
        {R_KEY_PAGE_DOWN, "PageDown"},
        {R_KEY_INSERT, "Insert"},
        {R_KEY_MINUS, "Minus"},
        {R_KEY_EQUAL, "Equal"},
        {R_KEY_COMMA, "Comma"},
        {R_KEY_PERIOD, "Period"},
        {R_KEY_SEMICOLON, "Semicolon"},
        {R_KEY_BACKSLASH, "Backslash"},
        {R_KEY_LEFT_BRACKET, "LeftBracket"},
        {R_KEY_RIGHT_BRACKET, "RightBracket"}};
    auto found = special.find(key);
    if (found != special.end())
      name = found->second;
  }
  if (name.empty())
    return R_TRUE;
  keys::Shortcut shortcut;
  shortcut.valid = true;
  shortcut.key = name;
  if (event->modifiers & R_MOD_CTRL)
    shortcut.modifiers |= keys::Ctrl;
  if (event->modifiers & R_MOD_SHIFT)
    shortcut.modifiers |= keys::Shift;
  if (event->modifiers & R_MOD_ALT)
    shortcut.modifiers |= keys::Alt;
  if (event->modifiers & R_MOD_SUPER)
    shortcut.modifiers |= keys::Super;
  const auto sequence = keys::formatShortcut(shortcut);
  const auto normalized = keys::normalizeShortcut(sequence);
  if (normalized.empty()) {
    setLabel(
        app->settingsWindow_, "kb_status",
        "Use a modifier with letters/numbers, or a function/navigation key");
    return R_TRUE;
  }
  Rtext_input_set_text(widget(app->settingsWindow_, "kb_shortcut"),
                       normalized.c_str());
  app->recordingBinding_ = false;
  setLabel(app->settingsWindow_, "kb_status",
           "Captured " + normalized + ". Click Apply to save.");
  return R_TRUE;
}
void EditorApp::keybindingAction(const std::string &action) {
  if (action == "keybindings.show") {
    openSettings();
    const auto tabs = widget(settingsWindow_, "settings_sections");
    if (tabs != R_INVALID_ID)
      Rtab_view_set_active(tabs, 5);
    Rwidget_focus(widget(settingsWindow_, "kb_filter"));
    return;
  }
  if (settingsWindow_ == R_INVALID_ID || !Rwindow_is_valid(settingsWindow_))
    return;
  if (action == "keybindings.record") {
    recordingBinding_ = true;
    setLabel(settingsWindow_, "kb_status",
             "Press your desired key combination (Esc to cancel)");
    Rwidget_focus(widget(settingsWindow_, "kb_shortcut"));
    return;
  }
  recordingBinding_ = false;
  const auto list = widget(settingsWindow_, "kb_command_list");
  const int idx = Rlist_view_get_selected(list);
  const auto command =
      idx >= 0 && idx < static_cast<int>(keybindingRows_.size())
          ? keybindingRows_[static_cast<size_t>(idx)]
          : "";
  std::string error;
  if (action == "keybindings.reset_all")
    keybindings_.resetAll();
  else if (command.empty())
    return;
  else if (action == "keybindings.unbind")
    keybindings_.set(command, "", &error);
  else if (action == "keybindings.reset") {
    // A default shortcut may be occupied by a custom binding. Reject the
    // collision.
    const auto defaults = keys::KeybindingManager::defaults();
    const auto it = defaults.find(command);
    if (it == defaults.end())
      keybindings_.reset(command); // plugin command has no built-in key
    else if (!keybindings_.set(command, it->second, &error)) {
      setLabel(settingsWindow_, "kb_status", error);
      return;
    }
  } else if (action == "keybindings.apply") {
    keybindings_.set(command, trim(textInput(settingsWindow_, "kb_shortcut")),
                     &error);
  } else
    return;
  if (!error.empty()) {
    setLabel(settingsWindow_, "kb_status", error);
    return;
  }
  installKeybindings();
  if (!keybindings_.save(&error))
    setLabel(settingsWindow_, "kb_status",
             "Applied for this session; cannot save: " + error);
  else
    setLabel(settingsWindow_, "kb_status",
             "Keybindings applied and saved in keybindings.json");
  refreshKeybindingRows();
}
} // namespace dodev
