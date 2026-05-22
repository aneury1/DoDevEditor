 
#include <wx/wx.h>
#include <wx/listbox.h>
#include "FuzzyMatcher.h"
#include "CommandSystem.h"
#include "CommandPalette.h"

CommandPalette::CommandPalette(wxWindow *parent,
                               const CommandRegistry &registry )
    : wxDialog(parent, wxID_ANY, "Command Palette",
               wxDefaultPosition, wxSize(500, 400)),
      commands(registry)
{
    SetBackgroundColour(wxColour(30, 30, 30));

    auto *sizer = new wxBoxSizer(wxVERTICAL);

    input = new wxTextCtrl(this, wxID_ANY);
    list = new wxListBox(this, wxID_ANY);

    sizer->Add(input, 0, wxEXPAND | wxALL, 5);
    sizer->Add(list, 1, wxEXPAND | wxALL, 5);

    SetSizer(sizer);

    input->Bind(wxEVT_TEXT, &CommandPalette::OnSearch, this);
    list->Bind(wxEVT_LISTBOX_DCLICK, &CommandPalette::OnExecute, this);

    RefreshList("");
}

void CommandPalette::OnSearch(wxCommandEvent &)
{
    RefreshList(input->GetValue().ToStdString());
}

void CommandPalette::RefreshList(const std::string &query)
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

void CommandPalette::OnExecute(wxCommandEvent &)
{
    int i = list->GetSelection();
    if (i >= 0 && i < (int)results.size())
    {
        commands.Execute(results[i]);
        Close();
    }
}
