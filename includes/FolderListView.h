#pragma once

#include <wx/panel.h>
#include <wx/listctrl.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/datetime.h>
#include <functional>
#include <vector>

class FolderListView : public wxPanel
{
public:
    explicit FolderListView(wxWindow* parent);

    void SetFolder(const wxString& folder);
    void RefreshFolder();
    const wxString& GetFolder() const { return m_folder; }

    void SetOpenFileCallback(std::function<void(const wxString&)> callback);
    void SetNavigateFolderCallback(std::function<void(const wxString&)> callback);

private:
    struct Entry
    {
        wxString path;
        wxString name;
        bool directory = false;
        unsigned long long size = 0;
        wxDateTime modified;
    };

    wxString m_folder;
    wxStaticText* m_pathLabel = nullptr;
    wxListCtrl* m_list = nullptr;
    wxButton* m_upButton = nullptr;
    wxButton* m_refreshButton = nullptr;
    std::vector<Entry> m_entries;
    std::function<void(const wxString&)> m_openFile;
    std::function<void(const wxString&)> m_navigateFolder;

    void Populate();
    void ActivateRow(long row);
    static wxString FormatSize(unsigned long long bytes);
};
