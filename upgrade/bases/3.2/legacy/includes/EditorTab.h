#ifndef EDITOR_TAB_H_DEFINED
#define EDITOR_TAB_H_DEFINED
#include <wx/wx.h>
#include <vector>
#include <string>
#include <stdint.h>
#include "constant.h"
#include <Settings.h>
struct EditorTab: public wxPanel{    
   
    std::string filepath;
    bool is_file = false;

    EditorTab(wxWindow *parent) : wxPanel(parent, wxID_ANY){
        SetBackgroundColour(defaultSettings.getPanelBG());
    }
   
    virtual ~EditorTab(){}

    virtual int getTypeOfEditor() = 0;
    
    virtual std::vector<uint8_t> getData() = 0;

    virtual Response setData(std::vector<uint8_t> d)=0;

    virtual bool wasChanged() = 0;

    virtual Response saveDocument() = 0;

    virtual Response openFile(std::string filepath) = 0;

    void SetPath(std::string p){
        filepath = p;
        is_file = true;
    }
 
    std::string GetFileName(){
        if(filepath.size()>0){
            int i = filepath.find_last_of(FileSeparator);
            if(i!=std::string::npos){
                return filepath.substr(i+1, filepath.size()-i);
            }
            return filepath;
        }
        return "";
    }


};


#endif /// EDITOR_TAB_H_DEFINED