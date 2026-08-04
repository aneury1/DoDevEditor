 

#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/imaglist.h>
#include "SymbolTablePanel.h"
#include <wx/artprov.h>
 
    SymbolTablePanel::SymbolTablePanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        SetBackgroundColour(wxColour(30, 30, 30));

        auto* rootSizer = new wxBoxSizer(wxVERTICAL);

        // ─────────────────────────
        // HEADER
        // ─────────────────────────
        auto* header = new wxStaticText(this, wxID_ANY, "SYMBOLS");
        header->SetForegroundColour(wxColour(200, 200, 200));

        wxFont f = header->GetFont();
        f.SetWeight(wxFONTWEIGHT_BOLD);
        f.SetPointSize(9);
        header->SetFont(f);

        rootSizer->Add(header, 0, wxEXPAND | wxALL, 8);

        // ─────────────────────────
        // ICON LIST
        // ─────────────────────────
        m_icons = new wxImageList(16, 16, true);

        m_icons->Add(wxArtProvider::GetBitmap(wxART_EXECUTABLE_FILE, wxART_MENU, wxSize(16,16))); // function
        m_icons->Add(wxArtProvider::GetBitmap(wxART_NORMAL_FILE,     wxART_MENU, wxSize(16,16))); // variable
        m_icons->Add(wxArtProvider::GetBitmap(wxART_TIP,             wxART_MENU, wxSize(16,16))); // macro

        // ─────────────────────────
        // TREE VIEW
        // ─────────────────────────
        m_tree = new wxDataViewTreeCtrl(
            this,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxDV_ROW_LINES | wxDV_SINGLE | wxBORDER_NONE);

        m_tree->SetBackgroundColour(wxColour(30, 30, 30));
        m_tree->SetForegroundColour(wxColour(220, 220, 220));

        m_tree->AssignImageList(m_icons);

        // Root categories
        m_rootFunctions = m_tree->AppendContainer(wxDataViewItem(), "Functions", 0);
        m_rootVariables = m_tree->AppendContainer(wxDataViewItem(), "Variables", 1);
        m_rootMacros    = m_tree->AppendContainer(wxDataViewItem(), "Macros",    2);

        m_tree->Expand(m_rootFunctions);
        m_tree->Expand(m_rootVariables);
        m_tree->Expand(m_rootMacros);

        rootSizer->Add(m_tree, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

        SetSizer(rootSizer);
        Layout();
    }

 
    void SymbolTablePanel::AddFunction(const wxString& name)
    {
        m_tree->AppendItem(m_rootFunctions, name, 0);
    }

    void SymbolTablePanel::AddVariable(const wxString& name)
    {
        m_tree->AppendItem(m_rootVariables, name, 1);
    }

    void SymbolTablePanel::AddMacro(const wxString& name)
    {
        m_tree->AppendItem(m_rootMacros, name, 2);
    }

    void SymbolTablePanel::Clear()
    {
        m_tree->DeleteAllItems();
    }

 