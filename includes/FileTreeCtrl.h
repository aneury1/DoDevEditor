#ifndef FILETREECTRL_H
#define FILETREECTRL_H

#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <string>
#include <vector>
#include "constant.h"
#include "PathData.h"
#include "DOContextMenu.h"

class FileTreeCtrl : public wxTreeCtrl
{
public:
    // Kept for compatibility with older code. In a multi-root workspace this
    // is the first workspace folder.
    wxString rootPath;

    explicit FileTreeCtrl(wxWindow* parent);

    // Single-folder mode: folder contents are shown directly, as before.
    void LoadFolder(const wxString& path);

    // Workspace mode: each folder is shown as a top-level explorer node.
    void LoadFolders(const std::vector<wxString>& paths, bool workspaceMode);
    void ClearFolders();

    const std::vector<wxString>& GetRootPaths() const { return m_rootPaths; }
    wxString GetContainingRoot(const wxString& path) const;

    // Full selected filesystem path (file or directory).
    wxString GetSelectedPath();
    // Full selected file path, or empty when a directory is selected.
    wxString GetSelectedFilePath();

    std::vector<std::string> getFiles() const { return files; }

private:
    std::vector<std::string> files;
    std::vector<wxString> m_rootPaths;

    void PopulateDir(wxTreeItemId parent, const wxString& path);

public:
    void OnItemExpanding(wxTreeEvent& evt);
};

#endif // FILETREECTRL_H
