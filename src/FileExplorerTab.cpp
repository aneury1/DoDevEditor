#include <wx/imaglist.h>
#include <wx/artprov.h>
#include <wx/filename.h>

#include "utils.h"
#include "TabContainer.h"
#include "FileExplorerTab.h"
#include "frame.h"
#include "constant.h"

static const char *RootTree = "Root";

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
   fileTreePanel = new wxPanel(this, wxID_ANY);
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

void FileExplorerTabContainer::CreateGitPanel()
{
   gitPanel = new GitPanel(this);
}

void FileExplorerTabContainer::SetupPanel()
{
   CreateFileListPanel();
   CreateFileTreePanel();
   CreateFileSymbolPanel();
   CreateGitPanel();

   int flags = wxAUI_NB_DEFAULT_STYLE & ~wxAUI_NB_CLOSE_BUTTON & ~wxAUI_NB_CLOSE_ON_ACTIVE_TAB;
   //// Original wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ACTIVE_TAB;
   filemanagertabs = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                       flags);

   if (fileTreePanel)
   {
      filemanagertabs->AddPage(fileTreePanel, "Folder Tree", true);
   }
   if (fileListPanel)
   {
      filemanagertabs->AddPage(fileListPanel, "Files", false);
   }
   if (gitPanel)
   {
      filemanagertabs->AddPage(gitPanel, "Git Panel", false);
   }

   wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
   sizer->Add(filemanagertabs, 1, wxEXPAND | wxALL, 1);
   SetSizer(sizer);
}

void FileExplorerTabContainer::AddEvents()
{
   Bind(wxEVT_TREE_ITEM_ACTIVATED, &FileExplorerTabContainer::OnItemTreeActivate, this);
   Bind(wxEVT_LIST_ITEM_SELECTED, &FileExplorerTabContainer::OnItemListActivate, this);
}

void FileExplorerTabContainer::OnItemListActivate(wxListEvent &event)
{
   long itemIndex = event.GetIndex();
   // Example: Get text from column 1 (assuming columns exist)
   wxString fileFullPath = listCtrl->GetItemText(itemIndex, 1);
   auto textEditor = WindowFrame::get()->AddNewPage(); /// WindowFrame::get()->AddNewPage(fileFullPath.ToStdString());
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
         if (pos != std::string::npos)
         {
            str = str.substr(pos + 1, str.size() - pos);
         }
         WindowFrame::get()->GetTabContainer()->SetTitleToCurrentPage(str);
         textEditor->SetPath(fileFullPath.ToStdString());
         textEditor->setData(ds);
      }
      else
      {
      }
      return;
   }
   else
   {
   }
   /// wxLogMessage("Selected row: %ld, Column 1 data: %s", itemIndex, columnData);
}

void FileExplorerTabContainer::UpdateFileList()
{
   PopulateList(this->explorer_files);
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

void FileExplorerTabContainer::ClearFileList()
{
   explorer_files.clear();
   if (listCtrl)
   {
      listCtrl->DeleteAllItems();
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
      /// set_status_text("No folder selected.");
      return Response::Cancel;
   }

   ClearFolderTree();
   root = AddRoot(RootTree);
   currentEditorState = EditorState::OpeningFolder;
   PopulateFolderTree(folderPath.ToStdString(), root);
   rootPath = folderPath.ToStdString();
   if (gitPanel)
   {
      gitPanel->LoadCommitsInformationInFolder(rootPath);
   }
   UpdateFileList();
   /// explorerpanel->set_path_label(rootPath);
   /// gitpanel->load_commits_information_in_folder(std::string(folderPath.ToUTF8().data()));
   /// set_status_text("Opened folder: " + folderPath);
   return Response::Success;
}
#include "ParseHeaders.h"
void FileExplorerTabContainer::PopulateFolderTree(const std::string &path, wxTreeItemId parent)
{

   if (path.size() <= 0)
      return;

   if (path[0] == '.')
      return;

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
         auto s = fullPath.c_str();
         FileInfo info;
         info.file = ExtraOnlyFileName(std::string(s));
         info.path = fullPath.c_str();
         info.dateOfCreation = "00/00/0000";
         info.lastModification = "00/00/0000";
         info.size = 1000;

         AddEntry(fullPath.ToStdString() + "/00/000/000");
         
         if(fullPath.ToStdString().find_last_of(".h")!= std::string::npos)
            ParseHeaderFile(fullPath.ToStdString());
         
         explorer_files.insert(info);
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
   currentEditorState = EditorState::DoingNothing;
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

      auto textEditor = WindowFrame::get()->AddNewPage(); /// WindowFrame::get()->AddNewPage(fileFullPath.ToStdString());
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
            if (pos != std::string::npos)
            {
               str = str.substr(pos + 1, str.size() - pos);
            }
            WindowFrame::get()->GetTabContainer()->SetTitleToCurrentPage(str);
            textEditor->SetPath(fileFullPath.ToStdString());
            textEditor->setData(ds);
         }
         else
         {
         }
         return;
      }
      else
      {
      }
   }
}

bool FileExplorerTabContainer::isFileOpened(std::string fileFullPath)
{
   return false;
}

void FileExplorerTabContainer::PopulateList(const std::set<FileInfo> &fileSet)
{
   listCtrl->DeleteAllItems(); // Clear any existing items

   for (const auto &file : fileSet)
   {
      long index = listCtrl->InsertItem(listCtrl->GetItemCount(), file.file);
      listCtrl->SetItem(index, 1, file.path);
      listCtrl->SetItem(index, 2, file.lastModification);
      listCtrl->SetItem(index, 3, wxString::Format("%ld", file.size));
   }
}

wxTreeItemId FileExplorerTabContainer::FindOrCreateNode(wxTreeItemId parent, const wxString &label)
{
   if (!parent.IsOk())
      return wxTreeItemId();

   // Check if the node already exists
   wxTreeItemIdValue cookie;
   wxTreeItemId child = folderTree->GetFirstChild(parent, cookie);
   while (child.IsOk())
   {
      if (folderTree->GetItemText(child) == label)
      {
         return child;
      }
      child = folderTree->GetNextChild(parent, cookie);
   }

   // Create new node
   return folderTree->AppendItem(parent, label);
}

void FileExplorerTabContainer::AddEntry(const std::string &path)
{
   wxArrayString nodes = wxStringTokenize(path, FileSeparator);
   if (nodes.IsEmpty())
      return;

   wxTreeItemId parent = (nodes[0] == RootTree) ? root : wxTreeItemId();
   wxTreeItemId current = parent;

   for (size_t i = 1; i < nodes.GetCount(); ++i)
   {
      current = FindOrCreateNode(current, nodes[i]);
   }
}


std::set<std::string> FileExplorerTabContainer::GetFiles(){
   std::set<std::string> files;
   for(auto it : explorer_files){
      std::string pd = it.path+FileSeparator+it.file;
      files.insert(pd);
   }
   return files;
}