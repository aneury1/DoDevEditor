// Tool frontends use the inherited backends; no Raven widget API is required.
std::vector<ai::ProviderProfile> profiles() {
  auto p = ai::defaultProfiles();
  auto j = plugins.pluginSettingJson("com.dodev.aiassistant", "profiles");
  std::string err;
  std::vector<ai::ProviderProfile> saved;
  if (ai::profilesFromJson(j, saved, &err) && !saved.empty()) return saved;
  return p;
}
template <class Fn>
auto networkJob(const wxString& title, Fn fn)
    -> decltype(fn(static_cast<std::atomic<bool>*>(nullptr))) {
  std::atomic<bool> cancel{false};
  auto future = std::async(std::launch::async, [&] { return fn(&cancel); });
  wxProgressDialog progress(title, "Waiting for provider...", 100, this,
                            wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_AUTO_HIDE);
  while (future.wait_for(std::chrono::milliseconds(30)) !=
         std::future_status::ready) {
    if (!progress.Pulse()) cancel = true;
  }
  progress.Update(100);
  return future.get();
}
void aiProfiles() {
  auto values = profiles();
  wxDialog d(this, wxID_ANY, "AI Provider Profiles", wxDefaultPosition,
             wxSize(700, 580), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto s = new wxBoxSizer(wxVERTICAL);
  d.SetSizer(s);
  auto choice = new wxChoice(&d, wxID_ANY);
  s->Add(choice, 0, wxEXPAND | wxALL, 6);
  auto row = new wxBoxSizer(wxHORIZONTAL);
  s->Add(row, 0, wxEXPAND);
  auto grid = new wxFlexGridSizer(2, 7, 7);
  grid->AddGrowableCol(1);
  s->Add(grid, 0, wxEXPAND | wxALL, 10);
  auto field = [&](const char* title) {
    grid->Add(new wxStaticText(&d, wxID_ANY, title), 0,
              wxALIGN_CENTER_VERTICAL);
    auto e = new wxTextCtrl(&d, wxID_ANY);
    grid->Add(e, 1, wxEXPAND);
    return e;
  };
  auto name = field("Name"),
       kind = field("Provider (llama.cpp / ollama / remote)"),
       endpoint = field("Chat endpoint"),
       models = field("Models endpoint override"), model = field("Model"),
       auth = field("Auth mode (none / bearer / auto)"),
       key = field("API key environment variable"),
       timeout = field("Timeout seconds"),
       context = field("Maximum context bytes");
  auto refresh = [&](int i) {
    choice->Clear();
    for (auto& p : values) choice->Append(W(p.name));
    choice->SetSelection(i);
    auto& p = values.at(i);
    name->SetValue(W(p.name));
    kind->SetValue(W(p.kind));
    endpoint->SetValue(W(p.config.endpoint));
    models->SetValue(W(p.modelsEndpoint));
    model->SetValue(W(p.config.model));
    auth->SetValue(W(p.config.authMode));
    key->SetValue(W(p.config.keyEnvironment));
    timeout->SetValue(W(std::to_string(p.config.timeoutSeconds)));
    context->SetValue(W(std::to_string(p.config.maxContextBytes)));
  };
  int selected = 0;
  auto activeId =
      plugins.pluginSetting("com.dodev.aiassistant", "activeProfileId");
  for (size_t i = 0; i < values.size(); ++i)
    if (values[i].id == activeId) selected = i;
  auto collect = [&] {
    auto p = values.at(selected);
    p.name = S(name->GetValue());
    p.kind = S(kind->GetValue());
    p.config.endpoint = S(endpoint->GetValue());
    p.modelsEndpoint = S(models->GetValue());
    p.config.model = S(model->GetValue());
    p.config.authMode = S(auth->GetValue());
    p.config.keyEnvironment = S(key->GetValue());
    p.config.timeoutSeconds = std::stoi(S(timeout->GetValue()));
    p.config.maxContextBytes = std::stoull(S(context->GetValue()));
    std::string err;
    if (!ai::validateProfile(p, &err)) throw std::runtime_error(err);
    return p;
  };
  choice->Bind(wxEVT_CHOICE, [&](wxCommandEvent& e) {
    selected = e.GetSelection();
    refresh(selected);
  });
  button(&d, row, "Add local", [&] {
    auto p = ai::defaultProfiles().front();
    p.id = ai::nextProfileId(values, "local");
    p.name = "Local provider";
    values.push_back(p);
    selected = values.size() - 1;
    refresh(selected);
  });
  button(&d, row, "Add remote", [&] {
    auto p = ai::defaultProfiles().back();
    p.id = ai::nextProfileId(values, "remote");
    p.name = "Remote provider";
    p.kind = "remote";
    p.config.endpoint = "https://api.openai.com/v1/chat/completions";
    p.config.authMode = "bearer";
    p.config.keyEnvironment = "OPENAI_API_KEY";
    values.push_back(p);
    selected = values.size() - 1;
    refresh(selected);
  });
  button(&d, row, "Delete", [&] {
    if (values.size() > 1) {
      values.erase(values.begin() + selected);
      selected = 0;
      refresh(0);
    }
  });
  button(&d, s, "Refresh models", [&] {
    auto p = collect();
    auto r = networkJob("Fetch models", [&](auto* c) {
      return ai::listModels(p.config, p.modelsEndpoint, c);
    });
    if (!r.success) throw std::runtime_error(r.error);
    wxArrayString items;
    for (auto& m : r.models) items.Add(W(m));
    wxSingleChoiceDialog c(&d, "Select model (or cancel and type manually)",
                           "Models", items);
    if (showModal(c) == wxID_OK) model->SetValue(c.GetStringSelection());
  });
  button(&d, s, "Save selected profile", [&] {
    values.at(selected) = collect();
    if (!plugins.updatePluginSettingJson("com.dodev.aiassistant", "profiles",
                                         ai::profilesToJson(values)) ||
        !plugins.updatePluginSetting("com.dodev.aiassistant", "activeProfileId",
                                     values[selected].id))
      throw std::runtime_error("Could not save profiles");
    refresh(selected);
  });
  s->Add(d.CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 8);
  refresh(selected);
  palette.apply(&d);
  if (showModal(d) == wxID_OK) {
    values.at(selected) = collect();
    if (!plugins.updatePluginSettingJson("com.dodev.aiassistant", "profiles",
                                         ai::profilesToJson(values)) ||
        !plugins.updatePluginSetting("com.dodev.aiassistant", "activeProfileId",
                                     values[selected].id))
      throw std::runtime_error("Could not save profiles");
  }
}
bool aiPanel() {
  wxDialog d(this, wxID_ANY, "AI Assistant", wxDefaultPosition,
             wxSize(760, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto s = new wxBoxSizer(wxVERTICAL);
  d.SetSizer(s);
  auto choice = new wxChoice(&d, wxID_ANY);
  s->Add(choice, 0, wxEXPAND | wxALL, 5);
  auto values = profiles();
  auto reload = [&] {
    values = profiles();
    choice->Clear();
    int i = 0;
    auto selected =
        plugins.pluginSetting("com.dodev.aiassistant", "activeProfileId");
    for (size_t j = 0; j < values.size(); ++j) {
      choice->Append(W(values[j].name + " — " + values[j].config.model));
      if (values[j].id == selected) i = j;
    }
    choice->SetSelection(i);
  };
  reload();
  button(&d, s, "Provider profiles", [&] {
    aiProfiles();
    reload();
  });
  auto output = new wxTextCtrl(&d, wxID_ANY, "", wxDefaultPosition,
                               wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  s->Add(output, 1, wxEXPAND | wxALL, 5);
  auto q = new wxTextCtrl(&d, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 90),
                          wxTE_MULTILINE);
  s->Add(q, 0, wxEXPAND | wxALL, 5);
  auto include = new wxCheckBox(
      &d, wxID_ANY,
      "Include active file (sends its contents to the selected provider)");
  s->Add(include, 0, wxALL, 5);
  std::vector<ai::Message> history;
  std::string last;
  button(&d, s, "Send", [&] {
    auto question = S(q->GetValue());
    if (question.empty()) return;
    std::string doc;
    if (include->GetValue()) {
      if (wxMessageBox("Send active file contents to this provider?", "AI",
                       wxYES_NO | wxICON_WARNING, &d) != wxYES)
        return;
      if (auto e = active()) doc = S(e->GetText());
    }
    auto cfg = values.at(choice->GetSelection()).config;
    auto r = networkJob("AI request", [&](auto* c) {
      return ai::complete(cfg, history, question, doc, c);
    });
    if (!r.success) throw std::runtime_error(r.error);
    history.push_back({"user", question});
    history.push_back({"assistant", r.answer});
    last = r.answer;
    output->AppendText(
        W("You: " + question + "\n\nAssistant: " + r.answer + "\n\n"));
    q->Clear();
  });
  button(&d, s, "Copy last answer", [&] {
    if (wxTheClipboard->Open()) {
      wxTheClipboard->SetData(new wxTextDataObject(W(last)));
      wxTheClipboard->Close();
    }
  });
  button(&d, s, "Clear conversation", [&] {
    history.clear();
    last.clear();
    output->Clear();
  });
  palette.apply(&d);
  showModal(d);
  return true;
}
bool serialPanel() {
  serial::CaptureSession session;
  wxDialog d(this, wxID_ANY, "Serial Monitor", wxDefaultPosition,
             wxSize(800, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto s = new wxBoxSizer(wxVERTICAL);
  d.SetSizer(s);
  auto row = new wxBoxSizer(wxHORIZONTAL);
  s->Add(row, 0, wxEXPAND | wxALL, 5);
  auto ports = new wxComboBox(&d, wxID_ANY);
  row->Add(ports, 1, wxEXPAND);
  auto baud = new wxSpinCtrl(&d, wxID_ANY);
  baud->SetRange(50, 4000000);
  baud->SetValue(115200);
  row->Add(baud, 0);
  auto scan = [&] {
    ports->Clear();
    for (auto& p : serial::CaptureSession::availablePorts())
      ports->Append(W(p));
    if (ports->GetCount()) ports->SetSelection(0);
  };
  scan();
  button(&d, row, "Refresh", scan);
  button(&d, row, "Start", [&] {
    serial::PortOptions o;
    o.port = S(ports->GetValue());
    o.baud = baud->GetValue();
    std::string err;
    if (!session.start(o, &err)) throw std::runtime_error(err);
  });
  button(&d, row, "Stop", [&] { session.stop(); });
  auto filter = new wxTextCtrl(&d, wxID_ANY);
  s->Add(filter, 0, wxEXPAND | wxALL, 5);
  filter->Bind(wxEVT_TEXT, [&](wxCommandEvent&) {
    session.setFilter(serial::FilterMode::Fixed, S(filter->GetValue()), false);
  });
  auto status = new wxStaticText(&d, wxID_ANY, "Idle");
  s->Add(status, 0, wxALL, 4);
  auto out = new wxTextCtrl(&d, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                            wxTE_MULTILINE | wxTE_READONLY);
  s->Add(out, 1, wxEXPAND | wxALL, 4);
  auto send = new wxTextCtrl(&d, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                             wxTE_PROCESS_ENTER);
  s->Add(send, 0, wxEXPAND | wxALL, 4);
  send->Bind(wxEVT_TEXT_ENTER, [&](wxCommandEvent&) {
    std::string err;
    if (!session.sendBytes(S(send->GetValue()) + "\r\n", &err))
      wxMessageBox(W(err));
    else
      send->Clear();
  });
  button(&d, s, "Clear", [&] { session.clear(); });
  button(&d, s, "Export", [&] {
    wxFileDialog f(&d, "Save serial log", P(root), "serial.txt", "Text|*.txt",
                   wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (showModal(f) == wxID_OK)
      writeText(F(f.GetPath()), session.exportText());
  });
  wxTimer poll(&d);
  std::string last;
  d.Bind(
      wxEVT_TIMER,
      [&](wxTimerEvent&) {
        auto v = session.snapshot();
        status->SetLabel(W(v.status));
        if (v.output != last) {
          last = v.output;
          out->ChangeValue(W(last));
          out->SetInsertionPointEnd();
        }
      },
      poll.GetId());
  poll.Start(150);
  palette.apply(&d);
  showModal(d);
  poll.Stop();
  session.stop();
  return true;
}
bool journalPanel() {
  journal::Session session;
  wxDialog d(this, wxID_ANY, "Remote Journal (SSH)", wxDefaultPosition,
             wxSize(800, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto s = new wxBoxSizer(wxVERTICAL);
  d.SetSizer(s);
  auto g = new wxFlexGridSizer(2, 5, 5);
  g->AddGrowableCol(1);
  s->Add(g, 0, wxEXPAND | wxALL, 8);
  auto field = [&](const char* label, const wxString& v) {
    g->Add(new wxStaticText(&d, wxID_ANY, label));
    auto e = new wxTextCtrl(&d, wxID_ANY, v);
    g->Add(e, 1, wxEXPAND);
    return e;
  };
  auto host = field("Host", ""), user = field("User", ""),
       port = field("SSH port", "22"), unit = field("Systemd unit", ""),
       lines = field("Lines", "200");
  auto auth = new wxChoice(&d, wxID_ANY);
  auth->Append("SSH key / agent");
  auth->Append("SSH password (prompt)");
  auth->SetSelection(0);
  g->Add(new wxStaticText(&d, wxID_ANY, "Authentication"));
  g->Add(auth, 1, wxEXPAND);
  auto sudoMode =
      new wxCheckBox(&d, wxID_ANY, "Use sudo -n (remote passwordless sudo)");
  s->Add(sudoMode, 0, wxALL, 5);
  auto follow = new wxCheckBox(&d, wxID_ANY, "Follow");
  follow->SetValue(true);
  s->Add(follow, 0, wxALL, 5);
  auto row = new wxBoxSizer(wxHORIZONTAL);
  s->Add(row, 0, wxEXPAND);
  button(&d, row, "Start", [&] {
    journal::Options o;
    o.host = S(host->GetValue());
    o.user = S(user->GetValue());
    o.sshPort = std::stoi(S(port->GetValue()));
    o.unit = S(unit->GetValue());
    o.lines = std::stoul(S(lines->GetValue()));
    o.follow = follow->GetValue();
    o.sudoNonInteractive = sudoMode->GetValue();
    std::string err;
    if (!journal::Session::validate(o, &err)) throw std::runtime_error(err);
    if (auth->GetSelection() == 1) {
      wxPasswordEntryDialog pw(&d,
                               "SSH password for " + W(o.user + "@" + o.host),
                               "Remote Journal authentication");
      if (showModal(pw) != wxID_OK) return;
      auto password = S(pw.GetValue());
      pw.SetValue("");
      if (!session.startPassword(o, std::move(password), &err))
        throw std::runtime_error(err);
    } else if (!session.start(o, &err))
      throw std::runtime_error(err);
  });
  button(&d, row, "Stop", [&] { session.stop(); });
  button(&d, row, "Clear", [&] { session.clear(); });
  button(&d, row, "Export", [&] {
    wxFileDialog f(&d, "Save journal", P(root), "journal.txt", "Text|*.txt",
                   wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (showModal(f) == wxID_OK)
      writeText(F(f.GetPath()), session.exportText());
  });
  auto filter = new wxTextCtrl(&d, wxID_ANY);
  s->Add(filter, 0, wxEXPAND | wxALL, 5);
  filter->Bind(wxEVT_TEXT, [&](wxCommandEvent&) {
    session.setFilter(journal::Filter::Fixed, S(filter->GetValue()), false);
  });
  auto status = new wxStaticText(&d, wxID_ANY, "Idle");
  s->Add(status, 0, wxALL, 4);
  auto out = new wxTextCtrl(&d, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                            wxTE_MULTILINE | wxTE_READONLY);
  s->Add(out, 1, wxEXPAND | wxALL, 4);
  wxTimer poll(&d);
  std::string last;
  d.Bind(
      wxEVT_TIMER,
      [&](wxTimerEvent&) {
        auto v = session.snapshot();
        status->SetLabel(W(v.status));
        if (v.text != last) {
          last = v.text;
          out->ChangeValue(W(last));
          out->SetInsertionPointEnd();
        }
      },
      poll.GetId());
  poll.Start(150);
  palette.apply(&d);
  showModal(d);
  poll.Stop();
  session.stop();
  return true;
}
