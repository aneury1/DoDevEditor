#ifndef EXPLORER_PANEL_H_DEFINED
#define EXPLORER_PANEL_H_DEFINED
#include <wx/wfstream.h>
#include <wx/filename.h>
#include <wx/wx.h>
#include <wx/wx.h>
#include <wx/txtstrm.h>  
#include <wx/file.h>
#include <wx/stc/stc.h>
#include <wx/filedlg.h>
#include <wx/aui/aui.h>
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/file.h>

enum class selection_type{
   File,
   Folder,
   None
};

struct explorer_panel : wxPanel {
   private:


   wxStaticText *explorerLabel;
   wxTreeCtrl* folderTree;
   void on_tree_item_activated(wxTreeEvent &event);
   void on_context_menu(wxContextMenuEvent& event);
   void create_file_in_current_selection(wxCommandEvent &ev);
   void create_folder_in_current_selection(wxCommandEvent &ev);
   public:

   explorer_panel(wxWindow *parent);
  
   ~explorer_panel();
 
   void set_path_label(const wxString& path);

   void clear_folder_tree();

   wxTreeItemId add_root(const wxString& text);

   void populate_folder_tree(const wxString &path, wxTreeItemId parent);

   wxString getSelectedPath();

   selection_type get_seletion_type();

};





#endif  ///EXPLORER_PANEL_H_DEFINED