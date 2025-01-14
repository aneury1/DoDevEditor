#ifndef DEFAULT_CALLBACKS_DEFINED
#define DEFAULT_CALLBACKS_DEFINED
#include <wx/wx.h>
#include <wx/txtstrm.h>
#include <wx/file.h>
#include <wx/stc/stc.h>
#include <wx/filedlg.h>
#include <wx/aui/aui.h>
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/file.h>

void on_open_folder(const wxCommandEvent &event);
void on_close_folder(const wxCommandEvent &event);
void on_open_new_file(const wxCommandEvent &event);
void on_open_existing_file(const wxCommandEvent &event);
void on_save_file(const wxCommandEvent &event);
void on_close_tab(const wxAuiNotebookEvent &event);
void on_tree_item_activated(const wxTreeEvent &event);
void on_ctrl_i(const wxCommandEvent &event);
void on_ctrl_l(const wxCommandEvent &event);

#endif