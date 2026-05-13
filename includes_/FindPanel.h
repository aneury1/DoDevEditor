#pragma once
#include <wx/wx.h>
#include <wx/stc/stc.h>
#include <json/json.h>


class FindPanel : public wxPanel
{
public:
    wxTextCtrl* input;
    wxCheckBox* matchCase;
    wxButton* next;
    wxButton* prev;

    FindPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        auto* s = new wxBoxSizer(wxHORIZONTAL);

        input = new wxTextCtrl(this, wxID_ANY);
        matchCase = new wxCheckBox(this, wxID_ANY, "Aa");
        next = new wxButton(this, wxID_ANY, "Next");
        prev = new wxButton(this, wxID_ANY, "Prev");

        s->Add(input, 1, wxEXPAND | wxALL, 5);
        s->Add(matchCase, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
        s->Add(prev, 0, wxALL, 5);
        s->Add(next, 0, wxALL, 5);

        SetSizer(s);
        Hide();
    }
};