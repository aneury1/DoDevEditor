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



struct explorer_panel : wxPanel {
   private:


   wxStaticText *explorerLabel;
   wxTreeCtrl* folderTree;
   void on_tree_item_activated(wxTreeEvent &event);
   
   public:

   explorer_panel(wxWindow *parent);
   ~explorer_panel();

   
   void set_path_label(const wxString& path);

   void clear_folder_tree();

   wxTreeItemId add_root(const wxString& text);

   void populate_folder_tree(const wxString &path, wxTreeItemId parent);

};





#endif  ///EXPLORER_PANEL_H_DEFINED