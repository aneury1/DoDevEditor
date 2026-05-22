#ifndef FILETREECTRL_H
#define FILETREECTRL_H
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <string>
#include <vector>
#include "constant.h"
#include "PathData.h"
#include "Config.h"
#include "DOContextMenu.h"
 
class FileTreeCtrl : public wxTreeCtrl {
public:
    wxString rootPath;

    FileTreeCtrl(wxWindow* parent);

    void LoadFolder(const wxString& path);
    // Return full path of selected file item (empty string if it's a directory)
    wxString GetSelectedFilePath() ;

    inline std::vector<std::string> getFiles();

private:
    std::vector<std::string> files;

    void PopulateDir(wxTreeItemId parent, const wxString& path);
public:
    // Expand on demand (lazy loading)
    void OnItemExpanding(wxTreeEvent& evt);
};

 

#endif // FILETREECTRL_H