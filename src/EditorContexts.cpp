#include <algorithm>
#include <fstream>
#include <iostream>

#include "EditorApp.h"
#include "dodev/explorer/ExplorerOps.h"

namespace dodev {
namespace {
void command(Rid menu, const char* text, const char* action) {
  const Rid item = Rmenu_add_item(menu, text);
  Rwidget_set_action(item, action);
}
bool beneath(const fs::path& child, const fs::path& ancestor) {
  if (child.empty() || ancestor.empty()) return false;
  std::error_code ec;
  const auto a = fs::weakly_canonical(ancestor, ec);
  if (ec) return false;
  const auto b = fs::weakly_canonical(child, ec);
  if (ec) return false;
  auto x = b.begin(), y = a.begin();
  for (; y != a.end(); ++x, ++y)
    if (x == b.end() || *x != *y) return false;
  return true;
}
std::string fullText(Rid editor) {
  if (editor == R_INVALID_ID || !Rwidget_is_valid(editor)) return {};
  const char* text = Rtext_editor_get_text(editor);
  return text ? text : "";
}
}  // namespace
void EditorApp::initContextMenus() {
  // Standalone Raven menus; UI XML remains embedded, and these menus follow
  // the current theme. Keep them alive for as long as the main window lives.
  explorerContextMenu_ = Rwidget_create(R_WIDGET_MENU);
  command(explorerContextMenu_, "Nuevo archivo...", "explorer.new_file");
  command(explorerContextMenu_, "Nueva carpeta...", "explorer.new_folder");
  Rmenu_add_separator(explorerContextMenu_);
  command(explorerContextMenu_, "Abrir", "explorer.open");
  command(explorerContextMenu_, "Cambiar nombre...", "explorer.rename");
  command(explorerContextMenu_, "Eliminar...", "explorer.delete");
  Rmenu_add_separator(explorerContextMenu_);
  command(explorerContextMenu_, "Copiar ruta", "explorer.copy_path");
  command(explorerContextMenu_, "Actualizar explorador", "explorer.refresh");
  Rwidget_set_context_menu(browser_, explorerContextMenu_);

  editorContextMenu_ = Rwidget_create(R_WIDGET_MENU);
  command(editorContextMenu_, "Deshacer", "edit.undo");
  command(editorContextMenu_, "Rehacer", "edit.redo");
  Rmenu_add_separator(editorContextMenu_);
  command(editorContextMenu_, "Cortar", "edit.cut");
  command(editorContextMenu_, "Copiar", "edit.copy");
  command(editorContextMenu_, "Pegar", "edit.paste");
  command(editorContextMenu_, "Seleccionar todo", "selection.all");
  Rmenu_add_separator(editorContextMenu_);
  command(editorContextMenu_, "Buscar", "search.show");
  command(editorContextMenu_, "Buscar en archivos", "panel.search");
  command(editorContextMenu_, "Ir a linea...", "navigation.go_to_line");
  command(editorContextMenu_, "Alternar comentario de linea",
          "edit.toggle_line_comment");
  command(editorContextMenu_, "Formatear documento (clang-format)",
          "editor.format");

  tabContextMenu_ = Rwidget_create(R_WIDGET_MENU);
  command(tabContextMenu_, "Guardar", "tab.save");
  command(tabContextMenu_, "Cerrar", "tab.close");
  command(tabContextMenu_, "Cerrar las demas", "tab.close_others");
  command(tabContextMenu_, "Cerrar a la derecha", "tab.close_right");
  Rmenu_add_separator(tabContextMenu_);
  command(tabContextMenu_, "Copiar ruta del archivo", "tab.copy_path");
  command(tabContextMenu_, "Mostrar en explorador", "tab.reveal");
  Rwidget_set_context_menu(tabs_, tabContextMenu_);

  terminalContextMenu_ = Rwidget_create(R_WIDGET_MENU);
  command(terminalContextMenu_, "Copiar seleccion", "terminal.copy");
  command(terminalContextMenu_, "Copiar toda la salida", "terminal.copy_all");
  command(terminalContextMenu_, "Pegar", "terminal.paste");
  Rmenu_add_separator(terminalContextMenu_);
  command(terminalContextMenu_, "Guardar salida completa TXT...",
          "terminal.save_output");
  command(terminalContextMenu_, "Limpiar terminal", "terminal.clear");
}
void EditorApp::explorerSelectionCb(Rid browser, const char* path, void* ctx) {
  auto& app = *static_cast<EditorApp*>(ctx);
  if (path && *path)
    app.explorerSelected_ = fs::u8path(path);
  else if (Rfile_browser_get_selection_count(browser) > 0) {
    const char* selected = Rfile_browser_get_selection(browser, 0);
    app.explorerSelected_ =
        selected && *selected ? fs::u8path(selected) : fs::path{};
  } else
    app.explorerSelected_.clear();
}
void EditorApp::explorerAction(const std::string& action) {
  if (action == "explorer.refresh") {
    Rfile_browser_refresh(browser_);
    refreshGit();
    return;
  }
  fs::path selected = explorerSelected_;
  std::error_code ec;
  if (!selected.empty() && !selected.is_absolute())
    selected = fs::u8path(Rfile_browser_get_directory(browser_)) / selected;
  if (action == "explorer.new_file" || action == "explorer.new_folder") {
    const bool directory = action == "explorer.new_folder";
    fs::path parent = folder_;
    if (!selected.empty())
      parent =
          fs::is_directory(selected, ec) ? selected : selected.parent_path();
    if (parent.empty())
      parent = fs::u8path(Rfile_browser_get_directory(browser_));
    if (!fs::is_directory(parent, ec)) {
      setStatus("Abre una carpeta para crear archivos");
      return;
    }
    showPluginPrompt(directory ? "Nueva carpeta" : "Nuevo archivo",
                     "Nombre dentro de: " + parent.string(),
                     directory ? "new-folder" : "untitled.txt",
                     [this, parent, directory](const std::string& name) {
                       auto result =
                           explorer::createEntry(parent, name, directory);
                       if (!result.ok) {
                         setStatus(result.error);
                         Rmessage_box(window_, "Explorer", result.error.c_str(),
                                      R_MESSAGE_OK);
                         return;
                       }
                       Rfile_browser_refresh(browser_);
                       if (parent == folder_)
                         Rfile_browser_select(
                             browser_, result.path.filename().string().c_str());
                       if (!directory) opened(result.path);
                       setStatus(std::string(directory ? "Carpeta creada: "
                                                       : "Archivo creado: ") +
                                 result.path.string());
                     });
    return;
  }
  if (action == "explorer.open") {
    if (selected.empty()) return;
    if (fs::is_directory(selected, ec))
      setFolder(selected);
    else
      opened(selected);
    return;
  }
  if (action == "explorer.copy_path") {
    auto path = selected.empty() ? folder_ : selected;
    if (!path.empty()) {
      Rclipboard_set_text(path.string().c_str());
      setStatus("Ruta copiada: " + path.string());
    }
    return;
  }
  if (selected.empty()) {
    setStatus("Selecciona primero un archivo o carpeta en Explorer");
    return;
  }
  if (action == "explorer.rename") {
    if (selected == folder_) {
      setStatus(
          "No se puede renombrar la carpeta de proyecto desde su propio "
          "explorador");
      return;
    }
    showPluginPrompt(
        "Cambiar nombre", "Nuevo nombre:", selected.filename().string(),
        [this, selected](const std::string& name) {
          auto result = explorer::renameEntry(selected, name);
          if (!result.ok) {
            setStatus(result.error);
            Rmessage_box(window_, "Rename", result.error.c_str(), R_MESSAGE_OK);
            return;
          }
          for (auto& d : docs_) {
            if (d->path.empty() || !beneath(d->path, selected)) continue;
            const fs::path old = d->path;
            std::error_code e;
            auto remainder = fs::relative(old, selected, e);
            d->path =
                old == selected || e ? result.path : result.path / remainder;
            d->title = d->path.filename().string();
            Rtab_view_set_tab_title(
                tabs_, d->page,
                (std::string(d->dirty ? "● " : "") + d->title).c_str());
          }
          explorerSelected_ = result.path;
          Rfile_browser_refresh(browser_);
          setStatus("Renombrado: " + result.path.string());
        });
    return;
  }
  if (action == "explorer.delete") {
    if (selected == folder_) {
      setStatus("No se puede eliminar la carpeta abierta del proyecto");
      return;
    }
    for (const auto& d : docs_)
      if (d->dirty && beneath(d->path, selected)) {
        setStatus(
            "Guarda o cierra los archivos modificados antes de eliminarlos");
        return;
      }
    const std::string msg =
        "Eliminar permanentemente el elemento y, si es carpeta, todo su "
        "contenido?\n\n" +
        selected.string();
    if (Rmessage_box(window_, "Confirmar eliminacion", msg.c_str(),
                     R_MESSAGE_YES_NO) != R_MESSAGE_RESULT_YES)
      return;
    auto result = explorer::removeEntry(selected, folder_);
    if (!result.ok) {
      setStatus(result.error);
      Rmessage_box(window_, "Delete", result.error.c_str(), R_MESSAGE_OK);
      return;
    }
    for (auto it = docs_.begin(); it != docs_.end();) {
      if (!beneath((*it)->path, selected)) {
        ++it;
        continue;
      }
      const Rid page = (*it)->page;
      it = docs_.erase(it);
      Rwidget_destroy(page);
    }
    explorerSelected_.clear();
    Rfile_browser_refresh(browser_);
    if (docs_.empty()) newDocument();
    setStatus("Eliminado: " + selected.string());
    return;
  }
}
void EditorApp::editorTabAction(const std::string& action) {
  auto* doc = active();
  if (!doc) {
    setStatus("No hay documento activo");
    return;
  }
  if (action == "editor.format") {
    if (!pluginManager_.execute("clangformat.apply"))
      setStatus("Habilita el plugin ClangFormat en Settings > Plugins");
    return;
  }
  if (action == "tab.save") {
    save(*doc);
    return;
  }
  if (action == "tab.close") {
    closeActive();
    return;
  }
  if (action == "tab.copy_path") {
    if (doc->path.empty()) {
      setStatus("El documento no tiene una ruta hasta que se guarde");
      return;
    }
    Rclipboard_set_text(doc->path.string().c_str());
    setStatus("Ruta copiada");
    return;
  }
  if (action == "tab.reveal") {
    if (doc->path.empty()) {
      setStatus("Guarda primero el documento");
      return;
    }
    setFolder(doc->path.parent_path());
    Rfile_browser_select(browser_, doc->path.filename().string().c_str());
    Rdock_area_open_panel(dock_, explorer_);
    return;
  }
  const auto current = Rtab_view_find_tab(tabs_, doc->page);
  if (action == "tab.close_others" || action == "tab.close_right") {
    std::vector<Rid> victims;
    for (int i = 0; i < Rtab_view_get_tab_count(tabs_); ++i) {
      const Rid page = Rtab_view_get_tab(tabs_, i);
      if (page == doc->page || (action == "tab.close_right" && i <= current))
        continue;
      victims.push_back(page);
    }
    // Preflight confirmation before removing any tabs.
    for (const auto page : victims) {
      auto* other = findDocument(page);
      if (other && !allowClose(*other)) return;
    }
    for (const auto page : victims) {
      docs_.erase(std::remove_if(
                      docs_.begin(), docs_.end(),
                      [&](const auto& entry) { return entry->page == page; }),
                  docs_.end());
      if (Rwidget_is_valid(page)) Rwidget_destroy(page);
    }
    Rtab_view_show_tab(tabs_, doc->page);
    setStatus("Pestanas cerradas");
  }
}
void EditorApp::copyTaskOutput(const std::string& source) {
  std::string text;
  if (source == "terminal") {
    if (auto* tab = activeTerminal())
      text = tab->screen.fullText();
    else {
      setStatus("No active terminal");
      return;
    }
  } else if (source == "serial")
    text = serialCapture_.exportText(false);
  else if (source == "journal")
    text = journalCapture_.exportText(false);
  else if (source == "ai")
    text = aiTranscript_;
  Rclipboard_set_text(text.c_str());
  setStatus("Copied " + source + " output (" + std::to_string(text.size()) +
            " bytes)");
}
void EditorApp::saveTaskOutput(const std::string& source) {
  // Terminal panel and plugin Tasks tabs all share the same Save File dialog.
  std::string text, suggested;
  if (source == "terminal") {
    if (auto* tab = activeTerminal())
      text = tab->screen.fullText();
    else {
      setStatus("No active terminal");
      return;
    }
    suggested = "terminal_output.txt";
  } else if (source == "serial") {
    text = serialCapture_.exportText(false);
    suggested = "serial_all_output.txt";
  } else if (source == "journal") {
    text = journalCapture_.exportText(false);
    suggested = "journal_all_output.txt";
  } else if (source == "ai") {
    text = aiTranscript_;
    suggested = "ai_chat_output.txt";
  }
  if (suggested.empty()) return;
  RFileDialogDesc dialog{};
  dialog.parent = window_;
  dialog.title = "Save complete output as TXT";
  dialog.file_name = suggested.c_str();
  const char* chosen = Rfile_dialog_save(&dialog);
  if (!chosen || !*chosen) return;
  auto destination = fs::u8path(chosen);
  if (destination.extension().empty()) destination += ".txt";
  std::ofstream out(destination, std::ios::binary | std::ios::trunc);
  if (!out) {
    setStatus("Cannot create output file: " + destination.string());
    return;
  }
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  out.flush();
  if (!out) {
    setStatus("Error writing output file: " + destination.string());
    return;
  }
  std::clog << "[DoDevEditor][Output] " << source
            << " complete output saved to " << destination << " ("
            << text.size() << " bytes)\n";
  setStatus("Output saved: " + destination.string());
}
}  // namespace dodev
