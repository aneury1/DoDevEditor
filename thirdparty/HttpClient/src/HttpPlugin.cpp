#include <atomic>
#include <chrono>
#include <stdexcept>

#include "HttpClient.h"
#include "dodev/sdk/PluginSettings.h"
namespace {
using namespace dodev::http;
std::string setting(const DodevHostAPI* h, const char* key) {
  auto v = h->get_setting(h->context, key);
  return v ? v : "";
}
int integer(const DodevHostAPI* h, const char* key, int fallback) {
  try {
    return std::stoi(setting(h, key));
  } catch (...) {
    return fallback;
  }
}
std::filesystem::path database(const DodevHostAPI* h) {
  const char *w = nullptr, *f = nullptr;
  int line = 0;
  if (!DODEV_HOST_HAS(h, debug_context) ||
      !h->debug_context(h->context, &w, &f, &line) || !w || !*w)
    throw std::runtime_error("Open a workspace first");
  return std::filesystem::u8path(w) / ".dodev" / "plugins" /
         "com.dodev.httpclient" / "history.sqlite";
}
// Unique tool tab ID per execution, even for repeated requests to the same URL.
std::string nextId() {
  static std::atomic<unsigned long long> counter{0};
  return "request." +
         std::to_string(
             std::chrono::system_clock::now().time_since_epoch().count()) +
         "." + std::to_string(++counter);
}
void show(const DodevHostAPI* h, const std::string& id,
          const std::string& title, const std::string& text) {
  if (!h->show_text_view(h->context, id.c_str(), title.c_str(), "Editor",
                         text.c_str()))
    throw std::runtime_error("Host could not open editor tab");
}
struct Job {
  Request req;
  Response res;
};
// Worker callback has no access to the editor or its widgets.
void work(void* data, DodevCancelledFn cancel, void* context) {
  auto& j = *static_cast<Job*>(data);
  j.res = send(j.req, cancel, context);
}
void submit(const DodevHostAPI* h, const char* const* values, size_t count,
            void*) {
  if (count != 4) return;
  auto id = nextId();
  Job job;
  job.req = {values[0], values[1], values[2], values[3],
             integer(h, "timeoutSeconds", 30)};
  auto title =
      "HTTP: " + job.req.method + " #" + id.substr(id.find_last_of('.') + 1);
  // Capture workspace and settings before the modal background job.
  std::filesystem::path path;
  std::string historyError;
  try {
    path = database(h);
  } catch (const std::exception& e) {
    historyError = e.what();
  }
  auto mode = setting(h, "historyMode");
  auto limit = integer(h, "historyLimit", 100);
  if (!h->run_job(h->context, "HTTP request", &work, &job))
    job.res.error = "Host rejected request execution";
  // A failure or cancellation still gets a separate editor tab.
  if (mode != "off" && !path.empty()) try {
      remember(path, job.req, job.res, mode == "full", limit);
    } catch (const std::exception& e) {
      historyError = e.what();
    }
  auto text = report(job.req, job.res);
  if (!historyError.empty()) text += "\n\nHistory: " + historyError;
  show(h, id, title, text);
}
void builder(const DodevHostAPI* h, const DodevUiBuilder* ui, void*) {
  ui->label(ui->context,
            "HTTP Client — configure a request. Each Send opens a new editor "
            "tab with request and response.");
  if (ui->struct_size < offsetof(DodevUiBuilder, form) + sizeof(ui->form) ||
      !ui->form) {
    ui->label(ui->context, "This host does not support plugin forms.");
    return;
  }
  auto url = setting(h, "defaultUrl");
  DodevFormField fields[] = {
      {"Method (GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS or custom)", "GET",
       0},
      {"HTTP(S) URL", url.c_str(), 0},
      {"Headers — one Name: Value per line", "Accept: application/json", 1},
      {"Request body (text / JSON / XML)", "", 1}};
  ui->form(ui->context, fields, 4, "Send request", h, &submit, nullptr);
  ui->button(ui->context, "Request history",
             "plugin.reload.httpclient.history");
  ui->label(ui->context,
            "TLS verification is required. Redirects are not followed. "
            "Configure history retention in Settings > HTTP Client.");
}
void select(const DodevHostAPI* h, const char* key, void*) {
  try {
    auto e = lookup(database(h), std::stoll(key));
    std::string text =
        "HISTORY\nUTC: " + e.date + "\nMethod: " + e.method +
        "\nURL: " + e.url + "\nStatus: " + e.status + "\n\n" +
        (e.detail.empty()
             ? "Only metadata was stored for this request. Enable full history "
               "to retain future request and response details."
             : e.detail);
    show(h, nextId(), "HTTP history #" + std::to_string(e.id), text);
  } catch (const std::exception& e) {
    show(h, nextId(), "HTTP history error", e.what());
  }
}
void historyView(const DodevHostAPI* h, const DodevUiBuilder* ui, void*) {
  ui->label(ui->context,
            "Request history — this workspace. Select a row to inspect it in a "
            "new editor tab.");
  ui->button(ui->context, "Refresh history",
             "plugin.reload.httpclient.history");
  ui->button(ui->context, "Clear history...", "httpclient.clear");
  if (ui->struct_size < offsetof(DodevUiBuilder, table) + sizeof(ui->table) ||
      !ui->table)
    return;
  try {
    auto rows = history(database(h));
    const char* columns[] = {"Method", "URL / origin", "Status", "UTC"};
    std::vector<std::string> keys;
    std::vector<const char*> ids, cells;
    keys.reserve(rows.size());
    for (auto& r : rows) keys.push_back(std::to_string(r.id));
    for (size_t i = 0; i < rows.size(); ++i) {
      auto& r = rows[i];
      ids.push_back(keys[i].c_str());
      cells.insert(cells.end(), {r.method.c_str(), r.url.c_str(),
                                 r.status.c_str(), r.date.c_str()});
    }
    ui->table(ui->context, columns, 4, cells.data(), ids.data(), rows.size(), h,
              &select, nullptr);
  } catch (const std::exception& e) {
    ui->label(ui->context, e.what());
  }
}
void confirmed(const DodevHostAPI* h, const char* value, void*) {
  if (std::string(value ? value : "") != "CLEAR") return;
  try {
    clear(database(h));
    show(h, nextId(), "HTTP history",
         "Workspace HTTP history cleared. Refresh the history table.");
  } catch (const std::exception& e) {
    show(h, nextId(), "HTTP history error", e.what());
  }
}
void clearCommand(const DodevHostAPI* h, void*) {
  h->prompt_input(
      h->context, "Clear HTTP history",
      "Type CLEAR to delete this workspace's stored request history.", "",
      &confirmed, nullptr);
}
}  // namespace
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI* h,
                                                     DodevPluginAPI* p) {
  if (!h || !p || h->api_version != DODEV_PLUGIN_API_VERSION ||
      !DODEV_HOST_HAS(h, run_job) || !DODEV_HOST_HAS(h, show_text_view))
    return 0;
  if (!dodev::sdk::setting(h, "defaultUrl", "http://localhost:8080/",
                           "Initial URL for a new request form",
                           R"JSON({"label":"Default URL"})JSON"))
    return 0;
  if (!dodev::sdk::setting(
          h, "timeoutSeconds", "30", "Total request timeout in seconds",
          R"JSON({"label":"Timeout (seconds)","type":"int","min":1,"max":300})JSON"))
    return 0;
  if (!dodev::sdk::setting(
          h, "historyLimit", "100", "Maximum saved requests per workspace",
          R"JSON({"label":"History limit","type":"int","min":1,"max":1000})JSON"))
    return 0;
  if (!dodev::sdk::setting(
          h, "historyMode", "metadata",
          "Full mode stores URLs, headers and bodies unencrypted, including "
          "credentials. Metadata stores only origin, method, status and time.",
          R"JSON({"label":"History storage","type":"choice","choices":[{"value":"metadata","label":"Metadata only (recommended)"},{"value":"full","label":"Full request and response (unencrypted)"},{"value":"off","label":"Disabled"}]})JSON"))
    return 0;
  if (!h->register_command(h->context, "httpclient.clear",
                           "Clear request history", &clearCommand, nullptr) ||
      !h->register_panel(h->context, "httpclient.builder", "HTTP Client",
                         "Workspace", &builder, nullptr) ||
      !h->register_panel(h->context, "httpclient.history", "HTTP History",
                         "Workspace", &historyView, nullptr))
    return 0;
  h->register_menu(h->context, "Options", "HTTP Client: New request",
                   "plugin.view.httpclient.builder");
  h->register_menu(h->context, "Options", "HTTP Client: History",
                   "plugin.reload.httpclient.history");
  p->api_version = DODEV_PLUGIN_API_VERSION;
  p->shutdown = nullptr;
  return 1;
}
