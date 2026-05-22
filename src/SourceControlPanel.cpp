 
#include <wx/wx.h>
#include <wx/dataview.h>
#include <vector>
#include <wx/artprov.h>
 #include "SourceControlPanel.h"
   SourceControlPanel:: SourceControlPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        SetBackgroundColour(wxColour(30, 30, 30));

        auto* rootSizer = new wxBoxSizer(wxVERTICAL);

        // ─────────────────────────
        // HEADER
        // ─────────────────────────
        auto* header = new wxStaticText(this, wxID_ANY, "SOURCE CONTROL");
        header->SetForegroundColour(wxColour(200, 200, 200));

        wxFont f = header->GetFont();
        f.SetWeight(wxFONTWEIGHT_BOLD);
        f.SetPointSize(9);
        header->SetFont(f);

        rootSizer->Add(header, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

        // ─────────────────────────
        // TOOLBAR
        // ─────────────────────────
        auto* toolbar = new wxPanel(this);
        toolbar->SetBackgroundColour(wxColour(37, 37, 38));

        auto* tbSizer = new wxBoxSizer(wxHORIZONTAL);

        auto* btnCommit = new wxBitmapButton(
            toolbar,
            wxID_ANY,
            wxArtProvider::GetBitmap(wxART_TICK_MARK, wxART_MENU, wxSize(16,16)));

        auto* btnRefresh = new wxBitmapButton(
            toolbar,
            wxID_ANY,
            wxArtProvider::GetBitmap(wxART_REDO, wxART_MENU, wxSize(16,16)));

        auto* btnDiscard = new wxBitmapButton(
            toolbar,
            wxID_ANY,
            wxArtProvider::GetBitmap(wxART_DELETE, wxART_MENU, wxSize(16,16)));

        tbSizer->Add(btnCommit, 0, wxALL, 4);
        tbSizer->Add(btnRefresh, 0, wxALL, 4);
        tbSizer->Add(btnDiscard, 0, wxALL, 4);

        toolbar->SetSizer(tbSizer);
        rootSizer->Add(toolbar, 0, wxEXPAND);

        // ─────────────────────────
        // FILES VIEW
        // ─────────────────────────
        m_filesView = new wxDataViewCtrl(
            this,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxDV_ROW_LINES | wxDV_SINGLE | wxBORDER_NONE);

        m_filesView->SetBackgroundColour(wxColour(30, 30, 30));
        m_filesView->SetForegroundColour(wxColour(220, 220, 220));
        m_filesView->SetRowHeight(22);

        m_filesView->AppendTextColumn("Status", 0, wxDATAVIEW_CELL_INERT, 60);
        m_filesView->AppendTextColumn("File",   1, wxDATAVIEW_CELL_INERT, 200);
        m_filesView->AppendTextColumn("Action", 2, wxDATAVIEW_CELL_INERT, 80);

        m_filesModel = new wxDataViewListStore();
        m_filesView->AssociateModel(m_filesModel);
        m_filesModel->DecRef();

        rootSizer->Add(m_filesView, 1, wxEXPAND | wxALL, 6);

        // ─────────────────────────
        // COMMITS VIEW (history)
        // ─────────────────────────
        auto* commitLabel = new wxStaticText(this, wxID_ANY, "COMMITS");
        commitLabel->SetForegroundColour(wxColour(160, 160, 160));

        rootSizer->Add(commitLabel, 0, wxLEFT | wxRIGHT | wxTOP, 8);

        m_commitView = new wxDataViewCtrl(
            this,
            wxID_ANY,
            wxDefaultPosition,
            wxSize(-1, 120),
            wxDV_ROW_LINES | wxDV_SINGLE | wxBORDER_NONE);

        m_commitView->SetBackgroundColour(wxColour(30, 30, 30));
        m_commitView->SetForegroundColour(wxColour(200, 200, 200));
        m_commitView->SetRowHeight(20);

        m_commitView->AppendTextColumn("Message", 0, wxDATAVIEW_CELL_INERT, 180);
        m_commitView->AppendTextColumn("Author",  1, wxDATAVIEW_CELL_INERT, 100);
        m_commitView->AppendTextColumn("Date",    2, wxDATAVIEW_CELL_INERT, 120);

        m_commitModel = new wxDataViewListStore();
        m_commitView->AssociateModel(m_commitModel);
        m_commitModel->DecRef();

        rootSizer->Add(m_commitView, 0, wxEXPAND | wxALL, 6);

        SetSizer(rootSizer);
        Layout();

        // Sample commits
        AddCommit("Initial commit", "dev", "2026-05-15");
        AddCommit("Refactor UI", "dev", "2026-05-16");
    }
 
 
    void SourceControlPanel::AddFile(const wxString& status,
                 const wxString& file,
                 const wxString& action)
    {
        wxVector<wxVariant> row;
        row.push_back(status);
        row.push_back(file);
        row.push_back(action);
        m_filesModel->AppendItem(row);
    }

    void SourceControlPanel::AddCommit(const wxString& message,
                   const wxString& author,
                   const wxString& date)
    {
        CommitInfo commit{message, author, date};
        m_commits.push_back(commit);

        wxVector<wxVariant> row;
        row.push_back(message);
        row.push_back(author);
        row.push_back(date);

        m_commitModel->AppendItem(row);
    }
 
  