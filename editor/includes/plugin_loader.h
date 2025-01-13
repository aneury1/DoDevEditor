#ifndef PLUGIN_LOADER_H_DEFINED
#define PLUGIN_LOADER_H_DEFINED
#include <wx/wx.h>

bool load_plugin_by_shared_object(const wxString& path);

#endif