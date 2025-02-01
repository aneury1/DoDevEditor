#ifndef GIT_PANEL_ID_H_DEFINED
#define GIT_PANEL_ID_H_DEFINED

#include <wx/wx.h>
#include <wx/panel.h>
#include <libgit2.h>
#include <vector>
#include <string>
#include <iostream>

class git_diff_panel : public wxPanel {
public:
    git_diff_panel(wxWindow* parent, const std::string& repoPath, const std::string& commitHash)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(800, 600)) {
        
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        textCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
        sizer->Add(textCtrl, 1, wxEXPAND | wxALL, 5);
        SetSizer(sizer);


    }

     
    void load_text(const std::string& repoPath, const std::string& commitHash){
        // Load Git diff
        std::string diffText = getGitDiff(repoPath, commitHash);
        textCtrl->SetValue(diffText);
    }



private:
    wxTextCtrl* textCtrl;

    std::string getGitDiff(const std::string& repoPath, const std::string& commitHash) {
        std::string diffResult;

        git_libgit2_init();
        git_repository* repo = nullptr;
        git_object* commitObj = nullptr;
        git_commit* commit = nullptr;
        git_tree* tree = nullptr;
        git_diff* diff = nullptr;

        if (git_repository_open(&repo, repoPath.c_str()) != 0) {
            return "Error: Cannot open repository!";
        }

        if (git_revparse_single(&commitObj, repo, commitHash.c_str()) != 0) {
            git_repository_free(repo);
            return "Error: Invalid commit hash!";
        }

        commit = (git_commit*)commitObj;
        if (git_commit_tree(&tree, commit) != 0) {
            git_commit_free(commit);
            git_repository_free(repo);
            return "Error: Cannot get commit tree!";
        }

        git_diff_tree_to_workdir(&diff, repo, tree, nullptr);
        
        git_diff_print(diff, GIT_DIFF_FORMAT_PATCH, [](const git_diff_delta*, const git_diff_hunk*, const git_diff_line* line, void* payload) {
            std::string* result = static_cast<std::string*>(payload);
            result->append(line->content, line->content_len);
            return 0;
        }, &diffResult);

        // Cleanup
        git_diff_free(diff);
        git_tree_free(tree);
        git_commit_free(commit);
        git_repository_free(repo);
        git_libgit2_shutdown();

        return diffResult;
    }
};




#endif GIT_PANEL_ID_H_DEFINED