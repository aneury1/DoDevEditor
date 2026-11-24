#include <algorithm>
#include <fstream>

#include "EditorApp.h"
namespace dodev {
void EditorApp::refreshCallHierarchy() {
  auto table = Rwidget_find(window_, "calls_results"),
       status = Rwidget_find(window_, "calls_status");
  if (table == R_INVALID_ID || status == R_INVALID_ID || table != callTable_)
    return;
  Rtable_clear_rows(table);
  callNodes_.clear();
  if (callRootName_.empty()) {
    Rlabel_set_text(status, "Select a function in Symbols or enter its name");
    return;
  }
  if (callIndex_.root != folder_) {
    Rlabel_set_text(status, "Waiting for workspace index");
    return;
  }
  callNodes_ = symbols::hierarchy(callIndex_, callRootName_, callRootPath_,
                                  callRootLine_, callIncoming_, callDepth_);
  for (const auto& n : callNodes_) {
    int row = Rtable_add_row(table, nullptr);
    auto name = std::string(n.depth * 3, ' ') + (n.depth ? "|- " : "") + n.name;
    auto loc = n.path.lexically_relative(folder_).generic_string() + ":" +
               std::to_string(n.line);
    Rtable_set_cell(table, row, 0, name.c_str());
    Rtable_set_cell(table, row, 1, n.status.c_str());
    Rtable_set_cell(table, row, 2, loc.c_str());
  }
  auto summary = callRootName_ + " | " +
                 (callIncoming_ ? "Incoming" : "Outgoing") + " | depth " +
                 std::to_string(callDepth_) + " | static estimates";
  Rlabel_set_text(status, summary.c_str());
}
void EditorApp::callHierarchyAction(const std::string& action) {
  auto pane = Rwidget_find(window_, "calls_pane"),
       table = Rwidget_find(window_, "calls_results");
  if (pane == R_INVALID_ID || table == R_INVALID_ID) return;
  // Columns are installed once for this widget.
  if (callTable_ != table) {
    callTable_ = table;
    Rtable_add_column(table, "Call", 220);
    Rtable_add_column(table, "Resolution", 110);
    Rtable_add_column(table, "Source", 220);
    Rtable_set_activated_callback(table, &EditorApp::callActivatedCb, this);
  }
  Rdock_area_open_panel(dock_, pane);
  if (action == "calls.selected") {
    int i =
        symbolsList_ == R_INVALID_ID ? -1 : Rtable_get_selected(symbolsList_);
    if (i < 0 || static_cast<size_t>(i) >= symbolsVisible_.size()) {
      setStatus("Select a function in Symbols first");
      return;
    }
    const auto& s = symbolsVisible_[i];
    if (s.kind != "function" && s.kind != "method") {
      setStatus("Select a function or method");
      return;
    }
    callRootName_ = s.name;
    callRootPath_ = symbolsVisiblePaths_[i];
    callRootLine_ = s.line;
  } else if (action == "calls.export") {
    RFileDialogDesc desc{};
    desc.parent = window_;
    desc.title = "Export call hierarchy (DOT)";
    desc.file_name = "call-hierarchy.dot";
    const char* selected = Rfile_dialog_save(&desc);
    if (!selected || !*selected) return;
    std::ofstream out(fs::u8path(selected));
    auto escape = [](const std::string& s) {
      std::string r;
      for (char c : s) {
        if (c == '"' || c == '\\') r += '\\';
        if (c == '\n' || c == '\r')
          r += ' ';
        else
          r += c;
      }
      return r;
    };
    out << "digraph Calls {\n";
    std::vector<size_t> parents;
    for (size_t i = 0; i < callNodes_.size(); ++i) {
      const auto& n = callNodes_[i];
      out << "n" << i << " [label=\"" << escape(n.name) << "\"];\n";
      if (n.depth && parents.size() >= n.depth) {
        auto parent = parents[n.depth - 1];
        if (callIncoming_)
          out << "n" << i << " -> n" << parent << ";\n";
        else
          out << "n" << parent << " -> n" << i << ";\n";
      }
      parents.resize(n.depth + 1);
      parents[n.depth] = i;
    }
    out << "}\n";
    out.close();
    setStatus(out ? "Call graph exported" : "Call graph export failed");
    return;
  } else if (action == "calls.find") {
    const char* q = Rtext_input_get_text(Rwidget_find(window_, "calls_query"));
    std::string name = q ? q : "";
    size_t count = 0, chosen = 0;
    for (size_t i = 0; i < callIndex_.rows.size(); ++i) {
      auto& s = callIndex_.rows[i];
      if (s.name == name && (s.kind == "function" || s.kind == "method")) {
        ++count;
        chosen = i;
      }
    }
    if (count != 1) {
      Rlabel_set_text(
          Rwidget_find(window_, "calls_status"),
          count ? "Multiple matches: select the desired symbol in Symbols"
                : "Function not found in workspace index");
      return;
    }
    callRootName_ = name;
    callRootPath_ = callIndex_.paths[chosen];
    callRootLine_ = callIndex_.rows[chosen].line;
  } else if (action == "calls.reroot") {
    int i = Rtable_get_selected(table);
    if (i >= 0 && static_cast<size_t>(i) < callNodes_.size()) {
      auto n = callNodes_[i];
      if (n.status == "unresolved" || n.status == "ambiguous") return;
      callRootName_ = n.name;
      callRootPath_ = n.path;
      callRootLine_ = n.line;
    }
  } else if (action == "calls.incoming")
    callIncoming_ = true;
  else if (action == "calls.outgoing")
    callIncoming_ = false;
  else if (action == "calls.less")
    callDepth_ = std::max<size_t>(1, callDepth_ - 1);
  else if (action == "calls.more")
    callDepth_ = std::min<size_t>(15, callDepth_ + 1);
  refreshCallHierarchy();
}
void EditorApp::callActivatedCb(Rid, int i, void* ctx) {
  auto& app = *static_cast<EditorApp*>(ctx);
  if (i < 0 || static_cast<size_t>(i) >= app.callNodes_.size()) return;
  auto n = app.callNodes_[i];
  if (!fs::exists(n.path) || !n.line) return;
  app.opened(n.path);
  for (const auto& doc : app.docs_) {
    std::error_code ec;
    if (doc->kind != DocumentKind::Text || doc->editor == R_INVALID_ID ||
        !fs::equivalent(doc->path, n.path, ec) || ec)
      continue;
    Rtab_view_show_tab(app.tabs_, doc->page);
    Rtext_editor_set_cursor(doc->editor, static_cast<int>(n.line - 1), 0);
    Rtext_editor_scroll_to_line(doc->editor, static_cast<int>(n.line - 1));
    Rwidget_focus(doc->editor);
    break;
  }
}
}  // namespace dodev
