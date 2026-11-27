// Generic JSON profile editor. The calling plugin supplies its own defaults.
void workspaceProfilesEditor(wxWindow* parent, wxSizer* layout,
                             const std::string& owner,
                             const std::string& defaults) {
  const auto path = sdk::workspaceProfilePath(root, owner);
  auto read = [path]() -> std::optional<std::string> {
    if (!fs::exists(path)) return std::nullopt;
    if (!fs::is_regular_file(path) || fs::file_size(path) > 1024 * 1024)
      throw std::runtime_error("Invalid workspace profiles file");
    return readText(path);
  };
  auto snapshot = std::make_shared<std::optional<std::string>>(read());
  auto* label = new wxStaticText(parent, wxID_ANY, "Workspace profiles (JSON)");
  layout->Add(label, 0, wxALL, 4);
  auto* input = new wxTextCtrl(
      parent, wxID_ANY,
      W(snapshot->value_or(sdk::parseWorkspaceProfiles(defaults).dump(2))),
      wxDefaultPosition, wxSize(-1, 310), wxTE_MULTILINE | wxHSCROLL);
  input->SetName("dodev.workspace.profiles");
  layout->Add(input, 1, wxEXPAND | wxALL, 4);
  auto* row = new wxBoxSizer(wxVERTICAL);
  layout->Add(row, 0, wxEXPAND);
  button(parent, row, "Save profiles", [this, path, input, snapshot, read] {
    const auto updated =
        sdk::parseWorkspaceProfiles(S(input->GetValue())).dump(2) + "\n";
    // Never silently overwrite changes made by another editor or process.
    if (read() != *snapshot)
      throw std::runtime_error(
          "Profiles changed on disk. Reload before saving.");
    for (const auto& doc : docs)
      if (doc.second.path == path)
        throw std::runtime_error(
            "Close the profiles.json source editor before saving here.");
    fs::create_directories(path.parent_path());
    writeText(path, updated);
    *snapshot = updated;
    input->ChangeValue(W(updated));
    input->DiscardEdits();
    refreshTree();
    SetStatusText("Workspace plugin profiles saved");
  });
  button(parent, row, "Reload", [this, input, snapshot, read, defaults] {
    if (input->IsModified() &&
        wxMessageBox("Discard unsaved profile changes?", "Workspace profiles",
                     wxYES_NO | wxNO_DEFAULT, this) != wxYES)
      return;
    *snapshot = read();
    input->ChangeValue(
        W(snapshot->value_or(sdk::parseWorkspaceProfiles(defaults).dump(2))));
    input->DiscardEdits();
  });
  auto* note = new wxStaticText(parent, wxID_ANY,
                                "Save profiles before starting a capture.");
  note->Wrap(240);
  layout->Add(note, 0, wxALL, 4);
}

// The inline profile controls are not source documents; protect their edits
// separately.
bool confirmWorkspaceProfileEdits() {
  for (size_t i = 0; i < side->GetPageCount(); ++i) {
    auto* field = dynamic_cast<wxTextCtrl*>(wxWindow::FindWindowByName(
        "dodev.workspace.profiles", side->GetPage(i)));
    if (field && field->IsModified())
      return wxMessageBox("Discard unsaved workspace plugin profiles?",
                          "Workspace profiles", wxYES_NO | wxNO_DEFAULT,
                          this) == wxYES;
  }
  return true;
}
