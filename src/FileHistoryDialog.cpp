#include "FileHistoryDialog.h"

#include <wx/filename.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/stattext.h>

#include <algorithm>

namespace
{
wxString HumanSize(long long bytes)
{
    if (bytes < 1024)
        return wxString::Format("%lld B", bytes);
    if (bytes < 1024LL * 1024LL)
        return wxString::Format("%.1f KB", static_cast<double>(bytes) / 1024.0);
    return wxString::Format("%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
}
}

FileHistoryDialog::FileHistoryDialog(wxWindow* parent,
                                     const wxString& filePath,
                                     const wxString& workspaceRoot)
    : wxDialog(parent, wxID_ANY,
               "Local History - " + wxFileName(filePath).GetFullName(),
               wxDefaultPosition, wxSize(1000, 700),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_filePath(filePath),
      m_workspaceRoot(workspaceRoot)
{
    SetBackgroundColour(wxColour(30, 30, 30));
    wxString loadError;
    m_entries = LocalHistoryManager::List(filePath, workspaceRoot, &loadError);

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* header = new wxStaticText(this, wxID_ANY,
        "Saved revisions for " + filePath);
    header->SetForegroundColour(wxColour(220, 220, 220));
    wxFont headerFont = header->GetFont();
    headerFont.SetWeight(wxFONTWEIGHT_BOLD);
    header->SetFont(headerFont);
    root->Add(header, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

    auto* hint = new wxStaticText(this, wxID_ANY,
        "Select a revision to preview its patch and complete snapshot. Restore loads the revision into the editor as an unsaved change.");
    hint->SetForegroundColour(wxColour(155, 155, 155));
    hint->Wrap(900);
    root->Add(hint, 0, wxEXPAND | wxALL, 10);

    auto* splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                          wxSP_LIVE_UPDATE | wxSP_3DSASH);
    splitter->SetMinimumPaneSize(120);
    splitter->SetSashGravity(0.42);

    auto* listPanel = new wxPanel(splitter);
    listPanel->SetBackgroundColour(wxColour(37, 37, 38));
    auto* listSizer = new wxBoxSizer(wxVERTICAL);
    m_list = new wxListCtrl(listPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE);
    m_list->InsertColumn(0, "Revision", wxLIST_FORMAT_LEFT, 78);
    m_list->InsertColumn(1, "Saved", wxLIST_FORMAT_LEFT, 145);
    m_list->InsertColumn(2, "Type", wxLIST_FORMAT_LEFT, 72);
    m_list->InsertColumn(3, "Changes", wxLIST_FORMAT_LEFT, 95);
    m_list->InsertColumn(4, "Size", wxLIST_FORMAT_RIGHT, 78);
    m_list->InsertColumn(5, "Lines", wxLIST_FORMAT_RIGHT, 60);
    m_list->InsertColumn(6, "Note", wxLIST_FORMAT_LEFT, 160);
    listSizer->Add(m_list, 1, wxEXPAND);
    listPanel->SetSizer(listSizer);

    auto* previewPanel = new wxPanel(splitter);
    previewPanel->SetBackgroundColour(wxColour(30, 30, 30));
    auto* previewSizer = new wxBoxSizer(wxVERTICAL);
    m_details = new wxStaticText(previewPanel, wxID_ANY, wxEmptyString);
    m_details->SetForegroundColour(wxColour(180, 180, 180));
    previewSizer->Add(m_details, 0, wxEXPAND | wxALL, 8);

    auto* previewNotebook = new wxNotebook(previewPanel, wxID_ANY);
    m_patchPreview = new wxStyledTextCtrl(previewNotebook);
    m_snapshotPreview = new wxStyledTextCtrl(previewNotebook);
    ConfigurePreview(m_patchPreview);
    ConfigurePreview(m_snapshotPreview);
    previewNotebook->AddPage(m_patchPreview, "Patch", true);
    previewNotebook->AddPage(m_snapshotPreview, "Snapshot", false);
    previewSizer->Add(previewNotebook, 1, wxEXPAND);
    previewPanel->SetSizer(previewSizer);

    splitter->SplitVertically(listPanel, previewPanel, 420);
    root->Add(splitter, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    auto* closeButton = new wxButton(this, wxID_CANCEL, "Close");
    m_restoreButton = new wxButton(this, wxID_OK, "Restore Selected");
    m_restoreButton->Enable(false);
    buttons->AddStretchSpacer(1);
    buttons->Add(closeButton, 0, wxRIGHT, 8);
    buttons->Add(m_restoreButton, 0);
    root->Add(buttons, 0, wxEXPAND | wxALL, 10);
    SetSizer(root);

    Populate();

    if (!loadError.IsEmpty())
        m_details->SetLabel(loadError);
    else if (m_entries.empty())
        m_details->SetLabel("No local history exists for this file yet. Save the file to create the first revision.");

    m_list->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& event)
    {
        SelectEntry(event.GetIndex());
    });
    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& event)
    {
        SelectEntry(event.GetIndex());
        if (HasSelection())
            EndModal(wxID_OK);
    });
}

