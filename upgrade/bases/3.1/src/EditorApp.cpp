#include "EditorApp.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "EmbeddedSettingsUi.h"
#include "EmbeddedUi.h"

namespace dodev {
namespace {
std::string baseName(const fs::path& p) { return pathUtf8(p.filename()); }
std::string dataOf(Rid editor) {
  if (editor == R_INVALID_ID || !Rwidget_is_valid(editor)) return {};
  const char* p = Rtext_editor_get_text(editor);
  return p ? std::string(p) : std::string();
}
std::string inputOf(Rid window, const char* name) {
  const Rid id = Rwidget_find(window, name);
  if (id == R_INVALID_ID) return {};
  const char* p = Rtext_input_get_text(id);
  return p ? std::string(p) : std::string();
}
std::string trim(std::string s) {
  const auto x = s.find_first_not_of(" \r\n\t");
  if (x == std::string::npos) return {};
  const auto y = s.find_last_not_of(" \r\n\t");
  return s.substr(x, y - x + 1);
}
void addTableRow(Rid table, const std::vector<std::string>& row,
                 std::size_t cols) {
  const int r = Rtable_add_row(table, nullptr);
  for (std::size_t c = 0; c < std::min(cols, row.size()); ++c)
    Rtable_set_cell(table, r, static_cast<int>(c), row[c].c_str());
}
std::size_t absolutePos(const std::string& text, int line, int column) {
  std::size_t pos = 0;
  for (int i = 0; i < line && pos < text.size(); ++i) {
    const auto p = text.find('\n', pos);
    if (p == std::string::npos) return text.size();
    pos = p + 1;
  }
  return std::min(text.size(),
                  pos + static_cast<std::size_t>(std::max(0, column)));
}
std::pair<int, int> lineColumn(const std::string& s, std::size_t pos) {
  int line = 0, column = 0;
  for (std::size_t i = 0; i < std::min(pos, s.size()); ++i) {
    if (s[i] == '\n') {
      ++line;
      column = 0;
    } else
      ++column;
  }
  return {line, column};
}
}  // namespace

int EditorApp::run(int argc, char** argv) {
  if (!Rinit(R_BACKEND_VULKAN)) {
    std::cerr
        << "No se pudo iniciar Raven GUI (controlador Vulkan/ventanas).\n";
    return 1;
  }
  try {
    loadPrefs();
    for (const auto& theme : ui::kThemes) {
      ThemeDefinition definition;
      if (ThemeDefinition::fromXml(std::string(theme.xml), definition))
        themeRegistry_.add(std::move(definition));
    }
    loadUserThemes();
    std::vector<std::string> warnings;
    const auto customTemplates = settings_.getString("templates.directory");
    if (!customTemplates.empty())
      templateRegistry_.loadDirectory(fs::u8path(customTemplates), &warnings);
    else
      templateRegistry_.loadDirectory(settings_.directory() / "templates",
                                      &warnings);
    const auto pluginsPath = settings_.getString("plugins.directory");
    pluginManager_.discoverAndLoad(pluginsPath.empty()
                                       ? settings_.directory() / "plugins"
                                       : fs::u8path(pluginsPath));
    for (const auto& name : themeRegistry_.names()) {
      if (const auto* th = themeRegistry_.find(name)) {
        const auto xml = th->toXml();
        if (!Rtheme_load_from_memory(xml.c_str(), xml.size()))
          std::cerr << "Theme could not load: " << name << '\n';
      }
    }
    if (!Rtheme_use(theme_.c_str())) {
      theme_ = "Music Nocturne";
      Rtheme_use(theme_.c_str());
    }
    applyFonts();
    initUi();
    refreshThemeDecor();
    bindActions();
    registerPluginUi();
    applyEditorOptions();
    if (!folder_.empty()) setFolder(folder_);
    if (argc > 1) {
      const auto item = fs::u8path(argv[1]);
      if (fs::is_directory(item))
        setFolder(item);
      else
        opened(item);
    } else
      newDocument();
    newTerminal();
    Rtimer_create(0.075, R_TRUE, &EditorApp::timerCb, this);
    while (Rwait_frame(-1.0)) {
    }
    savePrefs();
    pluginManager_.emit("app.shutting_down", "{}");
    for (auto& t : terminals_) t->session.stop();
    const int exitCode = Rget_exit_code();
    pluginManager_.shutdown();
    Rshutdown();
    return exitCode;
  } catch (const std::exception& e) {
    std::cerr << "Error fatal: " << e.what() << '\n';
    pluginManager_.shutdown();
    Rshutdown();
    return 2;
  }
}
void EditorApp::initUi() {
  window_ =
      Rwindow_create_from_xml(ui::kMainWindow.data(), ui::kMainWindow.size());
  if (window_ == R_INVALID_ID)
    throw std::runtime_error("Raven rechazo el XML integrado de la ventana");
  tabs_ = Rwidget_find(window_, "documents");
  dock_ = Rwidget_find(window_, "dock");
  explorer_ = Rwidget_find(window_, "explorer_pane");
  git_pane_ = Rwidget_find(window_, "git_pane");
  output_pane_ = Rwidget_find(window_, "output_pane");
  status_ = Rwidget_find(window_, "status");
  browser_ = Rwidget_find(window_, "files");
  terminal_tabs_ = Rwidget_find(window_, "terminal_tabs");
  terminal_state_ = Rwidget_find(window_, "terminal_state");
  terminal_profile_ = Rwidget_find(window_, "terminal_profile");
  git_list_ = Rwidget_find(window_, "git_commits");
  git_detail_ = Rwidget_find(window_, "git_detail");
  cursor_ = Rwidget_find(window_, "cursor");
  themes_ = Rwidget_find(window_, "themes");
  if (tabs_ == R_INVALID_ID || browser_ == R_INVALID_ID ||
      terminal_tabs_ == R_INVALID_ID || themes_ == R_INVALID_ID)
    throw std::runtime_error("Faltan controles esenciales del XML de Raven");
  Rtab_view_set_documents(tabs_, R_TRUE);
  Rtab_view_set_changed_callback(tabs_, &EditorApp::tabCb, this);
  Rtab_view_set_close_callback(tabs_, &EditorApp::tabCloseCb, this);
  Rwindow_set_close_callback(window_, &EditorApp::windowCloseCb, this);
  Rfile_browser_set_controls_visible(browser_, R_FALSE);
  Rfile_browser_set_places_visible(browser_, R_FALSE);
  Rfile_browser_set_accepted_callback(browser_, &EditorApp::browserCb, this);
  Rlist_view_set_selection_changed_callback(git_list_, &EditorApp::gitCb, this);
  Rtab_view_set_changed_callback(terminal_tabs_, &EditorApp::terminalTabCb,
                                 this);
  Rtab_view_set_close_callback(terminal_tabs_, &EditorApp::terminalTabCloseCb,
                               this);
  Rtab_view_set_documents(terminal_tabs_, R_TRUE);
#ifdef _WIN32
  for (const char* profile : {"PowerShell", "PowerShell 7", "CMD"})
    Rcombo_box_add_item(terminal_profile_, profile);
#else
  for (const char* profile : {"Predeterminada", "bash", "zsh", "fish", "sh"})
    Rcombo_box_add_item(terminal_profile_, profile);
#endif
  Rcombo_box_set_selected(terminal_profile_, 0);
  refreshTasks();
  Rtext_editor_set_read_only(git_detail_, R_TRUE);
  refreshThemeChoices();
  Rcombo_box_set_changed_callback(themes_, &EditorApp::themeCb, this);
  Rdock_area_set_close_hides(dock_, R_TRUE);
  Rdock_area_set_panel_closable(dock_, explorer_, R_FALSE);
  setStatus(
      "DoDevEditor Raven 3.1 · VS Code menus · JSON Settings · Themes · "
      "Templates · Plugins");
}
void EditorApp::bindActions() {
  static constexpr const char* actions[] = {"file.new",
                                            "file.open",
                                            "file.save",
                                            "file.save_as",
                                            "file.close",
                                            "folder.open",
                                            "folder.refresh",
                                            "edit.undo",
                                            "edit.redo",
                                            "edit.cut",
                                            "edit.copy",
                                            "edit.paste",
                                            "edit.delete",
                                            "edit.find_workspace",
                                            "edit.toggle_line_comment",
                                            "edit.toggle_block_comment",
                                            "edit.uppercase",
                                            "edit.lowercase",
                                            "edit.titlecase",
                                            "edit.indent",
                                            "edit.outdent",
                                            "edit.insert_above",
                                            "edit.insert_below",
                                            "edit.delete_line",
                                            "edit.join_lines",
                                            "edit.trim_whitespace",
                                            "edit.sort_asc",
                                            "edit.sort_desc",
                                            "edit.copy_line_up",
                                            "edit.copy_line_down",
                                            "edit.move_line_up",
                                            "edit.move_line_down",
                                            "edit.duplicate",
                                            "selection.all",
                                            "selection.line",
                                            "selection.word",
                                            "selection.expand",
                                            "selection.shrink",
                                            "selection.next_occurrence",
                                            "selection.prev_occurrence",
                                            "selection.line_start",
                                            "selection.line_end",
                                            "selection.to_start",
                                            "selection.to_end",
                                            "selection.swap_anchor",
                                            "selection.cursor_up",
                                            "selection.cursor_down",
                                            "search.show",
                                            "search.hide",
                                            "search.next",
                                            "search.prev",
                                            "search.replace_show",
                                            "search.replace",
                                            "search.replace_all",
                                            "tool.csv",
                                            "tool.dlt",
                                            "tool.data_server",
                                            "git.refresh",
                                            "git.details",
                                            "panel.explorer",
                                            "panel.git",
                                            "panel.output",
                                            "terminal.new",
                                            "terminal.close",
                                            "terminal.interrupt",
                                            "terminal.clear",
                                            "terminal.copy",
                                            "terminal.paste",
                                            "terminal.split",
                                            "terminal.restart",
                                            "terminal.next",
                                            "terminal.previous",
                                            "terminal.rename",
                                            "terminal.focus",
                                            "terminal.run_task",
                                            "terminal.run_build",
                                            "terminal.run_file",
                                            "terminal.run_selected",
                                            "terminal.configure_tasks",
                                            "terminal.configure_build",
                                            "theme.next",
                                            "settings.show",
                                            "settings.apply",
                                            "settings.cancel",
                                            "settings.defaults",
                                            "settings.browse_editor_font",
                                            "settings.browse_ui_font",
                                            "settings.browse_terminal_font",
                                            "settings.browse_themes",
                                            "settings.browse_templates",
                                            "settings.browse_plugins",
                                            "theme.designer",
                                            "theme.import",
                                            "theme.export",
                                            "designer.preview",
                                            "designer.save_json",
                                            "designer.save_xml",
                                            "designer.close",
                                            "template.show",
                                            "template.create",
                                            "template.cancel",
                                            "plugin.enable",
                                            "plugin.disable",
                                            "plugin.rescan",
                                            "plugin.setting.save",
                                            "app.about",
                                            "app.exit"};
  for (const char* a : actions) Raction_bind(a, &EditorApp::actionCb, this);
}
void EditorApp::actionCb(Rid, const char* action, void* user_data) {
  auto& app = *static_cast<EditorApp*>(user_data);
  try {
    app.handleAction(action ? action : "");
  } catch (const std::exception& e) {
    app.setStatus(e.what());
    app.log(std::string("ERROR: ") + e.what() + "\n");
  }
}
void EditorApp::handleAction(const std::string& name) {
  if (name == "settings.show")
    openSettings();
  else if (name == "settings.apply")
    applySettings();
  else if (name == "settings.cancel") {
    if (settingsWindow_ != R_INVALID_ID && Rwindow_is_valid(settingsWindow_)) {
      Rwindow_destroy(settingsWindow_);
      settingsWindow_ = R_INVALID_ID;
    }
  } else if (name == "settings.defaults")
    applySettings(true);
  else if (name.rfind("settings.browse_", 0) == 0)
    browseSetting(name);
  else if (name == "template.show")
    openTemplateWindow();
  else if (name == "template.create")
    createTemplateDocument();
  else if (name == "template.cancel") {
    if (templateWindow_ != R_INVALID_ID && Rwindow_is_valid(templateWindow_)) {
      Rwindow_destroy(templateWindow_);
      templateWindow_ = R_INVALID_ID;
    }
  } else if (name == "theme.designer")
    openThemeDesigner();
  else if (name == "theme.import" || name == "theme.export" ||
           name.rfind("designer.", 0) == 0)
    themeDesignerAction(name);
  else if (name.rfind("plugin.", 0) == 0)
    pluginAction(name);
  else if (name == "file.new")
    newDocument();
  else if (name == "file.open")
    openDialog();
  else if (name == "file.save") {
    if (auto* d = active()) save(*d);
  } else if (name == "file.save_as") {
    if (auto* d = active()) save(*d, true);
  } else if (name == "file.close")
    closeActive();
  else if (name == "folder.open")
    openFolderDialog();
  else if (name == "folder.refresh") {
    Rfile_browser_refresh(browser_);
    refreshGit();
  } else if (name == "edit.undo") {
    if (auto* d = active(); d && !d->read_only) Rtext_editor_undo(d->editor);
  } else if (name == "edit.redo") {
    if (auto* d = active(); d && !d->read_only) Rtext_editor_redo(d->editor);
  } else if (name == "search.show")
    showSearch(true);
  else if (name == "search.hide")
    showSearch(false);
  else if (name == "search.next")
    searchNext();
  else if (name == "search.prev")
    searchNext(false, false, true);
  else if (name == "search.replace_show") {
    showSearch(true);
    Rwidget_focus(Rwidget_find(window_, "replace_text"));
  } else if (name == "search.replace")
    searchNext(true);
  else if (name == "search.replace_all")
    searchNext(false, true);
  else if (name == "tool.csv" || name == "tool.dlt")
    openDialog();
  else if (name == "tool.data_server")
    dataServerDocument();
  else if (name == "git.refresh") {
    Rdock_area_open_panel(dock_, git_pane_);
    refreshGit();
  } else if (name == "git.details")
    gitSelected(Rlist_view_get_selected(git_list_));
  else if (name == "panel.explorer")
    Rdock_area_open_panel(dock_, explorer_);
  else if (name == "panel.git")
    Rdock_area_open_panel(dock_, git_pane_);
  else if (name == "panel.output") {
    Rdock_area_open_panel(dock_, output_pane_);
    if (terminals_.empty())
      newTerminal();
    else
      focusTerminal();
  } else if (name.rfind("edit.", 0) == 0 || name.rfind("selection.", 0) == 0)
    dispatchEditSelection(name);
  else if (name.rfind("terminal.", 0) == 0)
    dispatchTerminalMenu(name);
  else if (name == "theme.next") {
    if (!themeNames_.empty()) {
      const int i = Rcombo_box_get_selected(themes_);
      applyTheme((i + 1) % static_cast<int>(themeNames_.size()));
    }
  } else if (name == "app.about")
    Rmessage_box(
        window_, "Acerca de DoDevEditor",
        "DoDevEditor Raven GUI 3.1\nC++17 · XML empotrado · once temas y "
        "terminal integrada\nEditor, CSV, DLT, Git y explorador de "
        "archivos.\n\nInterfaz completamente Raven (sin wxWidgets).",
        R_MESSAGE_OK);
  else if (name == "app.exit") {
    if (allowExit()) Rquit(0);
  } else if (name.rfind("plugin.view.", 0) == 0)
    openPluginView(name.substr(12));
  else if (!pluginManager_.execute(name))
    setStatus("Unknown action: " + name);
}
Document* EditorApp::findDocument(Rid id) {
  for (auto& d : docs_)
    if (d->page == id || d->editor == id) return d.get();
  return nullptr;
}
Document* EditorApp::active() {
  const int idx = Rtab_view_get_active(tabs_);
  if (idx < 0) return nullptr;
  return findDocument(Rtab_view_get_tab(tabs_, idx));
}
Document& EditorApp::addDocument(DocumentKind kind, const std::string& title,
                                 const fs::path& path) {
  auto d = std::make_unique<Document>();
  d->app = this;
  d->kind = kind;
  d->title = title;
  d->path = path;
  Document& ref = *d;
  docs_.push_back(std::move(d));
  return ref;
}
void EditorApp::newDocument() {
  auto& d = addDocument(DocumentKind::Text,
                        "Sin titulo " + std::to_string(++untitled_count_));
  d.page = d.editor = Rtext_editor_create("");
  Rwidget_set_class(d.editor, "music-editor");
  Rtext_editor_set_line_numbers(
      d.editor,
      settings_.getBool("editor.lineNumbers", true) ? R_TRUE : R_FALSE);
  Rtext_editor_set_tab_inserts_spaces(
      d.editor,
      settings_.getBool("editor.insertSpaces", true) ? R_TRUE : R_FALSE);
  Rtext_editor_set_tab_size(d.editor, settings_.getInt("editor.tabSize", 4));
  Rtext_editor_set_word_wrap(
      d.editor, settings_.getBool("editor.wordWrap") ? R_TRUE : R_FALSE);
  Rtext_editor_set_font(
      d.editor, editorFont_,
      static_cast<float>(settings_.getInt("editor.fontSize", 14)));
  Rtext_editor_set_placeholder(
      d.editor, "Escribe aqui: C++, C, Python, XML, JSON y mas...");
  Rtext_editor_set_changed_callback(d.editor, &EditorApp::textCb, this);
  Rtab_view_add_tab(tabs_, d.title.c_str(), d.page);
  Rtab_view_show_tab(tabs_, d.page);
  Rwidget_focus(d.editor);
  setStatus("Nuevo documento: " + d.title);
}
void EditorApp::opened(const fs::path& path) {
  if (!fs::exists(path) || !fs::is_regular_file(path)) {
    setStatus("No es un archivo regular: " + pathUtf8(path));
    return;
  }
  const fs::path real = fs::weakly_canonical(path);
  for (auto& d : docs_)
    if (!d->path.empty() && fs::weakly_canonical(d->path) == real) {
      Rtab_view_show_tab(tabs_, d->page);
      return;
    }
  const auto ext = fileExtension(real);
  const auto size = fs::file_size(real);
  if (size > 30 * 1024 * 1024) {
    setStatus("Archivo superior a 30 MB; no se abre para evitar bloqueo.");
    return;
  }
  const auto bytes = readBytes(real);
  if (ext == ".dlt") {
    dltDocument(real, bytes);
    return;
  }
  const auto content = std::string(bytes.begin(), bytes.end());
  if (ext == ".csv") {
    csvDocument(real, content);
    return;
  }
  const bool binary = looksBinary(bytes);
  auto& d = addDocument(binary ? DocumentKind::Binary : DocumentKind::Text,
                        baseName(real), real);
  d.read_only = binary;
  d.page = d.editor =
      Rtext_editor_create(binary ? hexDump(bytes).c_str() : content.c_str());
  Rwidget_set_class(d.editor, "music-editor");
  Rtext_editor_set_line_numbers(
      d.editor,
      settings_.getBool("editor.lineNumbers", true) ? R_TRUE : R_FALSE);
  Rtext_editor_set_tab_inserts_spaces(
      d.editor,
      settings_.getBool("editor.insertSpaces", true) ? R_TRUE : R_FALSE);
  Rtext_editor_set_tab_size(d.editor, settings_.getInt("editor.tabSize", 4));
  Rtext_editor_set_word_wrap(
      d.editor, settings_.getBool("editor.wordWrap") ? R_TRUE : R_FALSE);
  Rtext_editor_set_font(
      d.editor, editorFont_,
      static_cast<float>(settings_.getInt("editor.fontSize", 14)));
  if (binary)
    Rtext_editor_set_read_only(d.editor, R_TRUE);
  else {
    const char* lang = Rlanguage_for_file(pathUtf8(real).c_str());
    if (lang) Rtext_editor_set_language(d.editor, lang);
    Rtext_editor_set_changed_callback(d.editor, &EditorApp::textCb, this);
  }
  Rtab_view_add_tab(tabs_, d.title.c_str(), d.page);
  Rtab_view_show_tab(tabs_, d.page);
  Rwidget_focus(d.editor);
  setStatus("Abierto: " + pathUtf8(real) +
            (binary ? " (HEX, solo lectura)" : ""));
  Json evt = Json::object();
  evt["path"] = pathUtf8(real);
  pluginManager_.emit("document.opened", evt.dump(0));
}
void EditorApp::csvDocument(const fs::path& path, const std::string& content) {
  auto& d = addDocument(DocumentKind::Csv, baseName(path), path);
  d.page = Rwidget_create(R_WIDGET_VBOX);
  auto* note =
      "Vista tabular + fuente CSV editable (el guardado usa el texto fuente).";
  Rwidget_add_child(d.page, Rlabel_create(note));
  d.table = Rwidget_create(R_WIDGET_TABLE);
  Rwidget_add_child(d.page, d.table);
  Rslot_set_fill(d.table, 0.48f);
  d.editor = Rtext_editor_create(content.c_str());
  Rwidget_set_class(d.editor, "music-editor");
  Rwidget_add_child(d.page, d.editor);
  Rslot_set_fill(d.editor, 0.52f);
  Rtext_editor_set_line_numbers(
      d.editor,
      settings_.getBool("editor.lineNumbers", true) ? R_TRUE : R_FALSE);
  Rtext_editor_set_tab_inserts_spaces(
      d.editor,
      settings_.getBool("editor.insertSpaces", true) ? R_TRUE : R_FALSE);
  Rtext_editor_set_tab_size(d.editor, settings_.getInt("editor.tabSize", 4));
  Rtext_editor_set_word_wrap(
      d.editor, settings_.getBool("editor.wordWrap") ? R_TRUE : R_FALSE);
  Rtext_editor_set_font(
      d.editor, editorFont_,
      static_cast<float>(settings_.getInt("editor.fontSize", 14)));
  Rtext_editor_set_changed_callback(d.editor, &EditorApp::textCb, this);
  refreshCsv(d);
  Rtab_view_add_tab(tabs_, d.title.c_str(), d.page);
  Rtab_view_show_tab(tabs_, d.page);
  setStatus("CSV abierto: " + pathUtf8(path));
}
void EditorApp::refreshCsv(Document& d) {
  if (d.table == R_INVALID_ID) return;
  const auto csv = parseCsv(dataOf(d.editor));
  Rtable_clear_columns(d.table);
  if (!csv.valid) {
    setStatus(csv.error);
    return;
  }
  std::size_t cols = 0;
  for (const auto& row : csv.rows) cols = std::max(cols, row.size());
  cols = std::min<std::size_t>(cols, 60);
  for (std::size_t c = 0; c < cols; ++c)
    Rtable_add_column(d.table, ("C" + std::to_string(c + 1)).c_str(), 150);
  const auto n = std::min<std::size_t>(csv.rows.size(), 5000);
  for (std::size_t r = 0; r < n; ++r) addTableRow(d.table, csv.rows[r], cols);
  if (csv.rows.size() > n)
    setStatus(
        "Vista CSV limitada a 5000 filas; editor conserva el archivo "
        "completo.");
}
void EditorApp::dltDocument(const fs::path& path,
                            const std::vector<std::uint8_t>& content) {
  auto& d = addDocument(DocumentKind::Dlt, baseName(path), path);
  d.read_only = true;
  d.page = Rwidget_create(R_WIDGET_VBOX);
  Rid toolbar = Rwidget_create(R_WIDGET_HBOX);
  Rwidget_add_child(d.page, toolbar);
  Rwidget_add_child(toolbar, Rlabel_create("APID: "));
  d.filter = Rwidget_create(R_WIDGET_TEXT_INPUT);
  Rtext_input_set_placeholder(d.filter, "Todos los APID");
  Rwidget_add_child(toolbar, d.filter);
  Rtext_input_set_committed_callback(d.filter, &EditorApp::dltFilterCb, &d);
  Rid filterBtn = Rbutton_create("Filtrar");
  Rwidget_add_child(toolbar, filterBtn);
  Rbutton_set_clicked_callback(filterBtn, &EditorApp::dltRefreshCb, &d);
  d.table = Rwidget_create(R_WIDGET_TABLE);
  Rwidget_add_child(d.page, d.table);
  Rslot_set_fill(d.table, 1);
  Rtable_add_column(d.table, "#", 65);
  Rtable_add_column(d.table, "Tiempo", 165);
  Rtable_add_column(d.table, "ECU", 95);
  Rtable_add_column(d.table, "APID", 95);
  Rtable_add_column(d.table, "CTID", 95);
  Rtable_add_column(d.table, "Payload", 540);
  const auto parsed = parseDlt(content);
  d.dlt_entries = parsed.entries;
  filterDlt(d);
  Rtab_view_add_tab(tabs_, d.title.c_str(), d.page);
  Rtab_view_show_tab(tabs_, d.page);
  if (!parsed.error.empty()) log(parsed.error + "\n");
  setStatus("DLT: " + std::to_string(d.dlt_entries.size()) +
            " mensajes (solo lectura)");
}
void EditorApp::filterDlt(Document& d) {
  if (d.kind != DocumentKind::Dlt || !Rwidget_is_valid(d.table)) return;
  const char* t = Rtext_input_get_text(d.filter);
  const std::string filter = t ? t : "";
  // Table conserva columnas. Vaciamos las filas una por una, del final hacia
  // atras.
  while (Rtable_get_row_count(d.table) > 0)
    Rtable_remove_row(d.table, Rtable_get_row_count(d.table) - 1);
  for (std::size_t i = 0; i < d.dlt_entries.size(); ++i) {
    const auto& e = d.dlt_entries[i];
    if (!filter.empty() && e.apid.find(filter) == std::string::npos) continue;
    addTableRow(
        d.table,
        {std::to_string(i + 1), e.timestamp, e.ecu, e.apid, e.ctid, e.payload},
        6);
  }
}
void EditorApp::dataServerDocument() {
  auto& d = addDocument(DocumentKind::DataServer, "Data Server");
  d.read_only = true;
  d.page = Rwidget_create(R_WIDGET_VBOX);
  Rid title = Rlabel_create("Data Server · modulo experimental");
  Rlabel_set_text_size(title, 20);
  Rwidget_add_child(d.page, title);
  Rwidget_add_child(
      d.page,
      Rlabel_create(
          "La implementacion wxWidgets original contiene clases Start/Stop sin "
          "codigo de red.\nSe conserva su diseño funcional como panel nativo "
          "Raven, sin afirmar que exista un servidor activo."));
  Rid row = Rwidget_create(R_WIDGET_HBOX);
  Rwidget_add_child(d.page, row);
  Rwidget_add_child(row, Rlabel_create("Protocolo:"));
  Rid combo = Rwidget_create(R_WIDGET_COMBO_BOX);
  Rwidget_add_child(row, combo);
  for (const char* value : {"HTTP Server", "HTTP Client", "File Server",
                            "File Client", "Read Only Data", "Send Payload"})
    Rcombo_box_add_item(combo, value);
  Rcombo_box_set_selected(combo, 0);
  Rwidget_add_child(row, Rlabel_create("Direccion:"));
  Rid host = Rwidget_create(R_WIDGET_TEXT_INPUT);
  Rtext_input_set_text(host, "127.0.0.1");
  Rwidget_add_child(row, host);
  Rwidget_add_child(row, Rlabel_create("Puerto:"));
  Rid port = Rwidget_create(R_WIDGET_TEXT_INPUT);
  Rtext_input_set_text(port, "8080");
  Rwidget_add_child(row, port);
  Rid start = Rbutton_create("Iniciar (no disponible)");
  Rwidget_add_child(row, start);
  Rwidget_set_enabled(start, R_FALSE);
  Rid stop = Rbutton_create("Detener (no disponible)");
  Rwidget_add_child(row, stop);
  Rwidget_set_enabled(stop, R_FALSE);
  Rwidget_add_child(
      d.page,
      Rlabel_create(
          "Estado: no iniciado · controlador de red original incompleto."));
  Rtab_view_add_tab(tabs_, d.title.c_str(), d.page);
  Rtab_view_show_tab(tabs_, d.page);
}
void EditorApp::textCb(Rid editor, void* ptr) {
  static_cast<EditorApp*>(ptr)->changed(editor);
}
void EditorApp::changed(Rid editor) {
  auto* d = findDocument(editor);
  if (!d || d->read_only) return;
  if (!d->dirty) {
    d->dirty = true;
    Rtab_view_set_tab_title(tabs_, d->page, ("● " + d->title).c_str());
  }
  if (d->kind == DocumentKind::Csv) refreshCsv(*d);
  updateCursor();
}
void EditorApp::browserCb(Rid, const char* path, void* ptr) {
  if (!path || !*path) return;
  auto* app = static_cast<EditorApp*>(ptr);
  try {
    app->opened(fs::u8path(path));
  } catch (const std::exception& e) {
    app->setStatus(e.what());
  }
}
void EditorApp::openDialog() {
  const RFileFilter filter[] = {
      {"Archivos de codigo y datos",
       "*.cpp;*.cc;*.c;*.h;*.hpp;*.py;*.json;*.xml;*.csv;*.dlt;*.txt;*.md"},
      {"Todos los archivos", "*"}};
  RFileDialogDesc desc{};
  desc.parent = window_;
  desc.title = "Abrir archivo";
  desc.filters = filter;
  desc.filter_count = 2;
  const auto folder = pathUtf8(folder_);
  desc.directory = folder_.empty() ? nullptr : folder.c_str();
  const char* chosen = Rfile_dialog_open(&desc);
  if (chosen && *chosen) opened(fs::u8path(chosen));
}
void EditorApp::openFolderDialog() {
  RFileDialogDesc desc{};
  desc.parent = window_;
  desc.title = "Selecciona carpeta del proyecto";
  const auto folder = pathUtf8(folder_);
  desc.directory = folder_.empty() ? nullptr : folder.c_str();
  const char* selected = Rfile_dialog_select_folder(&desc);
  if (selected && *selected) setFolder(fs::u8path(selected));
}
void EditorApp::setFolder(const fs::path& folder) {
  if (folder.empty() || !fs::is_directory(folder)) return;
  folder_ = fs::weakly_canonical(folder);
  const auto name = pathUtf8(folder_);
  if (!Rfile_browser_set_directory(browser_, name.c_str()))
    setStatus("No se pudo listar carpeta: " + name);
  else
    setStatus("Proyecto: " + name);
  Rlabel_set_text(Rwidget_find(window_, "folder_label"), name.c_str());
  refreshGit();
  refreshTasks();
  savePrefs();
}
void EditorApp::refreshGit() {
  Rlist_view_clear_items(git_list_);
  commits_.clear();
  if (folder_.empty()) return;
  std::string error;
  commits_ = gitHistory(folder_, &error);
  for (const auto& c : commits_)
    Rlist_view_add_item(git_list_, (c.sha + "  " + c.subject).c_str());
  if (commits_.empty()) {
    Rtext_editor_set_text(git_detail_,
                          (error.empty() ? "No hay commits." : error).c_str());
  } else {
    Rlist_view_set_selected(git_list_, 0);
    gitSelected(0);
  }
}
void EditorApp::gitSelected(int index) {
  if (index < 0 || index >= static_cast<int>(commits_.size())) return;
  const auto& c = commits_[static_cast<std::size_t>(index)];
  Rtext_editor_set_text(git_detail_,
                        ("Commit " + c.sha + "\nAutor: " + c.author +
                         "\nFecha: " + c.date + "\n\n" + c.subject)
                            .c_str());
}
void EditorApp::gitCb(Rid, int index, void* ptr) {
  static_cast<EditorApp*>(ptr)->gitSelected(index);
}
bool EditorApp::save(Document& d, bool save_as) {
  if (d.read_only || d.editor == R_INVALID_ID) {
    setStatus("Esta vista no admite guardado.");
    return false;
  }
  fs::path target = d.path;
  if (save_as || target.empty()) {
    RFileDialogDesc desc{};
    desc.parent = window_;
    desc.title = "Guardar documento como...";
    const auto base = d.title;
    desc.file_name = base.c_str();
    const auto folder = pathUtf8(folder_);
    desc.directory = folder_.empty() ? nullptr : folder.c_str();
    const char* name = Rfile_dialog_save(&desc);
    if (!name || !*name) return false;
    target = fs::u8path(name);
  }
  try {
    writeText(target, dataOf(d.editor));
  } catch (const std::exception& e) {
    Rmessage_box(window_, "Error al guardar", e.what(), R_MESSAGE_OK);
    return false;
  }
  d.path = target;
  d.title = baseName(target);
  d.dirty = false;
  Rtab_view_set_tab_title(tabs_, d.page, d.title.c_str());
  setStatus("Guardado: " + pathUtf8(target));
  Json evt = Json::object();
  evt["path"] = pathUtf8(target);
  pluginManager_.emit("document.saved", evt.dump(0));
  if (d.kind == DocumentKind::Text) {
    const char* lang = Rlanguage_for_file(pathUtf8(target).c_str());
    if (lang) Rtext_editor_set_language(d.editor, lang);
  }
  return true;
}
bool EditorApp::allowClose(Document& d) {
  if (!d.dirty) return true;
  const auto prompt =
      "Hay cambios sin guardar en " + d.title +
      ".\n\nSi: guardar   No: descartar   Cancelar: continuar editando.";
  const auto result = Rmessage_box(window_, "Cambios sin guardar",
                                   prompt.c_str(), R_MESSAGE_YES_NO_CANCEL);
  if (result == R_MESSAGE_RESULT_CANCEL) return false;
  if (result == R_MESSAGE_RESULT_YES) return save(d);
  return result == R_MESSAGE_RESULT_NO;
}
bool EditorApp::allowExit() {
  for (auto& d : docs_)
    if (!allowClose(*d)) return false;
  return true;
}
void EditorApp::closeActive() {
  auto* d = active();
  if (!d || !allowClose(*d)) return;
  Rid page = d->page;
  docs_.erase(std::remove_if(docs_.begin(), docs_.end(),
                             [&](const auto& p) { return p->page == page; }),
              docs_.end());
  Rwidget_destroy(page);
  if (docs_.empty()) newDocument();
}
int EditorApp::tabCloseCb(Rid, int, Rid page, void* ptr) {
  auto* app = static_cast<EditorApp*>(ptr);
  Document* d = app->findDocument(page);
  if (!d || !app->allowClose(*d)) return R_FALSE;
  app->docs_.erase(
      std::remove_if(app->docs_.begin(), app->docs_.end(),
                     [&](const auto& p) { return p->page == page; }),
      app->docs_.end());
  // Raven destruye page despues de que el callback retorna R_TRUE.
  return R_TRUE;
}
void EditorApp::tabCb(Rid, int, void* ptr) {
  static_cast<EditorApp*>(ptr)->updateCursor();
}
int EditorApp::windowCloseCb(Rid, void* ptr) {
  return static_cast<EditorApp*>(ptr)->allowExit() ? R_TRUE : R_FALSE;
}
void EditorApp::setStatus(const std::string& message) {
  if (status_ != R_INVALID_ID && Rwidget_is_valid(status_))
    Rlabel_set_text(status_, message.c_str());
}
void EditorApp::log(const std::string& message) {
  // The terminal displays the real shell stream, never application logs.
  std::cerr << message;
}
void EditorApp::updateCursor() {
  auto* d = active();
  if (!d || d->editor == R_INVALID_ID || !Rwidget_is_valid(d->editor)) return;
  int line = 0, col = 0;
  Rtext_editor_get_cursor(d->editor, &line, &col);
  Rlabel_set_text(cursor_, ("Ln " + std::to_string(line + 1) + " : Col " +
                            std::to_string(col + 1))
                               .c_str());
}
void EditorApp::timerCb(Rid, void* ptr) {
  auto* app = static_cast<EditorApp*>(ptr);
  app->updateCursor();
  app->pumpTerminals();
}
void EditorApp::terminalMeasureCb(Rid, float* width, float* height, void*) {
  if (width) *width = 680.0f;
  if (height) *height = 250.0f;
}
namespace {
std::string utf8(Ruint cp) {
  std::string out;
  if (cp < 0x80)
    out.push_back(static_cast<char>(cp));
  else if (cp < 0x800) {
    out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 63)));
  } else if (cp < 0x10000) {
    out.push_back(static_cast<char>(0xe0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 63)));
    out.push_back(static_cast<char>(0x80 | (cp & 63)));
  } else if (cp <= 0x10ffff) {
    out.push_back(static_cast<char>(0xf0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 63)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 63)));
    out.push_back(static_cast<char>(0x80 | (cp & 63)));
  }
  return out;
}
float colorf(std::uint8_t v) { return static_cast<float>(v) / 255.0f; }
bool equalColor(TerminalColor a, TerminalColor b) {
  return a.r == b.r && a.g == b.g && a.b == b.b;
}
std::pair<int, int> pointOf(const TerminalTab& t, const REvent& e) {
  const int c = std::clamp(
      static_cast<int>((e.local_x - 10.0f) / std::max(1.0f, t.char_width)), 0,
      t.screen.columns() - 1);
  const int r = std::clamp(
      static_cast<int>((e.local_y - 8.0f) / std::max(1.0f, t.line_height)), 0,
      t.screen.rows() - 1);
  return {r, c};
}
bool selectedCell(const TerminalTab& t, int row, int col) {
  if (!t.has_selection) return false;
  auto a = std::make_pair(t.anchor_row, t.anchor_col);
  auto b = std::make_pair(t.selection_row, t.selection_col);
  if (b < a) std::swap(a, b);
  return std::make_pair(row, col) >= a && std::make_pair(row, col) < b;
}
}  // namespace
void EditorApp::terminalPaintCb(Rid widget, RPainter* painter, float width,
                                float height, void* user_data) {
  auto& t = *static_cast<TerminalTab*>(user_data);
  const auto* selectedTheme = t.app->themeRegistry_.find(t.app->theme_);
  auto asRgb = [](const std::string& hex, float backupR, float backupG,
                  float backupB) {
    std::array<float, 3> result{backupR, backupG, backupB};
    if (hex.size() == 7 || hex.size() == 9) {
      try {
        result[0] =
            static_cast<float>(std::stoi(hex.substr(1, 2), nullptr, 16)) /
            255.0f;
        result[1] =
            static_cast<float>(std::stoi(hex.substr(3, 2), nullptr, 16)) /
            255.0f;
        result[2] =
            static_cast<float>(std::stoi(hex.substr(5, 2), nullptr, 16)) /
            255.0f;
      } catch (...) {
      }
    }
    return result;
  };
  const auto bg = asRgb(selectedTheme ? selectedTheme->input : "#0A0F1A", 0.04f,
                        0.06f, 0.10f);
  const auto accent = asRgb(selectedTheme ? selectedTheme->accent : "#82E9E8",
                            0.52f, 0.91f, 0.91f);
  const bool daylight = selectedTheme && (selectedTheme->base == "Light" ||
                                          selectedTheme->base == "GlassLight");
  const float bgR = bg[0], bgG = bg[1], bgB = bg[2];
  const float acR = accent[0], acG = accent[1], acB = accent[2];
  Rdraw_rect(painter, 0, 0, width, height, 10, bgR, bgG, bgB, 1.0f);
  Rdraw_rect_outline(painter, 0, 0, width, height, 10, 1.0f, acR, acG, acB,
                     daylight ? 0.18f : 0.22f);
  Rid font = t.app->terminalFont_ != R_INVALID_ID ? t.app->terminalFont_
                                                  : Rfont_get_monospace();
  const float fontSize =
      static_cast<float>(t.app->settings_.getInt("terminal.fontSize", 14));
  float char_w = 8.2f, font_h = 17.0f;
  Rfont_measure_text(font, fontSize, "M", &char_w, &font_h);
  t.char_width = std::max(6.0f, char_w);
  t.line_height = std::max(16.0f, font_h + 3.0f);
  const int cols =
      std::clamp(static_cast<int>((width - 20.0f) / t.char_width), 10, 400);
  const int rows =
      std::clamp(static_cast<int>((height - 16.0f) / t.line_height), 4, 180);
  if (cols != t.screen.columns() || rows != t.screen.rows()) {
    t.screen.resize(cols, rows);
    t.session.resize(cols, rows);
  }
  Rdraw_push_clip_rect(painter, 8, 7, std::max(0.0f, width - 16),
                       std::max(0.0f, height - 14));
  const float x0 = 10.0f, y0 = 8.0f;
  for (int y = 0; y < t.screen.rows(); ++y) {
    float top = y0 + y * t.line_height;
    if (top > height) break;
    int c = 0;
    while (c < t.screen.columns()) {
      const auto& first = t.screen.cell(y, c);
      const bool highlight = selectedCell(t, y, c);
      int until = c + 1;
      while (until < t.screen.columns()) {
        const auto& next = t.screen.cell(y, until);
        if (highlight != selectedCell(t, y, until) ||
            !equalColor(first.foreground, next.foreground) ||
            !equalColor(first.background, next.background) ||
            first.bold != next.bold)
          break;
        ++until;
      }
      if (highlight)
        Rdraw_rect(painter, x0 + c * t.char_width, top,
                   (until - c) * t.char_width, t.line_height, 1, acR, acG, acB,
                   daylight ? 0.22f : 0.55f);
      else if (!equalColor(first.background, TerminalColor{10, 15, 26})) {
        auto bg = first.background;
        Rdraw_rect(painter, x0 + c * t.char_width, top,
                   (until - c) * t.char_width, t.line_height, 0, colorf(bg.r),
                   colorf(bg.g), colorf(bg.b), 1);
      }
      std::string text;
      for (int i = c; i < until; ++i) text += t.screen.cell(y, i).glyph;
      // Raven_music Daylight has pale surfaces: invert only the shell's default
      // foreground, leaving explicit ANSI colors unchanged.
      auto foreground = first.foreground;
      if (daylight && equalColor(foreground, TerminalColor{226, 232, 240}))
        foreground = TerminalColor{30, 30, 40};
      Rdraw_text(
          painter, first.bold ? Rfont_get_variant(font, R_FONT_BOLD) : font,
          fontSize, x0 + c * t.char_width, top + 1, text.c_str(),
          colorf(foreground.r), colorf(foreground.g), colorf(foreground.b), 1);
      c = until;
    }
  }
  if (t.screen.cursorVisible() && t.session.running()) {
    const int x = t.screen.cursorX(), y = t.screen.cursorY();
    if (x < t.screen.columns() && y < t.screen.rows()) {
      const float px = x0 + x * t.char_width, py = y0 + y * t.line_height;
      Rdraw_rect(painter, px, py, t.char_width, t.line_height, 1, acR, acG, acB,
                 0.88f);
      const auto& charAt = t.screen.cell(y, x);
      Rdraw_text(painter, font, fontSize, px, py + 1, charAt.glyph.c_str(),
                 0.02f, 0.04f, 0.06f, 1);
    }
  }
  Rdraw_pop_clip_rect(painter);
}
int EditorApp::terminalEventCb(Rid widget, const REvent* event,
                               void* user_data) {
  if (!event) return R_FALSE;
  auto& t = *static_cast<TerminalTab*>(user_data);
  auto& app = *t.app;
  if (event->type == R_EVENT_MOUSE_DOWN && event->button == R_MOUSE_LEFT) {
    Rwidget_focus(widget);
    app.active_terminal_canvas_ = t.canvas;
    auto [r, c] = pointOf(t, *event);
    t.anchor_row = t.selection_row = r;
    t.anchor_col = t.selection_col = c;
    t.dragging = true;
    t.has_selection = false;
    Rwidget_invalidate(widget);
    return R_TRUE;
  }
  if (event->type == R_EVENT_MOUSE_MOVE && t.dragging) {
    auto [r, c] = pointOf(t, *event);
    t.selection_row = r;
    t.selection_col = c;
    t.has_selection = t.anchor_row != r || t.anchor_col != c;
    Rwidget_invalidate(widget);
    return R_TRUE;
  }
  if (event->type == R_EVENT_MOUSE_UP && event->button == R_MOUSE_LEFT) {
    t.dragging = false;
    return R_TRUE;
  }
  if (event->type == R_EVENT_MOUSE_WHEEL) {
    t.screen.scroll(static_cast<int>(event->wheel_y * 3.0f));
    t.has_selection = false;
    Rwidget_invalidate(widget);
    return R_TRUE;
  }
  if (event->type == R_EVENT_TEXT) {
    app.active_terminal_canvas_ = t.canvas;
    if (event->modifiers & R_MOD_CTRL) return R_TRUE;
    auto value = utf8(event->codepoint);
    if ((event->modifiers & R_MOD_ALT) != 0) value.insert(0, 1, '\x1b');
    t.session.sendBytes(value);
    t.screen.toBottom();
    Rwidget_invalidate(widget);
    return R_TRUE;
  }
  if (event->type != R_EVENT_KEY_DOWN) return R_FALSE;
  app.active_terminal_canvas_ = t.canvas;
  const bool ctrl = (event->modifiers & R_MOD_CTRL) != 0;
  const bool shift = (event->modifiers & R_MOD_SHIFT) != 0;
  if (ctrl && shift) {
    if (event->key == R_KEY_C) {
      app.copyTerminal();
      return R_TRUE;
    }
    if (event->key == R_KEY_V) {
      app.pasteTerminal();
      return R_TRUE;
    }
    if (event->key == R_KEY_N || event->key == R_KEY_GRAVE) {
      app.newTerminal();
      return R_TRUE;
    }
  }
  if (shift && event->key == R_KEY_INSERT) {
    app.pasteTerminal();
    return R_TRUE;
  }
  if (ctrl && event->key >= R_KEY_A && event->key <= R_KEY_Z) {
    const char ch = static_cast<char>(event->key - R_KEY_A + 1);
    t.session.sendBytes(std::string(1, ch));
    t.screen.toBottom();
    Rwidget_invalidate(widget);
    return R_TRUE;
  }
  const char* sequence = nullptr;
  switch (event->key) {
    case R_KEY_ENTER:
    case R_KEY_KP_ENTER:
      sequence = "\r";
      break;
    case R_KEY_BACKSPACE:
      sequence = "\x7f";
      break;
    case R_KEY_TAB:
      sequence = "\t";
      break;
    case R_KEY_ESCAPE:
      sequence = "\x1b";
      break;
    case R_KEY_UP:
      sequence = "\x1b[A";
      break;
    case R_KEY_DOWN:
      sequence = "\x1b[B";
      break;
    case R_KEY_RIGHT:
      sequence = "\x1b[C";
      break;
    case R_KEY_LEFT:
      sequence = "\x1b[D";
      break;
    case R_KEY_HOME:
      sequence = "\x1b[H";
      break;
    case R_KEY_END:
      sequence = "\x1b[F";
      break;
    case R_KEY_DELETE:
      sequence = "\x1b[3~";
      break;
    case R_KEY_INSERT:
      sequence = "\x1b[2~";
      break;
    case R_KEY_PAGE_UP:
      sequence = "\x1b[5~";
      break;
    case R_KEY_PAGE_DOWN:
      sequence = "\x1b[6~";
      break;
    default:
      break;
  }
  if (sequence) {
    std::string bytes = sequence;
    if (event->modifiers & R_MOD_ALT) bytes.insert(0, 1, '\x1b');
    t.session.sendBytes(bytes);
    t.screen.toBottom();
    Rwidget_invalidate(widget);
    return R_TRUE;
  }
  return R_FALSE;
}
TerminalTab* EditorApp::activeTerminal() {
  if (terminal_tabs_ == R_INVALID_ID) return nullptr;
  const int index = Rtab_view_get_active(terminal_tabs_);
  if (index < 0) return nullptr;
  const Rid content = Rtab_view_get_tab(terminal_tabs_, index);
  for (auto& terminal : terminals_)
    if (terminal->group == content &&
        terminal->canvas == active_terminal_canvas_)
      return terminal.get();
  for (auto& terminal : terminals_)
    if (terminal->group == content) return terminal.get();
  return nullptr;
}
void EditorApp::newTerminal(bool split) {
  Rdock_area_open_panel(dock_, output_pane_);
  const fs::path cwd = !folder_.empty() && fs::is_directory(folder_)
                           ? folder_
                           : fs::current_path();
  const Rid existingGroup =
      split && activeTerminal() ? activeTerminal()->group : R_INVALID_ID;
  auto tab = std::make_unique<TerminalTab>();
  tab->app = this;
  tab->screen.setScrollbackLimit(
      static_cast<size_t>(settings_.getInt("terminal.scrollbackLines", 3000)));
  const int selected = Rcombo_box_get_selected(terminal_profile_);
  std::string profile;
#ifdef _WIN32
  profile = selected == 1 ? "pwsh" : selected == 2 ? "cmd" : "powershell";
#else
  profile = selected == 1   ? "bash"
            : selected == 2 ? "zsh"
            : selected == 3 ? "fish"
            : selected == 4 ? "sh"
                            : "";
#endif
  if (profile.empty()) profile = settings_.getString("terminal.defaultShell");
  tab->title = (profile.empty() ? "Shell" : profile) + std::string(" ") +
               std::to_string(++next_terminal_id_);
  tab->group = existingGroup == R_INVALID_ID ? Rwidget_create(R_WIDGET_HBOX)
                                             : existingGroup;
  tab->canvas = Rwidget_create(R_WIDGET_CUSTOM);
  Rwidget_add_child(tab->group, tab->canvas);
  Rslot_set_fill(tab->canvas, 1.0f);
  Rwidget_set_name(tab->canvas,
                   ("terminal_" + std::to_string(next_terminal_id_)).c_str());
  Rwidget_set_focusable(tab->canvas, R_TRUE);
  Rwidget_set_class(tab->canvas, "glass-console music-console");
  Rcustom_set_measure_callback(tab->canvas, &EditorApp::terminalMeasureCb,
                               tab.get());
  Rcustom_set_paint_callback(tab->canvas, &EditorApp::terminalPaintCb,
                             tab.get());
  Rwidget_set_event_callback(tab->canvas, &EditorApp::terminalEventCb,
                             tab.get());
  std::string error;
  if (!tab->session.start(cwd, &error, profile))
    tab->screen.feed("Error al iniciar terminal: " + error + "\r\n");
  else
    tab->session.resize(tab->screen.columns(), tab->screen.rows());
  if (existingGroup == R_INVALID_ID)
    Rtab_view_add_tab(terminal_tabs_, tab->title.c_str(), tab->group);
  Rtab_view_show_tab(terminal_tabs_, tab->group);
  active_terminal_canvas_ = tab->canvas;
  terminals_.push_back(std::move(tab));
  focusTerminal();
  setStatus("Terminal en: " + pathUtf8(cwd));
}
void EditorApp::closeTerminal() {
  auto* current = activeTerminal();
  if (!current) return;
  const Rid canvas = current->canvas, group = current->group;
  current->session.stop();
  terminals_.erase(
      std::remove_if(terminals_.begin(), terminals_.end(),
                     [&](const auto& p) { return p->canvas == canvas; }),
      terminals_.end());
  Rwidget_destroy(canvas);
  active_terminal_canvas_ = R_INVALID_ID;
  if (std::none_of(terminals_.begin(), terminals_.end(),
                   [&](const auto& p) { return p->group == group; }))
    Rwidget_destroy(group);
  if (terminal_state_ != R_INVALID_ID)
    Rlabel_set_text(terminal_state_, "Terminal cerrada");
  focusTerminal();
}
void EditorApp::terminalTabCb(Rid, int, void* ptr) {
  static_cast<EditorApp*>(ptr)->focusTerminal();
}
int EditorApp::terminalTabCloseCb(Rid, int, Rid content, void* ptr) {
  auto* self = static_cast<EditorApp*>(ptr);
  for (auto& t : self->terminals_)
    if (t->group == content) t->session.stop();
  self->terminals_.erase(
      std::remove_if(self->terminals_.begin(), self->terminals_.end(),
                     [&](const auto& p) { return p->group == content; }),
      self->terminals_.end());
  self->active_terminal_canvas_ = R_INVALID_ID;
  return R_TRUE;
}
void EditorApp::focusTerminal() {
  auto* tab = activeTerminal();
  if (!tab) return;
  if (terminal_state_ != R_INVALID_ID)
    Rlabel_set_text(
        terminal_state_,
        (tab->session.running() ? std::string("● ") + TerminalSession::backend()
                                : "● Proceso finalizado")
            .c_str());
  Rwidget_focus(tab->canvas);
}
void EditorApp::pumpTerminals() {
  for (auto& terminal : terminals_) {
    auto chunk = terminal->session.takeOutput();
    if (!chunk.empty()) {
      terminal->screen.feed(chunk);
      if (Rwidget_is_valid(terminal->canvas))
        Rwidget_invalidate(terminal->canvas);
    }
  }
  if (auto* terminal = activeTerminal();
      terminal && terminal_state_ != R_INVALID_ID) {
    const std::string state =
        terminal->session.running()
            ? std::string("● ") + TerminalSession::backend()
            : "● Finalizó (" + std::to_string(terminal->session.exitCode()) +
                  ")";
    if (std::string(Rlabel_get_text(terminal_state_)) != state)
      Rlabel_set_text(terminal_state_, state.c_str());
  }
}
void EditorApp::copyTerminal() {
  auto* t = activeTerminal();
  if (!t) return;
  std::string value;
  if (t->has_selection) {
    auto a = std::make_pair(t->anchor_row, t->anchor_col);
    auto b = std::make_pair(t->selection_row, t->selection_col);
    if (b < a) std::swap(a, b);
    for (int y = a.first; y <= b.first; ++y) {
      const int start = y == a.first ? a.second : 0;
      const int end = y == b.first ? b.second : t->screen.columns();
      for (int x = start; x < end; ++x) value += t->screen.cell(y, x).glyph;
      if (y != b.first) value += '\n';
    }
  } else
    value = t->screen.visibleText();
  Rclipboard_set_text(value.c_str());
  setStatus("Terminal: contenido copiado al portapapeles.");
}
void EditorApp::pasteTerminal() {
  auto* t = activeTerminal();
  if (!t) return;
  const char* data = Rclipboard_get_text();
  if (data && *data) {
    t->session.sendBytes(data);
    t->screen.toBottom();
  }
  Rwidget_focus(t->canvas);
}
void EditorApp::clearTerminal() {
  auto* t = activeTerminal();
  if (!t) return;
  t->screen.clear();
  t->session.sendBytes("\x0c");
  Rwidget_invalidate(t->canvas);
  Rwidget_focus(t->canvas);
}

