#include "GitPanel.h"
// #define __lib_git_2_defined 0
#ifdef __lib_git_2_defined
#include <git2.h>
#endif
#include <iostream>
#include <string>
#include <sstream>
#include <ctime>
#include <stdio.h>

std::vector<std::string> GetFileThaHasChanged(git_commit *commit)
{
   std::vector<std::string>ret;
   return ret;
   #ifdef __lib_git_2_defined
   std::vector<std::string> changedFiles;

   git_tree *tree = nullptr;
   git_tree *parentTree = nullptr;
   git_diff *diff = nullptr;

   try
   {
      // Get the tree of the current commit
      if (git_commit_tree(&tree, commit) != 0)
      {
         throw std::runtime_error("Failed to get the commit tree");
      }

      // Get the parent commit
      if (git_commit_parentcount(commit) > 0)
      {
         git_commit *parentCommit = nullptr;

         if (git_commit_parent(&parentCommit, commit, 0) != 0)
         {
            throw std::runtime_error("Failed to get parent commit");
         }

         // Get the tree of the parent commit
         if (git_commit_tree(&parentTree, parentCommit) != 0)
         {
            git_commit_free(parentCommit);
            throw std::runtime_error("Failed to get parent commit tree");
         }

         git_commit_free(parentCommit);
      }

      // Create a diff between the trees
      if (git_diff_tree_to_tree(&diff, git_commit_owner(commit), parentTree, tree, nullptr) != 0)
      {
         throw std::runtime_error("Failed to create diff");
      }

      // Iterate through the diff and collect changed file paths
      git_diff_foreach(diff, [](const git_diff_delta *delta, float /*progress*/, void *payload) -> int
                       {
                auto* files = static_cast<std::vector<std::string>*>(payload);
                files->push_back(delta->new_file.path);
                return 0; }, nullptr, nullptr, nullptr, &changedFiles);
   }
   catch (const std::exception &e)
   {
      // Clean up in case of exceptions
      if (tree)
         git_tree_free(tree);
      if (parentTree)
         git_tree_free(parentTree);
      if (diff)
         git_diff_free(diff);
      throw; // Re-throw the exception
   }

   // Cleanup
   if (tree)
      git_tree_free(tree);
   if (parentTree)
      git_tree_free(parentTree);
   if (diff)
      git_diff_free(diff);

   return changedFiles;
   #endif
}

void GitPanel::PrintCommitInfo(git_commit *commit)
{
#ifdef __lib_git_2_defined
   const git_signature *author = git_commit_author(commit);
   const char *message = git_commit_message(commit);
   const git_oid *oid = git_commit_id(commit);
   const git_signature *committer = git_commit_committer(commit);

   git_commit_information_base save_object;
   // Print commit hash
   char oid_str[GIT_OID_HEXSZ + 1];
   git_oid_tostr(oid_str, sizeof(oid_str), oid);
   std::cout  << oid_str << std::endl;
   save_object.commit_id = oid_str;

   if (committer)
   {
      std::stringstream stream;
      stream << committer->name << "(" << committer->email << ")";
      save_object.commiter_info = stream.str();
    
   }
   if (committer)
   {
      std::stringstream stream;
      stream << ctime(&author->when.time);
      save_object.commit_date = stream.str();
   }

   // Print author details
   if (author)
   {
      std::stringstream stream;
      stream  << author->name << " <" << author->email << ">" << std::endl;
      save_object.author = stream.str();
   } 
   if(author)
   {
      std::stringstream stream;
      stream << "Author Date: " << ctime(&author->when.time);
      save_object.author_date = stream.str();
   }

   // Print commit message
   if (message)
   {
      save_object.message = message;
      std::cout << "Message: " << message << std::endl;
   }
   save_object.files_changed = GetFileThaHasChanged(commit);

   commits_list.emplace_back(save_object);
   std::cout << "------------------------" << std::endl;
#endif
}