void FileHistoryDialog::ConfigurePreview(wxStyledTextCtrl* editor)
{
    editor->StyleSetBackground(wxSTC_STYLE_DEFAULT, wxColour(30, 30, 30));
    editor->StyleSetForeground(wxSTC_STYLE_DEFAULT, wxColour(220, 220, 220));
    editor->StyleSetFaceName(wxSTC_STYLE_DEFAULT, "Monospace");
    editor->StyleSetSize(wxSTC_STYLE_DEFAULT, 10);
    editor->StyleClearAll();
    editor->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    editor->SetMarginWidth(0, 48);
    editor->SetMarginBackground(0, wxColour(37, 37, 38));
    editor->StyleSetBackground(wxSTC_STYLE_LINENUMBER, wxColour(37, 37, 38));
    editor->StyleSetForeground(wxSTC_STYLE_LINENUMBER, wxColour(130, 130, 130));
    editor->SetWrapMode(wxSTC_WRAP_NONE);
    editor->SetReadOnly(true);
}

void FileHistoryDialog::Populate()
{
    m_list->DeleteAllItems();
    const int total = static_cast<int>(m_entries.size());
    for (int i = 0; i < total; ++i)
    {
        const LocalHistoryEntry& entry = m_entries[static_cast<size_t>(i)];
        const long row = m_list->InsertItem(i, wxString::Format("#%d", total - i));
        m_list->SetItem(row, 1, entry.timestamp);
        m_list->SetItem(row, 2, entry.kind == "baseline" ? wxString("Baseline") : wxString("Save"));
        m_list->SetItem(row, 3,
                        wxString::Format("+%d  -%d", entry.addedLines, entry.removedLines));
        m_list->SetItem(row, 4, HumanSize(entry.sizeBytes));
        m_list->SetItem(row, 5, wxString::Format("%d", entry.lineCount));
        m_list->SetItem(row, 6, entry.note);
    }

    if (!m_entries.empty())
    {
        m_list->SetItemState(0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                            wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
        SelectEntry(0);
    }
}

void FileHistoryDialog::SelectEntry(long row)
{
    if (row < 0 || static_cast<size_t>(row) >= m_entries.size())
        return;
    m_selectedIndex = row;
    const LocalHistoryEntry& entry = m_entries[static_cast<size_t>(row)];

    wxString patch;
    wxString snapshot;
    wxString error;
    LocalHistoryManager::LoadPatch(m_filePath, m_workspaceRoot, entry, patch, &error);
    LocalHistoryManager::LoadSnapshot(m_filePath, m_workspaceRoot, entry, snapshot, &error);

    m_patchPreview->SetReadOnly(false);
    m_patchPreview->SetText(patch);
    m_patchPreview->SetReadOnly(true);
    m_snapshotPreview->SetReadOnly(false);
    m_snapshotPreview->SetText(snapshot);
    m_snapshotPreview->SetReadOnly(true);

    m_details->SetLabel(wxString::Format("%s   %s   +%d / -%d   %s",
                                         entry.timestamp,
                                         entry.kind == "baseline" ? wxString("Baseline") : wxString("Saved revision"),
                                         entry.addedLines,
                                         entry.removedLines,
                                         HumanSize(entry.sizeBytes)));
    m_restoreButton->Enable(true);
}

bool FileHistoryDialog::HasSelection() const
{
    return m_selectedIndex >= 0 && static_cast<size_t>(m_selectedIndex) < m_entries.size();
}

const LocalHistoryEntry* FileHistoryDialog::GetSelectedEntry() const
{
    if (!HasSelection())
        return nullptr;
    return &m_entries[static_cast<size_t>(m_selectedIndex)];
}

bool FileHistoryDialog::GetSelectedSnapshot(wxString& content, wxString* error) const
{
    const LocalHistoryEntry* entry = GetSelectedEntry();
    if (!entry)
    {
        if (error)
            *error = "No local-history revision is selected.";
        return false;
    }
    return LocalHistoryManager::LoadSnapshot(m_filePath, m_workspaceRoot, *entry, content, error);
}
