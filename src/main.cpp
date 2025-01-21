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

#include "callbacks.h"
#include "do_devwindow.h"
#include "tabs.h"
#include "explorer_panel.h"

class DoDevEditorApp : public wxApp {

public:
    virtual bool OnInit() {
        auto frame =  new do_devwindow(); ////new editor_tab_panel(); ///do_editor::get(); ///new do_editor();
        set_default_callback();
        auto editortabs = new editor_tab(frame);  
        auto explorerpanel = new explorer_panel(frame);

        panel_info editorPanel;        
        editorPanel.panel = editortabs;
        editorPanel.info  = wxAuiPaneInfo().Name("editor_panel_name").CenterPane();

        panel_info explorerPanel;        
        explorerPanel.panel = explorerpanel;
        explorerPanel.info  = wxAuiPaneInfo().Name("file_explorer_panel_name").Left().Caption("Explorer").BestSize(300, 800).MinSize(200, 600);

        set_base_window(
            frame,
            editortabs,
            explorerpanel
        );
        frame->add_panel(&explorerPanel);
        frame->add_panel(&editorPanel);
 
        frame->update_components();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(DoDevEditorApp);