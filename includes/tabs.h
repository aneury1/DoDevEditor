#ifndef _TABS_H_DEFINED
#define _TABS_H_DEFINED

#include "do_devwindow.h"
#include "text_editor.h"

struct editor_tab : public wxPanel
{

private:
   wxAuiNotebook *editorTabs;
   wxButton *newTab;
   wxChoice *fontChoice;
   text_editor *current_text_editor = nullptr;

   void on_close_tab(wxAuiNotebookEvent &event);
   void on_font_change(wxCommandEvent &event);

public:
   editor_tab(do_devwindow *parent);
   ~editor_tab();

   void add_empty_page();

   text_editor *get_current_editor();

   void set_title_current_page(const wxString &title);

   bool is_file_already_in_the_editor(const wxString &path);
};

#endif /// _TABS_H_DEFINED