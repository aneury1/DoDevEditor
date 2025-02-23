#ifndef DLT_VIEWER_H_DEFINED
#define DLT_VIEWER_H_DEFINED

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <vector>
#include <string>
#include "EditorTab.h"

struct DLTInformation{
  int index;
  std::string headerText;
  std::string payload;
};

std::vector<DLTInformation> parseDLTFile(const std::string file);

struct DLTViewerTab : public EditorTab{
    

    wxBoxSizer *sizer;
    wxStaticText *label;
    wxPanel* optionPanel;
    wxButton* openDLTFile;
    wxListCtrl *dltentries;


    DLTViewerTab(wxWindow *parent);

    virtual ~DLTViewerTab(){}

    virtual int getTypeOfEditor(){
        return 2;
    }
    
    virtual std::vector<uint8_t> getData()override;

    virtual Response setData(std::vector<uint8_t> d)override;

    virtual bool wasChanged()override;

    virtual Response saveDocument()override;

    virtual Response openFile(std::string filepath)override;

};



#endif