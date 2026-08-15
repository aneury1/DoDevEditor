#pragma once

#include <wx/wx.h>
#include <functional>

class EditorPage;

class GitCommitDiffPage : public wxPanel
{
public:
    GitCommitDiffPage(wxWindow* parent,
                      const wxString& repositoryRoot,
                      const wxString& commitHash,
                      const wxString& oldPath,
                      const wxString& newPath,
                      const wxString& status);

    const wxString& GetRepositoryRoot() const { return m_repositoryRoot; }
    const wxString& GetCommitHash() const { return m_commitHash; }
    const wxString& GetGitPath() const { return m_newPath; }
    wxString GetTabTitle() const;
    wxString GetPatchForAI() const;

    void SetOpenFileCallback(std::function<void(const wxString&)> callback);
    void RefreshDiff();
    void ApplyTheme();

private:
    wxString m_repositoryRoot;
    wxString m_commitHash;
    wxString m_parentHash;
    wxString m_oldPath;
    wxString m_newPath;
    wxString m_status;

    wxStaticText* m_title = nullptr;
    wxStaticText* m_meta = nullptr;
    EditorPage* m_left = nullptr;
    EditorPage* m_right = nullptr;
    std::function<void(const wxString&)> m_openFileCallback;

    wxString Quote(const wxString& value) const;
    bool RunGit(const wxString& args, wxArrayString& output, wxArrayString& errors) const;
    wxString RunGitText(const wxString& args, bool* ok = nullptr) const;
    wxString RevisionText(const wxString& revision, const wxString& path) const;
    wxString AbsolutePath(const wxString& path) const;
    void ResolveParent();
    void LoadTexts();
    void OpenWorkingFile();
};
