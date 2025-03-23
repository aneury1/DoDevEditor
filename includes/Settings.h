#ifndef SETTINGS_H_DEFINED
#define SETTINGS_H_DEFINED

#include <string>
#include <wx/wx.h>


struct EditorSettings{
   wxColour panelColor;
   wxColour panelForegroundColor;
   wxColour editorColor;
  
   EditorSettings()
   :
   panelColor(0x25,0x25,0x26,255),
   panelForegroundColor(255,255,255,255),
   editorColor(0x25,0x25,0x26,255)
   {
   }

    const wxColour getPanelBG(){
        return editorColor;
    }
    const wxColour getPanelFG(){
        return panelForegroundColor;
    }


   const wxColour getEditorBG(){
    return editorColor;
   }
};

static EditorSettings defaultSettings;

#endif //// SETTINGS_H_DEFINED