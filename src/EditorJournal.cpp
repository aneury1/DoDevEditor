#include "EditorApp.h"
#include "EmbeddedJournalUi.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

namespace dodev {
namespace {
Rid w(Rid parent, const char *name) {
  return parent == R_INVALID_ID ? R_INVALID_ID : Rwidget_find(parent, name);
}
std::string text(Rid parent, const char *id) {
  const auto widget = w(parent, id);
  const char *value =
      widget == R_INVALID_ID ? nullptr : Rtext_input_get_text(widget);
  return value ? value : "";
}
void set(Rid parent, const char *id, const std::string &value) {
  const auto widget = w(parent, id);
  if (widget != R_INVALID_ID)
    Rtext_input_set_text(widget, value.c_str());
}
void label(Rid parent, const char *id, const std::string &value) {
  const auto widget = w(parent, id);
  if (widget != R_INVALID_ID)
    Rlabel_set_text(widget, value.c_str());
}
void button(Rid box, const char *label, const char *action) {
  auto widget = Rbutton_create(label);
  Rwidget_set_class(widget, "chip");
  Rwidget_set_action(widget, action);
  Rwidget_add_child(box, widget);
}
bool checked(Rid parent, const char *id) {
  const auto widget = w(parent, id);
  return widget != R_INVALID_ID && Rcheckbox_is_checked(widget) != R_FALSE;
}
void check(Rid parent, const char *id, bool yes) {
  const auto widget = w(parent, id);
  if (widget != R_INVALID_ID)
    Rcheckbox_set_checked(widget, yes ? R_TRUE : R_FALSE);
}
bool yes(const std::string &value) { return value == "true" || value == "1"; }
std::string cfg(const PluginManager &plugins, const char *key,
                const char *fallback) {
  const auto val = plugins.pluginSetting("com.dodev.remotejournal", key);
  return val.empty() ? fallback : val;
}
std::string modeName(int ix) {
  return ix == 1 ? "fixed" : ix == 2 ? "regex" : "all";
}
} // namespace
bool EditorApp::openRemoteJournal() {
  if (journalDialog_ != R_INVALID_ID && Rwindow_is_valid(journalDialog_)) {
    Rwindow_show(journalDialog_);
    return true;
  }
  journalDialog_ = Rwindow_create_from_xml(ui::kRemoteJournalWindow.data(),
                                           ui::kRemoteJournalWindow.size());
  if (journalDialog_ == R_INVALID_ID)
    return false;
  set(journalDialog_, "journal_host", cfg(pluginManager_, "host", ""));
  set(journalDialog_, "journal_user", cfg(pluginManager_, "user", ""));
  set(journalDialog_, "journal_port", cfg(pluginManager_, "port", "22"));
  set(journalDialog_, "journal_unit", cfg(pluginManager_, "unit", ""));
  set(journalDialog_, "journal_lines", cfg(pluginManager_, "lines", "200"));
  set(journalDialog_, "journal_filter_text",
      cfg(pluginManager_, "filterText", ""));
  check(journalDialog_, "journal_follow",
        yes(cfg(pluginManager_, "follow", "true")));
  check(journalDialog_, "journal_sudo",
        yes(cfg(pluginManager_, "sudoNonInteractive", "false")));
  const auto combo = w(journalDialog_, "journal_filter_mode");
  for (const char *option : {"All", "Fixed text", "Regex"})
    Rcombo_box_add_item(combo, option);
  const auto current = cfg(pluginManager_, "filterMode", "all");
  Rcombo_box_set_selected(combo, current == "fixed"   ? 1
                                 : current == "regex" ? 2
                                                      : 0);
  label(journalDialog_, "journal_status", journalCapture_.snapshot().status);
  Rwindow_show(journalDialog_);
  std::clog << "[DoDevEditor][Journal] Configuration dialog opened\n";
  return true;
}
void EditorApp::openJournalTasks() {
  if (journalTaskPage_ == R_INVALID_ID || !Rwidget_is_valid(journalTaskPage_)) {
    journalTaskPage_ = Rwidget_create(R_WIDGET_VBOX);
    Rwidget_set_class(journalTaskPage_, "music-surface surface");
    auto top = Rwidget_create(R_WIDGET_HBOX);
    Rwidget_add_child(journalTaskPage_, top);
    Rwidget_add_child(top, Rlabel_create("TASKS · REMOTE JOURNAL"));
    journalTaskStatus_ = Rlabel_create("Idle");
    Rwidget_add_child(top, journalTaskStatus_);
    button(top, "Configure...", "journal.open_dialog");
    button(top, "Disconnect", "journal.disconnect");
    button(top, "Clear", "journal.clear");
    button(top, "Export TXT...", "journal.export");
    auto filterRow = Rwidget_create(R_WIDGET_HBOX);
    Rwidget_add_child(journalTaskPage_, filterRow);
    journalTaskMode_ = Rwidget_create(R_WIDGET_COMBO_BOX);
    for (const char *option : {"All", "Fixed text", "Regex"})
      Rcombo_box_add_item(journalTaskMode_, option);
    const auto current = cfg(pluginManager_, "filterMode", "all");
    Rcombo_box_set_selected(journalTaskMode_, current == "fixed"   ? 1
                                              : current == "regex" ? 2
                                                                   : 0);
    Rwidget_add_child(filterRow, journalTaskMode_);
    journalTaskFilter_ = Rwidget_create(R_WIDGET_TEXT_INPUT);
    Rtext_input_set_text(journalTaskFilter_,
                         cfg(pluginManager_, "filterText", "").c_str());
    Rtext_input_set_committed_callback(journalTaskFilter_,
                                       &EditorApp::journalFilterCommitCb, this);
    Rwidget_add_child(filterRow, journalTaskFilter_);
    Rslot_set_fill(journalTaskFilter_, 1);
    button(filterRow, "Apply", "journal.task_filter");
    button(filterRow, "Matches / All", "journal.toggle_matches");
    journalTaskLog_ = Rtext_editor_create("");
    Rtext_editor_set_read_only(journalTaskLog_, R_TRUE);
    Rwidget_set_class(journalTaskLog_, "music-editor");
    Rwidget_add_child(journalTaskPage_, journalTaskLog_);
    Rslot_set_fill(journalTaskLog_, 1);
    Rtab_view_add_tab(terminal_tabs_, "Tasks · Journal", journalTaskPage_);
  }
  Rdock_area_open_panel(dock_, output_pane_);
  Rtab_view_show_tab(terminal_tabs_, journalTaskPage_);
  journalLastText_.clear();
  refreshJournalTasks();
}
void EditorApp::refreshJournalTasks() {
  if (journalTaskPage_ == R_INVALID_ID || !Rwidget_is_valid(journalTaskPage_))
    return;
  static auto last = std::chrono::steady_clock::time_point{};
  const auto now = std::chrono::steady_clock::now();
  if (now - last < std::chrono::milliseconds(200))
    return;
  last = now;
  const auto s = journalCapture_.snapshot(journalOnlyMatches_, 2500);
  if (journalTaskLog_ != R_INVALID_ID && Rwidget_is_valid(journalTaskLog_) &&
      s.text != journalLastText_) {
    Rtext_editor_set_text(journalTaskLog_, s.text.c_str());
    Rtext_editor_scroll_to_line(
        journalTaskLog_, std::max(0, static_cast<int>(std::count(
                                         s.text.begin(), s.text.end(), '\n')) -
                                         1));
    journalLastText_ = s.text;
  }
  const auto status = s.status + " | lines=" + std::to_string(s.received) +
                      " dropped=" + std::to_string(s.dropped);
  if (journalTaskStatus_ != R_INVALID_ID &&
      Rwidget_is_valid(journalTaskStatus_))
    Rlabel_set_text(journalTaskStatus_, status.c_str());
  if (journalDialog_ != R_INVALID_ID && Rwindow_is_valid(journalDialog_))
    label(journalDialog_, "journal_status", status);
  if (status != journalLastStatus_) {
    journalLastStatus_ = status;
    std::clog << "[DoDevEditor][Journal] " << status << "\n";
  }
}
void EditorApp::journalFilterCommitCb(Rid, const char *, void *ptr) {
  static_cast<EditorApp *>(ptr)->journalAction("journal.task_filter");
}
void EditorApp::journalAction(const std::string &action) {
  if (action == "journal.open_dialog") {
    openRemoteJournal();
    return;
  }
  if (action == "journal.open_tasks") {
    openJournalTasks();
    return;
  }
  if (action == "journal.close_dialog") {
    if (journalDialog_ != R_INVALID_ID && Rwindow_is_valid(journalDialog_))
      Rwindow_destroy(journalDialog_);
    journalDialog_ = R_INVALID_ID;
    return;
  }
  if (action == "journal.disconnect") {
    journalCapture_.stop();
    setStatus("Remote Journal disconnected");
    journalLastText_.clear();
    refreshJournalTasks();
    return;
  }
  if (action == "journal.clear") {
    journalCapture_.clear();
    journalLastText_.clear();
    if (journalTaskLog_ != R_INVALID_ID && Rwidget_is_valid(journalTaskLog_))
      Rtext_editor_set_text(journalTaskLog_, "");
    setStatus("Remote Journal output cleared");
    return;
  }
  if (action == "journal.toggle_matches") {
    journalOnlyMatches_ = !journalOnlyMatches_;
    journalLastText_.clear();
    refreshJournalTasks();
    return;
  }
  if (action == "journal.apply_filter" || action == "journal.task_filter") {
    const bool task = action == "journal.task_filter";
    const auto parent = task ? journalTaskPage_ : journalDialog_;
    const int mode =
        task ? Rcombo_box_get_selected(journalTaskMode_)
             : Rcombo_box_get_selected(w(parent, "journal_filter_mode"));
    const std::string pattern = task ? "" : text(parent, "journal_filter_text");
    // The task-side input is dynamically constructed, not named in XML.
    const char *taskValue = task && journalTaskFilter_ != R_INVALID_ID
                                ? Rtext_input_get_text(journalTaskFilter_)
                                : nullptr;
    const std::string expression =
        task ? (taskValue ? taskValue : "") : pattern;
    std::string error;
    if (!journalCapture_.setFilter(mode == 2   ? journal::Filter::Regex
                                   : mode == 1 ? journal::Filter::Fixed
                                               : journal::Filter::All,
                                   expression, false, &error)) {
      setStatus("Invalid journal filter: " + error);
      return;
    }
    pluginManager_.updatePluginSetting("com.dodev.remotejournal", "filterMode",
                                       modeName(mode));
    pluginManager_.updatePluginSetting("com.dodev.remotejournal", "filterText",
                                       expression);
    if (journalTaskFilter_ != R_INVALID_ID &&
        Rwidget_is_valid(journalTaskFilter_))
      Rtext_input_set_text(journalTaskFilter_, expression.c_str());
    if (journalTaskMode_ != R_INVALID_ID && Rwidget_is_valid(journalTaskMode_))
      Rcombo_box_set_selected(journalTaskMode_, mode);
    if (journalDialog_ != R_INVALID_ID && Rwindow_is_valid(journalDialog_)) {
      set(journalDialog_, "journal_filter_text", expression);
      Rcombo_box_set_selected(w(journalDialog_, "journal_filter_mode"), mode);
    }
    journalLastText_.clear();
    refreshJournalTasks();
    return;
  }
  if (action == "journal.export") {
    RFileDialogDesc desc{};
    desc.parent =
        journalDialog_ != R_INVALID_ID && Rwindow_is_valid(journalDialog_)
            ? journalDialog_
            : window_;
    desc.title = "Export remote journal TXT";
    const std::string fileName = "remote_journal.txt";
    desc.file_name = fileName.c_str();
    const char *choice = Rfile_dialog_save(&desc);
    if (!choice || !*choice)
      return;
    auto path = fs::u8path(choice);
    if (path.extension().empty())
      path += ".txt";
    const auto data = journalCapture_.exportText(journalOnlyMatches_);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
      setStatus("Cannot create remote journal TXT");
      return;
    }
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.flush();
    if (!out) {
      setStatus("Journal TXT export failed");
      return;
    }
    setStatus("Remote journal exported to: " + path.string());
    return;
  }
  if (action == "journal.connect") {
    if (journalDialog_ == R_INVALID_ID || !Rwindow_is_valid(journalDialog_))
      return;
    journalAction("journal.apply_filter");
    journal::Options options;
    options.host = text(journalDialog_, "journal_host");
    options.user = text(journalDialog_, "journal_user");
    options.unit = text(journalDialog_, "journal_unit");
    options.follow = checked(journalDialog_, "journal_follow");
    options.sudoNonInteractive = checked(journalDialog_, "journal_sudo");
    try {
      options.sshPort = std::stoi(text(journalDialog_, "journal_port"));
      options.lines = static_cast<unsigned int>(
          std::stoul(text(journalDialog_, "journal_lines")));
    } catch (...) {
      setStatus("Invalid SSH port or lines number");
      return;
    }
    std::string error;
    if (!journalCapture_.start(options, &error)) {
      setStatus(error);
      label(journalDialog_, "journal_status", error);
      return;
    }
    for (const auto &kv : std::vector<std::pair<std::string, std::string>>{
             {"host", options.host},
             {"user", options.user},
             {"port", std::to_string(options.sshPort)},
             {"unit", options.unit},
             {"lines", std::to_string(options.lines)},
             {"follow", options.follow ? "true" : "false"},
             {"sudoNonInteractive",
              options.sudoNonInteractive ? "true" : "false"}})
      pluginManager_.updatePluginSetting("com.dodev.remotejournal", kv.first,
                                         kv.second);
    openJournalTasks();
    setStatus("Remote journal connecting: " + options.user + "@" +
              options.host);
    std::clog << "[DoDevEditor][Journal] SSH " << options.user << "@"
              << options.host << ":" << options.sshPort
              << " (unit=" << options.unit << ")\n";
  }
}
} // namespace dodev
