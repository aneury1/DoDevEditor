#include <wx/imaglist.h>
#include <wx/artprov.h>
#include "explorer_panel.h"
#include "utils.h"
#include "callbacks.h"

explorer_panel::explorer_panel(wxWindow *parent) : wxPanel(parent, wxID_ANY)
{
   SetBackgroundColour(wxColour(0x43, 0x43, 0x43, 255));
   wxBoxSizer *explorerSizer = new wxBoxSizer(wxVERTICAL);
   explorerLabel = new wxStaticText(this, wxID_ANY, "File Explorer");
   folderTree = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT);
   explorerSizer->Add(explorerLabel, 0, wxEXPAND | wxALL, 5);
   explorerSizer->Add(folderTree, 1, wxEXPAND);
   // explorerSizer->Add(controlVersion, 1, wxEXPAND);
   SetSizer(explorerSizer);

   Bind(wxEVT_TREE_ITEM_ACTIVATED, &explorer_panel::on_tree_item_activated, this);
   folderTree->Bind(wxEVT_CONTEXT_MENU, &explorer_panel::on_context_menu, this);

   wxImageList *imageList = new wxImageList(16, 16, true);

   // Add icons to the image list
   wxBitmap folderIcon(wxArtProvider::GetBitmap(wxART_FOLDER, wxART_OTHER, wxSize(16, 16)));
   wxBitmap fileIcon(wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_OTHER, wxSize(16, 16)));
   imageList->Add(folderIcon);
   imageList->Add(fileIcon);
   folderTree->AssignImageList(imageList);
}

explorer_panel::~explorer_panel() {}

void explorer_panel::set_path_label(const wxString &path)
{
   if (explorerLabel && path.size() > 0)
   {
      /// explorerLabel->SetValue(path);
   }
}

void explorer_panel::clear_folder_tree()
{
   if (folderTree)
   {
      folderTree->DeleteAllItems();
   }
}

wxTreeItemId explorer_panel::add_root(const wxString &text)
{
   return folderTree->AddRoot(text);
}

void explorer_panel::populate_folder_tree(const wxString &path, wxTreeItemId parent)
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
         wxTreeItemId folderItem = folderTree->AppendItem(parent, filename, 0);
         populate_folder_tree(fullPath, folderItem);
      }
      else
      {
         folderTree->AppendItem(parent, filename, 1);
      }
      hasFiles = dir.GetNext(&filename);
   }
}

void explorer_panel::on_context_menu(wxContextMenuEvent &event)
{
   std::cout << "EVent has been triggered: " << event.GetId() << "\n";
   // Create the context menu
   wxMenu menu;
   if (get_root_path().size() > 0)
   {
    
      auto str = this->getSelectedPath();
      if(get_seletion_type()==selection_type::Folder)
      {
         menu.Append(CreateFileInCurrentSelection, "Create a file", "");
         menu.Append(CreateFolderInCurrentSelection, "Create a folder", "");
         menu.Append(CloseFolder, "Close Folder", "");
         menu.Append(ExpandSubFolder, "Expand Folder \""+str+"\"", "");
      }
      else
      {
        
         menu.Append(OpenFile         , "Open File \""+str+"\"", "");
         menu.Append(CloneFile        , "Clone file \""+str+"\"", "");
         menu.Append(DeleteCurrentFile, "Delete File \""+str+"\"", "");
         menu.Append(RenameFile       , "Rename File \""+str+"\"", "");
      }
   }
   else
   {
      menu.Append(OpenFolder, "Open Folder", "");
      menu.Append(wxID_OPEN, "Open File", "");
   }

   Bind(wxEVT_MENU, &explorer_panel::create_file_in_current_selection, this, CreateFileInCurrentSelection);
   Bind(wxEVT_MENU, &explorer_panel::create_folder_in_current_selection, this, CreateFolderInCurrentSelection);
   

   // Show the menu at the current mouse position
   PopupMenu(&menu, ScreenToClient(wxGetMousePosition()));
}

void explorer_panel::create_file_in_current_selection(wxCommandEvent &ev)
{
   /// CreateFileInCurrentSelection,

   wxTextEntryDialog dialog(this,
                            wxT("Enter the file Name"));

   if (dialog.ShowModal() == wxID_OK)
   {
     /// wxMessageBox(dialog.GetValue(), wxT("Got this"));
     auto str = this->getSelectedPath();
     
     auto drootPath = get_root_path()+"/"+str+"/"+dialog.GetValue();  
    
     wxMessageBox(drootPath, wxT("Got this"));
     

   }
}

void explorer_panel::create_folder_in_current_selection(wxCommandEvent &ev)
{
   /// CreateFileInCurrentSelection,

   wxTextEntryDialog dialog(this,
                            wxT("Enter the file Name"));

   if (dialog.ShowModal() == wxID_OK)
   {
      wxMessageBox(dialog.GetValue(), wxT("Got this"));
   }
}


void explorer_panel::on_tree_item_activated(wxTreeEvent &event)
{

   wxTreeItemId selected = event.GetItem();
   wxString selectedName = folderTree->GetItemText(selected);
   // Get parent path
   wxString fullPath = selectedName;
   wxTreeItemId parent = folderTree->GetItemParent(selected);

   std::cout << "selected name:" << fullPath.ToStdString() << "\n";
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

      wxString fileFullPath = get_root_path() + "/" + fullPath;
      if (wxFileName::DirExists(fileFullPath))
      {
         return;
      }

      if (is_file_already_in_the_editor(fileFullPath))
      {
         return;
      }

      add_new_page(selectedName);

      auto textEditor = get_current_text_editor();

      if (textEditor)
      {
         auto content = read_file(fileFullPath.ToStdString());
         textEditor->set_text(content);
         textEditor->set_filepath(fileFullPath);
         if (content.size() > 0)
         {
            set_status_text("File Loaded: " + fileFullPath);
         }
         else
         {
            set_status_text("File not Loaded: " + fileFullPath);
         }
         return;
      }
   }
}

wxString explorer_panel::getSelectedPath()
{
   wxTreeItemId selected = folderTree->GetSelection();
   if (selected.IsOk())
   {
      wxString itemText = folderTree->GetItemText(selected);
    ///  wxMessageBox("Selected item: " + itemText, "Info", wxOK | wxICON_INFORMATION);
      return itemText;
   }
   else
   {
     // wxMessageBox("No item is selected", "Info", wxOK | wxICON_INFORMATION);
   }
   return "";
}

selection_type explorer_panel::get_seletion_type(){
   wxTreeItemId selected = folderTree->GetSelection();
   if(!selected.IsOk()){
      return selection_type::None;
   }
   wxString selectedName = folderTree->GetItemText(selected);
   // Get parent path
   wxString fullPath = selectedName;
   wxTreeItemId parent = folderTree->GetItemParent(selected);

   std::cout << "selected name:" << fullPath.ToStdString() << "\n";
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

      wxString fileFullPath = get_root_path() + "/" + fullPath;
      if (wxFileName::DirExists(fileFullPath))
      {
         return selection_type::Folder;
      }

     selection_type::File;
   }
   return selection_type::None;
}