GitPanel::GitPanel(wxWindow *window) : wxPanel(window, wxID_ANY)
{
   ///SetBackgroundColour(wxColour(0x43, 0x43, 0x43, 255));

   wxStaticText *text = new wxStaticText(this, wxID_ANY, "Git Control version");
   wxStaticText *ctext = new wxStaticText(this, wxID_ANY, "Commit Information");
   wxStaticText *xtext = new wxStaticText(this, wxID_ANY, "File Changed in this commit");

   commits = new wxListBox(this, wxID_ANY);
   commit_information = new wxListBox(this, wxID_ANY);
   file_changed_in_this_commit = new wxListBox(this, wxID_ANY);

   wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

   sizer->Add(text, 0, wxEXPAND | wxWEST, 10);
   sizer->Add(commits, 2, wxEXPAND | wxALL, 10);
   sizer->Add(ctext, 0, wxEXPAND | wxWEST, 10);
   sizer->Add(commit_information, 2, wxEXPAND | wxALL, 10);
   sizer->Add(xtext, 0, wxEXPAND | wxWEST, 10);
   sizer->Add(file_changed_in_this_commit, 2, wxEXPAND | wxALL, 10);

   // Bind the selection event to a handler
   commits->Bind(wxEVT_LISTBOX, &GitPanel::OnListBoxSelect, this);

   SetSizer(sizer);
}

void GitPanel::OnListBoxSelect(wxCommandEvent &event)
{
#ifdef __lib_git_2_defined
   int selection = commits->GetSelection(); // Get the index of the selected item
   if (selection != wxNOT_FOUND)
   {
      std::stringstream stream;
      std::string value = commits->GetString(selection).ToUTF8().data();
      for (auto commit : commits_list)
      {

         int fnd = value.find("||=> ");
         auto strcommit = value.substr(fnd + 5);

         if (commit.commit_id == strcommit)
         {
            commit_information->Clear();
            commit_information->Append("Commit ID");
            commit_information->Append(commit.commit_id);
            commit_information->Append("Author:");
            commit_information->Append(commit.author.c_str());
            commit_information->Append("Commiter: ");
            commit_information->Append(commit.commiter_info.c_str());
            commit_information->Append("Commiter Date: ");
            commit_information->Append(commit.commit_date.c_str());
            commit_information->Append("message:");
            commit_information->Append(commit.message.c_str());
            file_changed_in_this_commit->Clear();
            for (auto file : commit.files_changed)
               file_changed_in_this_commit->Append(file.c_str());
            return;
         }
      }
   }
#endif
}

void GitPanel::LoadCommitsInformationInFolder(const std::string &path)
{
#ifdef __lib_git_2_defined
   const char *repo_path = path.c_str();
   git_repository *repo = nullptr;

   // Initialize libgit2
   if (git_libgit2_init() < 0)
   {
      std::cerr << "Failed to initialize libgit2" << std::endl;
      return;
   }

   // Open the repository
   if (git_repository_open(&repo, repo_path) < 0)
   {
      const git_error *e = git_error_last();
      std::cerr << "Error opening repository: " << (e ? e->message : "Unknown error") << std::endl;
      git_libgit2_shutdown();
      return;
   }
   commits_list.clear();
   // Create a revwalk to traverse commits
   git_revwalk *walker = nullptr;
   if (git_revwalk_new(&walker, repo) < 0)
   {
      std::cerr << "Failed to create revwalk" << std::endl;
      git_repository_free(repo);
      git_libgit2_shutdown();
      return;
   }

   // Start from HEAD
   if (git_revwalk_push_head(walker) < 0)
   {
      std::cerr << "Failed to push HEAD to revwalk" << std::endl;
      git_revwalk_free(walker);
      git_repository_free(repo);
      git_libgit2_shutdown();
      return;
   }

   // Traverse commits
   git_oid oid;
   while (git_revwalk_next(&oid, walker) == 0)
   {
      git_commit *commit = nullptr;
      if (git_commit_lookup(&commit, repo, &oid) == 0)
      {
         PrintCommitInfo(commit);
         git_commit_free(commit);
      }
      else
      {
         std::cerr << "Failed to lookup commit" << std::endl;
      }
   }

   // Cleanup
   git_revwalk_free(walker);
   git_repository_free(repo);
   git_libgit2_shutdown();

   if (commits_list.size() > 0)
   {
      for (auto commit : commits_list)
      {
         wxString strcommit;
         if (commit.message.size() > 16)
            strcommit = commit.message.SubString(0, 16);
         else
            strcommit = commit.message;
         commits->Append(strcommit + " ||=> " + commit.commit_id);
      }
   }

   return;
#endif
}