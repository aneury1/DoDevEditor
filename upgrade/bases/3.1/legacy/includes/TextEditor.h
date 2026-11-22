#ifndef __TEXTEDITOR_H_DEFINED
#define __TEXTEDITOR_H_DEFINED
#include <wx/aui/aui.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/stc/stc.h>
#include <wx/textcompleter.h>
#include <wx/treectrl.h>
#include <wx/txtstrm.h>
#include <wx/wx.h>

#include <string>
#include <vector>

#include "Dialogs.h"
#include "EditorTab.h"
#include "constant.h"

struct TextEditor : public EditorTab {
  FindDialogInCurrentTextEditor* findIn;

  virtual Response openFile(std::string filepath) override;

  virtual int getTypeOfEditor() override;

  virtual std::vector<uint8_t> getData() override;

  virtual bool wasChanged() override { return changed; }

  virtual Response setData(std::vector<uint8_t> d);

  virtual Response saveDocument() override;

  wxStyledTextCtrl* textEditor;

  bool changed = false;

  TextEditor(wxWindow* parent);

  void OnCharAdded(wxStyledTextEvent& event);

  void SetLineNumber(bool value);
};

#endif