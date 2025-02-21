#ifndef CSV_EDITOR_H_DEFINED
#define CSV_EDITOR_H_DEFINED
#include <wx/wx.h>
#include "EditorTab.h"
#include <wx/grid.h>


struct CSVEditor : public EditorTab{


    wxGrid* grid;

    CSVEditor(wxWindow *parent) : EditorTab(parent){}
   
    virtual ~CSVEditor(){}

    virtual int getTypeOfEditor();
    
    virtual std::vector<uint8_t> getData();

    virtual Response setData(std::vector<uint8_t> d);

    virtual bool wasChanged();

    virtual Response saveDocument();

    virtual Response openFile(std::string filepath);
};


#endif ///CSV_EDITOR_H_DEFINED