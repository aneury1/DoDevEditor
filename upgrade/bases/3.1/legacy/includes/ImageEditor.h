#ifndef IMAGEEDITOR_H_DEFINED
#define IMAGEEDITOR_H_DEFINED

#include "EditorTab.h"
#include <wx/wx.h>

struct ImageEditor : public EditorTab {

  ImageEditor(wxWindow *parent);

  virtual int getTypeOfEditor();

  virtual std::vector<uint8_t> getData();

  virtual Response setData(std::vector<uint8_t> d);

  virtual bool wasChanged();

  virtual Response saveDocument();

  virtual Response openFile(std::string filepath);
};

#endif /// IMAGEEDITOR_H_DEFINED