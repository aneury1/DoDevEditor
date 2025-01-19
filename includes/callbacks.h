#ifndef __CALLBACKS__H_DEFINED
#define __CALLBACKS__H_DEFINED
#include <wx/wx.h>
#include <functional>
#include <unordered_map>

#include "do_devwindow.h"
#include "menubar.h"
#include "tabs.h"
#include "text_editor.h"
#include "explorer_panel.h"
#include "text_editor.h"
void set_base_window(
    do_devwindow *window,
    editor_tab *tabamanger,
    explorer_panel *_explorerpanel
    
);

void set_default_callback();
void call_by_event(const wxCommandEvent& ev);

void add_new_page(wxString& selectedName);
text_editor* get_current_text_editor();
void set_status_text(const wxString text);
wxString get_root_path();

bool is_file_already_in_the_editor(const wxString& path);


#endif ///__CALLBACKS__H_DEFINED