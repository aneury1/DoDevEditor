#pragma once
#include <wx/listbox.h>
#include <wx/wx.h>

#include "CommandSystem.h"
#include "FuzzyMatcher.h"

class CommandPalette : public wxDialog {
 public:
  CommandPalette(wxWindow* parent,
                 const CommandRegistry& registry = CommandRegistry());

 protected:
  void OnSearch(wxCommandEvent&);

  void RefreshList(const std::string& query);

  void OnExecute(wxCommandEvent&);

 private:
  wxTextCtrl* input;
  wxListBox* list;

  CommandRegistry commands;
  std::vector<std::string> results;
};