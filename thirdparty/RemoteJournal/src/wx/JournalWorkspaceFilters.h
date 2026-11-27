#pragma once
#include <wx/scrolwin.h>

#include <algorithm>

#include "JournalPanel.h"
namespace wxui {
// Workspace owns the controls; each capture owns its filter state and SSH
// session. Weak references prevent callbacks from accessing a closed editor
// tab.
class JournalWorkspaceFilters final : public wxPanel {
  using Targets = std::vector<JournalPanel*>;
  std::function<Targets()> targets_;
  std::filesystem::path directory_;
  wxTimer timer_;
  std::vector<wxWeakRef<JournalPanel>> sessions_;
  wxWeakRef<JournalPanel> selected_;
  JournalFilterState defaults_, draft_;
  wxChoice *target_ = nullptr, *priority_ = nullptr;
  wxTextCtrl *text_ = nullptr, *payload_ = nullptr, *service_ = nullptr,
             *app_ = nullptr, *from_ = nullptr, *to_ = nullptr;
  wxCheckBox *enabled_ = nullptr, *regex_ = nullptr, *payloadRegex_ = nullptr,
             *case_ = nullptr;
  wxCheckListBox* list_ = nullptr;
  wxStaticText* status_ = nullptr;
  wxWindow* quick_ = nullptr;
  bool syncing_ = false;
  void status(const std::string& value) {
    status_->SetLabel(journalWx(value));
    status_->Wrap(245);
    Layout();
  }
  wxButton* button(wxSizer* s, const char* title, std::function<void()> fn) {
    auto* b = new wxButton(this, wxID_ANY, title);
    s->Add(b, 0, wxALL, 1);
    b->Bind(wxEVT_BUTTON, [this, fn](wxCommandEvent&) {
      try {
        fn();
      } catch (const std::exception& e) {
        status(e.what());
      }
    });
    return b;
  }
  void syncRules(int selected = wxNOT_FOUND) {
    list_->Clear();
    for (const auto& r : draft_.rules.rules) {
      auto i = list_->Append(journalWx(r.spec.name + " [" + r.spec.type + "]"));
      list_->Check(i, r.spec.enabled);
    }
    if (selected >= 0 &&
        static_cast<size_t>(selected) < draft_.rules.rules.size())
      list_->SetSelection(selected);
    enabled_->SetValue(draft_.rules.enabled);
  }
  void sync() {
    syncing_ = true;
    draft_ = selected_.get() ? selected_.get()->filterState() : defaults_;
    text_->ChangeValue(journalWx(draft_.text));
    payload_->ChangeValue(journalWx(draft_.payload));
    service_->ChangeValue(journalWx(draft_.service));
    app_->ChangeValue(journalWx(draft_.application));
    from_->ChangeValue(journalWx(draft_.from));
    to_->ChangeValue(journalWx(draft_.to));
    priority_->SetSelection(draft_.priority + 1);
    case_->SetValue(draft_.caseSensitive);
    regex_->SetValue(draft_.regex);
    payloadRegex_->SetValue(draft_.payloadRegex);
    quick_->Enable(selected_.get() != nullptr);
    syncRules();
    syncing_ = false;
    status(
        selected_.get()
            ? "Editing selected capture"
            : "Workspace defaults for new captures. Save defaults to persist.");
  }
  void refreshTargets() {
    auto next = targets_();
    bool same = next.size() == sessions_.size();
    if (same)
      for (size_t i = 0; i < next.size(); ++i)
        if (next[i] != sessions_[i].get()) {
          same = false;
          break;
        }
    if (same) return;
    auto* prior = selected_.get();
    sessions_.clear();
    target_->Clear();
    target_->Append("Workspace default rules");
    int index = 0;
    for (auto* p : next) {
      sessions_.emplace_back(p);
      target_->Append(p->captureTitle());
      if (p == prior) index = static_cast<int>(sessions_.size());
    }
    target_->SetSelection(index);
    selected_ =
        wxWeakRef<JournalPanel>(index ? sessions_[index - 1].get() : nullptr);
    sync();
  }
  void apply() {
    if (syncing_) return;
    if (auto* capture = selected_.get()) {
      auto next = draft_;
      next.text = journalUtf8(text_->GetValue());
      next.payload = journalUtf8(payload_->GetValue());
      next.service = journalUtf8(service_->GetValue());
      next.application = journalUtf8(app_->GetValue());
      next.from = journalUtf8(from_->GetValue());
      next.to = journalUtf8(to_->GetValue());
      next.priority = priority_->GetSelection() - 1;
      next.caseSensitive = case_->GetValue();
      next.regex = regex_->GetValue();
      next.payloadRegex = payloadRegex_->GetValue();
      std::string error;
      if (!capture->setFilters(next, error)) {
        status(error + " (previous filters remain active)");
        return;
      }
      draft_ = std::move(next);
      status("Filters applied to selected capture");
    } else if (target_->GetSelection() == 0) {
      defaults_.rules = draft_.rules;
      status("Workspace defaults changed; Save defaults to persist");
    } else
      refreshTargets();
  }
  void edit(bool existing, bool copy = false) {
    int i = list_->GetSelection();
    if ((existing || copy) &&
        (i < 0 || static_cast<size_t>(i) >= draft_.rules.rules.size()))
      return;
    dodev::journal::RuleSpec spec;
    if (existing || copy) spec = draft_.rules.rules[i].spec;
    if (copy) spec.name += " (copy)";
    if (!editJournalRule(this, spec)) return;
    dodev::journal::Rule rule;
    std::string error;
    if (!dodev::journal::compileRule(spec, rule, error)) {
      status(error);
      return;
    }
    if (existing)
      draft_.rules.rules[i] = std::move(rule);
    else {
      draft_.rules.rules.push_back(std::move(rule));
      i = static_cast<int>(draft_.rules.rules.size() - 1);
    }
    syncRules(i);
    apply();
  }
  void move(int delta) {
    int i = list_->GetSelection(), j = i + delta;
    if (i < 0 || j < 0 || static_cast<size_t>(j) >= draft_.rules.rules.size())
      return;
    std::swap(draft_.rules.rules[i], draft_.rules.rules[j]);
    syncRules(j);
    apply();
  }
  void save(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary);
    f << draft_.rules.encode();
    f.flush();
    if (!f) throw std::runtime_error("Cannot write filter set");
    status("Filter set saved");
  }
  void file(bool write) {
    wxFileDialog d(this, write ? "Save rules" : "Load rules",
                   journalWx(directory_.u8string()), "journal-filters.json",
                   "Journal filters (*.json)|*.json",
                   write ? wxFD_SAVE | wxFD_OVERWRITE_PROMPT
                         : wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (d.ShowModal() != wxID_OK) return;
    auto path = std::filesystem::u8path(journalUtf8(d.GetPath()));
    if (write) {
      save(path);
      return;
    }
    dodev::journal::RuleSet next;
    std::string error;
    if (!readJournalRules(path, next, error)) {
      status(error);
      return;
    }
    draft_.rules = std::move(next);
    syncRules();
    apply();
  }

 public:
  JournalWorkspaceFilters(wxWindow* parent, std::filesystem::path directory,
                          std::function<Targets()> targets)
      : wxPanel(parent),
        targets_(std::move(targets)),
        directory_(std::move(directory)),
        timer_(this) {
    auto* s = new wxBoxSizer(wxVERTICAL);
    SetSizer(s);
    s->Add(new wxStaticText(this, wxID_ANY, "Filter target"), 0, wxALL, 3);
    target_ = new wxChoice(this, wxID_ANY);
    target_->Append("Workspace default rules");
    target_->SetSelection(0);
    s->Add(target_, 0, wxEXPAND | wxALL, 3);
    target_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
      int i = target_->GetSelection();
      selected_ = wxWeakRef<JournalPanel>(i > 0 && static_cast<size_t>(i) <=
                                                       sessions_.size()
                                              ? sessions_[i - 1].get()
                                              : nullptr);
      sync();
    });
    enabled_ = new wxCheckBox(this, wxID_ANY, "Enable filter rules");
    s->Add(enabled_, 0, wxALL, 3);
    list_ =
        new wxCheckListBox(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 180));
    s->Add(list_, 0, wxEXPAND | wxALL, 3);
    auto row = [&] {
      auto* r = new wxBoxSizer(wxHORIZONTAL);
      s->Add(r, 0, wxEXPAND);
      return r;
    };
    auto* r = row();
    button(r, "Add", [this] { edit(false); });
    button(r, "Edit", [this] { edit(true); });
    button(r, "Copy", [this] { edit(false, true); });
    r = row();
    button(r, "Up", [this] { move(-1); });
    button(r, "Down", [this] { move(1); });
    button(r, "Delete", [this] {
      int i = list_->GetSelection();
      if (i >= 0 && static_cast<size_t>(i) < draft_.rules.rules.size()) {
        draft_.rules.rules.erase(draft_.rules.rules.begin() + i);
        syncRules(i);
        apply();
      }
    });
    r = row();
    button(r, "Load...", [this] { file(false); });
    button(r, "Save...", [this] { file(true); });
    button(s, "Save as workspace defaults", [this] {
      save(journalDefaultFilters(directory_));
      defaults_.rules = draft_.rules;
    });
    enabled_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
      draft_.rules.enabled = enabled_->GetValue();
      apply();
    });
    list_->Bind(wxEVT_CHECKLISTBOX, [this](wxCommandEvent& e) {
      auto i = e.GetInt();
      if (i >= 0 && static_cast<size_t>(i) < draft_.rules.rules.size()) {
        draft_.rules.rules[i].spec.enabled = list_->IsChecked(i);
        apply();
      }
    });
    list_->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent&) { edit(true); });
    auto* q = new wxPanel(this);
    quick_ = q;
    auto* qs = new wxBoxSizer(wxVERTICAL);
    q->SetSizer(qs);
    s->Add(q, 0, wxEXPAND | wxALL, 3);
    qs->Add(new wxStaticText(q, wxID_ANY, "Quick filters (selected capture)"),
            0, wxALL, 2);
    auto field = [&](const char* label) {
      qs->Add(new wxStaticText(q, wxID_ANY, label), 0, wxTOP, 3);
      auto* t = new wxTextCtrl(q, wxID_ANY);
      qs->Add(t, 0, wxEXPAND);
      t->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { apply(); });
      return t;
    };
    text_ = field("Search all columns");
    payload_ = field("Payload");
    service_ = field("Service");
    app_ = field("Application ID");
    from_ = field("From (UTC)");
    to_ = field("To (UTC)");
    from_->SetHint("YYYY-MM-DD HH:MM:SS");
    to_->SetHint("YYYY-MM-DD HH:MM:SS");
    priority_ = new wxChoice(q, wxID_ANY);
    for (auto n : {"All priorities", "Emergency", "Alert", "Critical", "Error",
                   "Warning", "Notice", "Info", "Debug", "Unknown / SSH"})
      priority_->Append(n);
    qs->Add(priority_, 0, wxEXPAND | wxTOP, 3);
    priority_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { apply(); });
    auto flag = [&](const char* label) {
      auto* c = new wxCheckBox(q, wxID_ANY, label);
      qs->Add(c, 0, wxALL, 2);
      c->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { apply(); });
      return c;
    };
    case_ = flag("Case sensitive");
    regex_ = flag("Regex search");
    payloadRegex_ = flag("Payload regex");
    auto* reset = new wxButton(q, wxID_ANY, "Reset quick filters");
    qs->Add(reset, 0, wxALL, 2);
    reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
      for (auto* f : {text_, payload_, service_, app_, from_, to_})
        f->ChangeValue("");
      priority_->SetSelection(0);
      case_->SetValue(false);
      regex_->SetValue(false);
      payloadRegex_->SetValue(false);
      apply();
    });
    status_ = new wxStaticText(this, wxID_ANY, "");
    s->Add(status_, 0, wxEXPAND | wxALL, 3);
    auto path = journalDefaultFilters(directory_);
    std::string error;
    std::error_code ec;
    if (std::filesystem::exists(path, ec))
      readJournalRules(path, defaults_.rules, error);
    refreshTargets();
    sync();
    if (!error.empty()) status(error);
    Bind(
        wxEVT_TIMER, [this](wxTimerEvent&) { refreshTargets(); },
        timer_.GetId());
    timer_.Start(400);
  }
  ~JournalWorkspaceFilters() override { timer_.Stop(); }
  void selectCapture(JournalPanel* capture) {
    refreshTargets();
    for (size_t i = 0; i < sessions_.size(); ++i)
      if (sessions_[i].get() == capture) {
        selected_ = wxWeakRef<JournalPanel>(capture);
        target_->SetSelection(static_cast<int>(i + 1));
        sync();
        return;
      }
  }
};
}  // namespace wxui
