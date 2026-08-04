#ifndef SOURCE_CONTROL_PANEL_H
#define SOURCE_CONTROL_PANEL_H

#include <wx/wx.h>
#include <wx/dataview.h>
#include <vector>

struct CommitInfo
{
    wxString message;
    wxString author;
    wxString date;
};

class SourceControlPanel : public wxPanel
{
public:
    SourceControlPanel(wxWindow* parent);

    // ─────────────────────────
    // PUBLIC API
    // ─────────────────────────

public:
    void AddFile(const wxString& status,
                 const wxString& file,
                 const wxString& action);
    void AddCommit(const wxString& message,
                   const wxString& author,
                   const wxString& date);

private:
    // FILES
    wxDataViewCtrl* m_filesView = nullptr;
    wxDataViewListStore* m_filesModel = nullptr;

    // COMMITS
    wxDataViewCtrl* m_commitView = nullptr;
    wxDataViewListStore* m_commitModel = nullptr;

    std::vector<CommitInfo> m_commits;
};

#endif