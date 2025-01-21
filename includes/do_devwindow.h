#ifndef DO_DEVWINDOW_H
#define DO_DEVWINDOW_H
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

 

struct panel_info{
    wxPanel *panel;
    wxAuiPaneInfo info;
};


class do_devwindow : public wxFrame{

    
    wxAuiManager auiManager;

    void on_exit(wxCommandEvent &event);
    void on_key_entered(wxCommandEvent &event);
    void on_context_menu(wxContextMenuEvent& event);    
    public:
    

    do_devwindow();

    static do_devwindow& get(){
        static do_devwindow instance;
        return instance;
    }


    ~do_devwindow();
    void update_components();
    void add_panel(panel_info *panel);
    
};




#endif