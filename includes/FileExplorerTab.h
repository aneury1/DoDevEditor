#ifndef __FILE_EXPLORER_TAB_H_DEFINED
#define __FILE_EXPLORER_TAB_H_DEFINED
#include <set>
#include <string>
#include <wx/wx.h>
#include <wx/listctrl.h>
#include "frame.h"
#include "constant.h"
#include "GitPanel.h"
#include <wx/tokenzr.h>

struct FileExplorerTabContainer : public wxPanel
{
    
    enum class EditorState{
       DoingNothing,
       OpeningFolder,
       OtherNotKnownState
    };
    wxTreeItemId root; 
    EditorState currentEditorState = EditorState::DoingNothing;


    // Structure for file info
    struct FileInfo
    {
        std::string file;
        std::string path;
        std::string dateOfCreation;
        long size;
        std::string lastModification;

        // Define ordering for std::set
        bool operator<(const FileInfo &other) const
        {
            return file < other.file; // Sort by filename
        }
    };
    void PopulateList(const std::set<FileInfo> &fileSet);
    std::string rootPath;
    std::set<FileInfo> explorer_files;

    wxBoxSizer *mainSizer;

    wxAuiNotebook *filemanagertabs;

    wxPanel *fileListPanel;
    wxPanel *fileTreePanel;
    wxPanel *fileSymbolPanel;

    /// 1
    wxListCtrl *listCtrl = nullptr;
    /// 2
    wxStaticText *explorerLabel;
    /// 3
    wxTreeCtrl *folderTree;
    /// 4
    GitPanel *gitPanel;

    wxTreeItemId AddRoot(const wxString &text);
    Response OpenFolder();
    void PopulateFolderTree(const std::string &path, wxTreeItemId parent);
    wxTreeItemId FindOrCreateNode(wxTreeItemId parent, const wxString &label);
    void AddEntry(const std::string &path);
    void ClearFolderTree();
    void ClearFileList();
    void CreateFileListPanel();
    void CreateFileTreePanel();
    void CreateFileSymbolPanel();
    void CreateGitPanel();

    void SetupPanel();

    void AddEvents();

    void UpdateFileList();

    bool isFileOpened(std::string fileFullPath);

    void OnItemTreeActivate(wxTreeEvent &event);
    
    void OnItemListActivate(wxListEvent& ev );

    FileExplorerTabContainer(wxWindow *parent);

    static FileExplorerTabContainer *get();
};

#endif /// __FILE_EXPLORER_TAB_H_DEFINED