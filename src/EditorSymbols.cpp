#include <algorithm>
#include <cctype>
#include <chrono>
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

}  // namespace
bool EditorApp::initSymbolsPanel() {
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
  return true;
}
bool EditorApp::openSymbolsPanel() {
  if (!initSymbolsPanel()) return false;
  Rdock_area_open_panel(dock_, symbolsTaskPage_);
  refreshSymbolsPanel();
  return true;
}
void EditorApp::scheduleSymbolIndex() {
  if (folder_.empty()) return;
  if (symbolJob_.valid()) {
    symbolPending_ = true;
    return;
  }
  auto root = folder_;
  auto options = parserOptions(pluginManager_);
  // Workspace indexing honors the configured parser and never accesses GUI
  // widgets on the worker.
  symbolJob_ = std::async(std::launch::async, [root, options] {
    return symbols::indexWorkspace(root, options);
  });
  if (symbolsStatus_ != R_INVALID_ID)
    Rlabel_set_text(symbolsStatus_, "Indexing workspace symbols...");
}
void EditorApp::pollSymbolIndex() {
  if (!symbolJob_.valid() ||
      symbolJob_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    return;
  try {
    auto result = symbolJob_.get();
    if (result.root == folder_) {
      std::clog << "[DoDevEditor][Symbols] parsed=" << result.parsed
                << " cached=" << result.hits << " skipped=" << result.skipped
                << '\n';
      callIndex_ = result;
      refreshCallHierarchy();
      if (symbolsList_ != R_INVALID_ID && Rwidget_is_valid(symbolsList_) &&
          Rcheckbox_is_checked(Rwidget_find(window_, "symbols_workspace")) !=
              R_FALSE) {
        symbolsAll_ = std::move(result.rows);
        symbolsPaths_ = std::move(result.paths);
        symbolsParser_ = "Workspace · " + std::to_string(result.hits) +
                         " cached / " + std::to_string(result.parsed) +
                         " parsed";
        symbolsDiagnostics_ = result.diagnostic;
        filterSymbolsPanel();
      }
    }
  } catch (const std::exception& e) {
    std::clog << "[DoDevEditor][Symbols] " << e.what() << '\n';
    if (symbolsStatus_ != R_INVALID_ID)
      Rlabel_set_text(symbolsStatus_, e.what());
  }
  if (symbolPending_) {
    symbolPending_ = false;
    scheduleSymbolIndex();
  }
}
void EditorApp::refreshSymbolsPanel() {
  if (!folder_.empty() && Rcheckbox_is_checked(Rwidget_find(
                              window_, "symbols_workspace")) != R_FALSE) {
    scheduleSymbolIndex();
    return;
  }
  symbolsAll_.clear();
  symbolsPaths_.clear();
  symbolsDiagnostics_.clear();
  auto* current = active();
  if (current && current->kind == DocumentKind::Text &&
      current->editor != R_INVALID_ID) {
    auto path =
        current->path.empty() ? fs::path(current->title) : current->path;
    const char* text = Rtext_editor_get_text(current->editor);
    auto result = symbols::extract(pathUtf8(path), text ? text : "",
                                   parserOptions(pluginManager_));
    symbolsParser_ = result.parser;
    symbolsDiagnostics_ = result.diagnostics;
    symbolsAll_ = std::move(result.rows);
    symbolsPaths_.assign(symbolsAll_.size(), path);
  }
  filterSymbolsPanel();
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
  const auto sym = app.symbolsVisible_[static_cast<std::size_t>(index)];
  const auto path = app.symbolsVisiblePaths_[static_cast<std::size_t>(index)];
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
