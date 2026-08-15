#ifndef FINDBAR_H
#define FINDBAR_H

#include <wx/wx.h>
#include <wx/listctrl.h>

// ─────────────────────────────────────────────────────────────────────────────
// FindBar – inline current-document/project search toolbar
// ─────────────────────────────────────────────────────────────────────────────

class FindBar : public wxPanel
{
public:
    enum class Scope
    {
        CurrentDocument = 0,
        AllDocuments = 1
    };

    wxTextCtrl* textCtrl = nullptr;
    wxCheckBox* caseChk = nullptr;
    wxCheckBox* wholeChk = nullptr;
    wxChoice* scopeChoice = nullptr;
    wxStaticText* resultLabel = nullptr;
    wxListCtrl* resultsList = nullptr;

    explicit FindBar(wxWindow* parent);

    Scope GetScope() const;
    void SetScope(Scope scope);
    void SetResultsVisible(bool visible);
    void ClearResults();
};

#endif
