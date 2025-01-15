#include "id_handler.h"
#include "utils.h"
#include "main_frame.h"
#include "text_editor.h"
#include "main_menu.h"
#include "drag_and_drop.h"
#include <wx/wfstream.h>
#include <wx/filename.h>

#include <iostream>

template <class T>
void print(T msg)
{
   std::cout << msg << "\n";
}

do_editor::do_editor() : wxFrame(nullptr, wxID_ANY, "DoDevEditor : Text Editor", wxDefaultPosition, wxSize(800, 600))

{
   menubar = nullptr;
   setup_main_settings();
   Maximize();
   Bind(wxEVT_MENU, &do_editor::on_exit, this, wxID_EXIT);
}

do_editor::~do_editor()
{
}

void do_editor::on_exit(wxCommandEvent &event)
{
   auiManager.UnInit();
   Close();
}
#include "default_dialog.h"
void do_editor::on_find(wxCommandEvent &event)
{
   find_dialog_text findDialog(this);
   auto editor = get_current_text_editor();
   if (findDialog.ShowModal() == wxID_OK)
   {
      wxString searchText = findDialog.GetSearchText();

      // Search for the text in the TextCtrl
      wxString content = editor->get_text();
      long pos = content.Find(searchText);

      if (pos != wxNOT_FOUND)
      {
         wxMessageBox("Text found at position: " + wxString::Format("%ld", pos), "Find Result", wxICON_INFORMATION);
         editor->set_selection(pos, pos + searchText.Length());
      }
      else
      {
         wxMessageBox("Text not found.", "Find Result", wxICON_INFORMATION);
      }
   }
}

void do_editor::setup_accelerator()
{
   {
      wxAcceleratorEntry entries[1];
      int keyp = get_accelerator_next_id();
      entries[0].Set(wxACCEL_CTRL, '+', keyp);
      wxAcceleratorTable accel(1, entries);
      SetAcceleratorTable(accel);
      Bind(wxEVT_MENU, &do_editor::on_ctrl_i, this, keyp);
   }
   {
      wxAcceleratorEntry entries[1];
      int keyp = get_accelerator_next_id();
      entries[0].Set(wxACCEL_CTRL, '-', keyp);
      wxAcceleratorTable accel(1, entries);
      SetAcceleratorTable(accel);
      Bind(wxEVT_MENU, &do_editor::on_ctrl_l, this, keyp);
   }
}

void do_editor::setup_main_settings()
{

   auiManager.SetManagedWindow(this);
   create_main_menubar();
   create_main_instances();
   bind_to_default_event();
   Bind(wxEVT_MENU, &do_editor::on_open_new_file, this, wxID_NEW);
   Bind(wxEVT_MENU, &do_editor::on_open_existing_file, this, wxID_OPEN);
   Bind(wxEVT_MENU, &do_editor::on_save_file, this, wxID_SAVE);
   Bind(wxEVT_MENU, &do_editor::on_open_folder, this, OpenFolder);
   Bind(wxEVT_MENU, &do_editor::on_close_folder, this, CloseFolder);
   Bind(wxEVT_MENU, &do_editor::on_view_file_explorer, this, ViewFileExplorer);

   Bind(wxEVT_MENU, &do_editor::on_exit, this, wxID_EXIT);
   Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &do_editor::on_close_tab, this);
   Bind(wxEVT_TREE_ITEM_ACTIVATED, &do_editor::on_tree_item_activated, this);
   setup_accelerator();
   CreateStatusBar(1);
   SetStatusText("Ready");

   add_new_page(untitled);

   Centre();
}

bool do_editor::check_if_this_file_is_opened(const wxString &title)
{
   return contains_page_with_title(title);
}

text_editor *do_editor::add_new_page(const wxString &title)
{

   text_editor *newPage = new text_editor(editorTabs);

   editorTabs->AddPage(newPage, title, true);

   current_text_editor = newPage;
   // possible memory leak?
   file_drop_target *dropTarget = new file_drop_target(current_text_editor);

   current_text_editor->set_drop_target(dropTarget);

   return newPage;
}

text_editor *do_editor::get_current_text_editor()
{
   int currentTabIndex = editorTabs->GetSelection();
   if (currentTabIndex == wxNOT_FOUND)
      return nullptr;

   std::cout << "Page Text: " << editorTabs->GetPageText(currentTabIndex) << "\n";
   auto currentPanel = dynamic_cast<text_editor *>(editorTabs->GetPage(currentTabIndex));
   return currentPanel;
}

