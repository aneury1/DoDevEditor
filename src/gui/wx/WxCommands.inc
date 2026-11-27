void persistSession() {
  Json::Array files;
  for (size_t i = 0; i < editors->GetPageCount(); ++i) {
    auto e = dynamic_cast<wxStyledTextCtrl*>(editors->GetPage(i));
    auto it = docs.find(e);
    if (it != docs.end() && !it->second.path.empty())
      files.emplace_back(it->second.path.u8string());
  }
  settings.set("wxSession.files", Json(std::move(files)));
  settings.set("appearance.theme", themeName);
  std::string err;
  if (!settings.save(&err)) std::clog << "Settings: " << err << '\n';
}
void copyPath(const fs::path& p) {
  if (p.empty()) return;
  if (wxTheClipboard->Open()) {
    wxTheClipboard->SetData(new wxTextDataObject(P(p)));
    wxTheClipboard->Close();
  }
}
void contextMenu(
    wxWindow* parent,
    const std::initializer_list<std::pair<const char*, const char*>>& items) {
  wxMenu m;
  for (auto& p : items) {
    int id = wxWindow::NewControlId();
    m.Append(id, W(p.first));
    m.Bind(
        wxEVT_MENU,
        [this, a = std::string(p.second)](wxCommandEvent&) {
          try {
            dispatch(a);
          } catch (const std::exception& ex) {
            error(ex);
          }
        },
        id);
  }
  parent->PopupMenu(&m);
}
static bool beneath(const fs::path& p, const fs::path& base) {
  if (p.empty()) return false;
  auto rel = p.lexically_relative(base);
  return p == base || (!rel.empty() && *rel.begin() != "..");
}
void explorerAction(const std::string& a) {
  auto id = tree->GetSelection();
  auto data =
      id.IsOk() ? static_cast<PathData*>(tree->GetItemData(id)) : nullptr;
  auto p = data ? data->path : root;
  std::error_code ec;
  if (a == "explorer.hidden") {
    showHidden = !showHidden;
    refreshTree();
    return;
  }
  if (a == "explorer.collapse") {
    refreshTree(false);
    return;
  }
  if (a == "explorer.refresh") {
    refreshTree();
    schedule();
    return;
  }
  if (a == "explorer.copy_path") {
    copyPath(p);
    return;
  }
  if (a == "explorer.open") {
    if (fs::is_directory(p, ec)) {
      if (!wxui::explorer::canExpand(p, root))
        throw std::runtime_error(
            "Directory link is outside workspace or recursive");
      populate(id);
      tree->Expand(id);
    } else
      open(p);
    return;
  }
  if (a == "explorer.terminal") {
    auto cwd = fs::is_directory(p, ec) ? p : p.parent_path();
    addTerminal(false, cwd);
    return;
  }
  if (a == "explorer.new_file" || a == "explorer.new_folder") {
    bool folder = a == "explorer.new_folder";
    auto name = wxGetTextFromUser("Name", "Create",
                                  folder ? "new-folder" : "untitled.txt");
    if (name.empty()) return;
    auto parent = fs::is_directory(p, ec) ? p : p.parent_path();
    if (!wxui::explorer::canExpand(parent, root))
      throw std::runtime_error(
          "Cannot create entries outside the workspace or through recursive "
          "directory links");
    auto r = explorer::createEntry(parent, S(name), folder);
    if (!r.ok) throw std::runtime_error(r.error);
    refreshTree();
    schedule();
    if (!folder) {
      open(r.path);
      reveal();
    }
    return;
  }
  if (p == root) throw std::runtime_error("Cannot modify workspace root");
  if (a == "explorer.rename") {
    auto name = wxGetTextFromUser("New name", "Rename", P(p.filename()));
    if (name.empty()) return;
    auto r = explorer::renameEntry(p, S(name));
    if (!r.ok) throw std::runtime_error(r.error);
    for (auto& d : docs)
      if (beneath(d.second.path, p)) {
        auto rel = d.second.path.lexically_relative(p);
        d.second.path = rel == "." ? r.path : r.path / rel;
        editors->SetPageText(editors->GetPageIndex(d.first),
                             P(d.second.path.filename()));
      }
  } else if (a == "explorer.delete") {
    std::vector<wxStyledTextCtrl*> victims;
    for (auto& d : docs)
      if (beneath(d.second.path, p)) {
        if (d.first->GetModify())
          throw std::runtime_error(
              "Save or close modified files before deleting");
        victims.push_back(d.first);
      }
    if (wxMessageBox("Permanently delete this item and its contents?\n" + P(p),
                     "Delete", wxYES_NO | wxICON_WARNING, this) != wxYES)
      return;
    auto r = explorer::removeEntry(p, root);
    if (!r.ok) throw std::runtime_error(r.error);
    for (auto e : victims) {
      auto i = editors->GetPageIndex(e);
      docs.erase(e);
      editors->DeletePage(i);
    }
  }
  refreshTree();
  schedule();
}
void tabAction(const std::string& a) {
  auto* page = editors->GetCurrentPage();
  if (!page) return;
  if (a == "tab.copy_path") {
    if (auto* e = active()) copyPath(docs.at(e).path);
    return;
  }
  std::vector<wxWindow*> victims;
  int i = editors->GetSelection();
  for (size_t k = 0; k < editors->GetPageCount(); ++k) {
    auto* other = editors->GetPage(k);
    if (other != page && (a == "tab.close_others" || int(k) > i))
      victims.push_back(other);
  }
  for (auto* v : victims)
    if (auto* e = dynamic_cast<wxStyledTextCtrl*>(v); e && !confirm(e)) return;
  for (auto* v : victims) {
    int k = editors->GetPageIndex(v);
    if (auto* j = dynamic_cast<wxui::JournalPanel*>(v)) j->shutdown();
    if (auto* e = dynamic_cast<wxStyledTextCtrl*>(v)) docs.erase(e);
    if (k != wxNOT_FOUND) editors->DeletePage(k);
  }
}
std::vector<std::string> menuActions;
wxChoice* themePicker = nullptr;
wxChoice* taskPicker = nullptr;
wxButton* taskRunButton = nullptr;
dodev::terminal::TaskCatalog taskCatalog;
std::chrono::steady_clock::time_point lastTaskPoll{};
void refreshTaskPicker(bool force = false) {
  if (!taskPicker) return;
  const auto previous =
      taskCatalog.workspace() == root && taskPicker->IsEnabled()
          ? S(taskPicker->GetStringSelection())
          : std::string();
  if (!taskCatalog.refresh(root, settings.directory() / "tasks.json", force))
    return;
  taskPicker->Clear();
  for (const auto& t : taskCatalog.tasks()) taskPicker->Append(W(t.label));
  const bool available =
      !taskCatalog.tasks().empty() && taskCatalog.error().empty();
  if (available)
    taskPicker->SetSelection(taskCatalog.selection(previous));
  else {
    taskPicker->Append(root.empty()                  ? "Open a folder"
                       : taskCatalog.error().empty() ? "No tasks configured"
                                                     : "Invalid tasks.json");
    taskPicker->SetSelection(0);
  }
  taskPicker->Enable(available);
  taskRunButton->Enable(available);
  auto tip = taskCatalog.error().empty() ? P(taskCatalog.path())
                                         : W(taskCatalog.error());
  if (root.empty()) tip = "Open a workspace to load VSCode tasks";
  taskPicker->SetToolTip(tip);
  taskRunButton->SetToolTip(
      available ? wxString("Run the selected task and dependencies") : tip);
}
void pollTaskPicker() {
  auto now = std::chrono::steady_clock::now();
  if (now - lastTaskPoll >= std::chrono::seconds(1)) {
    lastTaskPoll = now;
    refreshTaskPicker();
  }
}
void runSelectedTask() {
  if (!taskPicker || !taskPicker->IsEnabled()) return;
  auto label = S(taskPicker->GetStringSelection());
  runWorkspaceTask(false, label);
}

