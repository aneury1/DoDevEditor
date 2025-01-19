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

void do_devwindow::on_key_entered(wxCommandEvent &ev){
 
 /// std::cout << "EVent :" << ev.GetId()<<"\n";
  call_by_event(ev);
}

 