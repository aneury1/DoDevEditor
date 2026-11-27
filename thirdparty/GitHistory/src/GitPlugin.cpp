#include <stdexcept>

#include "GitHistory.h"
#include "dodev/sdk/PluginSettings.h"
namespace {
std::string get(const DodevHostAPI* h, const char* key) {
  const char* v = h->get_setting(h->context, key);
  return v ? v : "";
}
std::filesystem::path workspace(const DodevHostAPI* h) {
  const char *w = nullptr, *f = nullptr;
  int line = 0;
  if (!DODEV_HOST_HAS(h, debug_context) ||
      !h->debug_context(h->context, &w, &f, &line) || !w || !*w)
    throw std::runtime_error("Open a workspace first");
  return std::filesystem::u8path(w);
}
void selected(const DodevHostAPI* h, const char* id, void*) {
  try {
    const auto repo = workspace(h);
    auto text =
        "Workspace: " + repo.u8string() + "\n\n" +
        dodev::git::details(repo, id ? id : "", get(h, "showPatch") == "true",
                            get(h, "gitExecutable"));
    h->show_text_view(h->context, "commit.details", "Git commit details",
                      "Output", text.c_str());
  } catch (const std::exception& e) {
    h->show_text_view(h->context, "commit.details", "Git commit details",
                      "Output", e.what());
  }
}
void view(const DodevHostAPI* h, const DodevUiBuilder* ui, void*) {
  ui->label(ui->context,
            "Git history — select a commit to inspect its complete metadata "
            "and changed files in Output.");
  ui->button(ui->context, "Refresh history", "plugin.reload.git.history");
  if (ui->struct_size < offsetof(DodevUiBuilder, table) + sizeof(ui->table) ||
      !ui->table) {
    ui->label(ui->context, "This host does not support table views.");
    return;
  }
  try {
    int limit = 100;
    try {
      limit = std::stoi(get(h, "historyLimit"));
    } catch (...) {
    }
    auto rows =
        dodev::git::history(workspace(h), limit, get(h, "gitExecutable"));
    const char* cols[] = {"Message", "Commit hash", "Committer", "Commit date"};
    std::vector<const char*> cells, ids;
    for (auto& row : rows) {
      ids.push_back(row.hash.c_str());
      cells.push_back(row.message.c_str());
      cells.push_back(row.hash.c_str());
      cells.push_back(row.committer.c_str());
      cells.push_back(row.date.c_str());
    }
    ui->table(ui->context, cols, 4, cells.data(), ids.data(), rows.size(), h,
              &selected, nullptr);
  } catch (const std::exception& e) {
    ui->read_only_text(ui->context, e.what());
  }
}
}  // namespace
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI* h,
                                                     DodevPluginAPI* p) {
  if (!h || !p || h->api_version != DODEV_PLUGIN_API_VERSION ||
      !DODEV_HOST_HAS(h, show_text_view))
    return 0;
  if (!dodev::sdk::setting(h, "gitExecutable", "git", "Git executable path",
                           R"({"label":"Git executable"})"))
    return 0;
  if (!dodev::sdk::setting(h, "historyLimit", "100",
                           "Maximum commits per history load",
                           R"({"type":"int","min":1,"max":1000})"))
    return 0;
  if (!dodev::sdk::setting(h, "showPatch", "true",
                           "Include patch text in selected commit details",
                           R"({"type":"bool"})"))
    return 0;
  if (!h->register_panel(h->context, "git.history", "Git", "Workspace", &view,
                         nullptr))
    return 0;
  h->register_menu(h->context, "Options", "Git: Commit history",
                   "plugin.view.git.history");
  p->api_version = DODEV_PLUGIN_API_VERSION;
  p->shutdown = nullptr;
  return 1;
}
