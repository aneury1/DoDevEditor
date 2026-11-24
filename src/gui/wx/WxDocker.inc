// Native view of com.dodev.docker. The plugin still owns registration/approval.
docker::Connection dockerConnection() {
  docker::Connection c;
  auto get = [&](const char* k) {
    return plugins.pluginSetting("com.dodev.docker", k);
  };
  c.executable = get("executable");
  if (c.executable.empty()) c.executable = "docker";
  c.context = get("context");
  c.host = get("host");
  c.config = get("configDirectory");
  c.flags = docker::lines(get("globalFlags"));
  return c;
}
std::string dockerQuery(const docker::Connection& c, const docker::Args& args) {
  auto argv = c.command(args);
  auto cwd = root;
  return networkJob("Docker query", [argv, cwd](std::atomic<bool>* cancel) {
    return docker::capture(argv, cwd, cancel);
  });
}
void dockerTerminal(const docker::Args& argv, const wxString& title) {
  auto t = terminal(title);
  auto ptr = t.get();
  ptr->taskMode = true;
  ptr->cwd = root;
  extraTerminals.push_back(std::move(t));
  palette.apply(ptr->out->GetParent());
  ptr->out->setTheme(palette.background, palette.text, palette.selection,
                     settings.getInt("terminal.fontSize", 14),
                     W(settings.getString("terminal.fontFamily")));
  std::string err;
  if (!ptr->session.start(root, &err, argv.front(), argv))
    throw std::runtime_error(err);
  ptr->wasRunning = true;
  showTerminal(ptr);
}
bool dockerRunDialog(wxWindow* parent, const std::string& image,
                     docker::RunOptions& out) {
  wxDialog d(parent, wxID_ANY, "Run Docker image", wxDefaultPosition,
             wxSize(760, 760), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto outer = new wxBoxSizer(wxVERTICAL);
  d.SetSizer(outer);
  auto sc = new wxScrolledWindow(&d);
  sc->SetScrollRate(0, 15);
  outer->Add(sc, 1, wxEXPAND | wxALL, 5);
  auto g = new wxFlexGridSizer(2, 5, 8);
  g->AddGrowableCol(1);
  sc->SetSizer(g);
  auto field = [&](const char* label, const wxString& value = wxString(),
                   bool multiline = false) {
    g->Add(new wxStaticText(sc, wxID_ANY, W(label)), 0,
           wxALIGN_CENTER_VERTICAL);
    auto t = new wxTextCtrl(sc, wxID_ANY, value, wxDefaultPosition,
                            multiline ? wxSize(-1, 64) : wxDefaultSize,
                            multiline ? wxTE_MULTILINE : 0);
    g->Add(t, 1, wxEXPAND);
    return t;
  };
  auto check = [&](const char* label, bool v) {
    auto t = new wxCheckBox(sc, wxID_ANY, W(label));
    t->SetValue(v);
    g->Add(t, 0, wxALL, 2);
    g->AddSpacer(1);
    return t;
  };
  auto img = field("Image", W(image)), name = field("Container name"),
       hostname = field("Hostname"), workdir = field("Working directory"),
       user = field("User / UID"), network = field("Network"),
       restart = field("Restart policy"),
       memory = field("Memory limit (e.g. 512m)"),
       cpus = field("CPU limit (e.g. 1.5)"), entry = field("Entrypoint"),
       platform = field("Platform"),
       ports = field("Published ports (one per line)", "", true),
       env = field("Environment KEY=VALUE (not secrets)", "", true),
       vol = field("Volumes (source:target[:ro])", "", true),
       mount = field("Mount specifications", "", true),
       labels = field("Labels KEY=VALUE", "", true),
       devices = field("Devices", "", true),
       add = field("Capabilities to add", "", true),
       drop = field("Capabilities to drop", "", true),
       extra = field("Advanced run flags (one argv per line)", "", true),
       cmd = field("Command and arguments (one argv per line)", "", true);
  auto interactive = check("Keep STDIN open (-i)", true),
       tty = check("Allocate TTY (-t)", true),
       detach = check("Detached (-d)", false),
       rm = check("Remove on exit (--rm)", false),
       ro = check("Read-only filesystem", false),
       priv = check("Privileged (full host access)", false);
  sc->FitInside();
  outer->Add(
      new wxStaticText(&d, wxID_ANY,
                       "Advanced flags can configure GPUs, health checks, DNS, "
                       "security, resources and more.\nEach line is one "
                       "literal CLI argument; no shell expansion."),
      0, wxALL, 6);
  outer->Add(d.CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL,
             8);
  if (showModal(d) != wxID_OK) return false;
  out.image = S(img->GetValue());
  out.name = S(name->GetValue());
  out.hostname = S(hostname->GetValue());
  out.workdir = S(workdir->GetValue());
  out.user = S(user->GetValue());
  out.network = S(network->GetValue());
  out.restart = S(restart->GetValue());
  out.memory = S(memory->GetValue());
  out.cpus = S(cpus->GetValue());
  out.entrypoint = S(entry->GetValue());
  out.platform = S(platform->GetValue());
  out.ports = docker::lines(S(ports->GetValue()));
  out.env = docker::lines(S(env->GetValue()));
  out.volumes = docker::lines(S(vol->GetValue()));
  out.mounts = docker::lines(S(mount->GetValue()));
  out.labels = docker::lines(S(labels->GetValue()));
  out.devices = docker::lines(S(devices->GetValue()));
  out.capAdd = docker::lines(S(add->GetValue()));
  out.capDrop = docker::lines(S(drop->GetValue()));
  out.extra = docker::lines(S(extra->GetValue()));
  out.command = docker::lines(S(cmd->GetValue()));
  out.interactive = interactive->GetValue();
  out.tty = tty->GetValue();
  out.detach = detach->GetValue();
  out.remove = rm->GetValue();
  out.readOnly = ro->GetValue();
  out.privileged = priv->GetValue();
  return true;
}
void dockerPanel() {
  auto c = dockerConnection();
  wxDialog d(this, wxID_ANY, "Docker Manager", wxDefaultPosition,
             wxSize(1100, 720), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto s = new wxBoxSizer(wxVERTICAL);
  d.SetSizer(s);
  auto status =
      new wxStaticText(&d, wxID_ANY, "Refresh to query the Docker daemon.");
  s->Add(status, 0, wxALL | wxEXPAND, 6);
  auto tabs = new wxNotebook(&d, wxID_ANY);
  s->Add(tabs, 1, wxEXPAND | wxALL, 5);
  docker::Args pending;
  wxString title;
  auto tab = [&](const char* name, wxBoxSizer*& sz) {
    auto p = new wxPanel(tabs);
    sz = new wxBoxSizer(wxVERTICAL);
    p->SetSizer(sz);
    tabs->AddPage(p, W(name));
    return p;
  };
  auto row = [&](wxBoxSizer* sz) {
    auto r = new wxBoxSizer(wxHORIZONTAL);
    sz->Add(r, 0, wxEXPAND | wxALL, 2);
    return r;
  };
  auto ask = [&](const wxString& label, const wxString& initial = wxString(),
                 bool multi = false) -> std::optional<std::string> {
    wxTextEntryDialog q(&d, label, "Docker", initial,
                        wxOK | wxCANCEL | (multi ? wxTE_MULTILINE : 0));
    if (multi) q.SetSize(wxSize(650, 420));
    if (showModal(q) != wxID_OK) return {};
    return S(q.GetValue());
  };
  auto confirmDocker = [&](const wxString& action) {
    return wxMessageBox(action, "Docker operation",
                        wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, &d) == wxYES;
  };
  // Commands run only on explicit user action. Close the modal view before
  // switching to the PTY.
  auto launch = [&](const docker::Args& a, const wxString& name,
                    bool confirm = true) {
    if (confirm &&
        !confirmDocker("Execute Docker operation?\n" + name + "\nTarget: " +
                       W(!c.context.empty() ? c.context
                         : !c.host.empty()  ? c.host
                                            : "Current Docker CLI context")))
      return;
    pending = c.command(a);
    title = name;
    d.EndModal(wxID_OK);
  };
  auto textOut =
      new wxTextCtrl(&d, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 155),
                     wxTE_MULTILINE | wxTE_READONLY);
  s->Add(textOut, 0, wxEXPAND | wxALL, 5);
  auto inspect = [&](const docker::Args& a) {
    textOut->ChangeValue(W(dockerQuery(c, a)));
  };
  auto outputRow = row(s);
  button(&d, outputRow, "Export inspection output...", [&] {
    if (!confirmDocker(
            "Inspection output can contain environment values, host paths or "
            "credentials. Save it only to a trusted location."))
      return;
    wxFileDialog f(&d, "Export inspection output", P(root),
                   "docker-inspect.txt", "Text|*.txt|All|*",
                   wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (showModal(f) == wxID_OK)
      writeText(F(f.GetPath()), S(textOut->GetValue()));
  });
  auto choose = [&](wxListCtrl* l) {
    auto i = l->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (i < 0) throw std::runtime_error("Select a Docker item first");
    auto id = S(l->GetItemText(i));
    docker::validateOperand(id, "Docker ID");
    return id;
  };
  auto fill = [&](wxListCtrl* l, const docker::Args& a,
                  const std::vector<std::string>& keys) {
    auto items = docker::jsonLines(dockerQuery(c, a));
    l->DeleteAllItems();
    for (auto& obj : items) {
      auto get = [&](size_t i) {
        auto& v = obj[keys[i]];
        return W(v.isString() ? v.string() : v.dump(0));
      };
      auto n = l->InsertItem(l->GetItemCount(), get(0));
      for (size_t i = 1; i < keys.size(); ++i) l->SetItem(n, i, get(i));
    }
    status->SetLabel(wxString::Format("%zu items loaded", items.size()));
  };
  wxBoxSizer* z;
  auto p = tab("Images", z);
  auto images = list(p, {"ID", "Repository", "Tag", "Size", "Created"});
  z->Add(images, 1, wxEXPAND);
  auto imageRef = [&] {
    auto id = choose(images);
    auto i = images->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    auto repo = S(images->GetItemText(i, 1)),
         tag = S(images->GetItemText(i, 2));
    return repo.empty() || tag.empty() || repo == "<none>" || tag == "<none>"
               ? id
               : repo + ":" + tag;
  };
  auto refreshImages = [&] {
    fill(images, {"image", "ls", "--no-trunc", "--format", "{{json .}}"},
         {"ID", "Repository", "Tag", "Size", "CreatedSince"});
  };
  auto r = row(z);
  button(p, r, "Refresh", refreshImages);
  button(p, r, "Run image...", [&] {
    auto i = imageRef();
    docker::RunOptions o;
    if (dockerRunDialog(&d, i, o)) launch(docker::runArgs(o), "Docker run");
  });
  button(p, r, "Inspect",
         [&] { inspect({"image", "inspect", choose(images)}); });
  button(p, r, "Run other image...", [&] {
    docker::RunOptions o;
    if (dockerRunDialog(&d, "alpine:latest", o))
      launch(docker::runArgs(o), "Docker run");
  });
  button(p, r, "History",
         [&] { inspect({"image", "history", "--no-trunc", choose(images)}); });
  button(p, r, "Pull...", [&] {
    auto a = ask("Image reference", "alpine:latest");
    if (a) {
      docker::validateOperand(*a, "image");
      launch({"pull", *a}, "Docker pull");
    }
  });
  r = row(z);
  button(p, r, "Push...", [&] {
    auto a = ask("Tagged image reference");
    if (a) {
      docker::validateOperand(*a, "image");
      launch({"push", *a}, "Docker push");
    }
  });
  button(p, r, "Tag...", [&] {
    auto id = choose(images);
    auto a = ask("New repository:tag");
    if (a) {
      docker::validateOperand(*a, "tag");
      launch({"tag", id, *a}, "Docker tag");
    }
  });
  button(p, r, "Remove",
         [&] { launch({"image", "rm", imageRef()}, "Remove selected image"); });
  r = row(z);
  button(p, r, "Build Dockerfile...", [&] {
    wxFileDialog f(&d, "Select Dockerfile", P(root), "Dockerfile",
                   "All files|*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (showModal(f) != wxID_OK) return;
    wxDirDialog ctx(&d, "Build context", P(F(f.GetPath()).parent_path()));
    if (showModal(ctx) != wxID_OK) return;
    auto tag = ask("Image tag", "dodev-app:latest");
    if (!tag) return;
    docker::validateOperand(*tag, "tag");
    auto extra = ask("Additional build flags, one argument per line", "", true);
    if (!extra) return;
    docker::Args a{"build", "-f", S(f.GetPath()), "-t", *tag};
    auto flags = docker::lines(*extra);
    a.insert(a.end(), flags.begin(), flags.end());
    a.push_back(S(ctx.GetPath()));
    launch(a, "Docker build");
  });
  button(p, r, "Export Dockerfile...", [&] {
    auto id = choose(images);
    auto raw = dockerQuery(c, {"image", "inspect", id});
    Json j;
    std::string e;
    if (!Json::parse(raw, j, &e)) throw std::runtime_error(e);
    auto content = docker::derivedDockerfile(j, imageRef());
    if (!confirmDocker(
            "Export a derived Dockerfile? Original build instructions cannot "
            "be recovered. Untagged images need a local tag."))
      return;
    wxFileDialog f(&d, "Export derived Dockerfile", P(root), "Dockerfile",
                   "All files|*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (showModal(f) == wxID_OK) writeText(F(f.GetPath()), content);
  });
  r = row(z);
  button(p, r, "Save image tar...", [&] {
    auto id = imageRef();
    wxFileDialog f(&d, "Save image archive", P(root), "image.tar",
                   "Tar archive|*.tar", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (showModal(f) == wxID_OK)
      launch({"image", "save", "-o", S(f.GetPath()), id}, "Docker image save");
  });
  button(p, r, "Load image tar...", [&] {
    wxFileDialog f(&d, "Load Docker image archive", P(root), "", "All files|*",
                   wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (showModal(f) == wxID_OK)
      launch({"image", "load", "-i", S(f.GetPath())}, "Docker image load");
  });
  button(p, r, "Import rootfs...", [&] {
    wxFileDialog f(&d, "Import root filesystem tar", P(root), "", "All files|*",
                   wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (showModal(f) != wxID_OK) return;
    auto tag = ask("Image tag", "imported:latest");
    if (tag) {
      docker::validateOperand(*tag, "tag");
      launch({"image", "import", S(f.GetPath()), *tag}, "Docker import");
    }
  });
  p = tab("Containers", z);
  auto containers = list(p, {"ID", "Name", "Image", "Status", "Ports"});
  z->Add(containers, 1, wxEXPAND);
  auto refreshContainers = [&] {
    fill(containers,
         {"container", "ls", "-a", "--no-trunc", "--format", "{{json .}}"},
         {"ID", "Names", "Image", "Status", "Ports"});
  };
  r = row(z);
  button(p, r, "Refresh", refreshContainers);
  for (auto action :
       {"start", "stop", "restart", "pause", "unpause", "kill", "rm"})
    button(p, r, W(action), [&, action] {
      launch({"container", action, choose(containers)},
             W(std::string("Docker container ") + action));
    });
  button(p, r, "Inspect",
         [&] { inspect({"container", "inspect", choose(containers)}); });
  button(p, r, "Top",
         [&] { inspect({"container", "top", choose(containers)}); });
  r = row(z);
  button(p, r, "Exec interactive...", [&] {
    auto id = choose(containers);
    auto cmd = ask("Executable and arguments, one per line", "/bin/sh", true);
    if (!cmd) return;
    auto a = docker::Args{"exec", "-it", id};
    auto values = docker::lines(*cmd);
    if (values.empty()) throw std::runtime_error("Enter an executable");
    a.insert(a.end(), values.begin(), values.end());
    launch(a, "Docker exec");
  });
  button(p, r, "Start attached", [&] {
    launch({"start", "-ai", choose(containers)}, "Docker start -ai");
  });
  button(p, r, "Attach", [&] {
    launch({"attach", "--sig-proxy=false", choose(containers)},
           "Docker attach (detach: Ctrl+P, Ctrl+Q)");
  });
  button(p, r, "Logs -f", [&] {
    launch({"logs", "--tail", "200", "-f", choose(containers)}, "Docker logs",
           false);
  });
  button(p, r, "Stats",
         [&] { launch({"stats", choose(containers)}, "Docker stats", false); });
  r = row(z);
  button(p, r, "Update...", [&] {
    auto id = choose(containers);
    auto f = ask("docker update flags, one argument per line", "--memory=512m",
                 true);
    if (!f) return;
    auto a = docker::Args{"update"};
    auto v = docker::lines(*f);
    a.insert(a.end(), v.begin(), v.end());
    a.push_back(id);
    launch(a, "Docker update");
  });
  button(p, r, "Commit...", [&] {
    auto id = choose(containers);
    auto tag = ask("New image tag");
    if (tag) {
      docker::validateOperand(*tag, "tag");
      launch({"commit", id, *tag}, "Docker commit");
    }
  });
  button(p, r, "Export rootfs...", [&] {
    auto id = choose(containers);
    wxFileDialog f(&d, "Export container filesystem", P(root), "container.tar",
                   "Tar archive|*.tar", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (showModal(f) == wxID_OK)
      launch({"container", "export", "-o", S(f.GetPath()), id},
             "Docker export");
  });
  button(p, r, "Copy files...", [&] {
    auto id = choose(containers);
    auto from = ask("Source path (use container:path for remote)",
                    W(id + ":/tmp/file"));
    if (!from) return;
    auto to = ask("Destination path", P(root));
    if (to) {
      if (from->empty() || to->empty() || from->front() == '-' ||
          to->front() == '-')
        throw std::runtime_error("Invalid copy path");
      launch({"cp", *from, *to}, "Docker cp");
    }
  });
  p = tab("Networks & Volumes", z);
  auto networks = list(p, {"ID", "Name", "Driver", "Scope"});
  auto volumes = list(p, {"Name", "Driver", "Scope"});
  z->Add(networks, 1, wxEXPAND);
  r = row(z);
  button(p, r, "Refresh networks", [&] {
    fill(networks, {"network", "ls", "--no-trunc", "--format", "{{json .}}"},
         {"ID", "Name", "Driver", "Scope"});
  });
  button(p, r, "Inspect",
         [&] { inspect({"network", "inspect", choose(networks)}); });
  button(p, r, "Create...", [&] {
    auto f = ask("network create arguments, one per line", "my-network", true);
    if (f) {
      auto a = docker::Args{"network", "create"};
      auto v = docker::lines(*f);
      a.insert(a.end(), v.begin(), v.end());
      launch(a, "Docker network create");
    }
  });
  button(p, r, "Remove", [&] {
    launch({"network", "rm", choose(networks)}, "Remove network");
  });
  for (auto action : {"connect", "disconnect"})
    button(p, r, W(action), [&, action] {
      auto id = choose(networks);
      auto ct = ask("Container name or ID");
      if (ct) {
        docker::validateOperand(*ct, "container");
        launch({"network", action, id, *ct}, W(action));
      }
    });
  z->Add(volumes, 1, wxEXPAND);
  r = row(z);
  button(p, r, "Refresh volumes", [&] {
    fill(volumes, {"volume", "ls", "--format", "{{json .}}"},
         {"Name", "Driver", "Scope"});
  });
  button(p, r, "Inspect",
         [&] { inspect({"volume", "inspect", choose(volumes)}); });
  button(p, r, "Create...", [&] {
    auto f = ask("volume create arguments, one per line", "my-volume", true);
    if (f) {
      auto a = docker::Args{"volume", "create"};
      auto v = docker::lines(*f);
      a.insert(a.end(), v.begin(), v.end());
      launch(a, "Docker volume create");
    }
  });
  button(p, r, "Remove",
         [&] { launch({"volume", "rm", choose(volumes)}, "Remove volume"); });
  p = tab("Compose / CLI", z);
  auto compose = new wxTextCtrl(p, wxID_ANY, P(root / "compose.yaml"));
  z->Add(compose, 0, wxEXPAND | wxALL, 4);
  r = row(z);
  button(p, r, "Select Compose file", [&] {
    wxFileDialog f(&d, "Compose file", P(root), "", "YAML|*.yaml;*.yml|All|*",
                   wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (showModal(f) == wxID_OK) compose->SetValue(f.GetPath());
  });
  button(p, r, "Edit Compose", [&] {
    open(F(compose->GetValue()));
    d.EndModal(wxID_CANCEL);
  });
  for (auto action : {"up", "down", "build", "ps", "logs", "config", "restart"})
    button(p, r, W(action), [&, action] {
      auto a = docker::Args{"compose", "-f", S(compose->GetValue()), action};
      if (std::string(action) == "up") a.push_back("-d");
      if (std::string(action) == "logs") a.push_back("-f");
      launch(a, W(std::string("Docker compose ") + action));
    });
  z->Add(new wxStaticText(
             p, wxID_ANY,
             "Any Docker CLI operation: one literal argument per line (omit "
             "docker).\nSupports buildx, system prune, context, login, swarm, "
             "secret, plugin and other installed commands."),
         0, wxALL, 6);
  auto cli = new wxTextCtrl(p, wxID_ANY, "system\ndf", wxDefaultPosition,
                            wxDefaultSize, wxTE_MULTILINE);
  z->Add(cli, 1, wxEXPAND | wxALL, 4);
  button(p, z, "Run CLI in terminal...", [&] {
    auto a = docker::lines(S(cli->GetValue()));
    if (a.empty()) throw std::runtime_error("Enter Docker arguments");
    launch(a, "Docker CLI");
  });
  p = tab("Configuration", z);
  auto g = new wxFlexGridSizer(2, 6, 6);
  g->AddGrowableCol(1);
  z->Add(g, 0, wxEXPAND | wxALL, 8);
  auto field = [&](const char* label, const std::string& val,
                   bool multi = false) {
    g->Add(new wxStaticText(p, wxID_ANY, W(label)), 0, wxALIGN_CENTER_VERTICAL);
    auto t = new wxTextCtrl(p, wxID_ANY, W(val), wxDefaultPosition,
                            multi ? wxSize(-1, 90) : wxDefaultSize,
                            multi ? wxTE_MULTILINE : 0);
    g->Add(t, 1, wxEXPAND);
    return t;
  };
  auto exe = field("Docker executable", c.executable),
       context = field("Docker context", c.context),
       host = field("Daemon host URL", c.host),
       config = field("CLI config directory", c.config),
       flags = field("Global CLI flags (one per line)",
                     plugins.pluginSetting("com.dodev.docker", "globalFlags"),
                     true);
  button(p, z, "Apply connection settings", [&] {
    docker::Connection v;
    v.executable = S(exe->GetValue());
    v.context = S(context->GetValue());
    v.host = S(host->GetValue());
    v.config = S(config->GetValue());
    v.flags = docker::lines(S(flags->GetValue()));
    v.command({"version"});
    for (auto& kv : std::vector<std::pair<std::string, std::string>>{
             {"executable", v.executable},
             {"context", v.context},
             {"host", v.host},
             {"configDirectory", v.config},
             {"globalFlags", S(flags->GetValue())}})
      if (!plugins.updatePluginSetting("com.dodev.docker", kv.first, kv.second))
        throw std::runtime_error("Could not save Docker settings");
    c = v;
    images->DeleteAllItems();
    containers->DeleteAllItems();
    networks->DeleteAllItems();
    volumes->DeleteAllItems();
    textOut->Clear();
    status->SetLabel("Connection changed — refresh lists before operating");
  });
  r = row(z);
  button(p, r, "Version", [&] { inspect({"version"}); });
  button(p, r, "Daemon info", [&] { inspect({"info"}); });
  button(p, r, "Contexts", [&] { inspect({"context", "ls"}); });
  button(p, r, "Disk usage", [&] { inspect({"system", "df"}); });
  button(p, r, "Open daemon.json...", [&] {
    wxFileDialog f(
        &d, "Select daemon configuration (editing does not restart Docker)",
        "/etc/docker", "daemon.json", "JSON|*.json|All|*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (showModal(f) == wxID_OK) {
      open(F(f.GetPath()));
      d.EndModal(wxID_CANCEL);
    }
  });
  z->Add(new wxStaticText(
             p, wxID_ANY,
             "Use SSH/TLS Docker contexts for remote daemons. Docker access "
             "can grant host-level privileges.\nNo sudo, daemon restart, or "
             "destructive operation is performed automatically."),
         0, wxALL, 8);
  s->Add(d.CreateStdDialogButtonSizer(wxCANCEL), 0, wxEXPAND | wxALL, 6);
  palette.apply(&d);
  if (showModal(d) == wxID_OK && !pending.empty())
    dockerTerminal(pending, title);
}
