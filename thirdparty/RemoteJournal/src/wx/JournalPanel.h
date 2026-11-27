#pragma once
#include <wx/checklst.h>
#include <wx/choicdlg.h>
#include <wx/clipbrd.h>
#include <wx/filedlg.h>
#include <wx/listctrl.h>
#include <wx/textdlg.h>
#include <wx/timer.h>
#include <wx/weakref.h>
#include <wx/wx.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>

#include "JournalExport.h"
#include "JournalFilterState.h"
#include "JournalRuleEditor.h"
#include "JournalTable.h"

namespace wxui {
class JournalList final : public wxListCtrl {
  const dodev::journal::Table& model_;
  mutable std::map<std::pair<std::string, std::string>, wxListItemAttr>
      markerAttrs_;

 public:
  JournalList(wxWindow* parent, const dodev::journal::Table& model)
      : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                   wxLC_REPORT | wxLC_VIRTUAL),
        model_(model) {
    const char* names[] = {
        "Timestamp (UTC)", "Priority", "Host",    "Unit",
        "Process",         "PID",      "Message", "Application ID"};
    const int widths[] = {220, 105, 125, 175, 145, 65, 650, 160};
    for (int i = 0; i < 8; ++i)
      InsertColumn(i, names[i], wxLIST_FORMAT_LEFT, widths[i]);
  }
  void resetMarkerAttributes() { markerAttrs_.clear(); }
  wxListItemAttr* OnGetItemAttr(long item) const override {
    if (item < 0) return nullptr;
    auto* rule = model_.marker(static_cast<std::size_t>(item));
    if (!rule) return nullptr;
    auto key = std::make_pair(rule->foreground, rule->background);
    auto it = markerAttrs_.find(key);
    if (it != markerAttrs_.end()) return &it->second;
    wxListItemAttr attr;
    if (!key.first.empty()) attr.SetTextColour(wxColour(journalWx(key.first)));
    if (!key.second.empty())
      attr.SetBackgroundColour(wxColour(journalWx(key.second)));
    return &markerAttrs_.emplace(std::move(key), attr).first->second;
  }
  wxString OnGetItemText(long item, long column) const override {
    if (item < 0 || static_cast<std::size_t>(item) >= model_.visible().size())
      return {};
    auto text = model_.at(static_cast<std::size_t>(item))
                    .column(static_cast<int>(column));
    // Keep one physical row per event; the details pane and copy retain
    // newlines.
    std::string single;
    for (char c : text) {
      if (c == '\n')
        single += "\\n";
      else if (c == '\r')
        single += "\\r";
      else if (c == '\0')
        single += "\\0";
      else
        single += c;
    }
    return journalWx(single);
  }
};
class JournalPanel final : public wxPanel {
  dodev::journal::Session session_;
  dodev::journal::Table model_;
  wxTimer timer_;
  JournalList* table_ = nullptr;
  wxTextCtrl* details_ = nullptr;
  wxCheckBox *follow_ = nullptr, *paused_ = nullptr;
  wxStaticText* status_ = nullptr;
  wxButton* stop_ = nullptr;
  std::filesystem::path directory_;
  std::function<void()> close_;
  bool rebuilding_ = false;
  wxString target_;
  std::string foreground_, background_;
  JournalFilterState filters_;
  std::function<void()> openFilters_;
  std::set<std::size_t> selectedIds() const {
    std::set<std::size_t> ids;
    for (long i =
             table_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
         i != -1;
         i = table_->GetNextItem(i, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED))
      if (static_cast<std::size_t>(i) < model_.visible().size())
        ids.insert(model_.at(i).id);
    return ids;
  }
  void detail() {
    if (rebuilding_) return;
    auto i = table_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    details_->ChangeValue(i >= 0 && static_cast<std::size_t>(i) <
                                        model_.visible().size()
                              ? journalWx(model_.at(i).text())
                              : wxString{});
  }
  void refreshRows(const std::set<std::size_t>& ids, bool scroll) {
    rebuilding_ = true;
    table_->Freeze();
    // Clear old selection indices BEFORE changing item count.
    for (long i =
             table_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
         i != -1;) {
      const auto next =
          table_->GetNextItem(i, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
      table_->SetItemState(i, 0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
      i = next;
    }
    const auto count = static_cast<long>(model_.visible().size());
    table_->SetItemCount(count);
    for (long i = 0; i < count && !ids.empty(); ++i)
      if (ids.count(model_.at(i).id))
        table_->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    if (count > 0) {
      table_->RefreshItems(0, count - 1);
      if (scroll && ids.empty() && follow_->GetValue())
        table_->EnsureVisible(count - 1);
    }
    table_->Thaw();
    table_->Refresh();
    rebuilding_ = false;
    detail();
  }
  void updateStatus(const dodev::journal::RowBatch& batch) {
    status_->SetLabel(
        target_ + " | " + journalWx(batch.status) + " | " +
        wxString::Format("visible %lu / received %lu / evicted %lu",
                         static_cast<unsigned long>(model_.visible().size()),
                         static_cast<unsigned long>(batch.received),
                         static_cast<unsigned long>(batch.dropped)) +
        (paused_->GetValue() ? " | display paused" : ""));
    stop_->Enable(batch.running);
  }
  void poll() {
    auto b = session_.rowsAfter(model_.cursor(), !paused_->GetValue());
    if (!paused_->GetValue() && b.received != model_.cursor()) {
      auto ids = selectedIds();
      model_.ingest(b);
      refreshRows(ids, true);
    }
    updateStatus(b);
  }
  void clipboard(const std::string& text) {
    if (text.empty()) return;
    if (wxTheClipboard->Open()) {
      wxTheClipboard->SetData(new wxTextDataObject(journalWx(text)));
      wxTheClipboard->Close();
    } else
      wxLogWarning("Cannot open clipboard");
  }
  wxButton* button(wxSizer* s, const wxString& label,
                   std::function<void()> callback) {
    auto* b = new wxButton(this, wxID_ANY, label);
    s->Add(b, 0, wxALL, 2);
    b->Bind(wxEVT_BUTTON, [callback](wxCommandEvent&) {
      try {
        callback();
      } catch (const std::exception& e) {
        wxLogError("%s", journalWx(e.what()));
      }
    });
    return b;
  }

 public:
  JournalPanel(wxWindow* parent, std::filesystem::path directory,
               std::function<void()> close)
      : wxPanel(parent),
        timer_(this),
        directory_(std::move(directory)),
        close_(std::move(close)) {
    auto* s = new wxBoxSizer(wxVERTICAL);
    SetSizer(s);
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    s->Add(actions, 0, wxEXPAND);
    stop_ = button(actions, "Disconnect", [this] { stop(); });
    button(actions, "Copy selected", [this] { copySelected(); });
    button(actions, "Copy visible",
           [this] { clipboard(model_.exportText(true)); });
    button(actions, "Export...", [this] { exportLogs(); });
    button(actions, "Clear", [this] { clear(); });
    button(actions, "Close tab", [this] { requestClose(); });
    button(actions, "Workspace filters", [this] {
      if (openFilters_) openFilters_();
    });
    auto* flags = new wxBoxSizer(wxHORIZONTAL);
    s->Add(flags, 0, wxEXPAND | wxLEFT | wxRIGHT, 3);
    follow_ = new wxCheckBox(this, wxID_ANY, "Auto-scroll");
    follow_->SetValue(true);
    flags->Add(follow_, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
    paused_ = new wxCheckBox(this, wxID_ANY, "Pause view");
    flags->Add(paused_, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
    status_ = new wxStaticText(this, wxID_ANY, "Connecting...");
    s->Add(status_, 0, wxLEFT | wxRIGHT | wxTOP, 4);
    table_ = new JournalList(this, model_);
    s->Add(table_, 1, wxEXPAND | wxALL, 3);
    details_ = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                              wxSize(-1, 55), wxTE_MULTILINE | wxTE_READONLY);
    s->Add(details_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 3);
    paused_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { poll(); });
    table_->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent&) { detail(); });
    table_->Bind(wxEVT_LIST_ITEM_DESELECTED,
                 [this](wxListEvent&) { detail(); });
    table_->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& e) {
      if (e.CmdDown() && (e.GetKeyCode() == 'C' || e.GetKeyCode() == 'c'))
        copySelected();
      else if (e.CmdDown() && (e.GetKeyCode() == 'A' || e.GetKeyCode() == 'a'))
        selectAllRows();
      else
        e.Skip();
    });
    table_->Bind(wxEVT_CONTEXT_MENU, [this](wxContextMenuEvent&) {
      wxMenu menu;
      enum { Copy = wxID_HIGHEST + 710, Message, Raw, Visible };
      menu.Append(Copy, "Copy selected log lines");
      menu.Append(Message, "Copy selected messages");
      menu.Append(Raw, "Copy selected raw JSON / SSH output");
      menu.Append(Visible, "Copy visible logs");
      auto choice = table_->GetPopupMenuSelectionFromUser(menu);
      if (choice == Copy)
        copySelected();
      else if (choice == Message || choice == Raw) {
        std::string out;
        auto ids = selectedIds();
        for (auto i : model_.visible()) {
          const auto& r = model_.rows()[i];
          if (ids.count(r.id))
            out += (choice == Message ? r.message : r.raw) + '\n';
        }
        clipboard(out);
      } else if (choice == Visible)
        clipboard(model_.exportText(true));
    });
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { poll(); }, timer_.GetId());
  }
  const JournalFilterState& filterState() const { return filters_; }
  const std::filesystem::path& workspaceDirectory() const { return directory_; }
  wxString captureTitle() const { return target_; }
  void setOpenFilters(std::function<void()> fn) {
    openFilters_ = std::move(fn);
  }
  bool setFilters(const JournalFilterState& next, std::string& error) {
    dodev::journal::TableFilter compiled;
    if (!next.compile(compiled, error)) return false;
    auto ids = selectedIds();
    filters_ = next;
    model_.filter(std::move(compiled));
    model_.rules(
        std::make_shared<const dodev::journal::RuleSet>(filters_.rules));
    table_->resetMarkerAttributes();
    refreshRows(ids, false);
    updateStatus(session_.rowsAfter(model_.cursor(), false));
    return true;
  }
  bool loadRuleFile(const std::filesystem::path& path) {
    auto next = filters_;
    std::string error;
    if (!readJournalRules(path, next.rules, error) ||
        !setFilters(next, error)) {
      wxMessageBox(journalWx(error), "Journal filters", wxOK | wxICON_ERROR,
                   this);
      return false;
    }
    return true;
  }
  ~JournalPanel() override {
    timer_.Stop();
    session_.stop();
    DestroyChildren();
  }
  void initialFilters(const std::string& text, const std::string& payload,
                      const std::string& service, const std::string& from,
                      const std::string& to, int priority, bool sensitive,
                      bool scroll, bool regex) {
    auto next = filters_;
    next.text = text;
    next.payload = payload;
    next.service = service;
    next.from = from;
    next.to = to;
    next.priority = priority >= -1 && priority <= 8 ? priority : -1;
    next.caseSensitive = sensitive;
    next.regex = regex;
    follow_->SetValue(scroll);
    std::string error;
    if (!setFilters(next, error)) wxLogWarning("%s", journalWx(error));
  }
  void configurePresentation(const std::string& app, bool regex,
                             const std::string& fg, const std::string& bg) {
    auto next = filters_;
    next.application = app;
    next.payloadRegex = regex;
    std::string error;
    if (!setFilters(next, error)) wxLogWarning("%s", journalWx(error));
    foreground_ = fg;
    background_ = bg;
    applyCustomColors();
  }
  void applyCustomColors() {
    const auto& fg = foreground_;
    const auto& bg = background_;
    wxColour foreground(journalWx(fg)), background(journalWx(bg));
    if (!fg.empty() && foreground.IsOk()) {
      table_->SetTextColour(foreground);
      details_->SetForegroundColour(foreground);
    }
    if (!bg.empty() && background.IsOk()) {
      table_->SetBackgroundColour(background);
      details_->SetBackgroundColour(background);
    }
    table_->Refresh();
    details_->Refresh();
  }
  bool start(dodev::journal::Options o, std::string password,
             std::string* error) {
    o.structured = true;
    target_ = journalWx(o.user + "@" + o.host +
                        (o.unit.empty() ? "" : " / " + o.unit));
    const bool ok = o.passwordAuthentication
                        ? session_.startPassword(o, std::move(password), error)
                        : session_.start(o, error);
    if (ok) {
      timer_.Start(250);
      poll();
    }
    return ok;
  }
  void stop() {
    session_.stop();
    poll();
  }
  void shutdown() {
    timer_.Stop();
    session_.stop();
  }
  void setClose(std::function<void()> f) { close_ = std::move(f); }
  void requestClose() {
    auto f = close_;
    if (f) f();
  }
  void copySelected() {
    std::string out;
    auto ids = selectedIds();
    for (auto i : model_.visible()) {
      const auto& r = model_.rows()[i];
      if (ids.count(r.id)) out += r.text() + '\n';
    }
    clipboard(out);
  }
  void clear() {
    session_.clear();
    model_.clear();
    refreshRows({}, false);
    poll();
  }
  bool tableHasFocus() const { return wxWindow::FindFocus() == table_; }
  void selectAllRows() {
    rebuilding_ = true;
    for (long i = 0; i < table_->GetItemCount(); ++i)
      table_->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    rebuilding_ = false;
    detail();
  }
  void focusTable() { table_->SetFocus(); }
  void exportLogs() {
    wxArrayString choices;
    choices.Add("Visible (filtered) rows");
    choices.Add("All buffered rows");
    wxSingleChoiceDialog scope(this, "Choose log export scope",
                               "Export journal", choices);
    if (scope.ShowModal() != wxID_OK) return;
    const bool sqlite = dodev::journal::sqliteExportAvailable();
    wxString formats = "Text log (*.txt)|*.txt|CSV table (*.csv)|*.csv";
    if (sqlite) formats += "|SQLite database (*.sqlite;*.db)|*.sqlite;*.db";
    wxFileDialog d(this, "Export journal", journalWx(directory_.u8string()),
                   "journal.txt", formats, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (d.ShowModal() != wxID_OK) return;
    // Visible means the displayed snapshot; all includes records received while
    // paused.
    poll();
    bool filtered = scope.GetSelection() == 0;
    dodev::journal::Table live;
    const auto* source = &model_;
    if (!filtered && paused_->GetValue()) {
      live.ingest(session_.rowsAfter(0));
      source = &live;
    }
    auto path = std::filesystem::u8path(journalUtf8(d.GetPath()));
    if (d.GetFilterIndex() == 2) {
      std::string error;
      if (!dodev::journal::exportSqlite(*source, filtered, path, error))
        wxMessageBox(journalWx(error), "SQLite export", wxOK | wxICON_ERROR,
                     this);
      return;
    }
    auto data = d.GetFilterIndex() == 1 ? source->exportCsv(filtered)
                                        : source->exportText(filtered);
    std::ofstream out(path, std::ios::binary);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.flush();
    if (!out) wxLogError("Cannot write journal export");
  }
};
}  // namespace wxui
