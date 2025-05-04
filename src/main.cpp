#include <wx/wx.h>
#include "WindowFrame.h"

struct Application : public wxApp{
    bool OnInit(){
        auto frame = new WindowFrame();
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(Application);