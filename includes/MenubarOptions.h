#ifndef __MENUBAR_DEFINED_H
#define __MENUBAR_DEFINED_H
#include <wx/wx.h>
#include "constant.h"


static inline wxMenu* create_action_menu()
{
   wxMenu *fileMenu = new wxMenu();
   fileMenu->Append(ActionMenuCommands, "&Execute\tCtrl-+", "Execute");
   return fileMenu;
}

static inline wxMenu* create_file_menu_entries()
{
   wxMenu *fileMenu = new wxMenu();
   fileMenu->Append(wxID_NEW, "&New File\tCtrl-N", "Create a new document");
   fileMenu->Append(wxID_OPEN, "&Open File\tCtrl-O", "Open a File");
   ///fileMenu->Append(OpenBinaryFileReadOnly, "&Open Binary File\tCtrl-O", "Open a File as Read Only");
   fileMenu->Append(OpenFolder, "&Open Folder\tCtrl-O", "Open a Folder");
   fileMenu->Append(CloseFolder, "&Close Opened Folder\tCtrl-Q", "Close a Folder");
   fileMenu->Append(wxID_SAVE, "&Save\tCtrl-S", "Save the current document");
   fileMenu->Append(SaveAs, "&Save as \tCtrl-S", "Save the current document");
   fileMenu->AppendSeparator();
   fileMenu->Append(wxID_SAVE, "&Print File\tCtrl-P", "Print current file");
   fileMenu->AppendSeparator();
   fileMenu->Append(wxID_EXIT, "E&xit\tCtrl-Q", "Exit the application");
   return fileMenu;
}

static inline wxMenu *create_edit_menu_entries()
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


static inline wxMenu *create_view_menu_entries()
{
    wxMenu *menu = new wxMenu();
    menu->Append(ViewFileExplorer, "File Explorer ", " ");
    menu->Append(wxID_ANY, "Bottom Options", " ");
    menu->AppendSeparator();
    menu->Append(ViewGitExplorer, "View Control Version(git)", " ");
    menu->Append(ViewExecPanel, "View Exec Panel", " ");
    return menu;
}

static inline wxMenu *create_automotive_menu_entries()
{
    wxMenu *menu = new wxMenu();
    menu->Append(AddDLTViewer, "Open DLT Panel on Editor(READ ONLY MODE)", " ");
    menu->Append(wxID_ANY, "Open CAN Protocol Viewer(NOT IMPLEMENTED)", " ");
    menu->AppendSeparator();
 
    return menu;
}
static inline wxMenu *create_extra_menu_entries()
{
    wxMenu *menu = new wxMenu();
    menu->Append(AddCSVViewer, "Open CSV (READ ONLY MODE)", " ");
    menu->AppendSeparator();
 
    return menu;
}


static wxMenuBar *CreateMenuBarAndOptions()
{
   auto menubar = new wxMenuBar();

   if (menubar)
   {
      auto fileMenu = create_file_menu_entries();
      if(fileMenu)
         menubar->Append(fileMenu, "&File");
      
      auto edit_menu = create_edit_menu_entries();
      if(edit_menu)
          menubar->Append(edit_menu, "&Edit");
      
      auto view_menu = create_view_menu_entries();
      if(view_menu)
         menubar->Append(view_menu, "&View");
      auto action = create_action_menu();
      if(action)
         menubar->Append(action, "&Action");
      auto automotive = create_automotive_menu_entries();
      if(automotive)
         menubar->Append(automotive, "&Automotive DEV");

      auto extra = create_extra_menu_entries();
      if(extra)
         menubar->Append(extra, "&Extra Options");  
   }
   
   return menubar;
}

#endif /// __MENUBAR_DEFINED_H