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

static const char *untitled ="Untitled";

static const char* file_filter = 
    "All Files (*.*)|*.*|"
    "C++ Source Files (*.cpp)|*.cpp|"
    "C Header Files (*.h)|*.h|"
    "C++ Header Files (*.hpp)|*.hpp|"
    "C++ Source Files (*.cc)|*.cc|"
    "C++ Source Files (*.cxx)|*.cxx|"
    "JavaScript Files (*.js)|*.js|"
    "JSON Files (*.json)|*.json|"
    "Text Files (*.txt)|*.txt|"
    "Java Files (*.java)|*.java|"
    "Assembly Files (*.asm;*.s)|*.asm;*.s|"
    "COBOL Files (*.cob)|*.cob|"
    "Makefiles (*.Make)|*.Make";




struct do_editor_settings{
   bool exit_without_asking = true;
};


class do_editor : public wxFrame {
   
   do_editor();
   
   public:
  
   static do_editor* get(){
      static  do_editor* m;
      if(!m)
        m = new do_editor();
      return m;
   }
   
   ~do_editor();
   
   private:
   text_editor *current_text_editor;
   
   void on_exit(wxCommandEvent& event);
   void create_main_instances();
   void setup_main_settings();
   void setup_accelerator();
   void create_main_menubar();
   void insert_menu(wxMenu *menu, wxString title);

   bool contains_page_with_title(const wxString &title);
   bool check_if_this_file_is_opened(const wxString& title);
   
   wxAuiPaneInfo*  find_Panel_by_name(const wxString &name);


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
   void on_view_file_explorer(wxCommandEvent &event);
   

   /// @brier Accelerator
   /// @param event 
   void on_ctrl_i(wxCommandEvent &event);
   void on_ctrl_l(wxCommandEvent &event);


  



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