void EditorApp::showSearch(bool show) {
  const Rid bar = Rwidget_find(window_, "searchbar");
  Rwidget_set_visibility(bar, show ? R_VISIBLE : R_COLLAPSED);
  if (show) Rwidget_focus(Rwidget_find(window_, "search_text"));
}
void EditorApp::searchNext(bool replace, bool all, bool backwards) {
  Document* d = active();
  if (!d || d->editor == R_INVALID_ID || d->read_only) return;
  const auto needle = inputOf(window_, "search_text");
  const auto replacement = inputOf(window_, "replace_text");
  if (needle.empty()) {
    showSearch(true);
    return;
  }
  const auto text = dataOf(d->editor);
  if (all) {
    std::string out;
    std::size_t pos = 0, count = 0;
    while (true) {
      const auto i = text.find(needle, pos);
      if (i == std::string::npos) break;
      out.append(text, pos, i - pos);
      out += replacement;
      pos = i + needle.size();
      ++count;
    }
    if (count) {
      out.append(text, pos, std::string::npos);
      Rtext_editor_set_text(d->editor, out.c_str());
      changed(d->editor);
    }
    setStatus("Reemplazos: " + std::to_string(count));
    return;
  }
  if (replace) {
    const char* selected = Rtext_editor_get_selected_text(d->editor);
    if (selected && needle == selected) {
      Rtext_editor_insert_text(d->editor, replacement.c_str());
    }
  }
  int line = 0, col = 0;
  Rtext_editor_get_cursor(d->editor, &line, &col);
  const auto current = dataOf(d->editor);
  auto start = absolutePos(current, line, col);
  std::size_t hit = std::string::npos;
  if (backwards) {
    if (start > 0) hit = current.rfind(needle, start - 1);
    if (hit == std::string::npos) hit = current.rfind(needle);
  } else {
    hit = current.find(needle, start);
    if (hit == std::string::npos) hit = current.find(needle, 0);
  }
  if (hit == std::string::npos) {
    setStatus("No se encuentra: " + needle);
    return;
  }
  const auto a = lineColumn(current, hit),
             b = lineColumn(current, hit + needle.size());
  Rtext_editor_select(d->editor, a.first, a.second, b.first, b.second);
  Rtext_editor_scroll_to_line(d->editor, a.first);
  Rwidget_focus(d->editor);
  setStatus("Coincidencia: Ln " + std::to_string(a.first + 1));
}
void EditorApp::refreshThemeDecor() {
  if (window_ == R_INVALID_ID) return;
  const Rid backdrop = Rwidget_find(window_, "music_backdrop");
  const bool music = theme_.rfind("Music ", 0) == 0;
  if (backdrop != R_INVALID_ID) {
    Rwidget_set_visibility(backdrop, music ? R_VISIBLE : R_COLLAPSED);
    if (theme_ == "Music Nocturne") {
      Rwidget_set_attribute(backdrop, "EffectColor", "#3B2A6E");
      Rwidget_set_attribute(backdrop, "EffectColor2", "#0E3B4A");
      Rwidget_set_opacity(backdrop, 0.70f);
    } else if (theme_ == "Music Aurora Glass") {
      Rwidget_set_attribute(backdrop, "EffectColor", "#283E83");
      Rwidget_set_attribute(backdrop, "EffectColor2", "#0D778C");
      Rwidget_set_opacity(backdrop, 0.78f);
    } else if (theme_ == "Music Daylight") {
      Rwidget_set_attribute(backdrop, "EffectColor", "#D4C7F7");
      Rwidget_set_attribute(backdrop, "EffectColor2", "#B8E4E8");
      Rwidget_set_opacity(backdrop, 0.35f);
    }
  }
  for (const auto& terminal : terminals_)
    if (Rwidget_is_valid(terminal->canvas))
      Rwidget_invalidate(terminal->canvas);
}

