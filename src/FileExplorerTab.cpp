#include <wx/imaglist.h>
#include <wx/artprov.h>
#include <wx/filename.h>

#include "utils.h"
#include "TabContainer.h"
#include "FileExplorerTab.h"
#include "frame.h"
#include "constant.h"

void FileExplorerTabContainer::CreateFileListPanel()
{
    fileListPanel = new wxPanel(this, wxID_ANY);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText *panelTitle = new wxStaticText(fileListPanel, wxID_ANY, wxT("File List Metadata:"));
    listCtrl = new wxListCtrl(fileListPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);
    listCtrl->InsertColumn(0, "File Name", wxLIST_FORMAT_LEFT, 150);
    listCtrl->InsertColumn(1, "Directory", wxLIST_FORMAT_LEFT, 200);
    listCtrl->InsertColumn(2, "size", wxLIST_FORMAT_LEFT, 100);
    listCtrl->InsertColumn(2, "last Modified", wxLIST_FORMAT_LEFT, 150);
    sizer->Add(panelTitle, 0, wxEXPAND);
    sizer->Add(listCtrl, 1, wxEXPAND);
    fileListPanel->SetSizer(sizer);
}
void FileExplorerTabContainer::CreateFileTreePanel()
{
   fileTreePanel= new wxPanel(this, wxID_ANY);
   wxBoxSizer *explorerSizer = new wxBoxSizer(wxVERTICAL);
   explorerLabel = new wxStaticText(fileTreePanel, wxID_ANY, "File Explorer");
   folderTree = new wxTreeCtrl(fileTreePanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT);
   explorerSizer->Add(explorerLabel, 0, wxEXPAND | wxALL, 5);
   explorerSizer->Add(folderTree, 1, wxEXPAND);
   
      wxImageList *imageList = new wxImageList(16, 16, true);

   // Add icons to the image list
   wxBitmap folderIcon(wxArtProvider::GetBitmap(wxART_FOLDER, wxART_OTHER, wxSize(16, 16)));
   wxBitmap fileIcon(wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_OTHER, wxSize(16, 16)));
   imageList->Add(folderIcon);
   imageList->Add(fileIcon);
   folderTree->AssignImageList(imageList);
   
   
   fileTreePanel->SetSizer(explorerSizer);


}

void FileExplorerTabContainer::CreateFileSymbolPanel()
{
}

void FileExplorerTabContainer::SetupPanel()
{
    CreateFileListPanel();
    CreateFileTreePanel();
    CreateFileSymbolPanel();
    filemanagertabs = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                        wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ACTIVE_TAB);
    if(fileListPanel)
    {
        filemanagertabs->AddPage(fileListPanel, "Files", true);
    }
    if(fileTreePanel)
    {
        filemanagertabs->AddPage(fileTreePanel, "Folder Tree", true);
    }
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL); 
    sizer->Add(filemanagertabs,1, wxEXPAND|wxALL,1);
    SetSizer(sizer);
}

void FileExplorerTabContainer::AddEvents()
{
    Bind(wxEVT_TREE_ITEM_ACTIVATED, &FileExplorerTabContainer::OnItemTreeActivate, this);
}

FileExplorerTabContainer::FileExplorerTabContainer(wxWindow *parent) : wxPanel(parent, wxID_ANY)
{
    SetupPanel();
    AddEvents();
    SetBackgroundColour(wxColour(200, 200, 0, 255));
}

FileExplorerTabContainer *FileExplorerTabContainer::get()
{
    static FileExplorerTabContainer *instance = nullptr;
    if (instance == nullptr)
    {
        instance = new FileExplorerTabContainer(WindowFrame::get());
    }
    return instance;
}

wxTreeItemId FileExplorerTabContainer::AddRoot(const wxString &text)
{
   return folderTree->AddRoot(text);
}


void FileExplorerTabContainer::ClearFolderTree()
{
   if (folderTree)
   {
      folderTree->DeleteAllItems();
   }
}

