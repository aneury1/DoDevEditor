#ifndef FRAME_H_DEFINED
#define FRAME_H_DEFINED
#include <functional>
#include <map>
#include <wx/wx.h>
#include <wx/aui/aui.h>
#include "structures.h"
#include "constant.h"
#include "EditorTab.h"
#include "TabContainer.h"
#include "FileExplorerTab.h"

struct TabContainer;
struct FileExplorerTabContainer;

struct WindowFrame : public wxFrame{

    

    WindowFrame();

    void add_panel(panel_info *panel);

    void AddDefaultEvent();

    void SetDefaultPanel();

    void SetUpMenu();

    void OnEventHappened(wxCommandEvent& ev);

    void OnExit(wxCommandEvent &ev); 

    EditorTab *GetCurrentEditorTab();

    EditorTab* AddNewPage();

    void AddCMDCallback(int id, std::function<void(WindowFrame*)> cb);

    TabContainer* GetTabContainer();

    static WindowFrame* get(){
        static WindowFrame* inst = nullptr;
        if(!inst){
            inst =new WindowFrame();
        }
        return inst;
    }
 
    TabContainer* tabContainer = nullptr;
    
    FileExplorerTabContainer* explorerTabContainer = nullptr;
    
    std::map<int, std::function<void(WindowFrame*)>> commandEventFunctions;

    wxAuiManager auiManager;

};



#endif  /// FRAME_H_DEFINED