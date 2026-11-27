#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "dodev/plugins/PluginManager.h"
namespace fs = std::filesystem;
using namespace dodev;
void check(bool v, const char* msg) {
  if (!v) throw std::runtime_error(msg);
}
struct Capture {
  const DodevHostAPI* host = nullptr;
  DodevFormFn fn = nullptr;
  void* data = nullptr;
  size_t fields = 0, rows = 0;
  DodevRowFn selected = nullptr;
  std::string row;
};
int main() {
  auto root =
      fs::temp_directory_path() /
      ("httpplugin-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root / "plugins" / "http-client");
  try {
    fs::copy(
        fs::path(HTTP_PLUGIN_DIR), root / "plugins" / "http-client",
        fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    SettingsManager settings(root / "config");
    settings.load();
    TemplateRegistry templates;
    ThemeManager themes;
    PluginManager manager(settings, templates, themes);
    std::vector<std::string> ids, reports;
    PluginManager::HostHooks hooks;
    hooks.debugContext = [&](std::string& w, std::string& f, int& l) {
      w = root.u8string();
      f = "";
      l = 1;
      return true;
    };
    hooks.runJob = [](const std::string&, DodevWorkFn fn, void* d) {
      fn(d, [](void*) { return 0; }, nullptr);
      return true;
    };
    hooks.showTextView = [&](const std::string& id, const std::string&,
                             const std::string& side, const std::string& text) {
      check(side == "Editor", "response location");
      ids.push_back(id);
      reports.push_back(text);
      return true;
    };
    manager.setHostHooks(std::move(hooks));
    manager.setEnabled("com.dodev.httpclient", true);
    check(manager.discoverAndLoad(root / "plugins") == 1, "load native plugin");
    check(manager.settingSpecs().size() == 4, "settings owned by plugin");
    check(manager.views().size() == 2 &&
              isWorkspacePluginView(manager.views()[0]) &&
              isWorkspacePluginView(manager.views()[1]),
          "workspace placement");
    Capture c;
    DodevUiBuilder b{};
    b.struct_size = sizeof(b);
    b.context = &c;
    b.label = [](void*, const char*) {};
    b.button = [](void*, const char*, const char*) {};
    b.form = [](void* x, const DodevFormField*, size_t n, const char*,
                const DodevHostAPI* h, DodevFormFn f, void* d) {
      auto& c = *static_cast<Capture*>(x);
      c.host = h;
      c.fn = f;
      c.data = d;
      c.fields = n;
    };
    b.table = [](void* x, const char* const*, size_t, const char* const*,
                 const char* const* ids, size_t n, const DodevHostAPI* h,
                 DodevRowFn fn, void* d) {
      auto& c = *static_cast<Capture*>(x);
      c.rows = n;
      c.host = h;
      c.selected = fn;
      c.data = d;
      if (n) c.row = ids[0];
    };
    manager.buildView(manager.views()[0], b);
    check(c.fields == 4 && c.fn, "form registered");
    const char* request[] = {"GET", "file:///test", "", ""};
    c.fn(c.host, request, 4, c.data);
    c.fn(c.host, request, 4, c.data);
    check(ids.size() == 2 && ids[0] != ids[1], "unique editor tab per request");
    check(reports[0].find("REQUEST") != std::string::npos &&
              reports[0].find("RESPONSE") != std::string::npos,
          "both sections");
    manager.buildView(manager.views()[1], b);
    check(c.rows == 2 && c.selected, "history rows");
    c.selected(c.host, c.row.c_str(), c.data);
    check(ids.size() == 3 &&
              reports.back().find("Only metadata") != std::string::npos,
          "history opens editor tab");
    manager.shutdown();
    fs::remove_all(root);
    std::cout << "HTTP SDK integration passed\n";
    return 0;
  } catch (const std::exception& e) {
    fs::remove_all(root);
    std::cerr << e.what() << '\n';
    return 1;
  }
}
