#ifndef __TEXTEDITOR_H_DEFINED
#define __TEXTEDITOR_H_DEFINED
#include <wx/wx.h>
#include <wx/txtstrm.h>
#include <wx/file.h>
#include <wx/stc/stc.h>
#include <wx/filedlg.h>
#include <wx/aui/aui.h>
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <vector>
#include <string>
#include <wx/textcompleter.h>
#include "EditorTab.h"
#include "constant.h"

#include "Dialogs.h"

struct TextEditor : public EditorTab{ 
   
   FindDialogInCurrentTextEditor* findIn;

   virtual Response openFile(std::string filepath) override;   

   virtual int getTypeOfEditor() override;
   
   virtual std::vector<uint8_t> getData()override;

   virtual bool wasChanged()override{
      return changed;
   }

   virtual Response setData(std::vector<uint8_t> d);

   virtual Response saveDocument() override;
   
   wxStyledTextCtrl *textEditor;
   
   bool changed = false;

   TextEditor(wxWindow *parent);
   
   void OnCharAdded(wxStyledTextEvent &event);
   
   void SetLineNumber(bool value);

};


#endif