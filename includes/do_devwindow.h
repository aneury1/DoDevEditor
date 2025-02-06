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

static const char *editor_panel_name = "editor_panel_name";
static const char *file_explorer_panel_name = "file_explorer_panel_name";
static const char *exec_panel_name = "exec_panel_name";
static const char *symbol_panel_name = "symbol_panel_name";
static const char *git_panel_name = "git_panel_name";

struct panel_info
{
    wxPanel *panel;
    wxAuiPaneInfo info;
};

class do_devwindow : public wxFrame
{

    wxAuiManager auiManager;

    void on_exit(wxCommandEvent &event);
    void on_key_entered(wxCommandEvent &event);
    void on_context_menu(wxContextMenuEvent &event);    
    void show_explorer(bool show);
    void show_editor(bool show);
    void show_git_panel(bool show);
    void show_exec(bool show);
 
    
public:
    do_devwindow();


    void setup_default_panels();

    static do_devwindow &get()
    {
        static do_devwindow instance;
        return instance;
    }

    ~do_devwindow();
    void update_components();
    void add_panel(panel_info *panel);
};





class do_deveditor_window : public wxFrame{
   
   public: 
   do_deveditor_window();
   ~do_deveditor_window();
};






#endif