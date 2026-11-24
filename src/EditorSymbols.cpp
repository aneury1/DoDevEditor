#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <set>

#include "EditorApp.h"

namespace dodev {
namespace {
symbols::Options parserOptions(const PluginManager& manager) {
  symbols::Options options;
  const auto mode =
      manager.pluginSetting("com.dodev.sourcesymbols", "parserMode");
  options.mode = mode == "regex"   ? symbols::ParserMode::RegexOnly
                 : mode == "clang" ? symbols::ParserMode::ClangOnly
                                   : symbols::ParserMode::Auto;
  options.clangExecutable =
      manager.pluginSetting("com.dodev.sourcesymbols", "clangExecutable");
  const auto timeout =
      manager.pluginSetting("com.dodev.sourcesymbols", "timeoutMs");
  if (!timeout.empty()) {
    try {
      options.timeoutMs = std::stoi(timeout);
    } catch (...) {
    }
  }
  return options;
}
std::string lower(std::string s) {
  for (auto& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string csv(std::string s) {
  std::string output = "\"";
  for (char c : s) {
    if (c == '\"')
      output += "\"\"";
    else
      output += c;
  }
  return output + '"';
}
std::string displayPath(const fs::path& file, const fs::path& root) {
  std::error_code ec;
  auto rel = fs::relative(file, root, ec);
  return ec ? pathUtf8(file) : pathUtf8(rel);
}
bool isSourceFile(const fs::path& file) {
  const auto ext = lower(file.extension().string());
  return ext == ".h" || ext == ".hpp" || ext == ".hh" || ext == ".c" ||
         ext == ".cc" || ext == ".cpp" || ext == ".cxx" || ext == ".py" ||
         ext == ".js" || ext == ".ts" || ext == ".tsx" || ext == ".jsx" ||
         ext == ".java" || ext == ".kt" || ext == ".cs";
}
}  // namespace
bool EditorApp::openSymbolsPanel() {
  if (symbolsTaskPage_ == R_INVALID_ID) {
    symbolsTaskPage_ = Rwidget_find(window_, "symbols_pane");
    symbolsFilter_ = Rwidget_find(window_, "symbols_sidebar_filter");
    symbolsStatus_ = Rwidget_find(window_, "symbols_sidebar_status");
    symbolsList_ = Rwidget_find(window_, "symbols_sidebar_results");
    if (symbolsTaskPage_ == R_INVALID_ID || symbolsList_ == R_INVALID_ID) {
      setStatus("Symbols sidebar missing");
      return false;
    }
    Rtable_add_column(symbolsList_, "Entity name", 205);
    Rtable_add_column(symbolsList_, "Type", 105);
    Rtable_add_column(symbolsList_, "Filepath : line", 280);
    Rtable_set_striped(symbolsList_, R_TRUE);
    for (int col = 0; col < 3; ++col)
      Rtable_set_column_sortable(symbolsList_, col, R_FALSE);
    Rtable_set_selection_changed_callback(symbolsList_,
                                          &EditorApp::symbolsSelectCb, this);
    Rtable_set_activated_callback(symbolsList_, &EditorApp::symbolsActivatedCb,
                                  this);
    Rtext_input_set_changed_callback(symbolsFilter_,
                                     &EditorApp::symbolsFilterCb, this);
  }
  Rdock_area_open_panel(dock_, symbolsTaskPage_);
  refreshSymbolsPanel();
  return true;
}
void EditorApp::refreshSymbolsPanel() {
  symbolsAll_.clear();
  symbolsPaths_.clear();
  symbolsVisible_.clear();
  symbolsVisiblePaths_.clear();
  Document* current = active();
  const bool workspace =
      folder_ != fs::path{} && Rcheckbox_is_checked(Rwidget_find(
                                   window_, "symbols_workspace")) != R_FALSE;
  auto options = parserOptions(pluginManager_);
  std::set<fs::path> visited;
  auto add = [&](const fs::path& path, const std::string& text,
                 const symbols::Options& opts) {
    const auto result = symbols::extract(pathUtf8(path), text, opts);
    symbolsParser_ = result.parser;
    if (!result.diagnostics.empty()) symbolsDiagnostics_ = result.diagnostics;
    for (const auto& row : result.rows) {
      symbolsAll_.push_back(row);
      symbolsPaths_.push_back(path);
    }
  };
  symbolsDiagnostics_.clear();
  symbolsParser_ = workspace ? "Workspace Regex" : "Regex";
  if (current && current->kind == DocumentKind::Text &&
      current->editor != R_INVALID_ID) {
    symbolsEditor_ = current->editor;
    const auto path =
        current->path.empty() ? fs::path(current->title) : current->path;
    const char* text = Rtext_editor_get_text(current->editor);
    add(path, text ? text : "", options);
    if (!current->path.empty()) {
      std::error_code ec;
      visited.insert(fs::weakly_canonical(current->path, ec));
    }
  }
  if (workspace) {
    // Workspace-wide symbols use the lightweight lexical parser. Running
    // clang++ for every file on the UI thread would freeze a large project;
    // active source always retains its Clang-first parsing mode.
    symbols::Options workspaceOptions;
    workspaceOptions.mode = symbols::ParserMode::RegexOnly;
    std::error_code ec;
    std::size_t parsed = 0;
    for (fs::recursive_directory_iterator
             it(folder_, fs::directory_options::skip_permission_denied, ec),
         end;
         !ec && it != end && parsed < 150; it.increment(ec)) {
      const auto path = it->path();
      if (it->is_directory(ec)) {
        const auto name = path.filename().string();
        if (name == ".git" || name == "build" || name == "node_modules" ||
            name == "dist" || name == ".cache" || name == ".venv")
          it.disable_recursion_pending();
        continue;
      }
      if (!it->is_regular_file(ec) || ec || !isSourceFile(path)) continue;
      const auto canonical = fs::weakly_canonical(path, ec);
      if (ec || !visited.insert(canonical).second) {
        ec.clear();
        continue;
      }
      if (it->file_size(ec) > 400 * 1024 || ec) {
        ec.clear();
        continue;
      }
      std::string source;
      bool usedEditor = false;
      for (const auto& d : docs_) {
        std::error_code sameError;
        if (!d->path.empty() && d->kind == DocumentKind::Text &&
            fs::equivalent(d->path, path, sameError) && !sameError) {
          const char* buffer = Rtext_editor_get_text(d->editor);
          source = buffer ? buffer : "";
          usedEditor = true;
          break;
        }
      }
      if (!usedEditor) {
        std::ifstream in(path, std::ios::binary);
        if (!in) continue;
        source.assign(std::istreambuf_iterator<char>(in), {});
      }
      if (source.find('\0') != std::string::npos) continue;
      add(path, source, workspaceOptions);
      ++parsed;
    }
    symbolsParser_ = "Workspace Regex (active: Clang/Regex)";
    if (parsed >= 150)
      symbolsDiagnostics_ = "Workspace limit: 150 source files";
  }
  filterSymbolsPanel();
  std::clog << "[DoDevEditor][Symbols] " << symbolsAll_.size() << " symbols; "
            << symbolsParser_ << '\n';
}
void EditorApp::filterSymbolsPanel() {
  if (symbolsList_ == R_INVALID_ID || !Rwidget_is_valid(symbolsList_)) return;
  const char* query = Rtext_input_get_text(symbolsFilter_);
  const auto wanted = lower(query ? query : "");
  Rtable_clear_rows(symbolsList_);
  symbolsVisible_.clear();
  symbolsVisiblePaths_.clear();
  for (std::size_t i = 0; i < symbolsAll_.size(); ++i) {
    const auto& sym = symbolsAll_[i];
    const auto& path = symbolsPaths_[i];
    const auto label = displayPath(path, folder_);
    if (!wanted.empty() &&
        lower(sym.name + " " + sym.kind + " " + label).find(wanted) ==
            std::string::npos)
      continue;
    symbolsVisible_.push_back(sym);
    symbolsVisiblePaths_.push_back(path);
    const int row = Rtable_add_row(symbolsList_, nullptr);
    Rtable_set_cell(symbolsList_, row, 0, sym.name.c_str());
    Rtable_set_cell(symbolsList_, row, 1, sym.kind.c_str());
    const auto location = label + ":" + std::to_string(sym.line);
    Rtable_set_cell(symbolsList_, row, 2, location.c_str());
  }
  const auto summary = std::to_string(symbolsVisible_.size()) + " / " +
                       std::to_string(symbolsAll_.size()) + " symbols · " +
                       symbolsParser_ +
                       (symbolsDiagnostics_.empty()
                            ? ""
                            : " · " + symbolsDiagnostics_.substr(0, 100));
  Rlabel_set_text(symbolsStatus_, summary.c_str());
}
void EditorApp::symbolsFilterCb(Rid, const char*, void* ctx) {
  static_cast<EditorApp*>(ctx)->filterSymbolsPanel();
}
void EditorApp::symbolsSelectCb(Rid, int index, void* ctx) {
  auto& app = *static_cast<EditorApp*>(ctx);
  if (index < 0 ||
      static_cast<std::size_t>(index) >= app.symbolsVisible_.size())
    return;
  const auto& sym = app.symbolsVisible_[static_cast<std::size_t>(index)];
  app.setStatus(sym.name + " · " + sym.kind + " · " + sym.signature);
}
void EditorApp::symbolsActivatedCb(Rid, int index, void* ctx) {
  auto& app = *static_cast<EditorApp*>(ctx);
  if (index < 0 ||
      static_cast<std::size_t>(index) >= app.symbolsVisible_.size())
    return;
  const auto& sym = app.symbolsVisible_[static_cast<std::size_t>(index)];
  const auto& path = app.symbolsVisiblePaths_[static_cast<std::size_t>(index)];
  if (fs::exists(path)) app.opened(path);
  for (const auto& doc : app.docs_) {
    if (doc->kind != DocumentKind::Text || doc->editor == R_INVALID_ID)
      continue;
    bool match = false;
    if (!doc->path.empty() && !path.empty()) {
      std::error_code ec;
      match = fs::equivalent(doc->path, path, ec) && !ec;
    } else
      match = doc->title == path.string();
    if (!match) continue;
    Rtab_view_show_tab(app.tabs_, doc->page);
    Rtext_editor_set_cursor(doc->editor, static_cast<int>(sym.line - 1),
                            static_cast<int>(sym.column - 1));
    Rtext_editor_scroll_to_line(doc->editor, static_cast<int>(sym.line - 1));
    Rwidget_focus(doc->editor);
    return;
  }
  app.setStatus("Symbol source not available: " + path.string());
}
void EditorApp::symbolsAction(const std::string& action) {
  if (action == "symbols.refresh") {
    if (symbolsTaskPage_ != R_INVALID_ID) refreshSymbolsPanel();
    return;
  }
  if (action == "symbols.navigate") {
    if (symbolsList_ == R_INVALID_ID) return;
    symbolsActivatedCb(symbolsList_, Rtable_get_selected(symbolsList_), this);
    return;
  }
  if (action == "symbols.export") {
    RFileDialogDesc desc{};
    desc.parent = window_;
    desc.title = "Export source symbols CSV";
    const std::string fileName = "source_symbols.csv";
    desc.file_name = fileName.c_str();
    const char* choice = Rfile_dialog_save(&desc);
    if (!choice || !*choice) return;
    auto path = fs::u8path(choice);
    if (path.extension().empty()) path += ".csv";
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
      setStatus("Cannot open Symbols CSV output");
      return;
    }
    output << "Entity name,Type,Filepath,Line,Signature\n";
    for (std::size_t i = 0; i < symbolsVisible_.size(); ++i) {
      const auto& sym = symbolsVisible_[i];
      output << csv(sym.name) << ',' << csv(sym.kind) << ','
             << csv(pathUtf8(symbolsVisiblePaths_[i])) << ',' << sym.line << ','
             << csv(sym.signature) << '\n';
    }
    output.flush();
    setStatus(output ? "Symbols CSV exported: " + path.string()
                     : "Symbols export failed");
  }
}
}  // namespace dodev
