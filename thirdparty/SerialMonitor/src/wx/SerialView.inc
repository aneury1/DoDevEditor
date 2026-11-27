// Plugin-owned workspace profiles and persistent serial captures. One page owns
// one session.
bool serialPanel() {
  const std::string owner = "com.dodev.serialmonitor";
  auto get = [&](const char* k) { return plugins.pluginSetting(owner, k); };
  auto number = [&](const char* k, int fallback) {
    try {
      return std::stoi(get(k));
    } catch (...) {
      return fallback;
    }
  };
  auto profiles = sdk::workspaceProfiles(root, owner);
  std::vector<Json> selected;
  if (!profiles.empty()) {
    wxArrayString names;
    names.Add("Manual port");
    for (auto& p : profiles)
      names.Add(W(p["name"].string() + " - " + p["port"].string()));
    wxMultiChoiceDialog d(this, "Select one or more ports to capture",
                          "Serial workspace profiles", names);
    if (showModal(d) != wxID_OK) return false;
    for (int i : d.GetSelections())
      if (i > 0)
        selected.push_back(profiles.at(i - 1));
      else
        selected.push_back(Json::object());
  } else
    selected.push_back(Json::object());
  const bool privileged = get("captureAsAdministrator") == "true",
             fallback = get("allowPolkit") == "true";
  if ((privileged || fallback) && !selected.empty() &&
      wxMessageBox("Allow Polkit authorization for these serial captures?",
                   "Serial Monitor", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
                   this) != wxYES)
    return false;
  for (auto& profile : selected) {
    serial::PortOptions o;
    o.port = profile["port"].string(get("port"));
    o.baud = profile["baud"].integer(number("baud", 115200));
    o.dataBits = profile["dataBits"].integer(number("dataBits", 8));
    o.stopBits = profile["stopBits"].integer(number("stopBits", 1));
    auto parity = profile["parity"].string(get("parity"));
    o.parity = parity.empty() ? 'N' : parity.front();
    if (!profile.find("name")) {
      wxDialog d(this, wxID_ANY, "Serial connection", wxDefaultPosition,
                 wxSize(480, 240), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
      auto* s = new wxBoxSizer(wxVERTICAL);
      d.SetSizer(s);
      s->Add(new wxStaticText(&d, wxID_ANY, "Serial device"), 0, wxALL, 5);
      auto* ports = new wxComboBox(&d, wxID_ANY, W(o.port));
      for (auto& p : serial::CaptureSession::availablePorts())
        ports->Append(W(p));
      if (o.port.empty() && ports->GetCount()) ports->SetSelection(0);
      s->Add(ports, 0, wxEXPAND | wxALL, 5);
      auto* baud = new wxSpinCtrl(&d, wxID_ANY);
      baud->SetRange(50, 4000000);
      baud->SetValue(std::clamp(o.baud, 50, 4000000));
      s->Add(new wxStaticText(&d, wxID_ANY, "Baud"), 0, wxALL, 5);
      s->Add(baud, 0, wxALL, 5);
      s->Add(d.CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL,
             5);
      if (showModal(d) != wxID_OK) continue;
      o.port = S(ports->GetValue());
      o.baud = baud->GetValue();
    }
    o.allowPolkit = fallback;
    o.captureAsAdministrator = privileged;
    o.privilegedHelper = F(wxStandardPaths::Get().GetExecutablePath())
                             .parent_path()
                             .append("dodev-serial-reader-helper")
                             .u8string();
    auto name = W("dodev.plugin.view:serialmonitor.capture:" + o.port);
    bool exists = false;
    for (size_t i = 0; i < editors->GetPageCount(); ++i)
      if (editors->GetPage(i)->GetName() == name) {
        editors->SetSelection(i);
        exists = true;
        break;
      }
    if (exists) continue;
    auto* panel = new wxui::SerialCapturePanel(editors, root);
    panel->SetName(name);
    std::string error;
    auto mode = profile["filterMode"].string(get("filterMode"));
    auto text = profile["filterText"].string(get("filterText"));
    bool sensitive =
        profile["caseSensitive"].boolean(get("caseSensitive") == "true");
    if (!panel->start(o, mode, text, sensitive,
                      get("showOnlyMatches") != "false", error)) {
      panel->Destroy();
      wxMessageBox(W(error), "Serial Monitor", wxOK | wxICON_ERROR, this);
      continue;
    }
    editors->AddPage(panel, W("Serial: " + o.port), true);
    palette.apply(panel);
  }
  return true;
}
