#include <wx/wx.h>


struct Application : public wxApp{
    bool OnInit(){
        return true;
    }
};

wxIMPLEMENT_APP(Application);