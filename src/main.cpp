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

#if 0 
class editor_tab_panel;

struct editorpanel : public wxPanel{
   
   wxAuiNotebook* editorTabs;
   wxButton *newTab;
   editorpanel(wxWindow *frame) : wxPanel(frame, wxID_ANY){
       SetBackgroundColour(wxColour(255,0,0,255));
       wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
       newTab = new wxButton(this, wxID_LAST + 1, wxString("Agregar"));
       editorTabs = new wxAuiNotebook(this, wxID_LAST+2, wxDefaultPosition, wxDefaultSize,
                                  wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ACTIVE_TAB);


      
        Bind(wxEVT_BUTTON, [this](wxCommandEvent& event) {
            wxPanel *panel = new wxPanel(editorTabs, wxID_ANY);
            panel->SetBackgroundColour(wxColour(0,0,255,255));
            wxStyledTextCtrl *textEditor = new wxStyledTextCtrl(panel, wxID_ANY);
            textEditor->SetMinSize(wxSize(800, 600));
            textEditor->SetBackgroundColour(wxColour(255, 30, 30));
            textEditor->SetForegroundColour(wxColour(255, 255, 255));
            textEditor->SetBackgroundColour("#FFFFFF");
            textEditor->StyleSetSize(wxSTC_STYLE_DEFAULT, 12);
            wxBoxSizer *Ssizer = new wxBoxSizer(wxVERTICAL);
            Ssizer->Add(textEditor, 1, wxEXPAND);
            panel->SetSizer(Ssizer);
            editorTabs->AddPage(panel, "HP", true);
            
        }, wxID_LAST + 1);

       sizer->Add(newTab, 0);
       sizer->Add(editorTabs, 1, wxEXPAND);
       SetSizer(sizer);
   }
};
struct editor_tab_panel : public wxFrame{
   
   wxAuiManager auiManager;
   editorpanel *panel;
   editor_tab_panel(): wxFrame(nullptr, wxID_ANY,wxEmptyString,wxDefaultPosition,wxDefaultSize )
   {
       auiManager.SetManagedWindow(this);
       panel = new editorpanel(this);
       auiManager.AddPane(panel, wxAuiPaneInfo().Name("editor_panel_name").CenterPane());
       Maximize();
   }
};
#endif



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