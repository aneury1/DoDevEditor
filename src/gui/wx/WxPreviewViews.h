#pragma once
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/mstream.h>
#include <wx/scrolwin.h>
#include <wx/treectrl.h>
#include <wx/wx.h>

#include <algorithm>
#include <string>
#include <vector>

#include "dodev/sdk/dodev_plugin_api.h"
#ifdef DODEV_WX_MEDIA_VIEW
#include <wx/mediactrl.h>
#endif
namespace wxui {
// Generic SDK views: never dispatch on file extensions or plugin identity.
inline wxPanel* imagePreview(wxWindow* parent, const void* data, size_t size) {
  wxMemoryInputStream stream(data, size);
  wxImage image;
  if (!image.LoadFile(stream, wxBITMAP_TYPE_ANY) || !image.IsOk())
    return nullptr;
  if (image.GetWidth() > 16000 || image.GetHeight() > 16000 ||
      static_cast<long long>(image.GetWidth()) * image.GetHeight() > 64000000)
    return nullptr;
  auto* panel = new wxPanel(parent);
  auto* s = new wxBoxSizer(wxVERTICAL);
  panel->SetSizer(s);
  auto* bar = new wxBoxSizer(wxHORIZONTAL);
  s->Add(bar, 0, wxEXPAND);
  auto* zoom = new wxChoice(panel, wxID_ANY);
  for (auto t : {"25%", "50%", "100%", "150%", "200%"}) zoom->Append(t);
  zoom->SetSelection(2);
  bar->Add(zoom, 0, wxALL, 3);
  bar->Add(new wxStaticText(panel, wxID_ANY,
                            wxString::Format("%d x %d pixels", image.GetWidth(),
                                             image.GetHeight())),
           0, wxALL | wxALIGN_CENTER_VERTICAL, 3);
  auto* sc = new wxScrolledWindow(panel);
  sc->SetScrollRate(10, 10);
  s->Add(sc, 1, wxEXPAND);
  auto* layout = new wxBoxSizer(wxVERTICAL);
  sc->SetSizer(layout);
  auto* bitmap = new wxStaticBitmap(sc, wxID_ANY, wxBitmap(image));
  layout->Add(bitmap, 0, wxALL, 4);
  sc->FitInside();
  zoom->Bind(wxEVT_CHOICE, [image, bitmap, sc, zoom](wxCommandEvent&) {
    const double ratios[] = {.25, .5, 1, 1.5, 2};
    auto ratio = ratios[zoom->GetSelection()];
    int w = std::max(1, int(image.GetWidth() * ratio)),
        h = std::max(1, int(image.GetHeight() * ratio));
    if (static_cast<long long>(w) * h > 64000000) return;
    bitmap->SetBitmap(wxBitmap(image.Scale(w, h, wxIMAGE_QUALITY_HIGH)));
    bitmap->SetMinSize(wxSize(w, h));
    sc->Layout();
    sc->FitInside();
  });
  return panel;
}
inline wxPanel* treePreview(wxWindow* parent, const DodevTreeNode* nodes,
                            size_t count) {
  auto* p = new wxPanel(parent);
  auto* s = new wxBoxSizer(wxVERTICAL);
  p->SetSizer(s);
  auto* tree = new wxTreeCtrl(p, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                              wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT);
  s->Add(tree, 1, wxEXPAND);
  std::vector<wxTreeItemId> stack{tree->AddRoot("Root")};
  for (size_t i = 0; i < count; ++i) {
    auto depth = nodes[i].depth;
    stack.resize(depth + 1);
    stack.push_back(
        tree->AppendItem(stack.back(), wxString::FromUTF8(nodes[i].label)));
  }
  tree->Expand(stack.front());
  tree->Bind(wxEVT_CONTEXT_MENU, [tree](wxContextMenuEvent&) {
    wxMenu m;
    m.Append(wxID_COPY, "Copy node");
    m.Bind(
        wxEVT_MENU,
        [tree](wxCommandEvent&) {
          auto id = tree->GetSelection();
          if (id.IsOk() && wxTheClipboard->Open()) {
            wxTheClipboard->SetData(
                new wxTextDataObject(tree->GetItemText(id)));
            wxTheClipboard->Close();
          }
        },
        wxID_COPY);
    tree->PopupMenu(&m);
  });
  return p;
}
inline wxPanel* mediaPreview(wxWindow* parent, const wxString& path) {
#ifdef DODEV_WX_MEDIA_VIEW
  auto* p = new wxPanel(parent);
  auto* s = new wxBoxSizer(wxVERTICAL);
  p->SetSizer(s);
  auto* player = new wxMediaCtrl(p, wxID_ANY);
  if (!player->Load(path)) {
    p->Destroy();
    return nullptr;
  }
  s->Add(player, 1, wxEXPAND);
  auto* row = new wxBoxSizer(wxHORIZONTAL);
  s->Add(row, 0, wxEXPAND);
  for (auto label : {"Play", "Pause", "Stop"}) {
    auto* b = new wxButton(p, wxID_ANY, label);
    row->Add(b, 0, wxALL, 3);
    b->Bind(wxEVT_BUTTON, [player, label](wxCommandEvent&) {
      if (std::string(label) == "Play")
        player->Play();
      else if (std::string(label) == "Pause")
        player->Pause();
      else
        player->Stop();
    });
  }
  return p;
#else
  (void)parent;
  (void)path;
  return nullptr;
#endif
}
}  // namespace wxui
