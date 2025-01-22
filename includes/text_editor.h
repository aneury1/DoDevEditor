#ifndef __TEXT_EDITOR_H_DEFINED
#define __TEXT_EDITOR_H_DEFINED
#include <wx/wx.h>
#include <wx/wx.h>
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
#include <vector>

#include "id_handler.h"

struct text_editor : public wxPanel
{
private:
   int internal_id;
   wxStyledTextCtrl *textEditor;
   bool fromfile = false;
   bool changed = false;
   wxString path;
   void on_char_added(wxStyledTextEvent &event);
public:
   text_editor(wxWindow *parent);
   ~text_editor();
   void set_filepath(const wxString &path);
   void set_text(const wxString &text);
   wxString get_text();

   const bool get_changed(){
      return changed;
   }
   const bool get_from_file(){
      return fromfile;
   }

   const wxString get_path(){
      return path;
   }

   void set_font(const wxFont& font);

   inline void debug(){
      std::cout <<"Path" << path.ToStdString()<<" from file:"<< fromfile<<"\n";
   }
};

#endif // __TEXT_EDITOR_H_DEFINED