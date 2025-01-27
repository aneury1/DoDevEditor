#include "do_devwindow.h"
#include "menubar.h"
#include "callbacks.h"

do_devwindow::do_devwindow() : wxFrame(nullptr, wxID_ANY, wxEmptyString) {
    
    auiManager.SetManagedWindow(this);
    auto menubar = create_default_menubar();
    SetMenuBar(menubar);
    Maximize();
    Bind(wxEVT_MENU, &do_devwindow::on_exit, this, wxID_EXIT);

    Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, wxID_NEW);
    Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, wxID_OPEN);
    Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, wxID_SAVE);
    Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, SaveAs);
    Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, OpenFolder);
    Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, CloseFolder);
    Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, ActionMenuCommands);
    

    Bind(wxEVT_CONTEXT_MENU, &do_devwindow::on_context_menu, this); 
}
do_devwindow::~do_devwindow() {}

void do_devwindow::update_components(){
   auiManager.Update();
}


void do_devwindow::on_exit(wxCommandEvent &event)
{
  auiManager.UnInit();
  Close();
}

void do_devwindow::add_panel(panel_info *panel){
    auiManager.AddPane(panel->panel,panel->info);
}
void do_devwindow::on_context_menu(wxContextMenuEvent& event){
   std::cout <<"EVent has been triggered: "<< event.GetId() <<"\n";
           // Create the context menu
        wxMenu menu;
        menu.Append(wxID_OPEN,   "Open");
        menu.Append(OpenFolder,  "Open folder");
        menu.Append(CloseFolder, "Close Open folder");
        menu.Append(wxID_SAVE, "Save Current");
        menu.AppendSeparator();
        menu.Append(wxID_EXIT, "Exit");
        // Show the menu at the current mouse position
        PopupMenu(&menu, ScreenToClient(wxGetMousePosition()));
}

void do_devwindow::on_key_entered(wxCommandEvent &ev)
{ 
  call_by_event(ev);
}

 