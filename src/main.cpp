#include "main_frame.h"

class DoDevEditorApp : public wxApp {
public:
    virtual bool OnInit() {
        auto frame = new main_window_frame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(DoDevEditorApp);