#ifndef __FILE_EXPLORER_TAB_H_DEFINED
#define __FILE_EXPLORER_TAB_H_DEFINED
#include <set>
#include <string>
#include <wx/wx.h>
#include <wx/listctrl.h>
#include "frame.h"
#include "constant.h"


struct FileExplorerTabContainer : public wxPanel{
     
    std::string rootPath;
    std::set<std::string> explorer_file;

    wxBoxSizer *mainSizer;

    wxAuiNotebook *filemanagertabs;

    wxPanel *fileListPanel;    
    wxPanel *fileTreePanel;    
    wxPanel *fileSymbolPanel;    

    /// 1
    wxListCtrl* listCtrl = nullptr;
    /// 2 
    wxStaticText *explorerLabel;
    wxTreeCtrl* folderTree;

    wxTreeItemId AddRoot(const wxString &text);
    Response OpenFolder();
    void PopulateFolderTree(const std::string &path, wxTreeItemId parent);
    void ClearFolderTree();
    void CreateFileListPanel();
    void CreateFileTreePanel();
    void CreateFileSymbolPanel();
    
    void SetupPanel();

    void AddEvents();
    bool isFileOpened(std::string fileFullPath);

    void OnItemTreeActivate(wxTreeEvent &event);

    FileExplorerTabContainer(wxWindow *parent);

    static FileExplorerTabContainer *get();
};

#endif /// __FILE_EXPLORER_TAB_H_DEFINED