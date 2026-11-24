#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "SecureFolder.h"
#include "dodev/sdk/dodev_plugin_api.h"
namespace {
namespace fs = std::filesystem;
struct Job {
  bool decrypt = false;
  std::string source, output;
} job;
void log(const DodevHostAPI* h, int level, const std::string& s) {
  if (h->log) h->log(h->context, level, s.c_str());
}
void secret(const DodevHostAPI* h, const char* v, void*) {
  if (!v || !*v) return;
  try {
    auto a = fs::u8path(job.source), b = fs::u8path(job.output),
         c = fs::u8path(v);
    if (job.decrypt)
      secureqr::decrypt(a, b, c);
    else
      secureqr::encrypt(a, b, c);
    log(h, 1, "Secure Folder QR completed: " + job.output);
  } catch (const std::exception& e) {
    log(h, 2, std::string("Secure Folder QR: ") + e.what());
  }
}
void output(const DodevHostAPI* h, const char* v, void*) {
  if (!v || !*v) return;
  job.output = v;
  h->prompt_input(h->context, "Secure Folder QR",
                  "Path to secret file (not the password itself):", "", &secret,
                  nullptr);
}
void source(const DodevHostAPI* h, const char* v, void*) {
  if (!v || !*v) return;
  job.source = v;
  h->prompt_input(h->context, "Secure Folder QR",
                  "New output directory (must not exist):", "", &output,
                  nullptr);
}
void start(const DodevHostAPI* h, bool decrypt) {
  if (!DODEV_HOST_HAS(h, prompt_input)) {
    log(h, 2, "Input prompts unavailable");
    return;
  }
  job = {decrypt, {}, {}};
  h->prompt_input(h->context, "Secure Folder QR",
                  decrypt ? "Path to scanned text file or QR export folder:"
                          : "Folder to compress and encrypt:",
                  "", &source, nullptr);
}
void enc(const DodevHostAPI* h, void*) { start(h, false); }
void dec(const DodevHostAPI* h, void*) { start(h, true); }
void panel(const DodevHostAPI*, const DodevUiBuilder* ui, void*) {
  if (!ui || !ui->label || !ui->button || !ui->read_only_text) return;
  ui->label(ui->context, "Secure Folder QR — offline encrypted archive");
  ui->button(ui->context, "Compress, Encrypt and Generate QR...",
             "securefolder.encrypt");
  ui->button(ui->context, "Decrypt and Restore Folder...",
             "securefolder.decrypt");
  ui->read_only_text(
      ui->context,
      "AES-256-GCM / PBKDF2-SHA256 / DEFLATE.\nUse a secret file outside the "
      "source folder.\nScan all QR parts and save their text, one per "
      "line.\nNo password is saved in plugin settings.\nMaximum 256 QR parts; "
      "output must be a new directory.");
}
}  // namespace
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI* h,
                                                     DodevPluginAPI* p) {
  if (!h || !p || h->api_version != DODEV_PLUGIN_API_VERSION ||
      h->struct_size < offsetof(DodevHostAPI, subscribe_event) +
                           sizeof(h->subscribe_event) ||
      p->struct_size < sizeof(DodevPluginAPI) || !h->register_command ||
      !h->register_menu || !h->register_tab)
    return 0;
  p->api_version = DODEV_PLUGIN_API_VERSION;
  p->shutdown = nullptr;
  return h->register_command(h->context, "securefolder.encrypt",
                             "Secure Folder QR: Encrypt...", &enc, nullptr) &&
         h->register_command(h->context, "securefolder.decrypt",
                             "Secure Folder QR: Restore...", &dec, nullptr) &&
         h->register_menu(h->context, "Tools", "Secure Folder QR: Encrypt...",
                          "securefolder.encrypt") &&
         h->register_menu(h->context, "Tools", "Secure Folder QR: Restore...",
                          "securefolder.decrypt") &&
         h->register_tab(h->context, "securefolder.panel", "Secure Folder QR",
                         &panel, nullptr);
}
