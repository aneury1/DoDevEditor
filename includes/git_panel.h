#ifndef __GIT_PANEL_H_DEFINED
#define __GIT_PANEL_H_DEFINED
#include <iostream>
#include <wx/wx.h>
#include <vector>
#include <git2.h>
#include <string>

struct git_panel : public wxPanel
{

   git_panel(wxWindow *window);

   void load_commits_information_in_folder(const std::string &path);

private:
   struct git_commit_information_base
   {
      wxString author;
      wxString message;
      wxString commit_id;
      wxString author_date;
      wxString commiter_info;
      wxString commit_date;
      std::vector<std::string>  files_changed;
   };
   std::vector<git_commit_information_base> commits_list;
   void on_list_boxselect(wxCommandEvent &event);
   wxListBox *commits;
   wxListBox *commit_information;
   wxListBox *file_changed_in_this_commit;

   void print_commit_info(git_commit *commit);
};

#endif /// __SYMBOL_PANEL_H_DEFINED