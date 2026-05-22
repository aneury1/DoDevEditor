#include "Findbar.h"
#include "constant.h"

FindBar::FindBar(wxWindow *parent)
    : wxPanel(parent, wxID_ANY)
{
    SetBackgroundColour(Colors::BG_PANEL);
    auto *sizer = new wxBoxSizer(wxHORIZONTAL);

    auto label = new wxStaticText(this, wxID_ANY, "Find:");
    label->SetForegroundColour(Colors::FG);
    sizer->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 6);

    textCtrl = new wxTextCtrl(this, ID_FIND_TEXT, wxEmptyString,
                              wxDefaultPosition, wxSize(220, -1),
                              wxTE_PROCESS_ENTER);
    textCtrl->SetBackgroundColour(wxColour(60, 60, 60));
    textCtrl->SetForegroundColour(Colors::FG);
    sizer->Add(textCtrl, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

    auto btnPrev = new wxButton(this, ID_FIND_PREV, L"\u25b2", wxDefaultPosition, wxSize(28, 24));
    auto btnNext = new wxButton(this, ID_FIND_BTN, L"\u25bc", wxDefaultPosition, wxSize(28, 24));
    auto btnClose = new wxButton(this, ID_FIND_CLOSE, L"\u2715", wxDefaultPosition, wxSize(24, 24));
    for (auto *b : {btnPrev, btnNext, btnClose})
    {
        b->SetBackgroundColour(Colors::BG_PANEL);
        b->SetForegroundColour(Colors::FG);
    }
    sizer->Add(btnPrev, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
    sizer->Add(btnNext, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    caseChk = new wxCheckBox(this, wxID_ANY, "Match case");
    wholeChk = new wxCheckBox(this, wxID_ANY, "Whole word");
    caseChk->SetForegroundColour(Colors::FG);
    wholeChk->SetForegroundColour(Colors::FG);
    caseChk->SetBackgroundColour(Colors::BG_PANEL);
    wholeChk->SetBackgroundColour(Colors::BG_PANEL);
    sizer->Add(caseChk, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    sizer->Add(wholeChk, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    sizer->AddStretchSpacer();
    sizer->Add(btnClose, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

    SetSizer(sizer);
}
