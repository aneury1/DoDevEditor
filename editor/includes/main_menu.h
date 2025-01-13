#ifndef MAIN_MENU_H
#define MAIN_MENU_H
#include <wx/wx.h>
#include <functional>
#include <vector>
#include "main_frame.h"
#include "id_handler.h"


struct menu_entry{
    wxString title;
    int id;
};

wxMenu* create_file_menu_entries();
wxMenu* create_edit_menu_entries();
wxMenu* create_view_menu_entries();

#endif