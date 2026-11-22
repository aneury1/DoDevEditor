#ifndef GITPANEL_H_DEFINED
#define GITPANEL_H_DEFINED
/// __lib_git_2_defined 1
#include <wx/wx.h>
#include <wx/panel.h>
#ifdef __lib_git_2_defined
#include <git2.h>
#endif
#include <vector>
#include <string>
#include <iostream>


#ifndef __lib_git_2_defined
struct git_commit{
};
#endif

class GitPanel : public wxPanel {

public:
   GitPanel(wxWindow *window);

   void LoadCommitsInformationInFolder(const std::string &path);

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
   void OnListBoxSelect(wxCommandEvent &event);
   wxListBox *commits;
   wxListBox *commit_information;
   wxListBox *file_changed_in_this_commit;

   void PrintCommitInfo(git_commit *commit);
};





#endif