Response FileExplorerTabContainer::OpenFolder()
{
   wxDirDialog openFolderDialog(this,
                                "Select a Folder to Open", "",
                                wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

   if (openFolderDialog.ShowModal() == wxID_CANCEL)
      return Response::Cancel;

   wxString folderPath = openFolderDialog.GetPath();
   if (folderPath.IsEmpty())
   {
      ///set_status_text("No folder selected.");
      return Response::Cancel;
   }

   ClearFolderTree();
   wxTreeItemId root = AddRoot("Root");
   PopulateFolderTree(folderPath.ToStdString(), root);
   rootPath = folderPath.ToStdString();
   ///explorerpanel->set_path_label(rootPath);
   ///gitpanel->load_commits_information_in_folder(std::string(folderPath.ToUTF8().data()));
   ///set_status_text("Opened folder: " + folderPath);
   return Response::Success;
}


void FileExplorerTabContainer::PopulateFolderTree(const std::string &path, wxTreeItemId parent)
{
   wxDir dir(path.c_str());
   if (!dir.IsOpened())
      return;

   wxString filename;
   bool hasFiles = dir.GetFirst(&filename);
   while (hasFiles)
   {
      wxString fullPath = wxString(path.c_str()) + wxString(FileSeparator) + wxString(filename.c_str());
      if (wxDirExists(fullPath))
      {
         wxTreeItemId folderItem = folderTree->AppendItem(parent, filename, 0);
         PopulateFolderTree(fullPath.ToStdString(), folderItem);
      }
      else
      {
        /// explorer_file.insert(std::string(fullPath.c_str()));
         folderTree->AppendItem(parent, filename, 1);
      }
      hasFiles = dir.GetNext(&filename);
   }
}

void FileExplorerTabContainer::OnItemTreeActivate(wxTreeEvent &event)
{

   wxTreeItemId selected = event.GetItem();
   wxString selectedName = folderTree->GetItemText(selected);
   // Get parent path
   wxString fullPath = selectedName;
   wxTreeItemId parent = folderTree->GetItemParent(selected);

   ////std::cout << "selected name:" << fullPath.ToStdString() << "\n";
   while (parent.IsOk() && parent != folderTree->GetRootItem())
   {
      fullPath = folderTree->GetItemText(parent) + "/" + fullPath;
      parent = folderTree->GetItemParent(parent);
   }

   if (wxDirExists(fullPath))
   {
      PopulateFolderTree(fullPath.ToStdString(), selected);
   }
   else
   {

      wxString fileFullPath = rootPath + FileSeparator + fullPath;
      
      // std::cout << "full selected name:" << fileFullPath.ToStdString() << "\n";

      if (wxFileName::DirExists(fileFullPath))
      {
        /// std::cout << "xfull selected name:" << fileFullPath.ToStdString() << "\n";
         return;
      }

      if (isFileOpened(fileFullPath.ToStdString()))
      {
        /// std::cout << "zfull selected name:" << fileFullPath.ToStdString() << "\n";

         return;
      }

      auto textEditor =  WindowFrame::get()->AddNewPage();
      //// add_new_page(selectedName);
      //// std::cout <<"Call set Auto completer "<< explorer_file.size()<<"\n";
      if (textEditor)
      {
         auto content = ReadFile(fileFullPath.ToStdString());
         
         /// std::cout <<"Call set Auto completer "<< explorer_file.size()<<"\n";
         
         auto ds = fromStrTo8Vec(content);
         
        // textEditor->set_filepath(fileFullPath);
         
       //  if(explorer_file.size()>0)
        //    textEditor->set_auto_completer(new FileCompleter(this->explorer_file));
         
         if (ds.size() > 0)
         {
            auto str = fileFullPath.ToStdString();
            int pos = str.find_last_of(FileSeparator);
            if(pos != std::string::npos){
               str = str.substr(pos+1, str.size()-pos);
            }
            WindowFrame::get()->GetTabContainer()->SetTitleToCurrentPage(str); 
            textEditor->SetPath(fileFullPath.ToStdString());
            textEditor->setData(ds);
         }
         else
         {
         }
         return;
      }else
      {
       
      }
   }
}

bool FileExplorerTabContainer::isFileOpened(std::string fileFullPath){
    return false;
}