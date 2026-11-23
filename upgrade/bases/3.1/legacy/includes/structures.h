#ifndef STRUCTURES_H_DEFINED
#define STRUCTURES_H_DEFINED
#include <vector>
#include <wx/aui/aui.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/stc/stc.h>
#include <wx/treectrl.h>
#include <wx/txtstrm.h>
#include <wx/wx.h>

struct panel_info {
  wxPanel *panel;
  wxAuiPaneInfo info;
};

#endif /// STRUCTURES_H_DEFINED