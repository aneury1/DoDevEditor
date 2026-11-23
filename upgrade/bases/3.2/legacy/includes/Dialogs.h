#ifndef __DIALOG_H_DEFINED
#define __DIALOG_H_DEFINED
#include <set>
#include <string>
#include <vector>
#include <wx/wx.h>

struct FindDialogInCurrentTextEditor : wxDialog {

  FindDialogInCurrentTextEditor(wxWindow *parent);
};

struct OpenExistingFile : wxDialog {
  /// wxStringArray files;
  wxTextCtrl *values;
  wxPanel *panel;
  OpenExistingFile(wxWindow *parent, std::set<std::string> openfolder);
};

#endif