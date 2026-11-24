#pragma once
#include <wx/aui/aui.h>
#include <wx/combobox.h>
#include <wx/dataview.h>
#include <wx/listbox.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/stc/stc.h>
#include <wx/treectrl.h>
#include <wx/wx.h>

#include "WxNativeTheme.h"
#include "dodev/themes/ThemeManager.h"
namespace wxui {
struct Theme {
  wxColour background, panel, text, muted, selection, keyword, comment, string;
  wxColour control, window, separator;
  static wxColour color(const std::string& s) {
    return wxColour(wxString::FromUTF8(s.substr(0, 7).c_str()));
  }
  static Theme from(const dodev::ThemeDefinition& t) {
    return {color(t.input),   color(t.panel),       color(t.text),
            color(t.comment), color(t.accent),      color(t.keyword),
            color(t.comment), color(t.stringColor), color(t.control),
            color(t.window),  color(t.separator)};
  }
  static Theme dark() {
    dodev::ThemeDefinition t;
    t.input = "#1e1e1e";
    t.panel = "#252526";
    t.text = "#d4d4d4";
    t.accent = "#094771";
    t.keyword = "#569cd6";
    t.comment = "#6a9955";
    t.stringColor = "#ce9178";
    t.control = "#333333";
    t.window = t.input;
    t.separator = "#454545";
    return from(t);
  }
  static Theme light() {
    dodev::ThemeDefinition t;
    t.input = "#ffffff";
    t.panel = "#f0f0f0";
    t.text = "#202020";
    t.accent = "#b8d8ff";
    t.keyword = "#1955aa";
    t.comment = "#007d41";
    t.stringColor = "#a03719";
    t.control = "#e0e0e0";
    t.window = t.input;
    t.separator = "#cccccc";
    return from(t);
  }
  void native() const {
    applyNativeTheme(window, panel, background, control, text, selection,
                     separator);
  }
  void apply(wxWindow* w) const {
    if (!w) return;
    wxColour bg = panel;
    if (dynamic_cast<wxTopLevelWindow*>(w)) bg = window;
    if (dynamic_cast<wxTextCtrl*>(w) || dynamic_cast<wxListCtrl*>(w) ||
        dynamic_cast<wxTreeCtrl*>(w) || dynamic_cast<wxListBox*>(w) ||
        dynamic_cast<wxDataViewCtrl*>(w) ||
        dynamic_cast<wxStyledTextCtrl*>(w) || dynamic_cast<wxComboBox*>(w) ||
        dynamic_cast<wxSpinCtrl*>(w) || dynamic_cast<wxSpinCtrlDouble*>(w))
      bg = background;
    else if (dynamic_cast<wxButton*>(w) || dynamic_cast<wxChoice*>(w) ||
             dynamic_cast<wxToolBar*>(w) || dynamic_cast<wxStatusBar*>(w))
      bg = control;
    w->SetBackgroundColour(bg);
    w->SetForegroundColour(text);
    if (auto* n = dynamic_cast<wxAuiNotebook*>(w)) {
      auto* art = new wxAuiDefaultTabArt;
      art->SetColour(panel);
      art->SetActiveColour(background);
      n->SetArtProvider(art);
    }
    if (auto* e = dynamic_cast<wxStyledTextCtrl*>(w)) editor(e);
    for (auto* child : w->GetChildren()) apply(child);
    w->Refresh();
  }
  void editor(wxStyledTextCtrl* e) const {
    for (int i = 0; i < 256; ++i) {
      e->StyleSetForeground(i, text);
      e->StyleSetBackground(i, background);
    }
    e->StyleSetForeground(wxSTC_STYLE_LINENUMBER, muted);
    e->StyleSetBackground(wxSTC_STYLE_LINENUMBER, panel);
    e->SetCaretForeground(text);
    e->SetSelBackground(true, selection);
    e->SetSelForeground(true, contrastText(selection));
    e->SetFoldMarginColour(true, panel);
    e->SetFoldMarginHiColour(true, panel);
    if (e->GetLexer() == wxSTC_LEX_CPP) {
      for (auto i : {wxSTC_C_WORD, wxSTC_C_WORD2, wxSTC_C_PREPROCESSOR})
        e->StyleSetForeground(i, keyword);
      for (auto i : {wxSTC_C_COMMENT, wxSTC_C_COMMENTLINE, wxSTC_C_COMMENTDOC})
        e->StyleSetForeground(i, comment);
      for (auto i : {wxSTC_C_STRING, wxSTC_C_CHARACTER, wxSTC_C_STRINGEOL})
        e->StyleSetForeground(i, string);
    } else if (e->GetLexer() == wxSTC_LEX_PYTHON) {
      e->StyleSetForeground(wxSTC_P_WORD, keyword);
      e->StyleSetForeground(wxSTC_P_COMMENTLINE, comment);
      e->StyleSetForeground(wxSTC_P_STRING, string);
      e->StyleSetForeground(wxSTC_P_CHARACTER, string);
    } else if (e->GetLexer() == wxSTC_LEX_JSON) {
      e->StyleSetForeground(wxSTC_JSON_KEYWORD, keyword);
      e->StyleSetForeground(wxSTC_JSON_STRING, string);
      e->StyleSetForeground(wxSTC_JSON_PROPERTYNAME, keyword);
    }
    e->Colourise(0, -1);
    e->Refresh();
  }
  void docking(wxAuiManager& a) const {
    auto art = a.GetArtProvider();
    art->SetColour(wxAUI_DOCKART_BACKGROUND_COLOUR, background);
    art->SetColour(wxAUI_DOCKART_SASH_COLOUR, separator);
    art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR, panel);
    art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR, text);
    art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_COLOUR, control);
    art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_TEXT_COLOUR, text);
    art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_GRADIENT_COLOUR, control);
    art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_GRADIENT_COLOUR, panel);
    art->SetColour(wxAUI_DOCKART_BORDER_COLOUR, separator);
    art->SetColour(wxAUI_DOCKART_GRIPPER_COLOUR, control);
    art->SetMetric(wxAUI_DOCKART_GRADIENT_TYPE, wxAUI_GRADIENT_NONE);
  }
};
}  // namespace wxui
