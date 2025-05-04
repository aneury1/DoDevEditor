#ifndef __WINDOW_FRAME_H_DEFINED
#define __WINDOW_FRAME_H_DEFINED
#include <wx/wx.h>

struct WindowFrame : public wxFrame{
   WindowFrame():wxFrame(nullptr, wxID_ANY,wxT("DoDevEditor")){
    SetBackgroundColour(wxColour("#f5f4e9"));
   }
};


#endif /// __WINDOW_FRAME_H_DEFINED