// Login is only a configuration dialog; each connection owns its own journal
// page.
bool journalPanel() {
  const std::string owner = "com.dodev.remotejournal";
  Json profile = Json::object();
  auto profiles = sdk::workspaceProfiles(root, owner);
  if (!profiles.empty()) {
    wxArrayString choices;
    choices.Add("Manual connection (global defaults)");
    for (auto& p : profiles) choices.Add(W(p["name"].string()));
    wxSingleChoiceDialog choose(this, "Choose a saved SSH connection",
                                "Journal workspace profiles", choices);
    choose.SetSelection(0);
    if (showModal(choose) != wxID_OK) return false;
    if (choose.GetSelection() > 0)
      profile = profiles.at(choose.GetSelection() - 1);
  }
  auto get = [&](const char* k) {
    auto* v = profile.find(k);
    if (v) {
      if (v->isString()) return v->string();
      if (v->isNumber()) return std::to_string(v->integer());
      if (v->isBool()) return std::string(v->boolean() ? "true" : "false");
    }
    return plugins.pluginSetting(owner, k);
  };
  auto number = [&](const char* k, int fallback) {
    try {
      return std::stoi(get(k));
    } catch (...) {
      return fallback;
    }
  };
  wxDialog d(this, wxID_ANY, "Connect Remote Journal (SSH)", wxDefaultPosition,
             wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto* s = new wxBoxSizer(wxVERTICAL);
  d.SetSizer(s);
  auto* g = new wxFlexGridSizer(2, 6, 8);
  g->AddGrowableCol(1);
  s->Add(g, 0, wxEXPAND | wxALL, 10);
  auto field = [&](const char* name, const wxString& value) {
    g->Add(new wxStaticText(&d, wxID_ANY, name), 0, wxALIGN_CENTER_VERTICAL);
    auto* e = new wxTextCtrl(&d, wxID_ANY, value);
    g->Add(e, 1, wxEXPAND);
    return e;
  };
  auto* host = field("Host / IP", W(get("host")));
  auto* user = field("SSH user", W(get("user")));
  auto* unit = field("Systemd unit (optional)", W(get("unit")));
  g->Add(new wxStaticText(&d, wxID_ANY, "SSH port"), 0,
         wxALIGN_CENTER_VERTICAL);
  auto* port = new wxSpinCtrl(&d, wxID_ANY, "22", wxDefaultPosition,
                              wxDefaultSize, wxSP_ARROW_KEYS, 1, 65535, 22);
  port->SetValue(std::clamp(number("port", 22), 1, 65535));
  g->Add(port, 1, wxEXPAND);
  g->Add(new wxStaticText(&d, wxID_ANY, "Initial lines"), 0,
         wxALIGN_CENTER_VERTICAL);
  auto* lines = new wxSpinCtrl(&d, wxID_ANY, "200", wxDefaultPosition,
                               wxDefaultSize, wxSP_ARROW_KEYS, 1, 10000, 200);
  lines->SetValue(std::clamp(number("lines", 200), 1, 10000));
  g->Add(lines, 1, wxEXPAND);
  g->Add(new wxStaticText(&d, wxID_ANY, "Authentication"), 0,
         wxALIGN_CENTER_VERTICAL);
  auto* auth = new wxChoice(&d, wxID_ANY);
  auth->Append("SSH key / agent");
  auth->Append("SSH password (prompt)");
  auth->SetSelection(get("authentication") == "password" ? 1 : 0);
  g->Add(auth, 1, wxEXPAND);
  auto* sudoMode =
      new wxCheckBox(&d, wxID_ANY, "Use remote passwordless sudo (sudo -n)");
  sudoMode->SetValue(get("sudoNonInteractive") == "true");
  s->Add(sudoMode, 0, wxALL, 5);
  auto* follow = new wxCheckBox(&d, wxID_ANY, "Follow live journal");
  follow->SetValue(get("follow") != "false");
  s->Add(follow, 0, wxALL, 5);
  s->Add(d.CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 8);
  if (auto* ok = d.FindWindow(wxID_OK)) ok->SetLabel("Connect");
  d.SetMinSize(wxSize(460, 300));
  s->Fit(&d);
  journal::Options o;
  std::string err;
  for (;;) {
    if (showModal(d) != wxID_OK) return false;
    o.host = S(host->GetValue());
    o.user = S(user->GetValue());
    o.unit = S(unit->GetValue());
    o.sshPort = port->GetValue();
    o.lines = static_cast<unsigned>(lines->GetValue());
    o.follow = follow->GetValue();
    o.sudoNonInteractive = sudoMode->GetValue();
    o.passwordAuthentication = auth->GetSelection() == 1;
    if (journal::Session::validate(o, &err)) break;
    wxMessageBox(W(err), "Remote Journal", wxOK | wxICON_ERROR, &d);
  }
  std::string password;
  if (o.passwordAuthentication) {
    wxPasswordEntryDialog pw(this,
                             "SSH password for " + W(o.user + "@" + o.host),
                             "Remote Journal authentication");
    if (showModal(pw) != wxID_OK) return false;
    password = S(pw.GetValue());
    pw.SetValue("");
  }
  // Capture placement is fixed by the Journal plugin, not by saved legacy
  // preferences.
  auto* notebook = editors;
  auto* panel = new wxui::JournalPanel(notebook, root, {});
  panel->initialFilters(
      get("filterMode") == "all" ? "" : get("filterText"), get("payload"),
      get("service"), get("fromUtc"), get("toUtc"), number("priority", -1),
      get("caseSensitive") == "true", get("autoScroll") != "false",
      get("filterMode") == "regex");
  // A profile may refer to a reusable filter set relative to its workspace.
  const auto filterFile = get("filterSetFile");
  if (!filterFile.empty()) {
    auto path = std::filesystem::u8path(filterFile);
    if (path.is_relative()) path = root / path;
    panel->loadRuleFile(
        path);  // Report invalid files without replacing existing filters.
  } else {
    auto path = wxui::journalDefaultFilters(root);
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) panel->loadRuleFile(path);
  }
  if (!panel->start(o, std::move(password), &err)) {
    panel->Destroy();
    wxMessageBox(W(err), "Remote Journal", wxOK | wxICON_ERROR, this);
    return false;
  }
  panel->setClose([this, weak = wxWeakRef<wxui::JournalPanel>(panel)] {
    CallAfter([this, weak] {
      if (auto* p = weak.get()) {
        for (auto* n : {editors, bottom, side}) {
          auto i = n->GetPageIndex(p);
          if (i != wxNOT_FOUND) {
            p->shutdown();
            n->DeletePage(i);
            break;
          }
        }
      }
    });
  });
  panel->setOpenFilters([this, weak = wxWeakRef<wxui::JournalPanel>(panel)] {
    if (auto* capture = weak.get()) {
      if (capture->workspaceDirectory() != root) {
        wxMessageBox("Open the capture's workspace to edit its filters.",
                     "Journal filters", wxOK | wxICON_INFORMATION, this);
        return;
      }
      pluginView("remotejournal.filters");
      if (auto* filters = dynamic_cast<wxui::JournalWorkspaceFilters*>(
              wxWindow::FindWindowByName("journal.workspace.filters",
                                         side->GetCurrentPage())))
        filters->selectCapture(capture);
    }
  });
  notebook->AddPage(panel,
                    W("Journal: " + o.user + "@" + o.host +
                      (o.unit.empty() ? "" : " / " + o.unit)),
                    true);
  palette.apply(panel);
  panel->configurePresentation(get("applicationId"),
                               get("payloadRegex") == "true",
                               get("tableForeground"), get("tableBackground"));
  aui.Update();
  panel->focusTable();
  return true;
}
