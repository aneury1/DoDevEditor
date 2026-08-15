#pragma once

#include <wx/wx.h>
#include <wx/stc/stc.h>
#include <wx/splitter.h>
#include <functional>

class GitDiffPage : public wxPanel
{
public:
    GitDiffPage(wxWindow* parent,
                const wxString& repositoryRoot,
                const wxString& gitPath,
                const wxString& status);

    const wxString& GetRepositoryRoot() const { return m_repositoryRoot; }
    const wxString& GetGitPath() const { return m_gitPath; }
    const wxString& GetStatus() const { return m_status; }
    wxString GetTabTitle() const;
    wxString GetPatchForAI() const;

    void SetOpenFileCallback(std::function<void(const wxString&)> callback);
    void SetRepositoryChangedCallback(std::function<void()> callback);
    void RefreshDiff();
    void UpdateStatus(const wxString& status);

private:
    enum class DiffMode
    {
        Unstaged,
        Staged
    };

    wxString m_repositoryRoot;
    wxString m_gitPath;
    wxString m_status;
    wxString m_oldPath;
    wxString m_newPath;
    DiffMode m_mode = DiffMode::Unstaged;

    wxStaticText* m_title = nullptr;
    wxStaticText* m_modeLabel = nullptr;
    wxButton* m_stageButton = nullptr;
    wxButton* m_unstageButton = nullptr;
    wxStyledTextCtrl* m_left = nullptr;
    wxStyledTextCtrl* m_right = nullptr;

    std::function<void(const wxString&)> m_openFileCallback;
    std::function<void()> m_repositoryChangedCallback;

    wxString Quote(const wxString& value) const;
    bool RunGit(const wxString& args, wxArrayString& output, wxArrayString& errors) const;
    wxString RunGitText(const wxString& args, bool* ok = nullptr) const;
    wxString AbsolutePath(const wxString& relative) const;
    wxString ReadWorkingTreeFile() const;
    wxString ReadIndexFile() const;
    wxString ReadHeadFile() const;
    wxString DiffPatch() const;

    void ConfigureEditor(wxStyledTextCtrl* editor, bool addedSide);
    void LoadTexts();
    void ApplyHunkMarkers(const wxString& patch);
    void ClearMarkers();
    void SetModeFromStatus();
    void UpdateHeader();
    void ReloadStatus();
    void Stage();
    void Unstage();
    void Discard();
    void OpenFile();
    void ShowError(const wxString& title, const wxArrayString& errors) const;

    static wxString DecodeGitPath(const wxString& path);
    static void SplitRenamePath(const wxString& gitPath, wxString& oldPath, wxString& newPath);
};
