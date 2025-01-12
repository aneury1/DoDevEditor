#include "id_handler.h"
#include "main_frame.h"
#include "text_editor.h"
#include "main_menu.h"
#include <wx/wfstream.h>
#include <iostream>

template <class T>
void print(T msg)
{
   std::cout << msg << "\n";
}

do_editor::do_editor() : wxFrame(nullptr, wxID_ANY, "DoDevEditor : Text Editor", wxDefaultPosition, wxSize(800, 600))

{
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

void do_editor::setup_main_settings()
{

   auiManager.SetManagedWindow(this);

   if (!menubar)
   {
      menubar = new wxMenuBar();

      wxMenu *fileMenu = new wxMenu();
      fileMenu->Append(wxID_NEW, "&New File\tCtrl-N", "Create a new document");
      fileMenu->Append(wxID_OPEN, "&Open File\tCtrl-O", "Open a File");
      fileMenu->Append(OpenFolder, "&Open Folder\tCtrl-O", "Open a Folder");
      fileMenu->Append(CloseFolder, "&Close Opened Folder\tCtrl-Q", "Close a Folder");
      fileMenu->Append(wxID_SAVE, "&Save\tCtrl-S", "Save the current document");
      fileMenu->AppendSeparator();
      fileMenu->Append(wxID_SAVE, "&Print File\tCtrl-P", "Print current file");
      fileMenu->AppendSeparator();
      fileMenu->Append(wxID_EXIT, "E&xit\tCtrl-Q", "Exit the application");
      menubar->Append(fileMenu, "&File");
      auto edit_menu = create_edit_menu_entries();
      menubar->Append(edit_menu, "&Edit");
      auto view_menu = create_view_menu_entries();
      menubar->Append(view_menu, "&View");
 
      SetMenuBar(menubar);
   }
   else
   {
      
   }

   explorerPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(300, 800));
   wxBoxSizer *explorerSizer = new wxBoxSizer(wxVERTICAL);
   explorerLabel = new wxStaticText(explorerPanel, wxID_ANY, "File Explorer");
   folderTree = new wxTreeCtrl(explorerPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT);
   explorerSizer->Add(explorerLabel, 0, wxEXPAND | wxALL, 5);
   explorerSizer->Add(folderTree, 1, wxEXPAND);
   explorerPanel->SetSizer(explorerSizer);

   // Create the main editor area
   editorTabs = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                  wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ACTIVE_TAB);

   auiManager.AddPane(explorerPanel, wxAuiPaneInfo().Left().Caption("Explorer").BestSize(300, 800).MinSize(200, 600));
   auiManager.AddPane(editorTabs, wxAuiPaneInfo().CenterPane());
   
    


   auiManager.Update();
   Bind(wxEVT_MENU, &do_editor::on_open_new_file, this, wxID_NEW);
   Bind(wxEVT_MENU, & do_editor::on_open_existing_file, this, wxID_OPEN);
   Bind(wxEVT_MENU, &do_editor::on_save_file, this, wxID_SAVE);
   Bind(wxEVT_MENU, &do_editor::on_open_folder, this, OpenFolder);
   Bind(wxEVT_MENU, &do_editor::on_close_folder, this, CloseFolder);
   Bind(wxEVT_MENU, &do_editor::on_exit, this, wxID_EXIT);
   Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &do_editor::on_close_tab, this);
   Bind(wxEVT_TREE_ITEM_ACTIVATED, &do_editor::on_tree_item_activated, this);
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
   // Define the Ctrl+I shortcut

   CreateStatusBar(1);
   SetStatusText("Ready");

   add_new_page("Untitled");
   Centre();
}

text_editor *do_editor::add_new_page(const wxString &title)
{

   text_editor *newPage = new text_editor(editorTabs);

   editorTabs->AddPage(newPage, title, true);

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
   /// return dynamic_cast<wxStyledTextCtrl *>(currentPanel->GetChildren().Item(0)->GetData());
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
   this->add_new_page("Untitled");
}
void do_editor::on_open_existing_file(wxCommandEvent &event)
{
   wxFileDialog openFileDialog(this, _("Select file to open"), "", "",
                               "*.*",
                               //"files (*.hxx)|(*.hh)|(*.h)|(*.cpp)|(*.cc)|(*.cxx)|(*.js)|"
                               //"(*.json)|(*.txt)|(*.java)|(*.asm)|(*.s)|(*.cob)|(*Makefile)",
                               wxFD_OPEN | wxFD_FILE_MUST_EXIST );
   if(openFileDialog.ShowModal()== wxID_CANCEL)
      return ;
   wxString filet = openFileDialog.GetPath();
   if (filet.IsEmpty())
   {
      SetStatusText("No folder selected.");
      return;
   }
    this->add_new_page("Untitled");
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
   //this->explorerLabel->SetText(rootPath);
   SetStatusText("Opened folder: " + folderPath);
}

void do_editor::on_close_folder(wxCommandEvent& event){
   folderTree->DeleteAllItems();
   rootPath = "";
   //this->explorerLabel->SetText(" - - - ");
}


void do_editor::on_save_file(wxCommandEvent &event)
{
   auto current = get_current_text_editor();

   wxFileDialog saveFileDialog(this, _("Select file to save"), rootPath, "",
                               "files (*.*)",
                               //"files (*.hxx)|(*.hh)|(*.h)|(*.cpp)|(*.cc)|(*.cxx)|(*.js)|"
                               //"(*.json)|(*.txt)|(*.java)|(*.asm)|(*.s)|(*.cob)|(*Makefile)",
                               wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

   wxString file;

   if (current->has_changed() && !current->is_untitle())
   {
      std::cout << __LINE__ << "\n";
      file = current->get_path();
   }
   else
   {
      std::cout << __LINE__ << "\n";
      if (saveFileDialog.ShowModal() == wxID_CANCEL)
      {
         return;
      }
      else
      {
         std::cout << __LINE__ << "\n";
         file = saveFileDialog.GetPath();
      }
   }

   if (file.IsEmpty())
   {
      std::cout << __LINE__ << "\n";
      SetStatusText("No folder selected.");
      return;
   }
   std::cout << __LINE__ << "\n";
   wxFileOutputStream output_stream(saveFileDialog.GetPath());
   if (!output_stream.IsOk())
   {
      wxLogError("Cannot save current contents in file '%s'.", saveFileDialog.GetPath());
      return;
   }
   wxString content;
   content = current->get_text();
   std::cout << __LINE__ << "\n"
             << current->get_text() << "\n";
   output_stream.Write(content.c_str(), content.size());
   SetStatusText("Save file folder: " + folderPath);
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
      add_new_page(selectedName);
      /// wxStyledTextCtrl *textEditor = get_current_text_editor();
      auto textEditor = get_current_text_editor();

      if (textEditor)
      {
         wxString fileFullPath = rootPath + "/" + fullPath;
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
   /// wxLogMessage("Trying closing this window %d", pageIndex);
   auto textEditor = get_current_text_editor();

   if (textEditor && !textEditor->has_changed())
   {
      event.Skip();
      return;
   }
   int query = wxMessageBox("¿Do you want to close?",
                            "Confirm close",
                            wxYES_NO | wxCANCEL | wxICON_QUESTION);
   if (query != wxYES)
   {
      event.Veto();
      return;
   }
   event.Skip();
   return;
}


void do_editor::insert_menu(wxMenu *menu, wxString title){
   menubar->Append(menu, title );
}