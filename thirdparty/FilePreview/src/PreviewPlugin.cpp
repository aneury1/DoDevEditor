#include <algorithm>
#include <atomic>
#include <cctype>
#include <stdexcept>

#include "FilePreview.h"
#include "dodev/sdk/PluginSettings.h"
namespace {
std::atomic<unsigned long long> serial{0};
std::string setting(const DodevHostAPI* h, const char* key,
                    const char* fallback = "") {
  auto s = h->get_setting(h->context, key);
  return s ? s : fallback;
}
std::string extOf(const std::filesystem::path& f) {
  auto s = f.extension().u8string();
  for (auto& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
bool oneOf(const std::string& e, std::initializer_list<const char*> names) {
  for (auto x : names)
    if (e == x) return true;
  return false;
}
int show(const DodevHostAPI* h, const std::string& path, const char* buffer,
         size_t length) {
  auto file = std::filesystem::u8path(path);
  auto title = "Preview: " + file.filename().u8string();
  auto id = "preview-" + std::to_string(++serial);
  auto ext = extOf(file);
  auto html = [&](const std::string& text) {
    return h->show_html_view(h->context, id.c_str(), title.c_str(), "Editor",
                             text.c_str()) != 0;
  };
  auto image = [&](const std::string& bytes) {
    return h->show_image_view(h->context, id.c_str(), title.c_str(),
                              bytes.data(), bytes.size()) != 0;
  };
  auto source = [&](size_t limit, bool truncate = false) {
    if (!buffer) return preview::readBounded(file, limit, truncate);
    if (length > limit && !truncate)
      throw std::runtime_error("Preview size limit exceeded");
    return std::string(buffer, std::min(length, limit));
  };
  try {
    if (oneOf(ext, {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".ico", ".tif",
                    ".tiff"})) {
      if (buffer)
        throw std::runtime_error("Preview binary files directly from disk");
      if (!image(source(64 * 1024 * 1024)))
        throw std::runtime_error("Image decoding failed");
      return 1;
    }
    if (oneOf(ext, {".wav", ".mp3", ".ogg", ".flac"})) {
      if (!buffer && h->show_media_view(h->context, id.c_str(), title.c_str(),
                                        file.u8string().c_str()))
        return 1;
      throw std::runtime_error(
          "Native media playback unavailable. Install a wxWidgets media "
          "backend and codec, then rebuild the host.");
    }
    if (oneOf(ext, {".sqlite", ".sqlite3", ".db"})) {
      if (buffer)
        throw std::runtime_error(
            "Database preview requires the on-disk database");
      return html(preview::sqliteView(file, setting(h, "sqliteTable"),
                                      setting(h, "sqliteQuery"))
                      .html);
    }
    if (oneOf(ext,
              {".pdf", ".svg", ".puml", ".plantuml", ".mmd", ".mermaid"})) {
      if (setting(h, "externalRenderers", "false") != "true")
        throw std::runtime_error(
            "External rendering disabled. Enable trusted-file rendering in "
            "Settings > File Preview and install the relevant tool.");
      auto executable = ext == ".pdf" ? setting(h, "pdfRenderer", "pdftoppm")
                        : ext == ".svg"
                            ? setting(h, "svgRenderer", "rsvg-convert")
                        : oneOf(ext, {".puml", ".plantuml"})
                            ? setting(h, "plantumlRenderer", "plantuml")
                            : setting(h, "mermaidRenderer", "mmdc");
      int page = std::stoi(setting(h, "pdfPage", "1"));
      auto input = ext == ".pdf" ? std::string() : source(preview::MaxInput);
      if (!image(preview::externalImage(file, input, ext, executable, page)))
        throw std::runtime_error("Renderer output could not be decoded");
      return 1;
    }
    if (oneOf(ext, {".json", ".xml"}) &&
        setting(h, "structuredTree", "true") == "true") {
      auto text = source(preview::MaxInput);
      if (text.compare(0, 3, "\xef\xbb\xbf") == 0) text.erase(0, 3);
      auto nodes =
          ext == ".json" ? preview::jsonTree(text) : preview::xmlTree(text);
      std::vector<DodevTreeNode> view;
      for (auto& n : nodes) view.push_back({n.depth, n.label.c_str()});
      if (!h->show_tree_view(h->context, id.c_str(), title.c_str(), view.data(),
                             view.size()))
        throw std::runtime_error("Tree view unavailable");
      return 1;
    }
    if (oneOf(ext,
              {".html", ".htm", ".md", ".markdown", ".csv", ".tsv", ".json",
               ".xml", ".txt", ".log", ".yaml", ".yml", ".ini", ".toml"}))
      return html(preview::render(ext, source(preview::MaxInput),
                                  setting(h, "csvHeader", "true") != "false")
                      .html);
    constexpr size_t limit = 64 * 1024;
    auto bytes = source(limit, true);
    return html(preview::hexView(
                    bytes, buffer ? length > limit
                                  : std::filesystem::file_size(file) > limit)
                    .html);
  } catch (const std::exception& e) {
    return h->show_text_view(h->context, id.c_str(), title.c_str(), "Editor",
                             e.what());
  }
}
int registered(const DodevHostAPI* h, const char* path, const char* src,
               size_t size, void*) {
  return path && *path ? show(h, path, src, size) : 0;
}
void selected(const DodevHostAPI* h, const char* path, void*) {
  if (path && *path) show(h, path, nullptr, 0);
}
void choose(const DodevHostAPI* h, void*) {
  h->select_path(h->context, "Select file to preview", 0, &selected, nullptr);
}
void active(const DodevHostAPI* h, void*) {
  const char *path = nullptr, *contents = nullptr;
  if (h->get_active_document(h->context, &path, &contents) && path && *path &&
      contents) {
    std::string p = path, s = contents;
    show(h, p, s.data(), s.size());
  } else
    choose(h, nullptr);
}
}  // namespace
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI* h,
                                                     DodevPluginAPI* p) {
  if (!h || !p || h->api_version != DODEV_PLUGIN_API_VERSION ||
      !DODEV_HOST_HAS(h, show_html_view) ||
      !DODEV_HOST_HAS(h, show_text_view) || !DODEV_HOST_HAS(h, select_path) ||
      !DODEV_HOST_HAS(h, get_active_document) ||
      !DODEV_HOST_HAS(h, register_file_preview) ||
      !DODEV_HOST_HAS(h, show_image_view) ||
      !DODEV_HOST_HAS(h, show_tree_view) || !DODEV_HOST_HAS(h, show_media_view))
    return 0;
  auto add = [&](const char* key, const char* val, const char* desc,
                 const char* schema) {
    return dodev::sdk::setting(h, key, val, desc, schema);
  };
  if (!add("csvHeader", "true", "Treat first row as table header",
           R"({"label":"CSV / TSV first row is header","type":"bool"})") ||
      !add("structuredTree", "true",
           "Use expandable JSON/XML tree (XML requires Expat)",
           R"({"label":"JSON / XML tree view","type":"bool"})") ||
      !add("sqliteTable", "",
           "Table to preview; blank selects first ordinary table",
           R"({"label":"SQLite table name","type":"text"})") ||
      !add("sqliteQuery", "",
           "Optional read-only SELECT; leave blank for table browser",
           R"({"label":"SQLite SELECT query","type":"multiline"})") ||
      !add(
          "externalRenderers", "false",
          "Allow external tools for trusted files only; may read files or "
          "access network depending on tool",
          R"({"label":"Enable external renderers - trusted files only","type":"bool"})") ||
      !add("pdfPage", "1", "PDF page to render",
           R"({"label":"PDF page","type":"int","min":1,"max":10000})") ||
      !add("pdfRenderer", "pdftoppm", "Poppler executable",
           R"({"label":"PDF renderer","type":"text"})") ||
      !add("svgRenderer", "rsvg-convert", "librsvg executable",
           R"({"label":"SVG renderer","type":"text"})") ||
      !add("plantumlRenderer", "plantuml", "PlantUML executable",
           R"({"label":"PlantUML renderer","type":"text"})") ||
      !add("mermaidRenderer", "mmdc", "Mermaid CLI executable",
           R"({"label":"Mermaid renderer","type":"text"})"))
    return 0;
  if (!h->register_command(h->context, "preview.active", "Preview active file",
                           &active, nullptr) ||
      !h->register_command(h->context, "preview.choose", "Preview file...",
                           &choose, nullptr))
    return 0;
  if (!h->register_file_preview(
          h->context,
          ".html,.htm,.md,.markdown,.csv,.tsv,.json,.xml,.yaml,.yml,.ini,.toml,"
          ".txt,.log,.png,.jpg,.jpeg,.bmp,.gif,.ico,.tif,.tiff,.svg,.pdf,.puml,"
          ".plantuml,.mmd,.mermaid,.sqlite,.sqlite3,.db,.bin,.dat,.hex,.raw,."
          "exe,.dll,.so,.wav,.mp3,.ogg,.flac,*",
          &registered, nullptr))
    return 0;
  h->register_menu(h->context, "Options", "Preview active file",
                   "preview.active");
  h->register_menu(h->context, "Options", "Preview file...", "preview.choose");
  p->api_version = DODEV_PLUGIN_API_VERSION;
  p->shutdown = nullptr;
  return 1;
}
