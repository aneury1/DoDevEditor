#include "main_menu.h"


wxMenu* create_file_menu_entries()
{
   wxMenu *fileMenu = new wxMenu();
   fileMenu->Append(wxID_NEW, "&New File\tCtrl-N", "Create a new document");
   fileMenu->Append(wxID_OPEN, "&Open File\tCtrl-O", "Open a File");
   fileMenu->Append(OpenFolder, "&Open Folder\tCtrl-O", "Open a Folder");
   fileMenu->Append(CloseFolder, "&Close Opened Folder\tCtrl-Q", "Close a Folder");
   fileMenu->Append(wxID_SAVE, "&Save\tCtrl-S", "Save the current document");
   fileMenu->AppendSeparator();
   fileMenu->Append(wxID_SAVE, "&Print File\tCtrl-P", "Print current file");
   fileMenu->AppendSeparator();
   fileMenu->Append(wxID_EXIT, "E&xit\tCtrl-Q", "Exit the application");
   return fileMenu;
}

wxMenu *create_edit_menu_entries()
{
    wxMenu *menu = new wxMenu();
    menu->Append(wxID_UNDO, "&Undo \tCtrl-Z", " ");
    menu->Append(wxID_REDO, "&Redo\tCtrl-Y", " ");
    menu->AppendSeparator();
    menu->Append(wxID_CUT, "&Cut\tCtrl-X", " ");
    menu->Append(wxID_COPY, "&Copy\tCtrl-C", " ");
    menu->Append(wxID_PASTE, "&Paste\tCtrl-P", " ");
    menu->AppendSeparator();
    menu->Append(wxID_FIND, "&Find \tCtrl-F", " ");
    menu->Append(wxID_REPLACE, "&Replace\tCtrl-H", " ");
    menu->Append(wxID_REPLACE_ALL, "&Replace All File\tCtrl-SHIFT-H", " ");
    return menu;
}


wxMenu *create_view_menu_entries()
{
    wxMenu *menu = new wxMenu();
    menu->Append(ViewFileExplorer, "File Explorer ", " ");
    menu->Append(wxID_ANY, "Bottom Options", " ");
    menu->AppendSeparator();
    menu->Append(wxID_ANY, "View Control Version(Not Implememted)", " ");
    menu->Append(wxID_ANY, "View Symbols(Not Implemented)", " ");
    //menu->Append(wxID_COPY, "&Copy\tCtrl-C", " ");
    //menu->Append(wxID_PASTE, "&Paste\tCtrl-P", " ");
    //menu->AppendSeparator();
    //menu->Append(wxID_FIND, "&Find \tCtrl-F", " ");
    //menu->Append(wxID_REPLACE, "&Replace\tCtrl-H", " ");
    //menu->Append(wxID_REPLACE_ALL, "&Replace All File\tCtrl-SHIFT-H", " ");
    return menu;
}