void do_editor::populate_folder_tree(const wxString &path, wxTreeItemId parent)
{
   wxDir dir(path);
   if (!dir.IsOpened())
      return;

   wxString filename;
   bool hasFiles = dir.GetFirst(&filename);
   while (hasFiles)
   {
      wxString fullPath = path + "/" + filename;
      if (wxDirExists(fullPath))
      {
         wxTreeItemId folderItem = folderTree->AppendItem(parent, filename);
         populate_folder_tree(fullPath, folderItem);
      }
      else
      {
         folderTree->AppendItem(parent, filename);
      }
      hasFiles = dir.GetNext(&filename);
   }
}

void do_editor::on_open_new_file(wxCommandEvent &event)
{
   this->add_new_page(untitled);
}

void do_editor::on_open_existing_file(wxCommandEvent &event)
{
   wxFileDialog openFileDialog(this, _("Select file to open"), "", "",
                               file_filter,
                               wxFD_OPEN | wxFD_FILE_MUST_EXIST);
   if (openFileDialog.ShowModal() == wxID_CANCEL)
      return;
   wxString filet = openFileDialog.GetPath();

   if (this->check_if_this_file_is_opened(filet))
   {
      return;
   }

   if (filet.IsEmpty())
   {
      SetStatusText("No folder selected.");
      return;
   }

   if (wxFileName::DirExists(filet))
   {
      return;
   }

   this->add_new_page(untitled);
   auto current = get_current_text_editor();
   current->load_text_file(filet);
}

void do_editor::on_open_folder(wxCommandEvent &event)
{
   wxDirDialog openFolderDialog(this, "Select a Folder to Open", "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
   if (openFolderDialog.ShowModal() == wxID_CANCEL)
      return;

   folderPath = openFolderDialog.GetPath();
   if (folderPath.IsEmpty())
   {
      SetStatusText("No folder selected.");
      return;
   }

   folderTree->DeleteAllItems();
   wxTreeItemId root = folderTree->AddRoot("Root");
   populate_folder_tree(folderPath, root);
   rootPath = folderPath;
   this->explorerLabel->SetLabel(rootPath);
   SetStatusText("Opened folder: " + folderPath);
}

void do_editor::on_close_folder(wxCommandEvent &event)
{
   folderTree->DeleteAllItems();
   rootPath = "";
   this->explorerLabel->SetLabel(" - - - ");
}

void do_editor::on_save_file(wxCommandEvent &event)
{
   auto current = get_current_text_editor();

   wxFileDialog saveFileDialog(this, _("Select file to save"), rootPath, "",
                               file_filter,
                               wxFD_SAVE |
                                   wxFD_OVERWRITE_PROMPT);

   wxString file;

   if (current->has_changed() && !current->is_untitle())
   {

      file = current->get_path();
   }
   else if (current->has_changed() && current->is_untitle())
   {

      if (saveFileDialog.ShowModal() == wxID_CANCEL)
      {
         return;
      }
      else
      {

         file = saveFileDialog.GetPath();
      }
   }
   else
   {

      if (saveFileDialog.ShowModal() == wxID_CANCEL)
      {
         return;
      }
      else
      {

         file = saveFileDialog.GetPath();
      }
   }

   if (file.IsEmpty())
   {

      SetStatusText("No folder selected.");
      return;
   }

   if (save_text_file(file, current->get_text()))
   {
      SetStatusText("Save file folder: " + file);
   }
   else
   {
      SetStatusText("error  Saving this file: " + file);
   }

#if 0 
   std::cout << __LINE__ << "\n";
   wxFileOutputStream output_stream(file);
   if (!output_stream.IsOk())
   {
      wxLogError("Cannot save current contents in file '%s'.", file);
      return;
   }
   wxString content;
   content = current->get_text();
   std::cout << __LINE__ << "\n"
             << current->get_text() << "\n";
   output_stream.Write(content.c_str(), content.size());
#endif
}

void do_editor::on_ctrl_i(wxCommandEvent &event)
{
   auto current = get_current_text_editor();
   current->increase_font_size_by_one();
}

void do_editor::on_ctrl_l(wxCommandEvent &event)
{
   auto current = get_current_text_editor();
   current->decrease_font_size_by_one();
}

void do_editor::on_view_file_explorer(wxCommandEvent &event)
{
   auto inf = find_Panel_by_name("FileExplorer");
   if (inf != nullptr)
   {
      inf->Show(true);
      auiManager.Update();
   }
}

void do_editor::on_tree_item_activated(wxTreeEvent &event)
{
   wxTreeItemId selected = event.GetItem();
   wxString selectedName = folderTree->GetItemText(selected);

   // Get parent path
   wxString fullPath = selectedName;
   wxTreeItemId parent = folderTree->GetItemParent(selected);
   while (parent.IsOk() && parent != folderTree->GetRootItem())
   {
      fullPath = folderTree->GetItemText(parent) + "/" + fullPath;
      parent = folderTree->GetItemParent(parent);
   }

   if (wxDirExists(fullPath))
   {
      populate_folder_tree(fullPath, selected);
   }
   else
   {

      wxString fileFullPath = rootPath + "/" + fullPath;
      if (wxFileName::DirExists(fileFullPath))
      {
         return;
      }

      add_new_page(selectedName);

      auto textEditor = get_current_text_editor();

      if (textEditor)
      {

         bool loaded = textEditor->load_text_file(fileFullPath);
         if (loaded)
         {
            SetStatusText("File Loaded: " + fileFullPath);
         }
         else
         {
            SetStatusText("File not Loaded: " + fileFullPath);
         }
         return;
      }
   }
}

void do_editor::on_close_tab(wxAuiNotebookEvent &event)
{
   int pageIndex = event.GetSelection();

   auto textEditor = get_current_text_editor();

   std::cout << "textEditor->has_changed()=> " << textEditor->has_changed() << "\n"
             << textEditor->is_untitle() << "\n";

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
   return;
}
void do_editor::create_main_instances()
{
   explorerPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(300, 400));
   wxBoxSizer *explorerSizer = new wxBoxSizer(wxVERTICAL);
   explorerLabel = new wxStaticText(explorerPanel, wxID_ANY, "File Explorer");
   folderTree = new wxTreeCtrl(explorerPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT);
   explorerSizer->Add(explorerLabel, 0, wxEXPAND | wxALL, 5);
   explorerSizer->Add(folderTree, 1, wxEXPAND);
   // explorerSizer->Add(controlVersion, 1, wxEXPAND);
   explorerPanel->SetSizer(explorerSizer);
   // Create the main editor area
   editorTabs = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                  wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ACTIVE_TAB);
   auiManager.AddPane(explorerPanel, wxAuiPaneInfo().Left().Name("FileExplorer").Caption("Explorer").BestSize(300, 800).MinSize(200, 600));
   auiManager.AddPane(editorTabs, wxAuiPaneInfo().Name("Editor").CenterPane());

   auiManager.Update();
}

