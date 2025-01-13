#ifndef TAB_EDITOR_DEFINED
#define TAB_EDITOR_TEXT_DEFINED
#include <wx/wx.h>

struct editor_tab_meta_data{
   wxString path;
   bool     change;
   int      id;
};


#endif