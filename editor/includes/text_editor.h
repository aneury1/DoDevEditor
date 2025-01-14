#ifndef TEXT_EDITOR_H_DEFINED
#define TEXT_EDITOR_H_DEFINED
#include "id_handler.h"
#include <wx/wx.h>
#include <wx/txtstrm.h>  
#include <wx/file.h>
#include <wx/stc/stc.h>
#include <wx/filedlg.h>
#include <wx/aui/aui.h>
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/file.h>



class text_editor : public wxPanel {
   
   public: 
   text_editor(wxWindow *parent, int id = get_next_id());
   virtual ~text_editor();


   

   void set_text(const wxString& str);
   wxString get_text();
   bool load_text_file(const wxString& path);
   void increase_font_size_by_one();
   void decrease_font_size_by_one();
   
   const inline bool has_changed()const{return changed;}
   bool is_untitle(){
       return filepath.empty()|| filepath =="Untitled";
   }


   inline void set_drop_target(wxDropTarget *dropTarget){
      textEditor->SetDropTarget(dropTarget);
   }
 
   wxString get_path(){
      return filepath;
   }

   private:
    void configure_cpp_style();
    void on_paint(wxPaintEvent& event);
    void on_char_added(wxStyledTextEvent& event);
    int internal_editor_id;
    wxStyledTextCtrl *textEditor;
    wxString filepath;
    bool changed;
    int normal_font_size =16;
};



#endif ///TEXT_EDITOR_H_DEFINED
