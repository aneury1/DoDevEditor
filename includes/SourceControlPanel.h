#ifndef SOURCE_CONTROL_PANEL_H
#define SOURCE_CONTROL_PANEL_H

#include <wx/wx.h>
#include <wx/dataview.h>
#include <functional>
#include <vector>

struct CommitInfo
{
    wxString hash;
    wxString message;
    wxString author;
    wxString date;
};

struct GitFileInfo
{
    wxString status;
    wxString file;
};

struct GitCommitFileInfo
{
    wxString status;
    wxString oldFile;
    wxString file;
};

class SourceControlPanel : public wxPanel
{
public:
    explicit SourceControlPanel(wxWindow* parent);

    void SetRepositoryPath(const wxString& path);
    const wxString& GetRepositoryPath() const { return m_repositoryPath; }
    void RefreshRepository();
    void SetOpenFileCallback(std::function<void(const wxString&)> callback);
    void SetOpenDiffCallback(std::function<void(const wxString&, const wxString&, const wxString&)> callback);
    void SetOpenCommitDiffCallback(std::function<void(const wxString&, const wxString&, const wxString&, const wxString&, const wxString&)> callback);

private:
    wxString m_repositoryPath;
    wxString m_repositoryRoot;
    std::function<void(const wxString&)> m_openFileCallback;
    std::function<void(const wxString&, const wxString&, const wxString&)> m_openDiffCallback;
    std::function<void(const wxString&, const wxString&, const wxString&, const wxString&, const wxString&)> m_openCommitDiffCallback;

    wxStaticText* m_repoLabel = nullptr;
    wxTextCtrl* m_commitMessage = nullptr;

    wxDataViewCtrl* m_filesView = nullptr;
    wxDataViewListStore* m_filesModel = nullptr;

    wxDataViewCtrl* m_commitView = nullptr;
    wxDataViewListStore* m_commitModel = nullptr;

    wxStaticText* m_commitFilesLabel = nullptr;
    wxDataViewCtrl* m_commitFilesView = nullptr;
    wxDataViewListStore* m_commitFilesModel = nullptr;

    std::vector<GitFileInfo> m_files;
    std::vector<CommitInfo> m_commits;
    std::vector<GitCommitFileInfo> m_commitFiles;

    wxString Quote(const wxString& value) const;
    bool RunGit(const wxString& args, wxArrayString& output, wxArrayString& errors) const;
    bool ResolveRepositoryRoot();
    void LoadStatus();
    void LoadHistory();
    void LoadCommitFiles(const wxString& commitHash);
    int SelectedCommitRow() const;
    int SelectedCommitFileRow() const;
    void OpenSelectedCommitDiff();
    int SelectedFileRow() const;
    wxString SelectedFilePath() const;
    wxString DecodeGitPath(const wxString& path) const;
    void StageSelected();
    void UnstageSelected();
    void DiscardSelected();
    void Commit();
    void OpenSelectedFile();
    void OpenSelectedDiff();
    void ShowGitError(const wxString& title, const wxArrayString& errors) const;
};

#endif
