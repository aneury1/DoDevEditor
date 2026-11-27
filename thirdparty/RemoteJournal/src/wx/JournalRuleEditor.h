#pragma once
#include <wx/wx.h>

#include "JournalRules.h"
namespace wxui {
inline wxString journalWx(const std::string& s) {
  auto out = wxString::FromUTF8(s.data(), s.size());
  if (out.empty() && !s.empty())
    out = wxString::From8BitData(s.data(), s.size());
  return out;
}
inline std::string journalUtf8(const wxString& s) {
  auto b = s.ToUTF8();
  return b.data() ? std::string(b.data(), b.length()) : std::string();
}

// This dialog belongs to RemoteJournal and does not use editor internals.
inline bool editJournalRule(wxWindow* parent, dodev::journal::RuleSpec& spec) {
  wxDialog d(parent, wxID_ANY, "Journal filter configuration",
             wxDefaultPosition, wxDefaultSize,
             wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto* layout = new wxBoxSizer(wxVERTICAL);
  d.SetSizer(layout);
  auto* grid = new wxFlexGridSizer(2, 5, 8);
  grid->AddGrowableCol(1);
  layout->Add(grid, 1, wxALL | wxEXPAND, 10);
  auto field = [&](const char* label, const std::string& value) {
    grid->Add(new wxStaticText(&d, wxID_ANY, label), 0,
              wxALIGN_CENTER_VERTICAL);
    auto* c = new wxTextCtrl(&d, wxID_ANY, journalWx(value));
    grid->Add(c, 1, wxEXPAND);
    return c;
  };
  auto* name = field("Name", spec.name);
  grid->Add(new wxStaticText(&d, wxID_ANY, "Type"), 0, wxALIGN_CENTER_VERTICAL);
  auto* type = new wxChoice(&d, wxID_ANY);
  for (auto t : {"Positive", "Negative", "Marker"}) type->Append(t);
  type->SetSelection(spec.type == "negative" ? 1
                     : spec.type == "marker" ? 2
                                             : 0);
  grid->Add(type, 1, wxEXPAND);
  auto* payload = field("Payload / regex", spec.payload);
  auto* app = field("Application ID", spec.application);
  auto* service = field("Service", spec.service);
  auto* host = field("Host", spec.host);
  auto* process = field("Process", spec.process);
  auto* pid = field("PID", spec.pid);
  auto* from = field("From (UTC)", spec.from);
  auto* to = field("To (UTC)", spec.to);
  from->SetHint("YYYY-MM-DD HH:MM:SS");
  to->SetHint("YYYY-MM-DD HH:MM:SS");
  auto priority = [&](const char* label, int value) {
    grid->Add(new wxStaticText(&d, wxID_ANY, label), 0,
              wxALIGN_CENTER_VERTICAL);
    auto* c = new wxChoice(&d, wxID_ANY);
    for (auto t : {"Any", "0 Emergency", "1 Alert", "2 Critical", "3 Error",
                   "4 Warning", "5 Notice", "6 Info", "7 Debug"})
      c->Append(t);
    c->SetSelection(value + 1);
    grid->Add(c, 1, wxEXPAND);
    return c;
  };
  auto* min = priority("Min priority number", spec.minPriority);
  auto* max = priority("Max priority number", spec.maxPriority);
  auto* fg = field("Marker foreground (#RRGGBB)", spec.foreground);
  auto* bg = field("Marker background (#RRGGBB)", spec.background);
  auto* re = new wxCheckBox(&d, wxID_ANY, "Payload regular expression");
  re->SetValue(spec.regex);
  layout->Add(re, 0, wxALL, 4);
  auto* cs = new wxCheckBox(&d, wxID_ANY, "Case sensitive");
  cs->SetValue(spec.caseSensitive);
  layout->Add(cs, 0, wxALL, 4);
  auto* en = new wxCheckBox(&d, wxID_ANY, "Enabled");
  en->SetValue(spec.enabled);
  layout->Add(en, 0, wxALL, 4);
  layout->Add(d.CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0,
              wxEXPAND | wxALL, 8);
  d.SetMinSize(wxSize(550, -1));
  layout->Fit(&d);
  while (d.ShowModal() == wxID_OK) {
    auto v = spec;
    v.name = journalUtf8(name->GetValue());
    v.type = type->GetSelection() == 1   ? "negative"
             : type->GetSelection() == 2 ? "marker"
                                         : "positive";
    v.payload = journalUtf8(payload->GetValue());
    v.application = journalUtf8(app->GetValue());
    v.service = journalUtf8(service->GetValue());
    v.host = journalUtf8(host->GetValue());
    v.process = journalUtf8(process->GetValue());
    v.pid = journalUtf8(pid->GetValue());
    v.from = journalUtf8(from->GetValue());
    v.to = journalUtf8(to->GetValue());
    v.foreground = journalUtf8(fg->GetValue());
    v.background = journalUtf8(bg->GetValue());
    v.minPriority = min->GetSelection() - 1;
    v.maxPriority = max->GetSelection() - 1;
    v.regex = re->GetValue();
    v.caseSensitive = cs->GetValue();
    v.enabled = en->GetValue();
    dodev::journal::Rule test;
    std::string error;
    if (dodev::journal::compileRule(v, test, error)) {
      spec = std::move(v);
      return true;
    }
    wxMessageBox(journalWx(error), "Invalid filter", wxOK | wxICON_ERROR, &d);
  }
  return false;
}
}  // namespace wxui
