#include "FolderListView.h"

#include "constant.h"

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/artprov.h>

#include <algorithm>
#include <utility>

namespace
{
wxString ExtensionLabel(const wxString& name)
{
    wxFileName file(name);
    wxString ext = file.GetExt();
    if (ext.IsEmpty())
        return "File";
    ext.MakeUpper();
    return ext + " file";
}
}

FolderListView::FolderListView(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    SetBackgroundColour(Colors::BG_PANEL);

    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* header = new wxPanel(this, wxID_ANY);
    header->SetBackgroundColour(wxColour(37, 37, 38));
    auto* headerSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* title = new wxStaticText(header, wxID_ANY, "FILES IN FOLDER");
    title->SetForegroundColour(wxColour(187, 187, 187));
    wxFont titleFont = title->GetFont();
    titleFont.SetPointSize(8);
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    title->SetFont(titleFont);

    m_upButton = new wxButton(header, wxID_ANY, "↑", wxDefaultPosition, wxSize(30, 24));
    m_upButton->SetToolTip("Parent folder");
    m_refreshButton = new wxButton(header, wxID_ANY, "↻", wxDefaultPosition, wxSize(30, 24));
    m_refreshButton->SetToolTip("Refresh folder list");

    headerSizer->Add(title, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
    headerSizer->Add(m_upButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 3);
    headerSizer->Add(m_refreshButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    header->SetSizer(headerSizer);
    root->Add(header, 0, wxEXPAND);

    m_pathLabel = new wxStaticText(this, wxID_ANY, "No folder selected");
    m_pathLabel->SetForegroundColour(wxColour(145, 145, 145));
    root->Add(m_pathLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 6);

    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxBORDER_NONE);
    m_list->SetBackgroundColour(Colors::BG_PANEL);
    m_list->SetForegroundColour(Colors::SIDEBAR_TEXT);
    m_list->InsertColumn(0, "Name", wxLIST_FORMAT_LEFT, 180);
    m_list->InsertColumn(1, "Type", wxLIST_FORMAT_LEFT, 80);
    m_list->InsertColumn(2, "Size", wxLIST_FORMAT_RIGHT, 72);
    m_list->InsertColumn(3, "Modified", wxLIST_FORMAT_LEFT, 130);
    root->Add(m_list, 1, wxEXPAND);

    SetSizer(root);

    m_refreshButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RefreshFolder(); });
    m_upButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        if (m_folder.IsEmpty())
            return;
        wxFileName current(m_folder, wxEmptyString);
        if (current.GetDirCount() == 0)
            return;
        current.RemoveLastDir();
        const wxString parentFolder = current.GetFullPath();
        if (parentFolder.IsEmpty() || parentFolder == m_folder)
            return;
        SetFolder(parentFolder);
        if (m_navigateFolder)
            m_navigateFolder(parentFolder);
    });
    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& event)
    {
        ActivateRow(event.GetIndex());
    });
}

void FolderListView::SetOpenFileCallback(std::function<void(const wxString&)> callback)
{
    m_openFile = std::move(callback);
}

void FolderListView::SetNavigateFolderCallback(std::function<void(const wxString&)> callback)
{
    m_navigateFolder = std::move(callback);
}

void FolderListView::SetFolder(const wxString& folder)
{
    wxString normalized = folder;
    if (!normalized.IsEmpty())
    {
        wxFileName fn(normalized, wxEmptyString);
        fn.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
        normalized = fn.GetFullPath();
    }

    if (!normalized.IsEmpty() && !wxDirExists(normalized))
        return;

    m_folder = normalized;
    Populate();
}

void FolderListView::RefreshFolder()
{
    Populate();
}

wxString FolderListView::FormatSize(unsigned long long value)
{
    if (value < 1024ULL)
        return wxString::Format("%llu B", value);
    if (value < 1024ULL * 1024ULL)
        return wxString::Format("%.1f KB", static_cast<double>(value) / 1024.0);
    if (value < 1024ULL * 1024ULL * 1024ULL)
        return wxString::Format("%.1f MB", static_cast<double>(value) / (1024.0 * 1024.0));
    return wxString::Format("%.1f GB", static_cast<double>(value) / (1024.0 * 1024.0 * 1024.0));
}

void FolderListView::Populate()
{
    m_list->DeleteAllItems();
    m_entries.clear();

    if (m_folder.IsEmpty() || !wxDirExists(m_folder))
    {
        m_pathLabel->SetLabel("No folder selected");
        m_upButton->Enable(false);
        return;
    }

    m_pathLabel->SetLabel(m_folder);
    m_pathLabel->SetToolTip(m_folder);
    m_upButton->Enable(true);

    wxDir dir(m_folder);
    if (!dir.IsOpened())
        return;

    wxString name;
    if (dir.GetFirst(&name, wxEmptyString, wxDIR_DIRS | wxDIR_HIDDEN))
    {
        do
        {
            if (name == "." || name == "..")
                continue;
            Entry entry;
            entry.name = name;
            entry.directory = true;
            entry.path = m_folder;
            entry.path += wxFileName::GetPathSeparator();
            entry.path += name;
            wxFileName info(entry.path, wxEmptyString);
            entry.modified = info.GetModificationTime();
            m_entries.push_back(entry);
        } while (dir.GetNext(&name));
    }

    if (dir.GetFirst(&name, wxEmptyString, wxDIR_FILES | wxDIR_HIDDEN))
    {
        do
        {
            Entry entry;
            entry.name = name;
            entry.path = m_folder;
            entry.path += wxFileName::GetPathSeparator();
            entry.path += name;
            wxFileName info(entry.path);
            entry.size = info.GetSize().GetValue();
            entry.modified = info.GetModificationTime();
            m_entries.push_back(entry);
        } while (dir.GetNext(&name));
    }

    std::sort(m_entries.begin(), m_entries.end(), [](const Entry& a, const Entry& b)
    {
        if (a.directory != b.directory)
            return a.directory && !b.directory;
        return a.name.CmpNoCase(b.name) < 0;
    });

    long row = 0;
    for (const Entry& entry : m_entries)
    {
        const long item = m_list->InsertItem(row, entry.name);
        m_list->SetItem(item, 1, entry.directory ? wxString("Folder") : ExtensionLabel(entry.name));
        m_list->SetItem(item, 2, entry.directory ? wxString() : FormatSize(entry.size));
        if (entry.modified.IsValid())
            m_list->SetItem(item, 3, entry.modified.FormatISOCombined(' '));
        if (entry.directory)
            m_list->SetItemTextColour(item, wxColour(210, 190, 120));
        ++row;
    }
}

void FolderListView::ActivateRow(long row)
{
    if (row < 0 || static_cast<size_t>(row) >= m_entries.size())
        return;

    const Entry& entry = m_entries[static_cast<size_t>(row)];
    if (entry.directory)
    {
        SetFolder(entry.path);
        if (m_navigateFolder)
            m_navigateFolder(entry.path);
    }
    else if (m_openFile)
    {
        m_openFile(entry.path);
    }
}
