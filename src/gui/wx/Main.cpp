#include <wx/accel.h>
#include <wx/artprov.h>
#include <wx/aui/aui.h>
#include <wx/checklst.h>
#include <wx/choicdlg.h>
#include <wx/clipbrd.h>
#include <wx/combobox.h>
#include <wx/config.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/progdlg.h>
#include <wx/scrolwin.h>
#include <wx/spinctrl.h>
#include <wx/stc/stc.h>
#include <wx/stdpaths.h>
#include <wx/textdlg.h>
#include <wx/treectrl.h>
#include <wx/utils.h>
#include <wx/wx.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <set>

#include "../../../resources/EmbeddedUi.h"
#include "AiProfiles.h"
#include "CallHierarchy.h"
#include "EditorData.h"
#include "GdbConfig.h"
#include "RemoteJournal.h"
#include "SerialMonitor.h"
#include "SymbolCache.h"
#include "TerminalScreen.h"
#include "TerminalSession.h"
#include "WxExplorerModel.h"
#include "WxTerminal.h"
#include "WxTheme.h"
#include "dodev/editor/EditorCommands.h"
#include "dodev/explorer/ExplorerOps.h"
#include "dodev/keybindings/KeybindingManager.h"
#include "dodev/launch/ProcessLaunch.h"
#include "dodev/plugins/PluginManager.h"
#include "dodev/plugins/PluginPaths.h"
#include "dodev/search/WorkspaceSearch.h"
#include "dodev/terminal/TaskCatalog.h"
#include "dodev/terminal/TerminalTasks.h"
namespace fs = std::filesystem;
using namespace dodev;
static wxString W(const std::string& s) {
  return wxString::FromUTF8(s.data(), s.size());
}
static std::string S(const wxString& s) {
  auto b = s.ToUTF8();
  return b.data() ? std::string(b.data(), b.length()) : std::string();
}
static wxString P(const fs::path& p) { return W(p.u8string()); }
static fs::path F(const wxString& s) { return fs::u8path(S(s)); }
struct PathData : wxTreeItemData {
  fs::path path;
  bool loaded = false;
  explicit PathData(fs::path p) : path(std::move(p)) {}
};
struct Document {
  fs::path path;
};
class Frame : public wxFrame {
  SettingsManager settings;
  TemplateRegistry templates;
  ThemeManager themes;
  PluginManager plugins{settings, templates, themes};
  wxAuiManager aui;
  wxAuiNotebook *editors, *side, *bottom;
  wxTreeCtrl *tree, *calls;
  wxListCtrl *symbolList, *matches;
  wxTextCtrl *query, *filter, *git, *replacementText, *includePatterns,
      *excludePatterns, *callQuery;
  wxCheckBox *searchRegex, *searchCase, *searchWord, *allSymbols;
  wxSpinCtrl* callDepth;
  wxChoice* direction;
  wxTimer timer;
  fs::path root, dir;
  const fs::path startupDirectory = fs::current_path();
  std::map<wxStyledTextCtrl*, Document> docs;
  std::vector<size_t> visible;
  std::vector<search::Match> hits;
  symbols::WorkspaceIndex index;
  std::future<symbols::WorkspaceIndex> worker;
  bool pending = false;
  size_t selected = static_cast<size_t>(-1);
  std::vector<symbols::CallNode> callNodes;
  wxui::Theme palette = wxui::Theme::dark();
  bool showHidden = true;
  std::chrono::steady_clock::time_point lastAutoSave{};
  struct Terminal {
    TerminalSession session;
    TerminalScreen screen;
    WxTerminal* out = nullptr;
    bool wasRunning = false, taskMode = false;
    fs::path cwd;
    std::vector<dodev::terminal::TaskLaunch> taskQueue;
  };
  std::unique_ptr<Terminal> shell, debug;

