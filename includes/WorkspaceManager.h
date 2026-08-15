#ifndef WORKSPACE_MANAGER_H
#define WORKSPACE_MANAGER_H

#include <wx/string.h>
#include <vector>

struct WorkspaceFolder
{
    wxString path;
    wxString name;
};

class WorkspaceManager
{
public:
    void Clear();
    bool OpenFolder(const wxString& path);
    bool SetFolders(const std::vector<wxString>& paths);
    bool AddFolder(const wxString& path);
    bool RemoveFolder(const wxString& path);

    bool LoadWorkspace(const wxString& filePath, wxString* error = nullptr);
    bool SaveWorkspace(const wxString& filePath, wxString* error = nullptr);

    const std::vector<WorkspaceFolder>& GetFolders() const { return m_folders; }
    std::vector<wxString> GetFolderPaths() const;
    const wxString& GetWorkspaceFile() const { return m_workspaceFile; }

    bool HasFolders() const { return !m_folders.empty(); }
    bool IsWorkspaceMode() const { return !m_workspaceFile.IsEmpty() || m_folders.size() > 1; }
    wxString GetDisplayName() const;
    wxString FindContainingFolder(const wxString& fileOrDirectory) const;
    wxString GetFolderLabel(const wxString& rootPath) const;

private:
    std::vector<WorkspaceFolder> m_folders;
    wxString m_workspaceFile;

    static wxString NormalizeDirectory(const wxString& path);
    static wxString DefaultFolderName(const wxString& path);
    bool ContainsFolder(const wxString& normalizedPath) const;
};

#endif