void do_editor::bind_to_default_event()
{
   Bind(wxEVT_MENU, &do_editor::on_open_new_file, this, wxID_NEW);
   Bind(wxEVT_MENU, &do_editor::on_open_existing_file, this, wxID_OPEN);
   Bind(wxEVT_MENU, &do_editor::on_save_file, this, wxID_SAVE);
   Bind(wxEVT_MENU, &do_editor::on_open_folder, this, OpenFolder);
   Bind(wxEVT_MENU, &do_editor::on_close_folder, this, CloseFolder);
   Bind(wxEVT_MENU, &do_editor::on_view_file_explorer, this, ViewFileExplorer);

   Bind(wxEVT_MENU, &do_editor::on_exit, this, wxID_EXIT);
   Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &do_editor::on_close_tab, this);
   Bind(wxEVT_TREE_ITEM_ACTIVATED, &do_editor::on_tree_item_activated, this);

   Bind(wxEVT_MENU, &do_editor::on_find, this, wxID_FIND);
}

void do_editor::create_main_menubar()
{
   menubar = new wxMenuBar();

   if (menubar)
   {
      auto fileMenu = create_file_menu_entries();
      menubar->Append(fileMenu, "&File");
      auto edit_menu = create_edit_menu_entries();
      menubar->Append(edit_menu, "&Edit");
      auto view_menu = create_view_menu_entries();
      menubar->Append(view_menu, "&View");
      SetMenuBar(menubar);
   }
}

void do_editor::insert_menu(wxMenu *menu, wxString title)
{
   menubar->Append(menu, title);
}

bool do_editor::contains_page_with_title(const wxString &title)
{
   size_t pageCount = editorTabs->GetPageCount();
   for (size_t i = 0; i < pageCount; ++i)
   {
      if (editorTabs->GetPageText(i) == title)
      {
         return true;
      }
   }
   return false;
}

wxAuiPaneInfo *do_editor::find_Panel_by_name(const wxString &name)
{
   wxAuiPaneInfoArray &panes = auiManager.GetAllPanes();
   for (size_t i = 0; i < panes.GetCount(); ++i)
   {
      wxAuiPaneInfo &paneInfo = panes[i];
      if (paneInfo.name == name)
      {
         return &paneInfo;
      }
   }
   return nullptr;
}