
 
#include <wx/wx.h>
#include <wx/txtstrm.h>  
#include <wx/file.h>
#include <wx/stc/stc.h>
#include <wx/filedlg.h>
#include <wx/aui/aui.h>
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/file.h>

#include "utils.h"
#include "callbacks.h"
#include "do_devwindow.h"
#include "tabs.h"
#include "explorer_panel.h"
#include "exec_dialog.h"
#include "terminal_emulator.h"
#include "symbol_panel.h"
#include "git_panel.h"

#include <iostream>
#include <git2.h>


class DoDevEditorApp : public wxApp {


    wxString filenamePath;
public:

    virtual bool OnInit() {

 
        if (git_libgit2_init() < 0) {
            std::cerr << "Failed to initialize libgit2" << std::endl;
            return 1;
        }

        auto frame =  new do_devwindow();

        set_default_callback();

        auto editortabs = new editor_tab(frame);  
        auto explorerpanel = new explorer_panel(frame);
        auto exec_panel = new exec_dialog(frame);
        auto symbolpanel = new symbol_panel(frame);
        auto gitpanel = new git_panel(frame);

        panel_info editorPanel;        
        editorPanel.panel = editortabs;
        editorPanel.info  = wxAuiPaneInfo().Name("editor_panel_name").CenterPane();

        panel_info explorerPanel;        
        explorerPanel.panel = explorerpanel;
        explorerPanel.info  = wxAuiPaneInfo().Name("file_explorer_panel_name").Left().Caption("Explorer").BestSize(300, 800).MinSize(200, 600);

        panel_info execPanel;        
        execPanel.panel = exec_panel;
        execPanel.info  = wxAuiPaneInfo().Name("exec_panel_name").Bottom().Caption("exec").BestSize(300, 400).MinSize(200, 100);

        panel_info symbolPanel;        
        symbolPanel.panel = symbolpanel;
        symbolPanel.info  = wxAuiPaneInfo().Name("symbol_panel_name").Left().Caption("symbol").BestSize(300, 400).MinSize(200, 100);

        panel_info gitPanel;        
        gitPanel.panel = gitpanel;
        gitPanel.info  = wxAuiPaneInfo().Name("git_panel_name").Right().Caption("GIT").BestSize(200, 400).MinSize(100, 100);

        set_base_window(
            frame,
            editortabs,
            explorerpanel,
            gitpanel
        );
        frame->add_panel(&explorerPanel);
        frame->add_panel(&editorPanel);
        frame->add_panel(&execPanel);
        frame->add_panel(&symbolPanel);
        frame->add_panel(&gitPanel);
    
        frame->update_components();
        frame->Show(true);

        if(filenamePath.size()){
            editortabs->add_empty_page();
            auto current = editortabs->get_current_editor();
            auto text = read_file(filenamePath.ToStdString());
            if(text.size()>0)
            {
                current->set_text(text);
                current->set_filepath(filenamePath);
                editortabs->set_title_current_page(filenamePath);
            }

        }

        return true;
    }

    virtual bool Initialize(int& argc, wxChar **argv)override{
       wxApp::Initialize(argc, argv);
       int iter = 0;
       if(argc==2){
          filenamePath = argv[1];
       }
       for(int i=0;i<iter;i++){
         std::cout <<" iter = "<<i;

       }
       return true;
    }
};

wxIMPLEMENT_APP(DoDevEditorApp);
