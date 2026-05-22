#ifndef FINDBAR_H
#define FINDBAR_H
#include <wx/wx.h>
// ─────────────────────────────────────────────────────────────────────────────
// FindBar – inline find toolbar at the bottom
// ─────────────────────────────────────────────────────────────────────────────

class FindBar : public wxPanel {
public:
    wxTextCtrl*  textCtrl;
    wxCheckBox*  caseChk;
    wxCheckBox*  wholeChk;

    FindBar(wxWindow* parent);
};

#endif