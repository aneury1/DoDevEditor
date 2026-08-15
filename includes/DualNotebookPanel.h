#pragma once

#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/aui/aui.h>
#include <unordered_map>
#include <optional>

#include "ShellTerminalPanel.h"
#include "TermVTerm.h"

class DualNotebookPanel : public wxPanel
{
public:
    DualNotebookPanel(wxWindow *parent, wxWindowID id = wxID_ANY);
    wxAuiNotebook *GetTopNotebook();
    wxAuiNotebook *GetBottomNotebook();
    std::optional<wxPanel *>getPanel(const std::string& id);
private:
    wxSplitterWindow *m_splitter = nullptr;

    wxPanel *m_topPanel = nullptr;
    wxPanel *m_bottomPanel = nullptr;

    wxAuiNotebook *m_topNotebook = nullptr;
    wxAuiNotebook *m_bottomNotebook = nullptr;

    std::unordered_map<std::string, wxPanel*> m_bottom_panels;

private:
    void BuildUI();
    void CreateBottomTabs();
};