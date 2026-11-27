#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>

#include "GitHistory.h"
#include "JournalTable.h"
#include "SourceSymbols.h"
#include "dodev/docker/DockerQuery.h"
#include "dodev/plugins/PluginManager.h"
#include "dodev/sdk/WorkspaceProfiles.h"
namespace fs = std::filesystem;
int main() {
  auto root =
      fs::temp_directory_path() /
      ("dodev-plugin-profiles-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root);
  struct Cleanup {
    fs::path p;
    ~Cleanup() {
      std::error_code e;
      fs::remove_all(p, e);
    }
  } cleanup{root};
  dodev::SettingsManager settings(root / "config");
  settings.load();
  dodev::TemplateRegistry templates;
  dodev::ThemeManager themes;
  dodev::PluginManager manager(settings, templates, themes);
  manager.setEnabled("com.dodev.serialmonitor", true);
  manager.setEnabled("com.dodev.remotejournal", true);
  manager.setEnabled("com.dodev.sourcesymbols", true);
  manager.setEnabled("com.dodev.githistory", true);
  std::string owner;
  dodev::PluginManager::HostHooks hooks;
  hooks.openSerialMonitor = [] { return true; };
  hooks.openRemoteJournal = [] { return true; };
  hooks.openSourceSymbols = [] { return true; };
  hooks.openWorkspaceConfig = [&](const std::string& id,
                                  const std::string& text) {
    owner = id;
    auto file = dodev::sdk::workspaceProfilePath(root, id);
    fs::create_directories(file.parent_path());
    std::ofstream(file) << text;
    return true;
  };
  hooks.debugContext = [&](std::string& w, std::string& f, int& line) {
    w = root.u8string();
    f = "";
    line = 1;
    return true;
  };
  std::string details;
  std::vector<std::string> displayed;
  hooks.showTextView = [&](const std::string& id, const std::string&,
                           const std::string& side, const std::string& text) {
    assert(id == "com.dodev.githistory:commit.details" && side == "Output");
    details = text;
    displayed.push_back(text);
    return true;
  };
  manager.setHostHooks(std::move(hooks));
  assert(manager.discoverAndLoad(DODEV_PLUGIN_ROOT) == 4);
  assert(manager.execute("serialmonitor.workspace.configure"));
  assert(owner == "com.dodev.serialmonitor");
  auto ports = dodev::sdk::workspaceProfiles(root, owner);
  assert(ports.size() == 2 && ports[0]["port"].string() == "/dev/ttyUSB0");
  assert(manager.execute("remotejournal.workspace.configure"));
  assert(owner == "com.dodev.remotejournal");
  assert(dodev::sdk::workspaceProfiles(root, owner).size() == 1);
  assert(manager.updatePluginSetting("com.dodev.serialmonitor",
                                     "captureAsAdministrator", "true"));
  // The Journal workspace tab supplies editable profile defaults through the
  // generic SDK.
  struct ProfileView {
    bool config = false, connect = false;
  };
  ProfileView pv;
  DodevUiBuilder profilesBuilder{};
  profilesBuilder.struct_size = sizeof(profilesBuilder);
  profilesBuilder.context = &pv;
  profilesBuilder.label = [](void*, const char*) {};
  profilesBuilder.button = [](void* data, const char*, const char* command) {
    if (std::string(command) == "remotejournal.open")
      static_cast<ProfileView*>(data)->connect = true;
  };
  profilesBuilder.workspace_profiles = [](void* data, const char* defaults) {
    auto j = dodev::sdk::parseWorkspaceProfiles(defaults);
    assert(j["profiles"].asArray().size() == 1);
    static_cast<ProfileView*>(data)->config = true;
  };
  for (auto& v : manager.views())
    if (v.id == "remotejournal.workspace") {
      assert(v.title == "Journal Logs" && dodev::isWorkspacePluginView(v));
      manager.buildView(v, profilesBuilder);
    }
  assert(pv.config && pv.connect);
  bool filterView = false;
  for (auto& v : manager.views())
    if (v.id == "remotejournal.filters") {
      assert(v.title == "Journal Filters" && dodev::isWorkspacePluginView(v));
      manager.buildView(v, profilesBuilder);
      filterView = true;
    }
  assert(filterView);
  for (auto bad : {"{}", R"({"profiles":[{}]})", "not JSON"}) {
    bool rejected = false;
    try {
      dodev::sdk::parseWorkspaceProfiles(bad);
    } catch (...) {
      rejected = true;
    }
    assert(rejected);
  }

  auto rec = dodev::journal::parseRecord(
      1,
      R"({"MESSAGE":"timeout 42","APP_ID":"PAYM","SYSLOG_IDENTIFIER":"server"})");
  assert(rec.applicationId == "PAYM");
  dodev::journal::TableFilter f;
  f.applicationId = "paym";
  f.payloadRegex.emplace("timeout [0-9]+");
  assert(f.accepts(rec));
  f.applicationId = "server";
  assert(!f.accepts(rec));
  f.applicationId = "PAYM";
  f.payloadRegex.emplace("^PAYM$");
  assert(!f.accepts(rec));
  dodev::symbols::Options o;
  o.mode = dodev::symbols::ParserMode::RegexOnly;
  o.captureKinds = "function";
  auto symbols = dodev::symbols::extract(
      "main.cpp", "class A {};\nint fn(){return 0;}\n", o);
  assert(!symbols.rows.empty());
  for (auto& r : symbols.rows) assert(r.kind == "function");
  assert(!dodev::symbols::capturesKind("class, method", "function"));
  // Actual local repository: no network or shell. Skip only when git itself is
  // unavailable.
  try {
    dodev::docker::capture({"git", "--version"}, root);
  } catch (...) {
    std::cout << "Git unavailable: registry and profile checks passed\n";
    return 0;
  }
  auto run = [&](std::vector<std::string> args) {
    args.insert(args.begin(), "git");
    return dodev::docker::capture(args, root);
  };
  run({"init", "-q"});
  run({"config", "user.name", "Test Committer"});
  run({"config", "user.email", "tester@example.invalid"});
  std::ofstream(root / "example.txt") << "first";
  run({"add", "example.txt"});
  run({"commit", "-q", "-m", "First commit", "-m", "First body"});
  std::ofstream(root / "second.txt") << "second";
  run({"add", "second.txt"});
  run({"commit", "-q", "-m", "Second commit", "-m", "Second body"});
  auto rows = dodev::git::history(root, 10);
  assert(rows.size() == 2 &&
         rows[0].committer.find("Test Committer") != std::string::npos);
  assert(rows[0].message == "Second commit");
  assert(dodev::git::details(root, rows[1].hash).find("example.txt") !=
         std::string::npos);
  assert(!dodev::git::validHash("--help"));
  struct State {
    int rows = 0;
  };
  State state;
  DodevUiBuilder builder{};
  builder.struct_size = sizeof(builder);
  builder.context = &state;
  builder.label = [](void*, const char*) {};
  builder.button = [](void*, const char*, const char*) {};
  builder.read_only_text = [](void*, const char*) {};
  builder.table = [](void* data, const char* const* names, size_t columns,
                     const char* const* cells, const char* const* ids,
                     size_t count, const DodevHostAPI* host, DodevRowFn select,
                     void* arg) {
    assert(columns == 4 && count == 2);
    assert(std::string(names[0]) == "Message" &&
           std::string(cells[0]) == "Second commit");
    static_cast<State*>(data)->rows = static_cast<int>(count);
    select(host, ids[0], arg);
    select(host, ids[1], arg);
  };
  for (auto& v : manager.views())
    if (v.id == "git.history") {
      assert(dodev::pluginViewLocation(v) ==
             dodev::PluginViewLocation::Sidebar);
      manager.buildView(v, builder);
    }
  assert(displayed.size() == 2 &&
         displayed[0].find("Second body") != std::string::npos &&
         displayed[0].find("second.txt") != std::string::npos &&
         displayed[1].find("First body") != std::string::npos);
  assert(state.rows == 2 && details.find("First commit") != std::string::npos &&
         details.find("First body") != std::string::npos &&
         details.find("example.txt") != std::string::npos &&
         details.find("Committer:") != std::string::npos &&
         details.find("Parents:") != std::string::npos &&
         details.find("=== Patch ===") != std::string::npos &&
         details.find("Second body") == std::string::npos);
  std::cout << "Workspace profiles, journal filters, symbol kinds and native "
               "Git history passed\n";
}
