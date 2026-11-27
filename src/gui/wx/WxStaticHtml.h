#pragma once
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/html/htmlwin.h>
#include <wx/menu.h>
namespace wxui {
// Generic rendering widget for any native plugin. It never navigates to a URL.
class StaticHtmlView final : public wxHtmlWindow {
 public:
  explicit StaticHtmlView(wxWindow* parent) : wxHtmlWindow(parent, wxID_ANY) {
    Bind(wxEVT_HTML_LINK_CLICKED, [](wxHtmlLinkEvent&) {});
    Bind(wxEVT_CONTEXT_MENU, [this](wxContextMenuEvent&) {
      wxMenu menu;
      menu.Append(wxID_COPY, "Copy selected text");
      menu.Append(wxID_SELECTALL, "Select all");
      menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { copy(); }, wxID_COPY);
      menu.Bind(
          wxEVT_MENU, [this](wxCommandEvent&) { SelectAll(); }, wxID_SELECTALL);
      PopupMenu(&menu);
    });
  }

 protected:
  wxHtmlOpeningStatus OnOpeningURL(wxHtmlURLType, const wxString&,
                                   wxString*) const override {
    return wxHTML_BLOCK;
  }

 private:
  void copy() {
    auto text = SelectionToText();
    if (!text.empty() && wxTheClipboard->Open()) {
      wxTheClipboard->SetData(new wxTextDataObject(text));
      wxTheClipboard->Close();
    }
  }
};
}  // namespace wxui
