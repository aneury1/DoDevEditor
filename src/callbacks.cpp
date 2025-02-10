#include "callbacks.h"
#include "utils.h"
#include "exec_dialog.h"
#include "git_panel.h"
#include <functional>
#include <wx/textdlg.h>

namespace
{
   do_devwindow *window;
   editor_tab *tabamanger;
   explorer_panel *explorerpanel;
   git_panel *gitpanel;
   std::unordered_map<int, std::function<void(wxCommandEvent)>> functions;
   wxString rootPath;
}

void add_new_page(wxString &selectedName)
{
   if (tabamanger)
   {
      tabamanger->add_empty_page();
      tabamanger->set_title_current_page(selectedName);
   }
}

text_editor *get_current_text_editor()
{
   if (tabamanger)
      return tabamanger->get_current_editor();
   return nullptr;
}

void open_new_tab(wxCommandEvent ev)
{
   if (tabamanger)
   {
      tabamanger->add_empty_page();
   }
   else
   {
      std::cout << "Tab Manager is nullptr";
   }
}

void open_text_file_tab(wxCommandEvent ev)
{
   if (tabamanger)
   {
      tabamanger->add_empty_page();
      auto currenttexteditor = tabamanger->get_current_editor();
      auto file = obtain_existing_file("Select the file want open", (wxFrame *)window);

      auto content = read_file(file.ToStdString());

      if (content.size() > 0)
      {
         currenttexteditor->set_text(content);
         currenttexteditor->set_filepath(file);
         int lead = file.find_last_of("/");
         if (lead < 0)
            lead = 1;
         auto name = file.SubString(lead, file.size());
         tabamanger->set_title_current_page(name);
      }
   }
   else
   {
      std::cout << "Tab Manager is nullptr";
   }
}

void save_current_file_tab(wxCommandEvent ev)
{

   auto current_editor = tabamanger->get_current_editor();
   if (current_editor)
   {
      if (current_editor->get_from_file() && current_editor->get_changed())
      {
         bool accept = question_yes_or_not(wxString("The file has changed, do you want to save?"));
         if (accept)
         {
            std::cout << "save content at: " << current_editor->get_path() << "\n";
            write_content_to_file(current_editor->get_path(), current_editor->get_text());
         }
      }
      else if (current_editor->get_changed())
      {
         bool accept = question_yes_or_not(wxString("This file does not exist, do you want to save?"));
         if (accept)
         {
            auto status = create_new_file_with_content_with_dialog(window, current_editor->get_text());

            if (status.size() > 0)
               tabamanger->set_title_current_page(status);
         }
      }
   }
}

void save_current_file_tab_as(wxCommandEvent ev)
{

   auto current_editor = tabamanger->get_current_editor();
   if (current_editor)
   {
      auto status = create_new_file_with_content_with_dialog(window, current_editor->get_text());
      if (status.size() > 0)
         tabamanger->set_title_current_page(status);
   }
}

void set_status_text(const wxString text)
{
}

wxString get_root_path()
{
   return rootPath;
}

bool is_file_already_in_the_editor(const wxString &path)
{
   return tabamanger->is_file_already_in_the_editor(path);
}

void open_folder(const wxString& folderPath)
{
   explorerpanel->clear_folder_tree();
   wxTreeItemId root = explorerpanel->add_root("Root");
   explorerpanel->populate_folder_tree(folderPath, root);
   rootPath = folderPath;
   explorerpanel->set_path_label(rootPath);
   gitpanel->load_commits_information_in_folder(std::string(folderPath.ToUTF8().data()));
   set_status_text("Opened folder: " + folderPath);
}

void on_open_folder(wxCommandEvent event)
{
   wxDirDialog openFolderDialog(window,
                                "Select a Folder to Open", "",
                                wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

   if (openFolderDialog.ShowModal() == wxID_CANCEL)
      return;

   wxString folderPath = openFolderDialog.GetPath();
   if (folderPath.IsEmpty())
   {
      set_status_text("No folder selected.");
      return;
   }

   explorerpanel->clear_folder_tree();
   wxTreeItemId root = explorerpanel->add_root("Root");
   explorerpanel->populate_folder_tree(folderPath, root);
   rootPath = folderPath;
   explorerpanel->set_path_label(rootPath);
   gitpanel->load_commits_information_in_folder(std::string(folderPath.ToUTF8().data()));
   set_status_text("Opened folder: " + folderPath);
}

void on_close_folder(wxCommandEvent event)
{
   if (explorerpanel)
   {
      explorerpanel->clear_folder_tree();
      rootPath = "";
   }
}

void create_file_in_current_selection(wxCommandEvent event){
   ///CreateFileInCurrentSelection,
   
   wxTextEntryDialog dialog(window,
       wxT("Enter the file Name")
   );
    
   if(dialog.ShowModal() == wxID_OK){
      wxMessageBox(dialog.GetValue(), wxT("Got this"));
   }

}

void open_view_explorer(wxCommandEvent event)
{
   window->show_explorer();
}

void open_git_explorer(wxCommandEvent event)
{
   window->show_git_panel();
}

void open_exec_panel(wxCommandEvent event)
{
   window->show_exec();
}

void on_close_tab(wxAuiNotebookEvent &event)
{
   int pageIndex = event.GetSelection();

   auto textEditor = get_current_text_editor();
#if 0 
   if (textEditor && !textEditor->has_changed() && !textEditor->is_untitle())
   {
      event.Veto();
      return;
   }

   wxString prompt = "¿Do you want to close?";
   if (textEditor->has_changed())
   {
      prompt = "File has been changed, ¿Do you want to close?";
   }

   int query = wxMessageBox(prompt,
                            "Confirm close: " + prompt,
                            wxYES_NO | wxCANCEL | wxICON_QUESTION);
   if (query != wxYES)
   {
      event.Veto();
      return;
   }
   event.Skip();
#endif
   return;
}

void open_action_menu(wxCommandEvent ev)
{
   exec_dialog *dialog = new exec_dialog(nullptr);
  /// dialog->ShowModal();
  /// dialog->Destroy();
}

/// @brief end callback list
void set_default_callback()
{
   functions[wxID_NEW] = &open_new_tab;
   functions[wxID_OPEN] = &open_text_file_tab;
   functions[wxID_SAVE] = &save_current_file_tab;
   functions[SaveAs] = &save_current_file_tab_as;
   functions[OpenFolder] = &on_open_folder;
   functions[CloseFolder] = &on_close_folder;
   functions[ActionMenuCommands] = &open_action_menu;
   functions[CreateFileInCurrentSelection]=&create_file_in_current_selection;
   functions[ViewFileExplorer] = &open_view_explorer;
   functions[ViewGitExplorer] = &open_git_explorer;
   functions[ViewExecPanel] = &open_exec_panel;
}

void set_base_window(
    do_devwindow *_window,
    editor_tab *_tabamanager,
    explorer_panel *_explorerpanel,
    git_panel *_gitpanel
    /// Todo: Add other base element.
)
{
   window = _window;
   tabamanger = _tabamanager;
   explorerpanel = _explorerpanel;
   gitpanel = _gitpanel;
}

void call_by_event(const wxCommandEvent &ev)
{
   int id = ev.GetId();
   auto iter = functions.find(id);
   if (iter != functions.end())
   {
      iter->second(ev);
   }
   else
   {
      std::cout << "Callback not found for ID: " << id << "\n";
   }
}