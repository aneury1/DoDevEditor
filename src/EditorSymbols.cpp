#include "EditorApp.h"
#include <algorithm>
#include <fstream>
#include <iostream>

namespace dodev {
namespace {
void button(Rid box, const char *label, const char *action) {
  auto w = Rbutton_create(label);
  Rwidget_set_class(w, "chip");
  Rwidget_set_action(w, action);
  Rwidget_add_child(box, w);
}
} // namespace
bool EditorApp::openSymbolsPanel() {
  auto *doc = active();
  if (!doc || doc->kind != DocumentKind::Text || doc->editor == R_INVALID_ID) {
    setStatus("Open a source file in the text editor first");
    return false;
  }
  symbolsEditor_ = doc->editor;
  const char *text = Rtext_editor_get_text(symbolsEditor_);
  symbolsAll_ =
      symbols::extract(doc->path.empty() ? doc->title : doc->path.string(),
                       text ? text : "")
          .rows;
  if (symbolsTaskPage_ == R_INVALID_ID || !Rwidget_is_valid(symbolsTaskPage_)) {
    symbolsTaskPage_ = Rwidget_create(R_WIDGET_VBOX);
    Rwidget_set_class(symbolsTaskPage_, "music-surface surface");
    auto header = Rwidget_create(R_WIDGET_HBOX);
    Rwidget_add_child(symbolsTaskPage_, header);
    Rwidget_add_child(header, Rlabel_create("TASKS · SOURCE SYMBOLS"));
    button(header, "Refresh", "symbols.refresh");
    button(header, "Go To Symbol", "symbols.navigate");
    button(header, "Export CSV...", "symbols.export");
    symbolsFilter_ = Rwidget_create(R_WIDGET_TEXT_INPUT);
    Rtext_input_set_placeholder(symbolsFilter_,
                                "Filter symbols by name or kind...");
    Rtext_input_set_changed_callback(symbolsFilter_,
                                     &EditorApp::symbolsFilterCb, this);
    Rwidget_add_child(symbolsTaskPage_, symbolsFilter_);
    symbolsStatus_ = Rlabel_create("Symbols");
    Rwidget_add_child(symbolsTaskPage_, symbolsStatus_);
    symbolsList_ = Rwidget_create(R_WIDGET_TABLE);
    Rtable_add_column(symbolsList_, "Line", 80);
    Rtable_add_column(symbolsList_, "Kind", 115);
    Rtable_add_column(symbolsList_, "Symbol", 230);
    Rtable_add_column(symbolsList_, "Signature", 420);
    Rtable_set_striped(symbolsList_, R_TRUE);
    for (int col = 0; col < 4; ++col)
      Rtable_set_column_sortable(symbolsList_, col,
                                 R_FALSE); // row index must match symbol index
    Rtable_set_selection_changed_callback(symbolsList_,
                                          &EditorApp::symbolsSelectCb, this);
    Rtable_set_activated_callback(symbolsList_, &EditorApp::symbolsActivatedCb,
                                  this);
    Rwidget_add_child(symbolsTaskPage_, symbolsList_);
    Rslot_set_fill(symbolsList_, 1.0f);
    symbolsDescription_ = Rtext_editor_create("");
    Rtext_editor_set_read_only(symbolsDescription_, R_TRUE);
    Rwidget_add_child(symbolsTaskPage_, symbolsDescription_);
    Rtab_view_add_tab(terminal_tabs_, "Tasks · Symbols", symbolsTaskPage_);
  }
  filterSymbolsPanel();
  Rdock_area_open_panel(dock_, output_pane_);
  Rtab_view_show_tab(terminal_tabs_, symbolsTaskPage_);
  std::clog << "[DoDevEditor][Symbols] Extracted " << symbolsAll_.size()
            << " symbols from " << doc->title << "\n";
  return true;
}
void EditorApp::refreshSymbolsPanel() {
  if (auto *doc = findDocument(symbolsEditor_);
      doc && Rwidget_is_valid(doc->editor)) {
    const char *text = Rtext_editor_get_text(doc->editor);
    symbolsAll_ =
        symbols::extract(doc->path.empty() ? doc->title : doc->path.string(),
                         text ? text : "")
            .rows;
    filterSymbolsPanel();
  } else {
    symbolsAll_.clear();
    symbolsVisible_.clear();
    setStatus("Source file was closed");
  }
}
void EditorApp::filterSymbolsPanel() {
  if (symbolsList_ == R_INVALID_ID || !Rwidget_is_valid(symbolsList_))
    return;
  const char *text = symbolsFilter_ != R_INVALID_ID
                         ? Rtext_input_get_text(symbolsFilter_)
                         : nullptr;
  symbolsVisible_ = symbols::filter(symbolsAll_, text ? text : "");
  Rtable_clear_rows(symbolsList_);
  for (const auto &symbol : symbolsVisible_) {
    const int row = Rtable_add_row(symbolsList_, nullptr);
    Rtable_set_cell(symbolsList_, row, 0, std::to_string(symbol.line).c_str());
    Rtable_set_cell(symbolsList_, row, 1, symbol.kind.c_str());
    Rtable_set_cell(symbolsList_, row, 2, symbol.name.c_str());
    Rtable_set_cell(symbolsList_, row, 3, symbol.signature.c_str());
  }
  const auto status = std::to_string(symbolsVisible_.size()) + " / " +
                      std::to_string(symbolsAll_.size()) +
                      " symbols · double-click to navigate";
  Rlabel_set_text(symbolsStatus_, status.c_str());
  if (!symbolsVisible_.empty())
    Rtable_set_selected(symbolsList_, 0);
}
void EditorApp::symbolsFilterCb(Rid, const char *, void *ctx) {
  static_cast<EditorApp *>(ctx)->filterSymbolsPanel();
}
void EditorApp::symbolsSelectCb(Rid, int index, void *ctx) {
  auto &app = *static_cast<EditorApp *>(ctx);
  if (index < 0 ||
      static_cast<std::size_t>(index) >= app.symbolsVisible_.size())
    return;
  const auto &s = app.symbolsVisible_[static_cast<std::size_t>(index)];
  if (app.symbolsDescription_ != R_INVALID_ID &&
      Rwidget_is_valid(app.symbolsDescription_))
    Rtext_editor_set_text(app.symbolsDescription_,
                          (s.name + " · " + s.kind + " · line " +
                           std::to_string(s.line) + "\n" + s.signature)
                              .c_str());
}
void EditorApp::symbolsActivatedCb(Rid, int index, void *ctx) {
  auto &app = *static_cast<EditorApp *>(ctx);
  if (index < 0 ||
      static_cast<std::size_t>(index) >= app.symbolsVisible_.size())
    return;
  auto *doc = app.findDocument(app.symbolsEditor_);
  if (!doc || !Rwidget_is_valid(doc->editor)) {
    app.setStatus("Original editor no longer exists");
    return;
  }
  const auto &sym = app.symbolsVisible_[static_cast<std::size_t>(index)];
  Rtab_view_show_tab(app.tabs_, doc->page);
  Rtext_editor_set_cursor(doc->editor, static_cast<int>(sym.line - 1),
                          static_cast<int>(sym.column - 1));
  Rtext_editor_scroll_to_line(doc->editor, static_cast<int>(sym.line - 1));
  Rwidget_focus(doc->editor);
}
void EditorApp::symbolsAction(const std::string &action) {
  if (action == "symbols.refresh") {
    refreshSymbolsPanel();
    return;
  }
  if (action == "symbols.navigate") {
    const int selected = Rtable_get_selected(symbolsList_);
    symbolsActivatedCb(symbolsList_, selected, this);
    return;
  }
  if (action == "symbols.export") {
    RFileDialogDesc desc{};
    desc.parent = window_;
    desc.title = "Export source symbols CSV";
    const std::string fileName = "source_symbols.csv";
    desc.file_name = fileName.c_str();
    const char *choice = Rfile_dialog_save(&desc);
    if (!choice || !*choice)
      return;
    auto path = fs::u8path(choice);
    if (path.extension().empty())
      path += ".csv";
    const auto csv = symbols::toCsv(symbolsVisible_);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
      setStatus("Cannot open Symbols CSV output");
      return;
    }
    output << csv;
    output.flush();
    if (!output) {
      setStatus("Symbols export failed");
      return;
    }
    setStatus("Symbols CSV exported: " + path.string());
  }
}
} // namespace dodev
