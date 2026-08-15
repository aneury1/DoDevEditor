#include "Findbar.h"
#include "constant.h"

FindBar::FindBar(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    SetBackgroundColour(Colors::BG_PANEL);

    auto* rootSizer = new wxBoxSizer(wxVERTICAL);
    auto* toolbarSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* label = new wxStaticText(this, wxID_ANY, "Find:");
    label->SetForegroundColour(Colors::FG);
    toolbarSizer->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 6);

    textCtrl = new wxTextCtrl(this, ID_FIND_TEXT, wxEmptyString,
                              wxDefaultPosition, wxSize(220, -1),
                              wxTE_PROCESS_ENTER);
    textCtrl->SetBackgroundColour(wxColour(60, 60, 60));
    textCtrl->SetForegroundColour(Colors::FG);
    toolbarSizer->Add(textCtrl, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);

    scopeChoice = new wxChoice(this, wxID_ANY);
    scopeChoice->Append("Current Document");
    scopeChoice->Append("All Documents");
    scopeChoice->SetSelection(0);
    scopeChoice->SetMinSize(wxSize(150, -1));
    toolbarSizer->Add(scopeChoice, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);

    auto* btnPrev = new wxButton(this, ID_FIND_PREV, L"\u25b2",
                                 wxDefaultPosition, wxSize(28, 24));
    auto* btnNext = new wxButton(this, ID_FIND_BTN, L"\u25bc",
                                 wxDefaultPosition, wxSize(28, 24));
    auto* btnClose = new wxButton(this, ID_FIND_CLOSE, L"\u2715",
                                  wxDefaultPosition, wxSize(24, 24));

    for (auto* button : {btnPrev, btnNext, btnClose})
    {
        button->SetBackgroundColour(Colors::BG_PANEL);
        button->SetForegroundColour(Colors::FG);
    }

    toolbarSizer->Add(btnPrev, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
    toolbarSizer->Add(btnNext, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    caseChk = new wxCheckBox(this, wxID_ANY, "Match case");
    wholeChk = new wxCheckBox(this, wxID_ANY, "Whole word");
    caseChk->SetForegroundColour(Colors::FG);
    wholeChk->SetForegroundColour(Colors::FG);
    caseChk->SetBackgroundColour(Colors::BG_PANEL);
    wholeChk->SetBackgroundColour(Colors::BG_PANEL);
    toolbarSizer->Add(caseChk, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    toolbarSizer->Add(wholeChk, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    resultLabel = new wxStaticText(this, wxID_ANY, "");
    resultLabel->SetForegroundColour(wxColour(170, 170, 170));
    toolbarSizer->Add(resultLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    toolbarSizer->AddStretchSpacer();
    toolbarSizer->Add(btnClose, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    rootSizer->Add(toolbarSizer, 0, wxEXPAND | wxTOP | wxBOTTOM, 4);

    resultsList = new wxListCtrl(this, wxID_ANY,
                                 wxDefaultPosition, wxSize(-1, 165),
                                 wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE);
    resultsList->SetBackgroundColour(Colors::BG);
    resultsList->SetForegroundColour(Colors::FG);
    resultsList->InsertColumn(0, "File", wxLIST_FORMAT_LEFT, 260);
    resultsList->InsertColumn(1, "Line", wxLIST_FORMAT_LEFT, 70);
    resultsList->InsertColumn(2, "Match", wxLIST_FORMAT_LEFT, 700);
    rootSizer->Add(resultsList, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

    SetSizer(rootSizer);
    SetResultsVisible(false);
}

FindBar::Scope FindBar::GetScope() const
{
    return scopeChoice && scopeChoice->GetSelection() == 1
               ? Scope::AllDocuments
               : Scope::CurrentDocument;
}

void FindBar::SetScope(Scope scope)
{
    if (scopeChoice)
        scopeChoice->SetSelection(scope == Scope::AllDocuments ? 1 : 0);
    SetResultsVisible(scope == Scope::AllDocuments);
}

void FindBar::SetResultsVisible(bool visible)
{
    if (resultsList)
        resultsList->Show(visible);
    Layout();
}

void FindBar::ClearResults()
{
    if (resultsList)
        resultsList->DeleteAllItems();
    if (resultLabel)
        resultLabel->SetLabel("");
}
