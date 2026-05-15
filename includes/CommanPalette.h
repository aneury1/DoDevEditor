#pragma once
#include <wx/wx.h>
#include <wx/listbox.h>
#include "FuzzyMatcher.h"
#include "CommandSystem.h"

class CommandPalette : public wxDialog
{
public:
    CommandPalette(wxWindow* parent,
                   const CommandRegistry& registry=CommandRegistry())
        : wxDialog(parent, wxID_ANY, "Command Palette",
                   wxDefaultPosition, wxSize(500, 400)),
          commands(registry)
    {
        SetBackgroundColour(wxColour(30,30,30));

        auto* sizer = new wxBoxSizer(wxVERTICAL);

        input = new wxTextCtrl(this, wxID_ANY);
        list  = new wxListBox(this, wxID_ANY);

        sizer->Add(input, 0, wxEXPAND | wxALL, 5);
        sizer->Add(list, 1, wxEXPAND | wxALL, 5);

        SetSizer(sizer);

        input->Bind(wxEVT_TEXT, &CommandPalette::OnSearch, this);
        list->Bind(wxEVT_LISTBOX_DCLICK, &CommandPalette::OnExecute, this);

        RefreshList("");
    }

private:
    void OnSearch(wxCommandEvent&)
    {
        RefreshList(input->GetValue().ToStdString());
    }

    void RefreshList(const std::string& query)
    {
        list->Clear();
        results.clear();

       /* auto items = FuzzyMatcher::Search(query, commands.GetTitles());

        for (auto& r : items)
        {
            results.push_back(r.id);
            list->Append(r.title);
        }*/
    }

    void OnExecute(wxCommandEvent&)
    {
        int i = list->GetSelection();
        if (i >= 0 && i < (int)results.size())
        {
            commands.Execute(results[i]);
            Close();
        }
    }

private:
    wxTextCtrl* input;
    wxListBox* list;

    CommandRegistry commands;
    std::vector<std::string> results;
};