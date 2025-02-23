#ifndef __TAB_CONTAINER_H_DEFINED
#define __TAB_CONTAINER_H_DEFINED
#include <wx/wx.h>
#include "frame.h"
#include "EditorTab.h"


struct TabContainer : public wxPanel{
     

    EditorTab *currentTab = nullptr;

    wxBoxSizer *mainSizer;

    wxPanel *toolPanel;
   
    wxAuiNotebook *editorTabs;
   
    TabContainer(wxWindow *parent);
   
    void AddEmptyTextPage();

    void AddPage(const std::string file);
 
    void AddEmptyTextPage(wxCommandEvent& ev);

    void OnCloseTabEvent(wxAuiNotebookEvent& ev);
    
    void SetTitleToCurrentPage(const std::string &title);

    void SetDataToCurrentContainer(std::vector<uint8_t> da);

    void addCustomEditorTab(EditorTab *tab);

    EditorTab *GetCurrentTab(){
        return currentTab;
    }
   
};


#endif // __TAB_CONTAINER_H_DEFINED
