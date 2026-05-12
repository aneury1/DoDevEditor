#pragma once

#pragma once

#include <wx/wx.h>
#include <functional>
#include <vector>

class FileTab : public wxPanel
{
public:

    using CloseCallback = std::function<void(FileTab*)>;
    using SelectCallback = std::function<void(FileTab*)>;

    FileTab(
        wxWindow* parent,
        const wxString& filename,
        SelectCallback onSelect,
        CloseCallback onClose)
        : wxPanel(parent),
          m_onSelect(onSelect),
          m_onClose(onClose)
    {
        SetBackgroundColour(wxColour(45, 45, 45));

        auto* sizer = new wxBoxSizer(wxHORIZONTAL);

        m_label = new wxStaticText(this, wxID_ANY, filename);
        m_label->SetForegroundColour(wxColour(220, 220, 220));

        m_close = new wxButton(
            this,
            wxID_ANY,
            "×",
            wxDefaultPosition,
            wxSize(24, 24),
            wxBORDER_NONE);

        m_close->SetBackgroundColour(wxColour(45, 45, 45));
        m_close->SetForegroundColour(wxColour(220, 220, 220));

        sizer->Add(m_label, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
        sizer->Add(m_close, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

        SetSizer(sizer);

        Bind(wxEVT_LEFT_DOWN, &FileTab::OnSelected, this);
        m_label->Bind(wxEVT_LEFT_DOWN, &FileTab::OnSelected, this);

        m_close->Bind(wxEVT_BUTTON,
            [=](wxCommandEvent&)
            {
                if (m_onClose)
                    m_onClose(this);
            });
    }

    wxString GetFileName() const
    {
        return m_label->GetLabel();
    }

    void SetActive(bool active)
    {
        if (active)
        {
            SetBackgroundColour(wxColour(60, 60, 60));
        }
        else
        {
            SetBackgroundColour(wxColour(45, 45, 45));
        }

        Refresh();
    }

private:

    wxStaticText* m_label;
    wxButton* m_close;

    SelectCallback m_onSelect;
    CloseCallback m_onClose;

    void OnSelected(wxMouseEvent&)
    {
        if (m_onSelect)
            m_onSelect(this);
    }
};
#include <vector>
class FileTabBar : public wxScrolledWindow
{
public:

    std::vector<wxString> openedTabs;
    using TabSelectCallback = std::function<void(int)>;
    using TabCloseCallback = std::function<void(int)>;

    FileTabBar(wxWindow* parent)
        : wxScrolledWindow(parent)
    {
        SetScrollRate(5, 0);

        SetBackgroundColour(wxColour(37, 37, 38));

        m_sizer = new wxBoxSizer(wxHORIZONTAL);

        SetSizer(m_sizer);
    }

    void SetOnTabSelected(TabSelectCallback cb)
    {
        m_onTabSelected = cb;
    }

    void SetOnTabClosed(TabCloseCallback cb)
    {
        m_onTabClosed = cb;
    }

    void AddTab(const wxString& filename)
    {
        openedTabs.emplace_back(filename);
        auto* tab = new FileTab(
            this,
            filename,

            [=](FileTab* t)
            {
                SelectTab(t);
            },

            [=](FileTab* t)
            {
                CloseTab(t);
            });

        tab->SetMinSize(wxSize(160, 32));

        m_tabs.push_back(tab);

        m_sizer->Add(tab, 0, wxEXPAND | wxRIGHT, 1);

        Layout();
        FitInside();

        SelectTab(tab);
    }

private:

    wxBoxSizer* m_sizer;

    std::vector<FileTab*> m_tabs;

    TabSelectCallback m_onTabSelected;
    TabCloseCallback m_onTabClosed;

    void SelectTab(FileTab* selected)
    {
        for (size_t i = 0; i < m_tabs.size(); ++i)
        {
            bool active = (m_tabs[i] == selected);

            m_tabs[i]->SetActive(active);

            if (active && m_onTabSelected)
            {
                m_onTabSelected(i);
            }
        }
    }

    void CloseTab(FileTab* tab)
    {
        for (size_t i = 0; i < m_tabs.size(); ++i)
        {
            if (m_tabs[i] == tab)
            {
                if (m_onTabClosed)
                    m_onTabClosed(i);

                m_sizer->Detach(tab);

                tab->Destroy();

                m_tabs.erase(m_tabs.begin() + i);

                Layout();
                FitInside();

                break;
            }
        }

        if (!m_tabs.empty())
        {
            SelectTab(m_tabs.front());
        }
    }
};