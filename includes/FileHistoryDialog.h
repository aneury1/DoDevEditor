#pragma once

#include "LocalHistoryManager.h"

#include <wx/dialog.h>
#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h>

#include <vector>

class FileHistoryDialog : public wxDialog
{
public:
    FileHistoryDialog(wxWindow* parent,
                      const wxString& filePath,
                      const wxString& workspaceRoot);

    bool HasSelection() const;
    bool GetSelectedSnapshot(wxString& content, wxString* error = nullptr) const;
    const LocalHistoryEntry* GetSelectedEntry() const;

private:
    wxString m_filePath;
    wxString m_workspaceRoot;
    std::vector<LocalHistoryEntry> m_entries;
    wxListCtrl* m_list = nullptr;
    wxStyledTextCtrl* m_patchPreview = nullptr;
    wxStyledTextCtrl* m_snapshotPreview = nullptr;
    wxStaticText* m_details = nullptr;
    wxButton* m_restoreButton = nullptr;
    long m_selectedIndex = -1;

    void Populate();
    void SelectEntry(long row);
    void ConfigurePreview(wxStyledTextCtrl* editor);
};
