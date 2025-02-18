#include <wx/wx.h>
#include "constant.h"
#include "frame.h"


struct App : public wxApp{
  
    public:

    bool OnInit(){
        auto f = WindowFrame::get();
        f->Show(true);
        return true;
    }

};


wxIMPLEMENT_APP(App);