void EditorApp::themeCb(Rid, int index, void* ptr) {
  static_cast<EditorApp*>(ptr)->applyTheme(index);
}
void EditorApp::applyTheme(int index) {
  if (index < 0 || index >= static_cast<int>(themeNames_.size())) return;
  const auto selected = themeNames_[static_cast<size_t>(index)];
  if (!Rtheme_use(selected.c_str())) {
    setStatus("Failed to activate theme: " + selected);
    return;
  }
  theme_ = selected;
  applyFonts();
  refreshThemeDecor();
  if (Rcombo_box_get_selected(themes_) != index)
    Rcombo_box_set_selected(themes_, index);
  savePrefs();
  Json info = Json::object();
  info["name"] = theme_;
  pluginManager_.emit("theme.changed", info.dump(0));
  setStatus("Theme: " + theme_);
}
void EditorApp::loadPrefs() {
  settings_.load();
  theme_ = settings_.getString("appearance.theme", "Music Nocturne");
  const auto last = settings_.getString("general.lastFolder");
  if (!last.empty()) folder_ = fs::u8path(last);
  for (const auto& diag : settings_.diagnostics())
    std::cerr << "Settings: " << diag << '\n';
}
void EditorApp::savePrefs() {
  settings_.set("appearance.theme", theme_);
  settings_.set("general.lastFolder", pathUtf8(folder_));
  std::string error;
  if (!settings_.save(&error))
    std::cerr << "Settings not saved: " << error << '\n';
}
fs::path EditorApp::prefsFile() const { return settings_.file(); }
void EditorApp::dltFilterCb(Rid, const char*, void* ptr) {
  auto* d = static_cast<Document*>(ptr);
  d->app->filterDlt(*d);
}
void EditorApp::dltRefreshCb(Rid, void* ptr) {
  auto* d = static_cast<Document*>(ptr);
  d->app->filterDlt(*d);
}
}  // namespace dodev
