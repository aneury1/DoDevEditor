#ifndef SYMBOL_TABLE_PANEL_H
#define SYMBOL_TABLE_PANEL_H

#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/imaglist.h>

class SymbolTablePanel : public wxPanel
{
public:
    enum SymbolType
    {
        FUNCTION,
        VARIABLE,
        MACRO
    };

public:
    SymbolTablePanel(wxWindow* parent);
    // ─────────────────────────
    // PUBLIC API
    // ─────────────────────────

public:
    void AddFunction(const wxString& name);

    void AddVariable(const wxString& name);
    void AddMacro(const wxString& name);
    void Clear();
private:
    wxDataViewTreeCtrl* m_tree = nullptr;
    wxImageList* m_icons = nullptr;

    wxDataViewItem m_rootFunctions;
    wxDataViewItem m_rootVariables;
    wxDataViewItem m_rootMacros;
};

#endif