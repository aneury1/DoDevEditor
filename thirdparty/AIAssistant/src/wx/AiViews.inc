std::vector<ai::ProviderProfile> profiles() {
  auto p = ai::defaultProfiles();
  auto j = plugins.pluginSettingJson("com.dodev.aiassistant", "profiles");
  std::string err;
  std::vector<ai::ProviderProfile> saved;
  if (ai::profilesFromJson(j, saved, &err) && !saved.empty()) return saved;
  return p;
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