  template <class D>
  int showModal(D& d) {
    palette.apply(&d);
    return d.ShowModal();
  }
  wxStyledTextCtrl* active() {
    return dynamic_cast<wxStyledTextCtrl*>(editors->GetCurrentPage());
  }
  void error(const std::exception& e) {
    wxMessageBox(W(e.what()), "DoDevEditor", wxOK | wxICON_ERROR, this);
  }
  wxListCtrl* list(wxWindow* p, std::initializer_list<const char*> cols) {
    auto l = new wxListCtrl(p, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxLC_REPORT | wxLC_SINGLE_SEL);
    int i = 0;
    for (auto c : cols) l->InsertColumn(i++, c, wxLIST_FORMAT_LEFT, 150);
    return l;
  }
  wxPanel* page(wxAuiNotebook* n, const wxString& name, wxBoxSizer*& s) {
    auto p = new wxPanel(n);
    s = new wxBoxSizer(wxVERTICAL);
    p->SetSizer(s);
    n->AddPage(p, name);
    return p;
  }
  void button(wxWindow* p, wxSizer* s, const wxString& label,
              std::function<void()> f) {
    auto b = new wxButton(p, wxID_ANY, label);
    s->Add(b, 0, wxALL, 2);
    b->Bind(wxEVT_BUTTON, [f](wxCommandEvent&) {
      try {
        f();
      } catch (const std::exception& e) {
        wxLogError("%s", W(e.what()));
      }
    });
  }
  void configure(wxStyledTextCtrl* e, const fs::path& p) {
    e->SetCodePage(wxSTC_CP_UTF8);
    e->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    e->SetMarginWidth(0, 48);
    e->SetTabWidth(4);
    e->SetUseTabs(false);
    e->SetIndent(4);
    e->SetMultipleSelection(true);
    e->SetAdditionalSelectionTyping(true);
    e->StyleSetFont(wxSTC_STYLE_DEFAULT,
                    wxFont(wxFontInfo(11).Family(wxFONTFAMILY_TELETYPE)));
    e->StyleClearAll();
    auto ext = p.extension().string();
    if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp" ||
        ext == ".java" || ext == ".kt") {
      e->SetLexer(wxSTC_LEX_CPP);
      e->SetKeyWords(
          0,
          "auto bool break case char class const constexpr continue double "
          "else enum explicit float for if int long namespace new nullptr "
          "private protected public return short static struct switch template "
          "this throw try typedef typename using virtual void while");
      e->StyleSetForeground(wxSTC_C_WORD, wxColour(25, 85, 170));
      e->StyleSetForeground(wxSTC_C_COMMENT, wxColour(0, 125, 65));
      e->StyleSetForeground(wxSTC_C_COMMENTLINE, wxColour(0, 125, 65));
      e->StyleSetForeground(wxSTC_C_STRING, wxColour(160, 55, 25));
    } else if (ext == ".py") {
      e->SetLexer(wxSTC_LEX_PYTHON);
      e->SetKeyWords(
          0,
          "and as assert async await break class continue def del elif else "
          "except False finally for from global if import in is lambda None "
          "nonlocal not or pass raise return True try while with yield");
    } else if (ext == ".json") {
      e->SetLexer(wxSTC_LEX_JSON);
      e->SetKeyWords(0, "true false null");
    }
    e->SetProperty("fold", "1");
    e->SetMarginType(2, wxSTC_MARGIN_SYMBOL);
    e->SetMarginMask(2, wxSTC_MASK_FOLDERS);
    e->SetMarginWidth(2, 15);
    e->SetMarginSensitive(2, true);
    e->MarkerDefine(wxSTC_MARKNUM_FOLDER, wxSTC_MARK_BOXPLUS);
    e->MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_BOXMINUS);
    applyEditorSettings(e);
    e->Bind(wxEVT_STC_MARGINCLICK, [e](wxStyledTextEvent& v) {
      if (v.GetMargin() == 2)
        e->ToggleFold(e->LineFromPosition(v.GetPosition()));
    });
    e->UsePopUp(wxSTC_POPUP_NEVER);
    e->Bind(wxEVT_CONTEXT_MENU, [this, e](wxContextMenuEvent&) {
      contextMenu(e, {{"Undo", "edit.undo"},
                      {"Redo", "edit.redo"},
                      {"Cut", "edit.cut"},
                      {"Copy", "edit.copy"},
                      {"Paste", "edit.paste"},
                      {"Select all", "selection.all"},
                      {"Find", "search.show"},
                      {"Find in workspace", "panel.search"},
                      {"Go to line", "navigation.go_to_line"},
                      {"Toggle line comment", "edit.toggle_line_comment"},
                      {"Format document (clang-format)", "editor.format"}});
    });
  }
  void open(fs::path p, size_t line = 0) {
    p = fs::absolute(p).lexically_normal();
    for (auto& d : docs)
      if (d.second.path == p) {
        editors->SetSelection(editors->GetPageIndex(d.first));
        if (line) d.first->GotoLine(int(line - 1));
        return;
      }
    if (!fs::is_regular_file(p))
      throw std::runtime_error("File not found: " + p.u8string());
    auto e = new wxStyledTextCtrl(editors, wxID_ANY);
    configure(e, p);
    if (!e->LoadFile(P(p))) {
      e->Destroy();
      throw std::runtime_error("Cannot read file");
    }
    docs[e] = {p};
    palette.editor(e);
    editors->AddPage(e, P(p.filename()), true);
    if (line) e->GotoLine(int(line - 1));
    Json evt = Json::object();
    evt["path"] = p.u8string();
    plugins.emit("document.opened", evt.dump(0));
  }
  bool save(wxStyledTextCtrl* e, bool saveAs = false) {
    if (!e) return true;
    auto& d = docs.at(e);
    if (saveAs || d.path.empty()) {
      wxFileDialog dlg(this, "Save file", P(root), "", "All files|*",
                       wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
      if (showModal(dlg) != wxID_OK) return false;
      d.path = F(dlg.GetPath());
    }
    if (!e->SaveFile(P(d.path)))
      throw std::runtime_error("Cannot save file: " + d.path.u8string());
    e->SetSavePoint();
    Json evt = Json::object();
    evt["path"] = d.path.u8string();
    plugins.emit("document.saved", evt.dump(0));
    editors->SetPageText(editors->GetPageIndex(e), P(d.path.filename()));
    if (d.path.filename() == "tasks.json") refreshTaskPicker(true);
    schedule();
    return true;
  }
  bool confirm(wxStyledTextCtrl* e) {
    if (!e || !e->GetModify()) return true;
    auto r = wxMessageBox("Save modified file?", "DoDevEditor",
                          wxYES_NO | wxCANCEL, this);
    return r == wxNO || (r == wxYES && save(e));
  }
  void newDoc(const fs::path& typeHint = {}) {
    auto e = new wxStyledTextCtrl(editors, wxID_ANY);
    configure(e, typeHint);
    docs[e] = {};
    editors->AddPage(e, "Untitled", true);
  }
#include "WxExplorer.inc"
  void workspace(fs::path p) {
    for (auto* t : allTerminals()) {
      t->taskQueue.clear();
      if (t->session.running()) t->session.stop();
      t->cwd.clear();
    }
    root = fs::absolute(p).lexically_normal();
    dir = root;
    refreshTaskPicker(true);
    settings.set("general.lastFolder", root.u8string());
    refreshTree(false);
    index = {};
    symbolList->DeleteAllItems();
    calls->DeleteAllItems();
    try {
      startShell();
    } catch (const std::exception& e) {
      shell->screen.feed(std::string(e.what()) + "\r\n");
      shell->out->Refresh();
    }
    Json evt = Json::object();
    evt["path"] = root.u8string();
    plugins.emit("workspace.opened", evt.dump(0));
    schedule();
  }
  void schedule() {
    if (root.empty()) return;
    if (worker.valid()) {
      pending = true;
      return;
    }
    auto p = root;
    symbols::Options options;
    auto mode = plugins.pluginSetting("com.dodev.sourcesymbols", "parserMode");
    if (mode == "regex")
      options.mode = symbols::ParserMode::RegexOnly;
    else if (mode == "clang")
      options.mode = symbols::ParserMode::ClangOnly;
    options.clangExecutable =
        plugins.pluginSetting("com.dodev.sourcesymbols", "clangExecutable");
    try {
      options.timeoutMs =
          std::clamp(std::stoi(plugins.pluginSetting("com.dodev.sourcesymbols",
                                                     "timeoutMs")),
                     100, 60000);
    } catch (...) {
    }
    worker = std::async(std::launch::async, [p, options] {
      return symbols::indexWorkspace(p, options);
    });
    SetStatusText("Indexing workspace...");
  }
  void updateSymbols() {
    symbolList->DeleteAllItems();
    visible.clear();
    selected = static_cast<size_t>(-1);
    auto needle = S(filter->GetValue().Lower());
    for (size_t i = 0; i < index.rows.size(); ++i) {
      auto& r = index.rows[i];
      if (!allSymbols->GetValue() &&
          (!active() || index.paths[i] != docs.at(active()).path))
        continue;
      if (!needle.empty() && !W(r.name).Lower().Contains(W(needle))) continue;
      auto n = symbolList->InsertItem(symbolList->GetItemCount(), W(r.name));
      symbolList->SetItem(n, 1, W(r.kind));
      symbolList->SetItem(n, 2,
                          P(index.paths[i]) + ":" + W(std::to_string(r.line)));
      visible.push_back(i);
    }
  }
  void hierarchy() {
    if (selected >= index.rows.size()) return;
    callNodes = symbols::hierarchy(
        index, index.rows[selected].name, index.paths[selected],
        index.rows[selected].line, direction->GetSelection() == 1,
        callDepth->GetValue());
    calls->DeleteAllItems();
    std::vector<wxTreeItemId> stack;
    for (size_t i = 0; i < callNodes.size(); ++i) {
      auto& n = callNodes[i];
      auto text = W(n.name + " [" + n.status + "] ") + P(n.path.filename()) +
                  ":" + W(std::to_string(n.line));
      wxTreeItemId id;
      if (stack.empty())
        id = calls->AddRoot(text);
      else {
        id = calls->AppendItem(
            stack[std::min(n.depth ? n.depth - 1 : 0, stack.size() - 1)], text);
      }
      calls->SetItemData(id, new PathData(fs::path(std::to_string(i))));
      if (stack.size() > n.depth) stack.resize(n.depth);
      stack.push_back(id);
    }
    if (calls->GetRootItem().IsOk()) calls->Expand(calls->GetRootItem());
    side->SetSelection(4);
  }
  void searchNow() {
    auto o = searchOptions();
    previousSearch = o;
    matches->DeleteAllItems();
    hits.clear();
    if (o.query.empty()) return;
    auto r = search::scan(root, o, searchOverlays());
    if (!r.error.empty()) throw std::runtime_error(r.error);
    hits = std::move(r.matches);
    for (auto& m : hits) {
      auto i = matches->InsertItem(matches->GetItemCount(), P(m.path));
      matches->SetItem(i, 1, W(std::to_string(m.line)));
      matches->SetItem(i, 2, W(m.snippet));
    }
    SetStatusText(wxString::Format("%zu matches in %zu files", hits.size(),
                                   r.filesScanned) +
                  (r.truncated ? " (limited)" : ""));
  }
  std::unique_ptr<Terminal> terminal(const wxString& name) {
    auto t = std::make_unique<Terminal>();
    wxBoxSizer* s;
    auto p = page(bottom, name, s);
    auto ptr = t.get();
    auto row = new wxBoxSizer(wxHORIZONTAL);
    s->Add(row, 0, wxEXPAND);
    t->out = new WxTerminal(p, t->session, t->screen, [this, ptr] {
      if (ptr->taskMode) {
        wxLogWarning("Run the task again from Terminal > Run Task");
        return false;
      }
      if (ptr == debug.get()) {
        wxLogWarning("Start GDB from Run first");
        return false;
      }
      try {
        startTerminal(ptr);
        return true;
      } catch (const std::exception& e) {
        error(e);
        return false;
      }
    });
    s->Add(t->out, 1, wxEXPAND);
    button(p, row, "+", [this] { addTerminal(false); });
    button(p, row, "Split", [this] { addTerminal(true); });
    button(p, row, "Copy", [ptr] { ptr->out->copy(); });
    button(p, row, "Paste", [ptr] { ptr->out->paste(); });
    button(p, row, "Interrupt", [ptr] {
      ptr->taskQueue.clear();
      ptr->session.interrupt();
    });
    button(p, row, "Clear", [ptr] {
      ptr->screen.clear();
      ptr->out->clearSelection();
    });
    button(p, row, "Export", [this, ptr] {
      wxFileDialog d(this, "Export output", "", "terminal.txt", "Text|*.txt",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
      if (showModal(d) == wxID_OK) {
        std::ofstream f(F(d.GetPath()));
        f << ptr->screen.fullText();
      }
    });
    return t;
  }
  void exportCalls() {
    wxFileDialog d(this, "Export call hierarchy", P(root), "call-hierarchy.dot",
                   "Graphviz DOT|*.dot", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (showModal(d) != wxID_OK) return;
    std::ofstream out(F(d.GetPath()));
    if (!out) throw std::runtime_error("Cannot create graph");
    out << "digraph calls {\n";
    std::vector<size_t> parents;
    for (size_t i = 0; i < callNodes.size(); ++i) {
      const auto& n = callNodes[i];
      out << "n" << i
          << " [label=" << Json(n.name + " [" + n.status + "]").dump(0)
          << "];\n";
      if (n.depth && parents.size() >= n.depth)
        out << "n" << parents[n.depth - 1] << " -> n" << i << ";\n";
      if (parents.size() > n.depth) parents.resize(n.depth);
      parents.push_back(i);
    }
    out << "}\n";
  }
  void startShell() { startTerminal(shell.get()); }
  gdb::Context context() {
    auto e = active();
    return {root, e ? docs.at(e).path : fs::path{}};
  }
  void run() {
    if (debug->session.running()) {
      debug->session.sendLine("continue");
      return;
    }
    if (wxMessageBox("Debug configurations can execute workspace build "
                     "commands. Trust this workspace?",
                     "GDB", wxYES_NO | wxICON_WARNING, this) != wxYES)
      return;
    auto names = gdb::names(context());
    wxArrayString choices;
    for (auto& n : names) choices.Add(W(n));
    if (choices.empty()) throw std::runtime_error("No GDB configurations");
    wxSingleChoiceDialog d(this, "Choose debug configuration", "GDB", choices);
    if (showModal(d) != wxID_OK) return;
    auto l = gdb::makeLaunch(context(), S(d.GetStringSelection()), "gdb");
    if (l.needsPid) {
      auto pid = wxGetTextFromUser("Process ID", "Attach");
      if (pid.empty()) return;
      l = gdb::makeLaunch(context(), S(d.GetStringSelection()), "gdb", S(pid));
    }
    std::string err;
    auto executable = l.argv.front();
    debug->screen.resetSession();
    debug->out->clearSelection();
    if (!debug->session.start(l.cwd, &err, executable, l.argv))
      throw std::runtime_error(err);
    showTerminal(debug.get());
  }
  void tick() {
    pollTaskPicker();
    for (auto t : allTerminals()) {
      auto out = t->session.takeOutput();
      if (!out.empty()) {
        t->screen.feed(out);
        t->out->Refresh();
      }
      if (t->session.running())
        t->wasRunning = true;
      else if (t->wasRunning) {
        auto tail = t->session.takeOutput();
        if (!tail.empty()) t->screen.feed(tail);
        t->wasRunning = false;
        t->screen.feed("\r\n[Process exited: " +
                       std::to_string(t->session.exitCode()) + "]\r\n");
        t->out->Refresh();
        if (t->taskMode) {
          if (t->session.exitCode() == 0 && !t->taskQueue.empty()) {
            try {
              startNextTask(t);
            } catch (const std::exception& e) {
              t->taskQueue.clear();
              error(e);
            }
          } else {
            if (t->session.exitCode() != 0 && !t->taskQueue.empty())
              t->screen.feed("\r\n[Remaining tasks cancelled]\r\n");
            t->taskQueue.clear();
            t->out->Refresh();
          }
        }
      }
    }
    if (worker.valid() &&
        worker.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      try {
        auto r = worker.get();
        if (r.root == root) {
          index = std::move(r);
          updateSymbols();
          SetStatusText(W("Symbols: " + std::to_string(index.parsed) +
                          " parsed / " + std::to_string(index.hits) +
                          " cached"));
        }
      } catch (const std::exception& e) {
        error(e);
      }
      if (pending) {
        pending = false;
        schedule();
      }
    }
    if (settings.getBool("general.autoSave") &&
        std::chrono::steady_clock::now() - lastAutoSave >
            std::chrono::seconds(5)) {
      lastAutoSave = std::chrono::steady_clock::now();
      for (auto& d : docs)
        if (!d.second.path.empty() && d.first->GetModify()) try {
            save(d.first);
          } catch (const std::exception& e) {
            SetStatusText(W(e.what()));
          }
    }
  }

  search::Options searchOptions() const {
    search::Options o;
    o.query = S(query->GetValue());
    o.include = S(includePatterns->GetValue());
    o.exclude = S(excludePatterns->GetValue());
    o.regex = searchRegex->GetValue();
    o.caseSensitive = searchCase->GetValue();
    o.wholeWord = searchWord->GetValue();
    return o;
  }
  search::Options previousSearch;
  search::Overlays searchOverlays() {
    search::Overlays o;
    for (auto& d : docs)
      if (!d.second.path.empty()) o[d.second.path] = S(d.first->GetText());
    return o;
  }
  void replaceWorkspace(bool all) {
    auto o = searchOptions();
    if (o.query.empty() || o.query != previousSearch.query ||
        o.include != previousSearch.include ||
        o.exclude != previousSearch.exclude ||
        o.regex != previousSearch.regex ||
        o.caseSensitive != previousSearch.caseSensitive ||
        o.wholeWord != previousSearch.wholeWord)
      throw std::runtime_error("Run search again before replacing");
    auto selected =
        matches->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (!all && selected < 0) throw std::runtime_error("Select a result first");
    if (wxMessageBox(all ? "Replace matches in the listed files? Open "
                           "documents remain unsaved; closed files are updated "
                           "on disk. Make a backup first."
                         : "Replace the selected match in its editor?",
                     "Confirm replacement", wxYES_NO | wxICON_WARNING,
                     this) != wxYES)
      return;
    auto replacement = S(replacementText->GetValue());
    if (!all) {
      auto m = hits.at(selected);
      open(m.path);
      auto e = active();
      auto text = S(e->GetText());
      if (m.byteOffset + m.matchLength > text.size())
        throw std::runtime_error("File changed; rerun search");
      auto fresh = search::scan(root, o, searchOverlays());
      bool valid = false;
      for (auto& n : fresh.matches)
        if (n.path == m.path && n.byteOffset == m.byteOffset &&
            n.matchLength == m.matchLength && n.snippet == m.snippet) {
          valid = true;
          break;
        }
      if (!valid) throw std::runtime_error("Match changed; rerun search");
      std::string revised, err;
      size_t count = 0;
      if (!search::replaceText(text.substr(m.byteOffset, m.matchLength), o,
                               replacement, revised, count, &err) ||
          count != 1)
        throw std::runtime_error(
            err.empty()
                ? "Unable to replace this isolated match (try Replace All)"
                : err);
      e->SetTargetStart(m.byteOffset);
      e->SetTargetEnd(m.byteOffset + m.matchLength);
      e->ReplaceTarget(W(revised));
    } else {
      std::set<fs::path> paths;
      for (auto& m : hits) paths.insert(m.path);
      size_t count = 0, failed = 0;
      for (auto& p : paths) {
        std::string err, updated;
        size_t n = 0;
        wxStyledTextCtrl* e = nullptr;
        for (auto& d : docs)
          if (d.second.path == p) {
            e = d.first;
            break;
          }
        if (e) {
          if (search::replaceText(S(e->GetText()), o, replacement, updated, n,
                                  &err)) {
            if (n) {
              e->BeginUndoAction();
              e->SetTargetStart(0);
              e->SetTargetEnd(e->GetLength());
              e->ReplaceTarget(W(updated));
              e->EndUndoAction();
            }
            count += n;
          } else
            ++failed;
        } else if (search::replaceFile(p, o, replacement, n, &err))
          count += n;
        else
          ++failed;
      }
      wxMessageBox(
          wxString::Format("%zu replacements; %zu files failed", count, failed),
          "Workspace replace", wxOK, this);
    }
    searchNow();
    schedule();
  }
  void exportSymbols() {
    wxFileDialog d(this, "Export symbols", P(root), "symbols.csv", "CSV|*.csv",
                   wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (showModal(d) != wxID_OK) return;
    std::vector<std::vector<std::string>> rows{
        {"Entity", "Type", "File", "Line"}};
    for (auto i : visible) {
      auto& r = index.rows.at(i);
      rows.push_back({r.name, r.kind, index.paths.at(i).u8string(),
                      std::to_string(r.line)});
    }
    writeText(F(d.GetPath()), writeCsv(rows));
  }
  void findCall() {
    auto name = S(callQuery->GetValue());
    for (size_t i = 0; i < index.rows.size(); ++i)
      if (index.rows[i].name == name) {
        selected = i;
        hierarchy();
        return;
      }
    SetStatusText("No exact symbol found");
  }
  void rerootCall() {
    auto item = calls->GetSelection();
    if (!item.IsOk()) return;
    auto data = static_cast<PathData*>(calls->GetItemData(item));
    if (!data) return;
    auto n = callNodes.at(std::stoul(data->path.string()));
    for (size_t i = 0; i < index.rows.size(); ++i)
      if (index.rows[i].name == n.name && index.paths[i] == n.path &&
          index.rows[i].line == n.line) {
        selected = i;
        hierarchy();
        return;
      }
    for (size_t i = 0; i < index.rows.size(); ++i)
      if (index.rows[i].name == n.name) {
        selected = i;
        hierarchy();
        return;
      }
    SetStatusText("Unresolved call cannot be a root");
  }

#include "WxCommands.inc"
#include "WxSettings.inc"
#include "WxTools.inc"
 public:
  Frame()
      : wxFrame(nullptr, wxID_ANY, "DoDevEditor wx 5.1.4", wxDefaultPosition,
                wxSize(1250, 850)),
        aui(this),
        timer(this) {
    CreateStatusBar();
    editors = new wxAuiNotebook(
        this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ACTIVE_TAB);
    side = new wxAuiNotebook(this, wxID_ANY);
    bottom = new wxAuiNotebook(this, wxID_ANY);
    aui.AddPane(editors, wxAuiPaneInfo().CenterPane().Name("editors"));
    aui.AddPane(side, wxAuiPaneInfo()
                          .Left()
                          .Name("workspace")
                          .Caption("Workspace")
                          .BestSize(340, 500)
                          .MinSize(220, 150));
    aui.AddPane(bottom, wxAuiPaneInfo()
                            .Bottom()
                            .Name("output")
                            .Caption("Output")
                            .BestSize(700, 230)
                            .MinSize(200, 100));
    wxBoxSizer* s;
    auto p = page(side, "Files", s);
    auto bar = new wxBoxSizer(wxHORIZONTAL);
    s->Add(bar, 0, wxEXPAND);
    button(p, bar, "Folder", [this] {
      wxDirDialog d(this, "Open folder", P(root));
      if (showModal(d) == wxID_OK) workspace(F(d.GetPath()));
    });
    button(p, bar, "Refresh", [this] {
      refreshTree();
      schedule();
    });
    tree = new wxTreeCtrl(p, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                          wxTR_DEFAULT_STYLE);
    s->Add(tree, 1, wxEXPAND);
    auto images = new wxImageList(16, 16);
    images->Add(
        wxArtProvider::GetBitmap(wxART_FOLDER, wxART_OTHER, wxSize(16, 16)));
    images->Add(wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_OTHER,
                                         wxSize(16, 16)));
    tree->AssignImageList(images);
    tree->Bind(wxEVT_TREE_ITEM_EXPANDING,
               [this](wxTreeEvent& e) { populate(e.GetItem()); });
    tree->Bind(wxEVT_TREE_ITEM_ACTIVATED, [this](wxTreeEvent& e) {
      try {
        auto d = static_cast<PathData*>(tree->GetItemData(e.GetItem()));
        if (d && fs::is_regular_file(d->path))
          open(d->path);
        else if (d && wxui::explorer::canExpand(d->path, root)) {
          if (tree->IsExpanded(e.GetItem()))
            tree->Collapse(e.GetItem());
          else {
            populate(e.GetItem());
            tree->Expand(e.GetItem());
          }
        }
      } catch (const std::exception& ex) {
        error(ex);
      }
    });
    tree->Bind(wxEVT_TREE_ITEM_MENU, [this](wxTreeEvent& e) {
      if (!e.GetItem().IsOk() || !tree->GetItemData(e.GetItem())) return;
      tree->SelectItem(e.GetItem());
      contextMenu(tree, {{"New file...", "explorer.new_file"},
                         {"New directory...", "explorer.new_folder"},
                         {"Open", "explorer.open"},
                         {"Rename...", "explorer.rename"},
                         {"Delete...", "explorer.delete"},
                         {"Copy path", "explorer.copy_path"},
                         {"Open terminal here", "explorer.terminal"},
                         {"Show/hide hidden files", "explorer.hidden"},
                         {"Collapse directories", "explorer.collapse"},
                         {"Refresh explorer", "explorer.refresh"}});
    });
    p = page(side, "Search", s);
    query = new wxTextCtrl(p, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                           wxTE_PROCESS_ENTER);
    s->Add(query, 0, wxEXPAND);
    query->SetHint("Find in workspace");
    replacementText = new wxTextCtrl(p, wxID_ANY);
    replacementText->SetHint("Replace with");
    s->Add(replacementText, 0, wxEXPAND);
    auto searchFlags = new wxBoxSizer(wxHORIZONTAL);
    s->Add(searchFlags, 0, wxEXPAND);
    searchRegex = new wxCheckBox(p, wxID_ANY, "Regex");
    searchCase = new wxCheckBox(p, wxID_ANY, "Case");
    searchWord = new wxCheckBox(p, wxID_ANY, "Whole word");
    for (auto c : {searchRegex, searchCase, searchWord})
      searchFlags->Add(c, 0, wxALL, 2);
    includePatterns = new wxTextCtrl(p, wxID_ANY);
    includePatterns->SetHint("Include globs");
    s->Add(includePatterns, 0, wxEXPAND);
    excludePatterns = new wxTextCtrl(p, wxID_ANY);
    excludePatterns->SetHint("Exclude globs");
    s->Add(excludePatterns, 0, wxEXPAND);
    button(p, s, "Search workspace", [this] { searchNow(); });
    button(p, s, "Replace selected", [this] { replaceWorkspace(false); });
    button(p, s, "Replace all", [this] { replaceWorkspace(true); });
    matches = list(p, {"File", "Line", "Text"});
    s->Add(matches, 1, wxEXPAND);
    query->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) {
      try {
        searchNow();
      } catch (const std::exception& e) {
        error(e);
      }
    });
    matches->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& e) {
      auto& m = hits.at(e.GetIndex());
      try {
        open(m.path, m.line);
      } catch (const std::exception& ex) {
        error(ex);
      }
    });
    p = page(side, "Git", s);
    git = new wxTextCtrl(p, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                         wxTE_MULTILINE | wxTE_READONLY);
    button(p, s, "Git history", [this] { refreshGitView(); });
    button(p, s, "Git status (terminal)", [this] {
      if (!shell->session.running()) startShell();
      shell->session.sendLine("git status --short --branch");
      bottom->SetSelection(0);
    });
    s->Add(git, 1, wxEXPAND);
    p = page(side, "Symbols", s);
    filter = new wxTextCtrl(p, wxID_ANY);
    s->Add(filter, 0, wxEXPAND);
    allSymbols = new wxCheckBox(p, wxID_ANY, "Whole workspace");
    allSymbols->SetValue(true);
    s->Add(allSymbols, 0, wxALL, 3);
    allSymbols->Bind(wxEVT_CHECKBOX,
                     [this](wxCommandEvent&) { updateSymbols(); });
    button(p, s, "Export CSV", [this] { exportSymbols(); });
    button(p, s, "Reindex", [this] { schedule(); });
    button(p, s, "Call hierarchy", [this] { hierarchy(); });
    symbolList = list(p, {"Entity", "Type", "File:line"});
    s->Add(symbolList, 1, wxEXPAND);
    filter->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { updateSymbols(); });
    symbolList->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& e) {
      selected = visible.at(e.GetIndex());
    });
    symbolList->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& e) {
      auto i = visible.at(e.GetIndex());
      try {
        open(index.paths[i], index.rows[i].line);
      } catch (const std::exception& ex) {
        error(ex);
      }
    });
    p = page(side, "Calls", s);
    direction = new wxChoice(p, wxID_ANY);
    direction->Append("Outgoing");
    direction->Append("Incoming");
    direction->SetSelection(0);
    s->Add(direction, 0, wxEXPAND);
    callQuery = new wxTextCtrl(p, wxID_ANY);
    callQuery->SetHint("Exact function name");
    s->Add(callQuery, 0, wxEXPAND);
    button(p, s, "Find function", [this] { findCall(); });
    callDepth = new wxSpinCtrl(p, wxID_ANY);
    callDepth->SetRange(1, 15);
    callDepth->SetValue(8);
    s->Add(callDepth, 0, wxEXPAND);
    callDepth->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent&) { hierarchy(); });
    button(p, s, "Use selected as root", [this] { rerootCall(); });
    button(p, s, "Export DOT", [this] { exportCalls(); });
    direction->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { hierarchy(); });
    calls = new wxTreeCtrl(p, wxID_ANY);
    s->Add(calls, 1, wxEXPAND);
    calls->Bind(wxEVT_TREE_ITEM_ACTIVATED, [this](wxTreeEvent& e) {
      auto d = static_cast<PathData*>(calls->GetItemData(e.GetItem()));
      if (!d) return;
      auto& n = callNodes.at(std::stoul(d->path.string()));
      try {
        if (!n.path.empty()) open(n.path, n.line);
      } catch (const std::exception& ex) {
        error(ex);
      }
    });
    shell = terminal("Terminal");
    debug = terminal("GDB");
    settings.load();
    loadThemes();
    auto customTemplates = settings.getString("templates.directory");
    templates.loadDirectory(customTemplates.empty()
                                ? settings.directory() / "templates"
                                : fs::u8path(customTemplates));
    PluginManager::HostHooks hooks;
    hooks.activeDocument = [this](std::string& path, std::string& text) {
      auto a = active();
      if (!a) return false;
      path = docs.at(a).path.u8string();
      text = S(a->GetText());
      return true;
    };
    hooks.replaceDocument = [this](const std::string& text) {
      auto a = active();
      if (!a) return false;
      a->SetText(W(text));
      return true;
    };
    hooks.terminalCommand = [this](const std::string& cmd) {
      if (root.empty()) return false;
      sendShell(cmd);
      return true;
    };
    hooks.openSourceSymbols = [this] {
      showSide(3);
      return true;
    };
    hooks.openAiAssistant = [this] { return aiPanel(); };
    hooks.openSerialMonitor = [this] { return serialPanel(); };
    hooks.openRemoteJournal = [this] { return journalPanel(); };
    hooks.debugContext = [this](std::string& w, std::string& f, int& line) {
      auto c = context();
      w = c.workspace.u8string();
      f = c.file.u8string();
      auto a = active();
      line = a ? a->GetCurrentLine() + 1 : 1;
      return !w.empty();
    };
    hooks.debugLaunch = [this](const std::string& cwd,
                               const std::vector<std::string>& argv) {
      if (argv.empty() || debug->session.running()) return false;
      if (wxMessageBox(
              "Allow this debugger configuration to run workspace commands?",
              "GDB", wxYES_NO | wxICON_WARNING, this) != wxYES)
        return false;
      std::string err;
      debug->screen.resetSession();
      debug->out->clearSelection();
      bool ok = debug->session.start(fs::u8path(cwd), &err, argv.front(), argv);
      if (ok) {
        showTerminal(debug.get());
      } else
        wxLogError("%s", W(err));
      return ok;
    };
    hooks.debugControl = [this](const std::string& cmd) {
      if (cmd == "@state") return debug->session.running() ? 1 : 0;
      if (!debug->session.running()) return 0;
      if (cmd == "@stop") {
        debug->session.stop();
        return 1;
      }
      if (cmd == "@pause") return debug->session.interrupt() ? 1 : -1;
      return debug->session.sendLine(cmd) ? 1 : -1;
    };
    hooks.debugOpenFile = [this](const std::string& path) {
      try {
        open(fs::u8path(path));
        return true;
      } catch (const std::exception& e) {
        error(e);
        return false;
      }
    };
    hooks.debugStatus = [this](const std::string& msg) {
      SetStatusText(W(msg));
      if (msg.rfind("GDB error:", 0) == 0)
        wxMessageBox(W(msg), "GDB", wxOK | wxICON_ERROR, this);
    };
    hooks.promptInput = [this](const std::string& title,
                               const std::string& label,
                               const std::string& initial,
                               std::function<void(const std::string&)> cb) {
      wxTextEntryDialog d(this, W(label), W(title), W(initial));
      if (showModal(d) != wxID_OK) return false;
      cb(S(d.GetValue()));
      return true;
    };
    plugins.setHostHooks(std::move(hooks));
    auto customPlugins = settings.getString("plugins.directory");
    plugins.discoverAndLoad(std::vector<fs::path>{
        customPlugins.empty() ? settings.directory() / "plugins"
                              : fs::u8path(customPlugins),
        bundledPluginDirectory()});
    for (auto& b : std::map<std::string, std::string>{
             {"gdb.start", "F5"},
             {"gdb.stop", "Shift+F5"},
             {"gdb.restart", "Ctrl+Shift+F5"},
             {"gdb.breakpoint", "F9"},
             {"gdb.next", "F10"},
             {"gdb.step", "F11"},
             {"gdb.finish", "Shift+F11"},
             {"plugin.view.gdb.debug", "Ctrl+Shift+D"}})
      keybindings.registerCommand(b.first, b.second);
    for (auto& cmd : plugins.commands()) keybindings.registerCommand(cmd.id);
    for (auto& v : plugins.views())
      keybindings.registerCommand("plugin.view." + v.id);
    keybindings.load();
    buildMenus();
    buildToolbar();
    Bind(wxEVT_CHAR_HOOK, &Frame::keyHook, this);
    bottom->Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE,
                 [](wxAuiNotebookEvent& e) { e.Veto(); });
    side->Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE,
               [](wxAuiNotebookEvent& e) { e.Veto(); });
    editors->Bind(
        wxEVT_AUINOTEBOOK_TAB_RIGHT_DOWN, [this](wxAuiNotebookEvent& e) {
          if (e.GetSelection() >= 0) editors->SetSelection(e.GetSelection());
          contextMenu(editors, {{"Save", "file.save"},
                                {"Close", "file.close"},
                                {"Close others", "tab.close_others"},
                                {"Close to the right", "tab.close_right"},
                                {"Copy path", "tab.copy_path"},
                                {"Reveal in explorer", "explorer.reveal"}});
        });
    editors->Bind(wxEVT_AUINOTEBOOK_PAGE_CHANGED,
                  [this](wxAuiNotebookEvent& e) {
                    if (!allSymbols->GetValue()) updateSymbols();
                    e.Skip();
                  });
    editors->Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, [this](wxAuiNotebookEvent& e) {
      auto a =
          dynamic_cast<wxStyledTextCtrl*>(editors->GetPage(e.GetSelection()));
      if (!confirm(a))
        e.Veto();
      else
        docs.erase(a);
    });
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) {
      for (auto& d : docs)
        if (!confirm(d.first) && e.CanVeto()) {
          e.Veto();
          return;
        }
      wxConfig cfg("DoDevEditorWx");
      cfg.Write("perspective", aui.SavePerspective());
      cfg.Flush();
      persistSession();
      timer.Stop();
      Destroy();
    });
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { tick(); });
    aui.Update();
    wxConfig cfg("DoDevEditorWx");
    wxString perspective;
    if (cfg.Read("perspective", &perspective)) aui.LoadPerspective(perspective);
    auto name = settings.getString("appearance.theme", "VS Code Dark Modern");
    if (!themes.find(name) || !fs::exists(settings.file()))
      name = "VS Code Dark Modern";
    settings.set("appearance.theme", name);
    selectTheme(name, false);
    timer.Start(30);
  }
  ~Frame() {
    timer.Stop();
    plugins.shutdown();
    aui.UnInit();
  }
  void start(const std::vector<fs::path>& paths) {
    auto p = fs::current_path();
    if (paths.empty() && settings.getBool("general.restoreSession", true)) {
      auto last = settings.getString("general.lastFolder");
      if (!last.empty() && fs::is_directory(fs::u8path(last)))
        p = fs::u8path(last);
    }
    for (auto& a : paths)
      if (fs::is_directory(a)) {
        p = a;
        break;
      } else if (fs::is_regular_file(a)) {
        p = a.parent_path();
        break;
      }
    workspace(p);
    for (auto& a : paths)
      if (fs::is_regular_file(a)) open(a);
    if (paths.empty() && settings.getBool("general.restoreSession", true)) {
      for (auto& item : settings.get("wxSession.files").asArray()) {
        auto f = fs::u8path(item.string());
        if (fs::is_regular_file(f)) try {
            open(f);
          } catch (const std::exception& e) {
            SetStatusText(W(e.what()));
          }
      }
    }
    if (editors->GetPageCount() == 0) newDoc();
  }
};
static std::vector<fs::path> startup;
class App : public wxApp {
 public:
  bool OnInit() override {
    auto f = new Frame;
    f->Show();
    try {
      f->start(startup);
    } catch (const std::exception& e) {
      wxLogError("%s", W(e.what()));
    }
    return true;
  }
};
wxIMPLEMENT_APP_NO_MAIN(App);
int main(int argc, char** argv) {
  return dodev::launch::startEditor(argc, argv, true, [](int n, char** a) {
    for (int i = 1; i < n; ++i)
      if (std::string(a[i]) != "--") startup.push_back(fs::u8path(a[i]));
    int count = 1;
    return wxEntry(count, a);
  });
}
