// Generic renderer: the plugin supplies all fields, labels, defaults and
// actions.
void addPluginSettingsPages(wxNotebook* tabs,
                            std::vector<std::function<void()>>& store,
                            std::vector<std::function<void()>>& reset) {
  struct Field {
    PluginSetting spec;
    std::function<std::string()> read;
    std::function<void(const std::string&)> write;
  };
  for (const auto& rec : plugins.records()) {
    const auto owner = rec.manifest.id;
    auto fields = std::make_shared<std::vector<Field>>();
    std::vector<PluginSetting> specs;
    for (const auto& sp : plugins.settingSpecs())
      if (sp.owner == owner) specs.push_back(sp);
    std::vector<PluginSettingsAction> actions;
    for (const auto& a : plugins.settingsActions())
      if (a.owner == owner) actions.push_back(a);
    if (specs.empty() && actions.empty()) continue;
    auto* p = new wxScrolledWindow(tabs, wxID_ANY);
    p->SetScrollRate(0, 12);
    auto* s = new wxBoxSizer(wxVERTICAL);
    p->SetSizer(s);
    tabs->AddPage(p, W(rec.manifest.name));
    s->Add(new wxStaticText(p, wxID_ANY, W(owner)), 0, wxALL, 8);
    for (const auto& sp : specs) {
      auto* label = new wxStaticText(p, wxID_ANY, W(sp.label));
      s->Add(label, 0, wxLEFT | wxRIGHT | wxTOP, 8);
      Field f;
      f.spec = sp;
      if (sp.type == "bool") {
        auto* c = new wxCheckBox(p, wxID_ANY, "Enabled");
        s->Add(c, 0, wxALL, 6);
        f.read = [c] { return std::string(c->GetValue() ? "true" : "false"); };
        f.write = [c](const std::string& v) { c->SetValue(v == "true"); };
      } else if (sp.type == "int") {
        auto* c = new wxSpinCtrl(p, wxID_ANY);
        c->SetRange(sp.minimum, sp.maximum);
        s->Add(c, 0, wxALL, 6);
        f.read = [c] { return std::to_string(c->GetValue()); };
        f.write = [c, sp](const std::string& v) {
          c->SetValue(std::stoi(sp.accepts(v) ? v : sp.defaultValue));
        };
      } else if (sp.type == "choice") {
        auto* c = new wxChoice(p, wxID_ANY);
        for (const auto& item : sp.choices) c->Append(W(item.second));
        s->Add(c, 0, wxEXPAND | wxALL, 6);
        f.read = [c, sp] {
          auto i = c->GetSelection();
          return i == wxNOT_FOUND ? sp.defaultValue : sp.choices.at(i).first;
        };
        f.write = [c, sp](const std::string& v) {
          auto value = sp.accepts(v) ? v : sp.defaultValue;
          for (size_t i = 0; i < sp.choices.size(); ++i)
            if (sp.choices[i].first == value) {
              c->SetSelection(i);
              break;
            }
        };
      } else {
        auto* c = new wxTextCtrl(
            p, wxID_ANY, "", wxDefaultPosition,
            sp.type == "multiline" ? wxSize(-1, 85) : wxDefaultSize,
            sp.type == "multiline" ? wxTE_MULTILINE : 0);
        s->Add(c, 0, wxEXPAND | wxALL, 6);
        f.read = [c] { return S(c->GetValue()); };
        f.write = [c](const std::string& v) { c->ChangeValue(W(v)); };
      }
      auto current = plugins.pluginSetting(owner, sp.key);
      f.write(sp.accepts(current) ? current : sp.defaultValue);
      if (!sp.description.empty()) {
        auto* help = new wxStaticText(p, wxID_ANY, W(sp.description));
        help->Wrap(570);
        s->Add(help, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
      }
      fields->push_back(std::move(f));
    }
    auto commit = [this, owner, fields] {
      std::map<std::string, std::string> values;
      for (auto& f : *fields) {
        auto value = f.read();
        if (value != plugins.pluginSetting(owner, f.spec.key))
          values.emplace(f.spec.key, std::move(value));
      }
      if (!values.empty() && !plugins.updatePluginSettings(owner, values))
        throw std::runtime_error("Unable to save settings for " + owner);
    };
    if (!fields->empty()) {
      store.push_back(commit);
      reset.push_back([fields] {
        for (auto& f : *fields) f.write(f.spec.defaultValue);
      });
    }
    for (const auto& a : actions)
      button(p, s, W(a.title), [this, owner, fields, a] {
        if (wxMessageBox("Open plugin tool? Unsaved changes on this page will "
                         "be replaced by current plugin settings.",
                         "Plugin settings", wxOK | wxCANCEL, this) != wxOK)
          return;
        if (!plugins.execute(a.command))
          throw std::runtime_error("Plugin action unavailable");
        for (auto& f : *fields)
          f.write(plugins.pluginSetting(owner, f.spec.key));
      });
    p->FitInside();
  }
}