std::string shortcutFromEvent(const wxKeyEvent& e) {
  int k = e.GetKeyCode();
  std::string name;
  if (k >= 'A' && k <= 'Z')
    name = std::string(1, char(k));
  else if (k >= '0' && k <= '9')
    name = std::string(1, char(k));
  else if (k >= WXK_F1 && k <= WXK_F12)
    name = "F" + std::to_string(k - WXK_F1 + 1);
  else {
    switch (k) {
      case WXK_BACK:
        name = "Backspace";
        break;
      case WXK_DELETE:
        name = "Delete";
        break;
      case WXK_UP:
        name = "Up";
        break;
      case WXK_DOWN:
        name = "Down";
        break;
      case WXK_LEFT:
        name = "Left";
        break;
      case WXK_RIGHT:
        name = "Right";
        break;
      case WXK_HOME:
        name = "Home";
        break;
      case WXK_END:
        name = "End";
        break;
      case WXK_PAGEUP:
        name = "PageUp";
        break;
      case WXK_PAGEDOWN:
        name = "PageDown";
        break;
      case WXK_RETURN:
        name = "Enter";
        break;
      case WXK_TAB:
        name = "Tab";
        break;
      case WXK_ESCAPE:
        name = "Escape";
        break;
      case WXK_INSERT:
        name = "Insert";
        break;
      case WXK_SPACE:
        name = "Space";
        break;
      case '`':
      case '~':
        name = "Grave";
        break;
      case '/':
      case '?':
        name = "Slash";
        break;
      case '-':
        name = "Minus";
        break;
      case '=':
        name = "Equal";
        break;
      case ';':
        name = "Semicolon";
        break;
      case ',':
        name = "Comma";
        break;
      case '.':
        name = "Period";
        break;
      case '%':
        if (e.ShiftDown()) name = "5";
        break;
      default:
        break;
    }
  }
  if (name.empty()) return {};
  keys::Shortcut key;
  key.key = name;
  key.valid = true;
  if (e.ControlDown()) key.modifiers |= keys::Ctrl;
  if (e.ShiftDown()) key.modifiers |= keys::Shift;
  if (e.AltDown()) key.modifiers |= keys::Alt;
  if (e.MetaDown()) key.modifiers |= keys::Super;
  return keys::formatShortcut(key);
}
void keyHook(wxKeyEvent& e) {
  auto focus = wxWindow::FindFocus();
  if (!focus || wxGetTopLevelParent(focus) != this) {
    e.Skip();
    return;
  }
  auto key = shortcutFromEvent(e);
  if (key.empty()) {
    e.Skip();
    return;
  }
  for (auto& a : menuActions) {
    if (keybindings.get(a) != key) continue;
    if (dynamic_cast<WxTerminal*>(focus) && a.rfind("terminal.", 0) != 0 &&
        a.rfind("gdb.", 0) != 0 && a.rfind("launch.", 0) != 0 &&
        a != "plugin.view.gdb.debug") {
      e.Skip();
      return;
    }
    if (dynamic_cast<wxTextCtrl*>(focus) && a == "edit.delete") {
      e.Skip();
      return;
    }
    try {
      dispatch(a);
    } catch (const std::exception& ex) {
      error(ex);
    }
    return;
  }
  e.Skip();
}
void refreshThemePicker() {
  if (!themePicker) return;
  themePicker->Clear();
  for (auto& t : themes.names()) themePicker->Append(W(t));
  themePicker->SetStringSelection(W(themeName));
}
void buildToolbar() {
  auto tb = CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_TEXT);
  auto add = [&](const char* title, const wxArtID& art, const char* action) {
    int id = wxWindow::NewControlId();
    tb->AddTool(id, W(title),
                wxArtProvider::GetBitmap(art, wxART_TOOLBAR, wxSize(16, 16)));
    Bind(
        wxEVT_TOOL,
        [this, a = std::string(action)](wxCommandEvent&) {
          try {
            dispatch(a);
          } catch (const std::exception& ex) {
            error(ex);
          }
        },
        id);
  };
  add("New", wxART_NEW, "file.new");
  add("Open", wxART_FILE_OPEN, "file.open");
  add("Folder", wxART_FOLDER_OPEN, "folder.open");
  add("Save", wxART_FILE_SAVE, "file.save");
  tb->AddSeparator();
  add("Settings", wxART_HELP_SETTINGS, "settings.show");
  add("Find", wxART_FIND, "edit.find_workspace");
  tb->AddSeparator();
  themePicker = new wxChoice(tb, wxID_ANY, wxDefaultPosition, wxSize(220, -1));
  tb->AddControl(themePicker);
  refreshThemePicker();
  themePicker->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
    try {
      selectTheme(S(themePicker->GetStringSelection()));
    } catch (const std::exception& ex) {
      error(ex);
    }
  });
  tb->AddSeparator();
  taskPicker = new wxChoice(tb, wxID_ANY, wxDefaultPosition, wxSize(205, -1));
  tb->AddControl(taskPicker);
  taskRunButton = new wxButton(tb, wxID_ANY, "Run Task", wxDefaultPosition,
                               wxDefaultSize, wxBU_EXACTFIT);
  tb->AddControl(taskRunButton);
  taskRunButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    try {
      runSelectedTask();
    } catch (const std::exception& ex) {
      error(ex);
    }
  });
  refreshTaskPicker(true);
  tb->Realize();
}

