
 
#include <wx/wx.h>
#include <wx/txtstrm.h>  
#include <wx/file.h>
#include <wx/stc/stc.h>
#include <wx/filedlg.h>
#include <wx/aui/aui.h>
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/file.h>

#include "callbacks.h"
#include "do_devwindow.h"
#include "tabs.h"
#include "explorer_panel.h"
#include "exec_dialog.h"
#include "terminal_emulator.h"
#include "symbol_panel.h"

#include <iostream>



class DoDevEditorApp : public wxApp {

public:

    virtual bool OnInit() {

        auto frame =  new do_devwindow();

        set_default_callback();

        auto editortabs = new editor_tab(frame);  
        auto explorerpanel = new explorer_panel(frame);
        auto exec_panel = new exec_dialog(frame);
        auto symbolpanel = new symbol_panel(frame);

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

        set_base_window(
            frame,
            editortabs,
            explorerpanel
        );
        frame->add_panel(&explorerPanel);
        frame->add_panel(&editorPanel);
        frame->add_panel(&execPanel);
        frame->add_panel(&symbolPanel);
    
        frame->update_components();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(DoDevEditorApp);
