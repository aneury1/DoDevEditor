#ifndef __MAIN_FRAME_H_DEFINED
#define __MAIN_FRAME_H_DEFINED
#include <wx/wx.h>
#include <wx/txtstrm.h>  
#include <wx/file.h>
#include <wx/stc/stc.h>
#include <wx/filedlg.h>
#include <wx/aui/aui.h>
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/file.h>

#include "text_editor.h"


struct main_window_frame_settings{
   bool exit_without_asking = true;
};


class main_window_frame : public wxFrame {
   
   main_window_frame();
   
   public:
  
   static main_window_frame* get(){
      static  main_window_frame* m;
      if(!m)
        m = new main_window_frame();
      return m;
   }
   
   ~main_window_frame();
   
   private:
   
   void on_exit(wxCommandEvent& event);

   void setup_main_settings();

   text_editor * add_new_page(const wxString& title);

   text_editor * get_current_text_editor();
  
   void populate_folder_tree(const wxString &path, wxTreeItemId parent);
   void on_open_folder(wxCommandEvent& event);
   void on_close_folder(wxCommandEvent& event);
   void on_open_new_file(wxCommandEvent& event);
   void on_open_existing_file(wxCommandEvent &event);
   void on_save_file(wxCommandEvent &event);
   void on_close_tab(wxAuiNotebookEvent &event);
   void on_tree_item_activated(wxTreeEvent &event);
   void on_ctrl_i(wxCommandEvent &event);
   void on_ctrl_l(wxCommandEvent &event);


   void insert_menu(wxMenu *menu, wxString title);

   private:
   wxMenuBar *menubar;
   wxAuiManager auiManager;
   wxPanel* explorerPanel;
   wxStaticText *explorerLabel;
   wxTreeCtrl* folderTree;
   wxAuiNotebook* editorTabs;
   wxString folderPath;
   wxString rootPath;

};


#endif //__MAIN_FRAME_H_DEFINED