void deferMenus() {
  CallAfter([this] {
    if (!IsBeingDeleted()) buildMenus();
  });
}
// Native adapters for the original Raven command IDs.
std::vector<std::unique_ptr<Terminal>> extraTerminals;
std::vector<Terminal*> allTerminals() {
  std::vector<Terminal*> r;
  if (shell) r.push_back(shell.get());
  if (debug) r.push_back(debug.get());
  for (auto& t : extraTerminals) r.push_back(t.get());
  return r;
}
Terminal* currentTerminal() {
  auto p = bottom->GetCurrentPage();
  for (auto* t : allTerminals())
    if (t->out->GetParent() == p) return t;
  return shell.get();
}
void showTerminal(Terminal* t) {
  aui.GetPane(bottom).Show();
  aui.Update();
  auto i = bottom->GetPageIndex(t->out->GetParent());
  if (i != wxNOT_FOUND) bottom->SetSelection(i);
  t->out->resizeTerminal();
  t->out->SetFocus();
}
void startTerminal(Terminal* t, const fs::path& cwd = {}) {
  if (t->session.running()) t->session.stop();
  t->screen.resetSession();
  t->screen.setScrollbackLimit(
      settings.getInt("terminal.scrollbackLines", 3000));
  t->out->clearSelection();
  std::string err;
  if (!cwd.empty()) t->cwd = cwd;
  if (t->cwd.empty()) t->cwd = root;
  if (!t->session.start(t->cwd, &err,
                        settings.getString("terminal.defaultShell")))
    throw std::runtime_error(err);
  t->wasRunning = true;
  showTerminal(t);
}
void sendShell(const std::string& cmd) {
  auto t = currentTerminal();
  if (t->taskMode || t == debug.get()) t = shell.get();
  if (!t->session.running()) startTerminal(t);
  showTerminal(t);
  if (!t->session.sendLine(cmd))
    throw std::runtime_error("Terminal write failed");
}
void addTerminal(bool split, const fs::path& cwd = {}) {
  auto t = terminal("Terminal " +
                    wxString::Format("%zu", extraTerminals.size() + 2));
  auto ptr = t.get();
  extraTerminals.push_back(std::move(t));
  if (split) {
    auto i = bottom->GetPageIndex(ptr->out->GetParent());
    if (i != wxNOT_FOUND) bottom->Split(i, wxRIGHT);
  }
  palette.apply(ptr->out->GetParent());
  ptr->out->setTheme(palette.background, palette.text, palette.selection,
                     settings.getInt("terminal.fontSize", 14),
                     W(settings.getString("terminal.fontFamily")));
  startTerminal(ptr, cwd);
}
void exportOutput(bool all) {
  wxFileDialog d(this, "Export terminal output", P(root), "terminal.txt",
                 "Text files|*.txt", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (showModal(d) != wxID_OK) return;
  std::ofstream f(F(d.GetPath()));
  if (!f) throw std::runtime_error("Cannot write terminal output");
  for (auto* t : allTerminals())
    if (all || t == currentTerminal()) {
      if (all)
        f << "\n=== "
          << S(bottom->GetPageText(bottom->GetPageIndex(t->out->GetParent())))
          << " ===\n";
      f << t->screen.fullText();
    }
}
void startNextTask(Terminal* t) {
  if (t->taskQueue.empty()) return;
  auto step = std::move(t->taskQueue.front());
  t->taskQueue.erase(t->taskQueue.begin());
  if (!step.requiredExecutable.empty() &&
      !fs::is_regular_file(step.requiredExecutable))
    throw std::runtime_error("Launch executable not found: " +
                             step.requiredExecutable.u8string());
  t->cwd = step.cwd;
  t->screen.feed("\r\n[Task: " + step.label + "]\r\n");
  std::string err;
  if (!t->session.start(step.cwd, &err, step.argv.front(), step.argv))
    throw std::runtime_error(err);
  t->wasRunning = true;
  t->out->resizeTerminal();
  t->out->Refresh();
}
std::optional<std::string> promptTaskInput(
    const dodev::terminal::TaskInput& i) {
  if (i.type == "pickString") {
    wxArrayString choices;
    int selected = 0;
    for (size_t k = 0; k < i.options.size(); ++k) {
      auto& o = i.options[k];
      choices.Add(W(o.label == o.value ? o.label : o.label + " - " + o.value));
      if (i.hasDefault && o.value == i.defaultValue)
        selected = static_cast<int>(k);
    }
    wxSingleChoiceDialog d(this, W(i.description), W("Task input: " + i.id),
                           choices);
    d.SetSelection(selected);
    if (showModal(d) != wxID_OK) return std::nullopt;
    return i.options.at(d.GetSelection()).value;
  }
  if (i.password &&
      wxMessageBox(
          "Task input may be exposed in process arguments, environment, or "
          "task output. Do not use sensitive credentials here. Continue?",
          "Task password input", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
          this) != wxYES)
    return std::nullopt;
  wxTextEntryDialog d(
      this, W(i.description), W("Task input: " + i.id), W(i.defaultValue),
      wxOK | wxCANCEL | wxCENTRE | (i.password ? wxTE_PASSWORD : 0));
  if (showModal(d) != wxID_OK) return std::nullopt;
  return S(d.GetValue());
}
dodev::terminal::TaskContext taskContext() {
  dodev::terminal::TaskContext ctx;
  ctx.workspace = root;
  ctx.executable = F(wxStandardPaths::Get().GetExecutablePath());
  ctx.startupCwd = startupDirectory;
  if (auto e = active()) {
    ctx.file = docs.at(e).path;
    ctx.line = e->GetCurrentLine() + 1;
    ctx.column = e->GetColumn(e->GetCurrentPos()) + 1;
    ctx.selectedText = S(e->GetSelectedText());
  }
  ctx.prompt = [this](const auto& i) { return promptTaskInput(i); };
  ctx.config = [this](const std::string& key) -> std::optional<std::string> {
    const auto& v = settings.get(key);
    if (v.isString()) return v.string();
    if (v.isBool() || v.isNumber()) return v.dump(0);
    return std::nullopt;
  };
  return ctx;
}
void runWorkspaceTask(bool build, const std::string& requestedLabel = {}) {
  namespace tt = dodev::terminal;
  if (root.empty()) throw std::runtime_error("Open a workspace first");
  // Reload on every explicit run, never execute a stale catalog entry.
  refreshTaskPicker(true);
  if (!taskCatalog.error().empty())
    throw std::runtime_error(taskCatalog.error());
  auto tasks = taskCatalog.tasks();
  if (tasks.empty())
    throw std::runtime_error("No tasks in " + taskCatalog.path().u8string());
  std::vector<size_t> eligible;
  for (size_t i = 0; i < tasks.size(); ++i)
    if (!build || tasks[i].group == "build") eligible.push_back(i);
  if (eligible.empty()) throw std::runtime_error("No build task configured");
  size_t k = eligible.front();
  bool chosen = false;
  if (!requestedLabel.empty()) {
    auto it = std::find_if(eligible.begin(), eligible.end(), [&](size_t i) {
      return tasks[i].label == requestedLabel;
    });
    if (it == eligible.end())
      throw std::runtime_error("Selected task no longer exists: " +
                               requestedLabel);
    k = *it;
    chosen = true;
  }
  if (build && !chosen) {
    for (auto i : eligible)
      if (tasks[i].defaultBuild) {
        k = i;
        chosen = true;
        break;
      }
  }
  if (!chosen && (!build || eligible.size() > 1)) {
    wxArrayString names;
    for (auto i : eligible) names.Add(W(tasks[i].label));
    wxSingleChoiceDialog d(this, "Choose VSCode task", "Tasks", names);
    if (showModal(d) != wxID_OK) return;
    k = eligible.at(d.GetSelection());
  }
  if (taskPicker) taskPicker->SetStringSelection(W(tasks[k].label));
  if (wxMessageBox(W("Run task '" + tasks[k].label +
                     "' and its dependencies from this workspace?"),
                   "Trust workspace task",
                   wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES)
    return;
  auto ctx = taskContext();
  std::vector<tt::TaskLaunch> plan;
  try {
    plan = tt::planTasks(tasks, tasks[k].label, ctx);
  } catch (const tt::TaskCancelled&) {
    SetStatusText("Task cancelled");
    return;
  }
  auto t = terminal(W("Task: " + tasks[k].label));
  auto ptr = t.get();
  ptr->taskMode = true;
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
void terminalAction(const std::string& a) {
  if (isPluginPage(bottom->GetCurrentPage())) {
    if (a == "terminal.close") {
      bottom->DeletePage(bottom->GetSelection());
      return;
    }
    if (a == "terminal.focus") {
      bottom->GetCurrentPage()->SetFocus();
      return;
    }
    if (a == "terminal.copy") {
      auto* t = dynamic_cast<wxTextCtrl*>(wxWindow::FindFocus());
      wxWindow* ancestor = t;
      while (ancestor && ancestor != bottom->GetCurrentPage())
        ancestor = ancestor->GetParent();
      if (t && ancestor)
        t->Copy();
      else
        SetStatusText("Select text in the plugin output to copy");
      return;
    }
    if (a == "terminal.restart" || a == "terminal.paste" ||
        a == "terminal.rename" || a == "terminal.clear" ||
        a == "terminal.interrupt" || a == "terminal.export") {
      SetStatusText("Select a terminal tab for this action");
      return;
    }
  }
  if (auto* j = dynamic_cast<wxui::JournalPanel*>(bottom->GetCurrentPage())) {
    if (a == "terminal.close") {
      j->requestClose();
      return;
    }
    if (a == "terminal.copy") {
      j->copySelected();
      return;
    }
    if (a == "terminal.clear") {
      j->clear();
      return;
    }
    if (a == "terminal.export") {
      j->exportLogs();
      return;
    }
    if (a == "terminal.focus") {
      j->focusTable();
      return;
    }
    if (a == "terminal.interrupt") {
      j->stop();
      return;
    }
    if (a == "terminal.restart" || a == "terminal.paste" ||
        a == "terminal.rename") {
      SetStatusText("Select a terminal tab for this action");
      return;
    }
  }
  auto t = currentTerminal();
  if (a == "terminal.new" || a == "terminal.split") {
    addTerminal(a == "terminal.split");
    return;
  }
  if (a == "terminal.next" || a == "terminal.previous") {
    auto n = bottom->GetPageCount();
    if (n) {
      bottom->SetSelection(
          (bottom->GetSelection() + n + (a == "terminal.next" ? 1 : n - 1)) %
          n);
      if (auto* j = dynamic_cast<wxui::JournalPanel*>(bottom->GetCurrentPage()))
        j->focusTable();
      else if (!isPluginPage(bottom->GetCurrentPage()))
        showTerminal(currentTerminal());
    }
    return;
  }
  if (a == "terminal.close") {
    t->session.stop();
    if (t != shell.get() && t != debug.get()) {
      auto i = bottom->GetPageIndex(t->out->GetParent());
      if (i != wxNOT_FOUND) bottom->DeletePage(i);
      extraTerminals.erase(
          std::remove_if(extraTerminals.begin(), extraTerminals.end(),
                         [t](auto& p) { return p.get() == t; }),
          extraTerminals.end());
    } else
      t->out->Refresh();
    return;
  }
  if (a == "terminal.restart") {
    if (t->taskMode) {
      t->taskQueue.clear();
      t->session.stop();
      if (t->launchProfile.empty())
        runWorkspaceTask(false);
      else
        runWithoutDebug(t->launchProfile);
      return;
    }
    if (t == debug.get()) {
      t->session.stop();
      run();
    } else
      startTerminal(t);
    return;
  }
  if (a == "terminal.rename") {
    auto i = bottom->GetPageIndex(t->out->GetParent());
    auto name =
        wxGetTextFromUser("Terminal name", "Rename", bottom->GetPageText(i));
    if (!name.empty()) bottom->SetPageText(i, name);
    return;
  }
  if (a == "terminal.copy")
    t->out->copy();
  else if (a == "terminal.paste")
    t->out->paste();
  else if (a == "terminal.clear") {
    t->screen.clear();
    t->out->clearSelection();
  } else if (a == "terminal.interrupt") {
    t->taskQueue.clear();
    t->session.interrupt();
  } else if (a == "terminal.focus")
    showTerminal(t);
  else if (a == "terminal.export" || a == "terminal.export_all")
    exportOutput(a == "terminal.export_all");
  else if (a == "terminal.configure_tasks" || a == "terminal.configure_build") {
    auto p = root / ".vscode" / "tasks.json";
    if (!fs::exists(p)) {
      fs::create_directories(p.parent_path());
      writeText(p, dodev::terminal::defaultTasksJson());
    }
    refreshTaskPicker(true);
    open(p);
  } else if (a == "terminal.run_task" || a == "terminal.run_build") {
    runWorkspaceTask(a == "terminal.run_build");
  } else if (a == "terminal.run_file") {
    auto e = active();
    if (!e || !save(e)) return;
    auto cmd = dodev::terminal::launchFileCommand(docs.at(e).path,
#ifdef _WIN32
                                                  true
#else
                                                  false
#endif
    );
    if (cmd.empty())
      throw std::runtime_error("No run command for this extension");
    if (wxMessageBox("Run this file?", "Terminal", wxYES_NO | wxICON_WARNING,
                     this) == wxYES)
      sendShell(cmd);
  } else if (a == "terminal.run_selected") {
    auto e = active();
    if (e && !e->GetSelectedText().empty() &&
        wxMessageBox("Execute selected text in the shell?", "Terminal",
                     wxYES_NO | wxICON_WARNING, this) == wxYES)
      sendShell(S(e->GetSelectedText()));
  }
}
void showSide(size_t i) {
  aui.GetPane(side).Show();
  aui.Update();
  side->SetSelection(i);
}
void reveal() {
  auto e = active();
  if (!e || docs.at(e).path.empty()) return;
  auto node = treeFind(docs.at(e).path);
  if (node.IsOk()) {
    tree->SelectItem(node);
    tree->EnsureVisible(node);
    showSide(0);
  }
}
void newTemplate() {
  auto values = templates.all();
  wxArrayString names;
  for (auto& t : values) names.Add(W(t.name));
  wxSingleChoiceDialog d(this, "Choose template", "New from template", names);
  if (showModal(d) != wxID_OK) return;
  auto t = values.at(d.GetSelection());
  auto name =
      wxGetTextFromUser("File name", "Template", W("NewFile" + t.extension));
  if (name.empty()) return;
  auto path = F(name);
  newDoc(path);
  auto e = active();
  e->SetText(
      W(templates.render(t, {{"FILE_NAME", S(name)},
                             {"CLASS_NAME", path.stem().u8string()},
                             {"PROJECT_NAME", root.filename().u8string()}})));
  editors->SetPageText(editors->GetSelection(), name);
}
void quickOpen() {
  std::vector<fs::path> paths;
  std::error_code ec;
  for (fs::recursive_directory_iterator
           it(root, fs::directory_options::skip_permission_denied, ec),
       end;
       it != end && !ec; it.increment(ec)) {
    if (it->is_directory(ec)) {
      auto n = it->path().filename();
      if (n == ".git" || n == ".dodev" || n == "node_modules" || n == "build")
        it.disable_recursion_pending();
    } else if (it->is_regular_file(ec)) {
      paths.push_back(it->path());
      if (paths.size() >= 20000) break;
    }
  }
  wxDialog d(this, wxID_ANY, "Quick Open", wxDefaultPosition, wxSize(660, 460),
             wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto s = new wxBoxSizer(wxVERTICAL);
  d.SetSizer(s);
  auto q = new wxTextCtrl(&d, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                          wxTE_PROCESS_ENTER);
  auto l = new wxListBox(&d, wxID_ANY);
  s->Add(q, 0, wxEXPAND | wxALL, 5);
  s->Add(l, 1, wxEXPAND | wxALL, 5);
  std::vector<size_t> map;
  auto refresh = [&] {
    l->Clear();
    map.clear();
    for (size_t i = 0; i < paths.size() && map.size() < 2000; ++i) {
      auto n = P(paths[i].lexically_relative(root));
      if (n.Lower().Contains(q->GetValue().Lower())) {
        l->Append(n);
        map.push_back(i);
      }
    }
    if (!map.empty()) l->SetSelection(0);
  };
  q->Bind(wxEVT_TEXT, [&](wxCommandEvent&) { refresh(); });
  q->Bind(wxEVT_TEXT_ENTER, [&](wxCommandEvent&) {
    if (l->GetSelection() != wxNOT_FOUND) d.EndModal(wxID_OK);
  });
  l->Bind(wxEVT_LISTBOX_DCLICK, [&](wxCommandEvent&) { d.EndModal(wxID_OK); });
  refresh();
  palette.apply(&d);
  if (showModal(d) == wxID_OK && l->GetSelection() != wxNOT_FOUND)
    open(paths[map.at(l->GetSelection())]);
}
wxString findText, replaceWith;
void findInEditor(const std::string& a) {
  auto e = active();
  if (!e) return;
  if (a == "search.show" || a == "search.replace_show" || findText.empty()) {
    wxTextEntryDialog d(this, "Find text (case sensitive)", "Find",
                        findText.empty() ? e->GetSelectedText() : findText);
    if (showModal(d) != wxID_OK) return;
    findText = d.GetValue();
  }
  if (findText.empty()) return;
  if (a == "search.replace_all" || a == "search.replace_show" ||
      a == "search.replace") {
    wxTextEntryDialog d(this, "Replace with", "Replace", replaceWith);
    if (showModal(d) != wxID_OK) return;
    replaceWith = d.GetValue();
    auto text = S(e->GetText());
    search::Options o;
    o.query = S(findText);
    o.caseSensitive = true;
    std::string result, err;
    size_t count = 0;
    if (a == "search.replace_all") {
      if (!search::replaceText(text, o, S(replaceWith), result, count, &err))
        throw std::runtime_error(err);
      e->BeginUndoAction();
      e->SetTargetStart(0);
      e->SetTargetEnd(e->GetLength());
      e->ReplaceTarget(W(result));
      e->EndUndoAction();
      SetStatusText(wxString::Format("%zu replacements", count));
      return;
    }
    if (e->GetSelectedText() == findText) e->ReplaceSelection(replaceWith);
  }
  int flags = wxSTC_FIND_MATCHCASE;
  int p =
      a == "search.prev"
          ? e->FindText(e->GetSelectionStart(), 0, findText, flags)
          : e->FindText(e->GetSelectionEnd(), e->GetLength(), findText, flags);
  if (p < 0)
    p = a == "search.prev" ? e->FindText(e->GetLength(), 0, findText, flags)
                           : e->FindText(0, e->GetLength(), findText, flags);
  if (p < 0) {
    SetStatusText("No match");
    return;
  }
  e->SetSelection(p, p + S(findText).size());
  e->EnsureCaretVisible();
  e->SetFocus();
}
void editorAction(const std::string& a) {
  for (auto* n : {editors, bottom})
    if (auto* j = dynamic_cast<wxui::JournalPanel*>(n->GetCurrentPage());
        j && j->tableHasFocus()) {
      if (a == "edit.copy")
        j->copySelected();
      else if (a == "selection.all")
        j->selectAllRows();
      return;
    }
  auto e = active();
  auto* input = dynamic_cast<wxTextCtrl*>(wxWindow::FindFocus());
  auto* term = dynamic_cast<WxTerminal*>(wxWindow::FindFocus());
  if (a == "edit.copy" && term) {
    term->copy();
    return;
  }
  if (a == "edit.paste" && term) {
    term->paste();
    return;
  }
  if (input) {
    if (a == "edit.copy")
      input->Copy();
    else if (a == "edit.cut")
      input->Cut();
    else if (a == "edit.paste")
      input->Paste();
    else if (a == "edit.undo")
      input->Undo();
    else if (a == "edit.redo")
      input->Redo();
    else if (a == "selection.all")
      input->SelectAll();
    else if (a == "edit.delete") {
      long x = 0, y = 0;
      input->GetSelection(&x, &y);
      input->Remove(x, y);
    } else
      return;
    return;
  }
  if (!e) return;
  if (a == "edit.undo") {
    e->Undo();
    return;
  }
  if (a == "edit.redo") {
    e->Redo();
    return;
  }
  if (a == "edit.copy") {
    e->Copy();
    return;
  }
  if (a == "edit.cut") {
    e->Cut();
    return;
  }
  if (a == "edit.paste") {
    e->Paste();
    return;
  }
  editor::Options o;
  o.tabSize = settings.getInt("editor.tabSize", 4);
  o.useSpaces = settings.getBool("editor.insertSpaces", true);
  auto ext = docs.at(e).path.extension();
  if (ext == ".py" || ext == ".sh" || ext == ".yaml" || ext == ".cmake")
    o.lineComment = "#";
  else if (ext == ".sql")
    o.lineComment = "--";
  auto r = editor::execute(a, S(e->GetText()), e->GetAnchor(),
                           e->GetCurrentPos(), o);
  if (!r.handled) {
    SetStatusText(W("Command not handled: " + a));
    return;
  }
  if (r.changed) {
    e->BeginUndoAction();
    e->SetTargetStart(r.replaceBegin);
    e->SetTargetEnd(r.replaceEnd);
    e->ReplaceTarget(W(r.replacement));
    e->EndUndoAction();
  }
  e->SetSelection(r.selectBegin, r.selectEnd);
  e->EnsureCaretVisible();
}
void openTool(const std::string& a) {
  if (a == "tool.data_server") {
    wxMessageBox(
        "Data Server is experimental in Raven too; no network controller is "
        "implemented.",
        "Data Server", wxOK | wxICON_INFORMATION, this);
    return;
  }
  wxFileDialog d(this, "Open file", P(root), "",
                 a == "tool.csv" ? "CSV files|*.csv" : "DLT files|*.dlt",
                 wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (showModal(d) != wxID_OK) return;
  auto path = F(d.GetPath());
  wxDialog view(this, wxID_ANY, P(path.filename()), wxDefaultPosition,
                wxSize(850, 560), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto s = new wxBoxSizer(wxVERTICAL);
  view.SetSizer(s);
  if (a == "tool.csv") {
    auto data = parseCsv(readText(path));
    if (!data.valid) throw std::runtime_error(data.error);
    auto l = list(&view, {});
    size_t cols = 0;
    for (auto& row : data.rows) cols = std::max(cols, row.size());
    for (size_t i = 0; i < cols; ++i)
      l->InsertColumn(i, wxString::Format("Column %zu", i + 1));
    for (auto& row : data.rows) {
      auto i = l->InsertItem(l->GetItemCount(),
                             row.empty() ? wxString() : W(row.front()));
      for (size_t c = 1; c < row.size(); ++c) l->SetItem(i, c, W(row[c]));
    }
    s->Add(l, 1, wxEXPAND);
    button(&view, s, "Open raw CSV in editor", [&] { open(path); });
  } else {
    auto data = parseDlt(readBytes(path));
    auto l = list(&view, {"Timestamp", "ECU", "APID", "CTID", "Payload"});
    for (auto& r : data.entries) {
      auto i = l->InsertItem(l->GetItemCount(), W(r.timestamp));
      l->SetItem(i, 1, W(r.ecu));
      l->SetItem(i, 2, W(r.apid));
      l->SetItem(i, 3, W(r.ctid));
      l->SetItem(i, 4, W(r.payload));
    }
    s->Add(l, 1, wxEXPAND);
    s->Add(new wxStaticText(&view, wxID_ANY, W(data.error)), 0, wxALL, 5);
  }
  palette.apply(&view);
  showModal(view);
}
// Generic plugin views are identified by their registered ID, not their title.
bool isPluginPage(wxWindow* page) const {
  return page && (page->GetName().StartsWith("dodev.plugin.view:") ||
                  page->GetName().StartsWith("dodev.plugin.pending:"));
}
// Workspace views are registered by plugins, and loaded only when selected.
// This avoids running Git queries or other plugin work during project startup.
void resetWorkspacePluginTabs() {
  side->SetSelection(0);
  for (size_t i = side->GetPageCount(); i > 0; --i)
    if (isPluginPage(side->GetPage(i - 1))) side->DeletePage(i - 1);
  for (const auto& v : plugins.views())
    if (isWorkspacePluginView(v)) {
      auto* p = new wxScrolledWindow(side, wxID_ANY);
      p->SetName(W("dodev.plugin.pending:" + v.id));
      p->SetScrollRate(0, 12);
      auto* s = new wxBoxSizer(wxVERTICAL);
      p->SetSizer(s);
      s->Add(new wxStaticText(p, wxID_ANY,
                              "Select this tab to load its plugin view."),
             0, wxALL | wxEXPAND, 6);
      side->AddPage(p, W(v.title), false);
      palette.apply(p);
    }
}
void loadSelectedWorkspacePlugin() {
  auto* p = side->GetCurrentPage();
  if (!p || !p->GetName().StartsWith("dodev.plugin.pending:")) return;
  const wxString prefix = "dodev.plugin.pending:";
  auto id = S(p->GetName().Mid(prefix.length()));
  // A project switch or close may destroy the pending page before this event
  // runs.
  CallAfter([this, id, weak = wxWeakRef<wxWindow>(p)] {
    auto* page = weak.get();
    if (!page || side->GetCurrentPage() != page ||
        !page->GetName().StartsWith("dodev.plugin.pending:"))
      return;
    try {
      pluginView(id);
    } catch (const std::exception& e) {
      error(e);
    }
  });
}
#include "WxWorkspaceProfiles.inc"
void pluginView(const std::string& id, bool reload = false) {
  for (const auto& v : plugins.views()) {
    if (v.id != id) continue;
    // Existing legacy Docker graphical adapter; generic SDK views below.
    if (id == "docker.dashboard") {
      dockerPanel();
      return;
    }
    const auto placement = pluginViewLocation(v);
    auto* book = placement == PluginViewLocation::Editor   ? editors
                 : placement == PluginViewLocation::Output ? bottom
                                                           : side;
    const auto name = W("dodev.plugin.view:" + id);
    auto reveal = [&] {
      if (book != editors) aui.GetPane(book).Show();
      aui.Update();
    };
    // Populate lazy pages in place, keeping tab order and the Workspace
    // selection stable.
    wxScrolledWindow* p = nullptr;
    const auto pending = W("dodev.plugin.pending:" + id);
    for (size_t i = 0; i < book->GetPageCount(); ++i) {
      auto* old = book->GetPage(i);
      if (old->GetName() != name && old->GetName() != pending) continue;
      if (!reload && old->GetName() == name) {
        book->SetSelection(i);
        reveal();
        return;
      }
      p = dynamic_cast<wxScrolledWindow*>(old);
      if (p) break;
    }
    const bool existing = p != nullptr;
    if (!p) {
      p = new wxScrolledWindow(book, wxID_ANY);
      p->SetScrollRate(0, 12);
      p->SetSizer(new wxBoxSizer(wxVERTICAL));
    }
    p->SetName(name);
    auto* s = p->GetSizer();
    s->Clear(true);
    // The builder is a short-lived adapter. Widgets and copied strings are
    // owned by wx.
    struct Ctx {
      Frame* f;
      wxWindow* p;
      wxSizer* s;
      std::string owner;
      bool sidebar;
    };
    Ctx c{this, p, s, v.owner, book == side};
    DodevUiBuilder b{};
    b.struct_size = sizeof(b);
    b.context = &c;
    b.label = [](void* x, const char* text) {
      auto& c = *static_cast<Ctx*>(x);
      auto* label = new wxStaticText(c.p, wxID_ANY, W(text ? text : ""));
      label->Wrap(c.sidebar ? 240 : 550);
      c.s->Add(label, 0, wxALL | wxEXPAND, 6);
    };
    b.button = [](void* x, const char* text, const char* cmd) {
      auto& c = *static_cast<Ctx*>(x);
      auto* f = c.f;
      f->button(c.p, c.s, W(text ? text : ""),
                [f, key = std::string(cmd ? cmd : "")] { f->dispatch(key); });
    };
    b.read_only_text = [](void* x, const char* text) {
      auto& c = *static_cast<Ctx*>(x);
      auto* t =
          new wxTextCtrl(c.p, wxID_ANY, W(text ? text : ""), wxDefaultPosition,
                         wxSize(-1, 100), wxTE_MULTILINE | wxTE_READONLY);
      c.s->Add(t, 1, wxEXPAND | wxALL, 5);
    };
    b.table = [](void* x, const char* const* cols, size_t nc,
                 const char* const* cells, const char* const* ids, size_t nr,
                 const DodevHostAPI* host, DodevRowFn fn, void* data) {
      auto& c = *static_cast<Ctx*>(x);
      auto* list =
          new wxListCtrl(c.p, wxID_ANY, wxDefaultPosition, wxSize(-1, 240),
                         wxLC_REPORT | wxLC_SINGLE_SEL);
      for (size_t k = 0; k < nc; ++k)
        list->InsertColumn(static_cast<long>(k), W(cols[k]), wxLIST_FORMAT_LEFT,
                           k == 0 ? 250 : 180);
      auto keys = std::make_shared<std::vector<std::string>>();
      for (size_t i = 0; i < nr; ++i) {
        auto row = list->InsertItem(static_cast<long>(i), W(cells[i * nc]));
        for (size_t k = 1; k < nc; ++k)
          list->SetItem(row, static_cast<int>(k), W(cells[i * nc + k]));
        keys->emplace_back(ids[i]);
      }
      c.s->Add(list, 1, wxEXPAND | wxALL, 4);
      auto* frame = c.f;
      list->Bind(wxEVT_LIST_ITEM_SELECTED,
                 [frame, host, fn, data, keys](wxListEvent& e) {
                   try {
                     if (fn && e.GetIndex() >= 0 &&
                         static_cast<size_t>(e.GetIndex()) < keys->size())
                       fn(host, keys->at(e.GetIndex()).c_str(), data);
                   } catch (const std::exception& ex) {
                     frame->error(ex);
                   }
                 });
    };
    b.workspace_profiles = [](void* x, const char* defaults) {
      auto& c = *static_cast<Ctx*>(x);
      try {
        c.f->workspaceProfilesEditor(c.p, c.s, c.owner,
                                     defaults ? defaults : "{}");
      } catch (const std::exception& ex) {
        auto* label = new wxStaticText(c.p, wxID_ANY, W(ex.what()));
        label->Wrap(240);
        c.s->Add(label, 0, wxEXPAND | wxALL, 4);
      }
    };
    b.form = [](void* x, const DodevFormField* fields, size_t count,
                const char* submit, const DodevHostAPI* host, DodevFormFn fn,
                void* data) {
      auto& c = *static_cast<Ctx*>(x);
      if (!fields || !fn || count == 0 || count > 64) return;
      auto inputs = std::make_shared<std::vector<wxTextCtrl*>>();
      for (size_t i = 0; i < count; ++i) {
        c.s->Add(new wxStaticText(c.p, wxID_ANY,
                                  W(fields[i].label ? fields[i].label : "")),
                 0, wxLEFT | wxTOP, 5);
        auto* t = new wxTextCtrl(
            c.p, wxID_ANY, W(fields[i].initial ? fields[i].initial : ""),
            wxDefaultPosition,
            fields[i].multiline ? wxSize(-1, 95) : wxDefaultSize,
            fields[i].multiline ? wxTE_MULTILINE : 0);
        inputs->push_back(t);
        c.s->Add(t, 0, wxEXPAND | wxALL, 5);
      }
      auto* frame = c.f;
      frame->button(c.p, c.s, W(submit ? submit : "Submit"),
                    [frame, host, fn, data, inputs] {
                      try {
                        std::vector<std::string> values;
                        std::vector<const char*> args;
                        for (auto* t : *inputs)
                          values.push_back(S(t->GetValue()));
                        for (auto& v : values) args.push_back(v.c_str());
                        fn(host, args.data(), args.size(), data);
                      } catch (const std::exception& e) {
                        frame->error(e);
                      }
                    });
    };
    plugins.buildView(v, b);
    // Legacy native Journal view adapter; filter UI and behavior live in the
    // plugin.
    if (id == "remotejournal.filters") {
      const auto directory = root;
      auto* filters =
          new wxui::JournalWorkspaceFilters(p, directory, [this, directory] {
            std::vector<wxui::JournalPanel*> result;
            for (size_t i = 0; i < editors->GetPageCount(); ++i)
              if (auto* capture =
                      dynamic_cast<wxui::JournalPanel*>(editors->GetPage(i));
                  capture && capture->workspaceDirectory() == directory)
                result.push_back(capture);
            return result;
          });
      filters->SetName("journal.workspace.filters");
      s->Add(filters, 0, wxEXPAND | wxALL, 2);
    }
    if (existing)
      book->SetSelection(book->GetPageIndex(p));
    else
      book->AddPage(p, W(v.title), true);
    p->Layout();
    p->FitInside();
    palette.apply(p);
    reveal();
    return;
  }
  throw std::runtime_error("Plugin view unavailable: " + id +
                           ". Enable its plugin in Settings and restart.");
}
struct MenuSpec {
  const char* path;
  const char* label;
  const char* action;
};
void buildMenus() {
#include "RavenMenuSpec.inc"
  menuActions.clear();
  auto bar = new wxMenuBar;
  std::map<std::string, wxMenu*> menus;
  std::map<std::string, bool> added;
  auto get = [&](const std::string& path) {
    auto pos = path.find('|');
    auto top = path.substr(0, pos);
    if (!menus.count(top)) {
      menus[top] = new wxMenu;
      bar->Append(menus[top], W(top));
    }
    if (pos == std::string::npos) return menus[top];
    if (!menus.count(path)) {
      menus[path] = new wxMenu;
      menus[top]->AppendSubMenu(menus[path], W(path.substr(pos + 1)));
    }
    return menus[path];
  };
  auto item = [&](wxMenu* m, const std::string& label,
                  const std::string& action) {
    auto id = wxWindow::NewControlId();
    auto key = keybindings.get(action);
    m->Append(id, W(label + (key.empty() ? "" : "    [" + key + "]")));
    m->Bind(
        wxEVT_MENU,
        [this, action](wxCommandEvent&) {
          try {
            dispatch(action);
          } catch (const std::exception& e) {
            error(e);
          }
        },
        id);
    added[action] = true;
    menuActions.push_back(action);
  };
  for (auto& spec : ravenMenus) {
    auto m = get(spec.path);
    if (!*spec.action)
      m->AppendSeparator();
    else
      item(m, spec.label, spec.action);
  }
  auto themeMenu = new wxMenu;
  get("Ver")->AppendSubMenu(themeMenu, "Color Theme");
  for (auto& n : themes.names()) item(themeMenu, n, "theme.select:" + n);
  item(themeMenu, "Import theme...", "theme.import");
  item(themeMenu, "Export current...", "theme.export");
  get("Run")->AppendSeparator();
  auto lm = get("Run|Launch.json");
  item(lm, "Select Configuration...", "launch.select");
  item(lm, "Open / Configure launch.json...", "launch.configure");
  item(lm, "Add C++ Launch Configuration...", "launch.add");
  lm->AppendSeparator();
  item(lm, "Start Debugging", "launch.debug");
  item(lm, "Run Without Debugging", "launch.run");
  for (auto& cmd : plugins.menus()) {
    auto where = cmd.location == "Tools"  ? "Herramientas"
                 : cmd.location == "View" ? "Ver"
                 : cmd.location == "File" ? "Archivo"
                                          : cmd.location;
    if (!added[cmd.command]) item(get(where), cmd.title, cmd.command);
  }
  for (auto& cmd : plugins.commands())
    if (!added[cmd.id]) item(get("Plugins"), cmd.title, cmd.id);
  for (auto& v : plugins.views())
    if (!added["plugin.view." + v.id])
      item(get("Plugins"), v.title, "plugin.view." + v.id);
  for (auto& pair : std::vector<std::pair<std::string, std::string>>{
           {"gdb.configure", "Open launch.json"},
           {"gdb.stop", "Stop Debugging"},
           {"gdb.restart", "Restart Debugging"},
           {"gdb.next", "Step Over"},
           {"gdb.step", "Step Into"},
           {"gdb.finish", "Step Out"},
           {"gdb.pause", "Pause"},
           {"gdb.breakpoint", "Toggle Breakpoint"}})
    if (!added[pair.first]) item(get("Run"), pair.second, pair.first);
  SetMenuBar(bar);
  refreshThemePicker();
}
void commandPalette() {
  std::vector<std::string> actions;
  wxArrayString names;
  for (auto& a : menuActions) {
    actions.push_back(a);
    names.Add(W(a));
  }
  wxSingleChoiceDialog d(this, "Choose command", "Command Palette", names);
  if (showModal(d) == wxID_OK) dispatch(actions.at(d.GetSelection()));
}
void dispatch(const std::string& a) {
  if (a.rfind("plugin.reload.", 0) == 0) {
    auto id = a.substr(14);
    CallAfter([this, id] {
      try {
        pluginView(id, true);
      } catch (const std::exception& e) {
        error(e);
      }
    });
    return;
  }
  if (a.rfind("launch.", 0) == 0) {
    launchAction(a);
    return;
  }
  if (a.rfind("theme.select:", 0) == 0) {
    selectTheme(a.substr(13));
    return;
  }
  if (a == "settings.show")
    settingsDialog();
  else if (a == "theme.designer")
    themeDesigner();
  else if (a == "theme.import")
    themeImport();
  else if (a == "theme.export")
    themeExport();
  else if (a == "theme.next") {
    auto names = themes.names();
    auto i = std::find(names.begin(), names.end(), themeName);
    selectTheme(
        names[(i == names.end() ? 0 : (i - names.begin() + 1) % names.size())]);
  } else if (a == "file.new")
    newDoc();
  else if (a == "file.open") {
    wxFileDialog d(this, "Open file", P(root));
    if (showModal(d) == wxID_OK) open(F(d.GetPath()));
  } else if (a == "folder.open") {
    wxDirDialog d(this, "Open folder", P(root));
    if (showModal(d) == wxID_OK) workspace(F(d.GetPath()));
  } else if (a == "file.save" || a == "file.save_as")
    save(active(), a == "file.save_as");
  else if (a == "file.close") {
    if (isPluginPage(editors->GetCurrentPage())) {
      editors->DeletePage(editors->GetSelection());
      return;
    }
    if (auto* j =
            dynamic_cast<wxui::JournalPanel*>(editors->GetCurrentPage())) {
      j->requestClose();
      return;
    }
    auto e = active();
    if (e && confirm(e)) {
      docs.erase(e);
      editors->DeletePage(editors->GetSelection());
    }
  } else if (a == "template.show")
    newTemplate();
  else if (a == "app.exit")
    Close();
  else if (a == "app.about")
    wxMessageBox("DoDevEditor wxWidgets 5.2.9\nC++17 · wxAUI · Scintilla",
                 "About", wxOK, this);
  else if (a == "panel.explorer")
    showSide(0);
  else if (a == "panel.search" || a == "edit.find_workspace") {
    showSide(1);
    query->SetFocus();
  } else if (a == "panel.git") {
    pluginView("git.history");
  } else if (a == "panel.symbols")
    showSide(2);
  else if (a == "calls.open") {
    showSide(3);
    hierarchy();
  } else if (a == "panel.output") {
    aui.GetPane(bottom).Show();
    aui.Update();
    if (auto* j = dynamic_cast<wxui::JournalPanel*>(bottom->GetCurrentPage()))
      j->focusTable();
    else if (!isPluginPage(bottom->GetCurrentPage()))
      showTerminal(currentTerminal());
  } else if (a == "explorer.reveal")
    reveal();
  else if (a.rfind("explorer.", 0) == 0)
    explorerAction(a);
  else if (a.rfind("tab.", 0) == 0)
    tabAction(a);
  else if (a == "editor.format") {
    if (!plugins.execute("clangformat.apply"))
      throw std::runtime_error("Enable ClangFormat in Settings and restart.");
  } else if (a == "git.refresh" || a == "git.details") {
    pluginView("git.history", true);
  } else if (a == "navigation.quick_open")
    quickOpen();
  else if (a == "navigation.go_to_line") {
    auto e = active();
    if (!e) return;
    auto text = wxGetTextFromUser("Line[:column]", "Go to Line");
    if (text.empty()) return;
    auto line = std::stoi(S(text.BeforeFirst(':')));
    auto col = text.Contains(":") ? std::stoi(S(text.AfterFirst(':'))) : 1;
    line = std::clamp(line, 1, e->GetLineCount());
    e->GotoPos(std::min(e->GetLineEndPosition(line - 1),
                        e->PositionFromLine(line - 1) + std::max(0, col - 1)));
    e->SetFocus();
  } else if (a == "navigation.command_palette")
    commandPalette();
  else if (a == "keybindings.show")
    shortcutDialog();
  else if (a.rfind("search.", 0) == 0)
    findInEditor(a);
  else if (a.rfind("edit.", 0) == 0 || a.rfind("selection.", 0) == 0)
    editorAction(a);
  else if (a.rfind("terminal.", 0) == 0)
    terminalAction(a);
  else if (a == "tool.qrcode.capture") {
    if (!plugins.execute("qrcode.generate"))
      throw std::runtime_error(
          "Enable QR Code in Settings > Plugins and restart.");
  } else if (a == "tool.csv" || a == "tool.dlt" || a == "tool.data_server")
    openTool(a);
  else if (a.rfind("plugin.view.", 0) == 0)
    pluginView(a.substr(12));
  else if (a == "gdb.start") {
    if (!debug->session.running() && chooseLaunchConfig().empty()) return;
    if (!plugins.execute(a)) run();
  } else if (a == "gdb.configure")
    openLaunchConfig();
  else if (a == "gdb.select")
    chooseLaunchConfig(true);
  else if (a.rfind("gdb.", 0) == 0) {
    if (!plugins.execute(a))
      throw std::runtime_error("Enable GDB plugin in Settings and restart.");
  } else if (!plugins.execute(a))
    SetStatusText(W("Command unavailable: " + a));
}
