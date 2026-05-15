#pragma once

#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/aui/aui.h>
#include <unordered_map>
#include <optional>

class DualNotebookPanel : public wxPanel
{
public:
    DualNotebookPanel(wxWindow *parent, wxWindowID id = wxID_ANY)
        : wxPanel(parent, id)
    {
        BuildUI();
    }

    wxAuiNotebook *GetTopNotebook() { return m_topNotebook; }
    wxAuiNotebook *GetBottomNotebook() { return m_bottomNotebook; }
    std::optional<wxPanel *>getPanel(const std::string& id){
        auto f = m_bottom_panels.find(id);
        if(f != m_bottom_panels.end())return m_bottom_panels[id];
        return std::nullopt;
    }
private:
    wxSplitterWindow *m_splitter = nullptr;

    wxPanel *m_topPanel = nullptr;
    wxPanel *m_bottomPanel = nullptr;

    wxAuiNotebook *m_topNotebook = nullptr;
    wxAuiNotebook *m_bottomNotebook = nullptr;

    std::unordered_map<std::string, wxPanel*> m_bottom_panels;

private:
    void BuildUI()
    {
        SetBackgroundColour(wxColour(30, 30, 30));

        // Splitter
        m_splitter = new wxSplitterWindow(this, wxID_ANY,
                                          wxDefaultPosition,
                                          wxDefaultSize,
                                          wxSP_LIVE_UPDATE);

        m_splitter->SetMinimumPaneSize(100);

        // Panels
        m_topPanel = new wxPanel(m_splitter);
        m_bottomPanel = new wxPanel(m_splitter);

        m_topPanel->SetBackgroundColour(wxColour(45, 45, 48));
        m_bottomPanel->SetBackgroundColour(wxColour(37, 37, 38));

        // ===== TOP NOTEBOOK =====

        m_topNotebook = new wxAuiNotebook(
            m_topPanel,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_TAB_MOVE);

        // Layout top
        auto *topSizer = new wxBoxSizer(wxVERTICAL);
        topSizer->Add(m_topNotebook, 1, wxEXPAND | wxALL, 5);
        m_topPanel->SetSizer(topSizer);

        // ===== BOTTOM NOTEBOOK =====
        m_bottomNotebook = new wxAuiNotebook(
            m_bottomPanel,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxAUI_NB_TOP |
                wxAUI_NB_TAB_MOVE |
                wxAUI_NB_SCROLL_BUTTONS);

        // Create 5 tabs
        CreateBottomTabs();

        auto *bottomSizer = new wxBoxSizer(wxVERTICAL);
        bottomSizer->Add(m_bottomNotebook, 1, wxEXPAND | wxALL, 5);
        m_bottomPanel->SetSizer(bottomSizer);

        // Split vertically
        m_splitter->SplitHorizontally(m_topPanel, m_bottomPanel, 400);

        // Main layout
        auto *mainSizer = new wxBoxSizer(wxVERTICAL);
        mainSizer->Add(m_splitter, 1, wxEXPAND);
        SetSizer(mainSizer);
    }

    void CreateBottomTabs()
    {
        struct TabDef
        {
            const char *name;
        };

        TabDef tabs[5] = {
            {"Logs"},
            {"Console"},
            {"Errors"},
            {"Output"},
            {"Inspector"}};

        for (int i = 0; i < 5; i++)
        {
            wxPanel *page = new wxPanel(m_bottomNotebook);
            page->SetBackgroundColour(wxColour(50, 50, 50));

            auto *sizer = new wxBoxSizer(wxVERTICAL);
            sizer->Add(new wxStaticText(page, wxID_ANY, tabs[i].name),
                       0, wxALL, 10);

            page->SetSizer(sizer);

            m_bottomNotebook->AddPage(page, tabs[i].name, i == 0);
            m_bottom_panels[tabs[i].name] = page;
        }
    }
};