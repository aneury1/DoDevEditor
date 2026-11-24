// Native launch.json menu, sharing the GDB plugin's persisted selection.
fs::path launchFile() const {
  if (root.empty()) throw std::runtime_error("Open a workspace folder first");
  return root / ".vscode" / "launch.json";
}
bool saveLaunchIfDirty() {
  auto p = launchFile();
  for (auto& d : docs)
    if (d.second.path == p && d.first->GetModify()) {
      if (wxMessageBox(
              "Save launch.json before running or selecting a configuration?",
              "Launch.json", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION,
              this) != wxYES)
        return false;
      return save(d.first);
    }
  return true;
}
void openLaunchConfig() {
  auto p = launchFile();
  if (!fs::exists(p)) {
    addLaunchConfig();
    return;
  }
  open(p);
}
void addLaunchConfig() {
  auto p = launchFile();
  if (!saveLaunchIfDirty()) return;
  wxTextEntryDialog n(this, "Configuration name",
                      "Add C++ launch configuration", "C++: Launch");
  if (showModal(n) != wxID_OK) return;
  wxTextEntryDialog exe(this,
                        "Executable path (relative to the workspace, absolute, "
                        "or a VSCode variable)",
                        "Launch program", "${workspaceFolder}/build/app");
  if (showModal(exe) != wxID_OK) return;
  auto name = S(n.GetValue());
  auto program = S(exe.GetValue());
  const bool exists = fs::exists(p);
  auto doc = exists ? gdb::readJsonc(p) : Json::object();
  auto revised =
      gdb::appendLaunchProfile(std::move(doc), name, program).dump(4) + "\n";
  if (exists) {
    if (wxMessageBox("Append the profile in the editor? JSON comments and "
                     "formatting will be replaced; existing configuration "
                     "values are preserved. Save to apply, or Undo to cancel.",
                     "Launch.json", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION,
                     this) != wxYES)
      return;
    open(p);
    auto e = active();
    if (!e) return;
    e->BeginUndoAction();
    e->SetTargetStart(0);
    e->SetTargetEnd(e->GetTextLength());
    e->ReplaceTarget(W(revised));
    e->EndUndoAction();
    SetStatusText("Launch profile added to editor. Save launch.json to apply.");
  } else {
    fs::create_directories(p.parent_path());
    writeText(p, revised);
    refreshTree();
    open(p);
    if (!plugins.updatePluginSetting("com.dodev.gdb", "configuration", name))
      throw std::runtime_error("Could not save launch configuration selection");
  }
}
std::string chooseLaunchConfig(bool force = false) {
  auto p = launchFile();
  if (!saveLaunchIfDirty()) return {};
  if (!fs::exists(p)) {
    openLaunchConfig();
    return {};
  }
  auto names = gdb::names(context());
  if (names.empty())
    throw std::runtime_error("No cppdbg/GDB configurations in launch.json");
  auto selected =
      plugins.pluginSettingJson("com.dodev.gdb", "configuration").string();
  auto i = std::find(names.begin(), names.end(), selected);
  if (!force && i != names.end()) return selected;
  wxArrayString choices;
  for (auto& name : names) choices.Add(W(name));
  wxSingleChoiceDialog d(this, "Select a launch.json configuration",
                         "Launch configurations", choices);
  d.SetSelection(i == names.end() ? 0 : static_cast<int>(i - names.begin()));
  if (showModal(d) != wxID_OK) return {};
  selected = S(d.GetStringSelection());
  if (!plugins.updatePluginSetting("com.dodev.gdb", "configuration", selected))
    throw std::runtime_error("Could not save launch configuration selection");
  SetStatusText(W("Launch configuration: " + selected));
  return selected;
}
void runWithoutDebug(std::string name = {}) {
  if (name.empty())
    name = chooseLaunchConfig();
  else if (!saveLaunchIfDirty())
    return;
  if (name.empty()) return;
  if (wxMessageBox(W("Run launch configuration '" + name +
                     "' and its pre-launch tasks? Only run trusted workspace "
                     "configurations."),
                   "Run Without Debugging",
                   wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES)
    return;
  std::vector<dodev::terminal::TaskLaunch> plan;
  try {
    plan = gdb::planWithoutDebug(taskContext(), name);
  } catch (const dodev::terminal::TaskCancelled&) {
    SetStatusText("Launch cancelled");
    return;
  }
  auto t = terminal(W("Launch: " + name));
  auto ptr = t.get();
  ptr->taskMode = true;
  ptr->launchProfile = name;
  ptr->taskQueue = std::move(plan);
  extraTerminals.push_back(std::move(t));
  palette.apply(ptr->out->GetParent());
  ptr->out->setTheme(palette.background, palette.text, palette.selection,
                     settings.getInt("terminal.fontSize", 14),
                     W(settings.getString("terminal.fontFamily")));
  try {
    startNextTask(ptr);
    showTerminal(ptr);
  } catch (...) {
    ptr->taskQueue.clear();
    throw;
  }
}
void launchAction(const std::string& a) {
  if (a == "launch.configure")
    openLaunchConfig();
  else if (a == "launch.add")
    addLaunchConfig();
  else if (a == "launch.select")
    chooseLaunchConfig(true);
  else if (a == "launch.debug")
    dispatch("gdb.start");
  else if (a == "launch.run")
    runWithoutDebug();
}
