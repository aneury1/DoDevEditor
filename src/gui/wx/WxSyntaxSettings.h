#pragma once
#include <wx/clrpicker.h>
#include <wx/spinctrl.h>

#include <sstream>

#include "WxSyntax.h"
namespace wxui {
class SyntaxSettingsPanel : public wxPanel {
  struct Key {
    std::string theme, lang, category, word;
  };
  dodev::ThemeManager& themes_;
  std::string activeTheme_;
  wxFont font_;
  Json draft_;
  Key key_;
  bool loading_ = false;
  wxChoice *scope_, *language_, *token_, *word_, *weight_, *italic_,
      *underline_, *strike_;
  wxCheckBox *foregroundOn_, *backgroundOn_, *sizeOn_;
  wxColourPickerCtrl *foreground_, *background_;
  wxSpinCtrl* size_;
  SyntaxEditor* preview_;
  wxStaticText* detail_;
  static wxString w(const std::string& v) {
    return wxString::FromUTF8(v.data(), v.size());
  }
  static std::string s(const wxString& v) {
    auto b = v.ToUTF8();
    return b.data() ? std::string(b.data(), b.length()) : std::string();
  }
  static std::string hex(const wxColour& c) {
    return s(c.GetAsString(wxC2S_HTML_SYNTAX));
  }
  static int tri(const Json& v) {
    return v.isBool() ? (v.boolean() ? 2 : 1) : 0;
  }
  const std::string previewTheme() const {
    return key_.theme.empty() ? activeTheme_ : key_.theme;
  }
  Theme selectedPalette() const {
    auto t = themes_.find(previewTheme());
    return t ? Theme::from(*t) : Theme::dark();
  }
  std::string previewLang() const {
    return key_.lang == "*" ? "cpp" : key_.lang;
  }
  void commit() {
    if (loading_) return;
    Json r = Json::object();
    if (foregroundOn_->GetValue())
      r["foreground"] = hex(foreground_->GetColour());
    if (backgroundOn_->GetValue())
      r["background"] = hex(background_->GetColour());
    if (sizeOn_->GetValue()) r["fontSize"] = size_->GetValue();
    if (weight_->GetSelection() > 0)
      r["fontWeight"] = weight_->GetSelection() * 100;
    for (auto x : {std::make_pair("italic", italic_),
                   std::make_pair("underline", underline_),
                   std::make_pair("strike", strike_)})
      if (x.second->GetSelection() > 0)
        r[x.first] = x.second->GetSelection() == 2;
    syn::setRule(draft_, key_.theme, key_.lang, key_.category, key_.word, r);
  }
  void read() {
    loading_ = true;
    auto r = syn::rule(draft_, key_.theme, key_.lang, key_.category, key_.word);
    auto p = selectedPalette();
    auto inherited = syn::resolve(draft_, previewTheme(), previewLang(),
                                  key_.category, key_.word);
    foregroundOn_->SetValue(r["foreground"].isString());
    backgroundOn_->SetValue(r["background"].isString());
    sizeOn_->SetValue(r["fontSize"].isNumber());
    foreground_->SetColour(inherited["foreground"].isString()
                               ? Theme::color(inherited["foreground"].string())
                               : tokenColor(p, key_.category));
    background_->SetColour(inherited["background"].isString()
                               ? Theme::color(inherited["background"].string())
                               : p.background);
    size_->SetValue(inherited["fontSize"].integer(font_.GetPointSize()));
    weight_->SetSelection(
        r["fontWeight"].isNumber()
            ? std::clamp(r["fontWeight"].integer() / 100, 1, 9)
            : 0);
    italic_->SetSelection(tri(r["italic"]));
    underline_->SetSelection(tri(r["underline"]));
    strike_->SetSelection(tri(r["strike"]));
    enableFields();
    loading_ = false;
  }
  void enableFields() {
    foreground_->Enable(foregroundOn_->GetValue());
    background_->Enable(backgroundOn_->GetValue());
    size_->Enable(sizeOn_->GetValue());
  }
  void updatePreview() {
    auto lang = previewLang();
    auto p = selectedPalette();
    preview_->SetReadOnly(false);
    preview_->setLanguage(lang);
    std::string text = syn::language(lang).sample;
    if (!key_.word.empty()) text += "\n" + key_.word + "\n";
    preview_->SetText(w(text));
    preview_->SetReadOnly(true);
    preview_->applySyntax(p, previewTheme(), draft_, font_);
    detail_->SetLabel(w("Preview: " + previewTheme() + " / " +
                        syn::language(lang).label +
                        " — unchecked properties inherit."));
  }
  void switchKey() {
    if (loading_) return;
    commit();
    loading_ = true;
    auto oldWord = key_.word;
    auto newLang =
        language_->GetSelection() == 0
            ? std::string("*")
            : std::string(
                  syn::languages()
                      .at(static_cast<size_t>(language_->GetSelection() - 1))
                      .id);
    bool langChanged = newLang != key_.lang;
    key_.theme = scope_->GetSelection() == 0 ? std::string()
                                             : s(scope_->GetStringSelection());
    key_.lang = newLang;
    if (langChanged || word_->GetCount() == 0) {
      word_->Clear();
      std::istringstream words(syn::language(newLang).keywords);
      std::string word;
      std::vector<std::string> list;
      while (words >> word) list.push_back(word);
      std::sort(list.begin(), list.end());
      list.erase(std::unique(list.begin(), list.end()), list.end());
      for (auto& v : list) word_->Append(w(v));
      if (word_->GetCount()) word_->SetSelection(0);
      if (!oldWord.empty()) word_->SetStringSelection(w(oldWord));
    }
    bool exact =
        token_->GetSelection() == static_cast<int>(syn::categories().size());
    if (exact && (!word_->GetCount() || newLang == "*")) {
      token_->SetSelection(1);
      exact = false;
    }
    word_->Enable(exact);
    key_.category =
        exact
            ? "keyword"
            : syn::categories().at(static_cast<size_t>(token_->GetSelection()));
    key_.word = exact ? s(word_->GetStringSelection()) : std::string();
    loading_ = false;
    read();
    updatePreview();
  }

 public:
  SyntaxSettingsPanel(wxWindow* parent, dodev::ThemeManager& themes,
                      const std::string& active, const Json& initial,
                      const wxFont& font)
      : wxPanel(parent),
        themes_(themes),
        activeTheme_(active),
        font_(font),
        draft_(initial.isObject() ? initial : Json::object()) {
    auto outer = new wxBoxSizer(wxVERTICAL);
    SetSizer(outer);
    outer->Add(
        new wxStaticText(this, wxID_ANY,
                         "Syntax styles inherit the selected theme. Enable "
                         "only the properties you want to override."),
        0, wxEXPAND | wxALL, 6);
    auto g = new wxFlexGridSizer(2, 5, 10);
    g->AddGrowableCol(1);
    outer->Add(g, 0, wxEXPAND | wxALL, 6);
    auto add = [&](const char* label, wxWindow* control) {
      g->Add(new wxStaticText(this, wxID_ANY, label), 0,
             wxALIGN_CENTER_VERTICAL);
      g->Add(control, 1, wxEXPAND);
    };
    scope_ = new wxChoice(this, wxID_ANY);
    scope_->Append("All themes (general)");
    for (auto& n : themes.names()) scope_->Append(w(n));
    scope_->SetSelection(0);
    add("Theme scope", scope_);
    language_ = new wxChoice(this, wxID_ANY);
    language_->Append("All languages (general)");
    for (auto& l : syn::languages()) language_->Append(w(l.label));
    language_->SetSelection(0);
    add("Language", language_);
    token_ = new wxChoice(this, wxID_ANY);
    for (auto& c : syn::categories()) token_->Append(w(c));
    token_->Append("Exact keyword (language-specific)");
    token_->SetSelection(1);
    add("Token category / rule", token_);
    word_ = new wxChoice(this, wxID_ANY);
    add("Exact keyword", word_);
    word_->Disable();
    auto colors = [&](const char* label, wxCheckBox*& on,
                      wxColourPickerCtrl*& picker) {
      g->Add(new wxStaticText(this, wxID_ANY, label), 0,
             wxALIGN_CENTER_VERTICAL);
      auto row = new wxBoxSizer(wxHORIZONTAL);
      on = new wxCheckBox(this, wxID_ANY, "Override");
      picker = new wxColourPickerCtrl(this, wxID_ANY, *wxBLACK);
      row->Add(on, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
      row->Add(picker, 1, wxEXPAND);
      g->Add(row, 1, wxEXPAND);
    };
    colors("Foreground", foregroundOn_, foreground_);
    colors("Background", backgroundOn_, background_);
    g->Add(new wxStaticText(this, wxID_ANY, "Font size (pt)"), 0,
           wxALIGN_CENTER_VERTICAL);
    auto sizeRow = new wxBoxSizer(wxHORIZONTAL);
    sizeOn_ = new wxCheckBox(this, wxID_ANY, "Override");
    size_ = new wxSpinCtrl(this, wxID_ANY);
    size_->SetRange(8, 64);
    sizeRow->Add(sizeOn_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    sizeRow->Add(size_, 1, wxEXPAND);
    g->Add(sizeRow, 1, wxEXPAND);
    weight_ = new wxChoice(this, wxID_ANY);
    for (auto v : {"Inherit", "100 Thin", "200 Extra light", "300 Light",
                   "400 Normal", "500 Medium", "600 Semi-bold", "700 Bold",
                   "800 Extra bold", "900 Black"})
      weight_->Append(v);
    add("Font weight", weight_);
    auto toggle = [&](const char* label) {
      auto c = new wxChoice(this, wxID_ANY);
      c->Append("Inherit");
      c->Append("Off");
      c->Append("On");
      add(label, c);
      return c;
    };
    italic_ = toggle("Italic");
    underline_ = toggle("Underline");
    strike_ = toggle("Strikethrough");
    auto row = new wxBoxSizer(wxHORIZONTAL);
    outer->Add(row, 0, wxEXPAND | wxALL, 4);
    auto btn = [&](const char* label, auto callback) {
      auto b = new wxButton(this, wxID_ANY, label);
      row->Add(b, 0, wxALL, 2);
      b->Bind(wxEVT_BUTTON, [callback](wxCommandEvent&) { callback(); });
    };
    btn("Preview rule", [this] {
      commit();
      updatePreview();
    });
    btn("Reset rule", [this] {
      syn::setRule(draft_, key_.theme, key_.lang, key_.category, key_.word,
                   Json::object());
      read();
      updatePreview();
    });
    btn("Reset all syntax", [this] {
      if (wxMessageBox("Remove all syntax overrides in this settings draft?",
                       "Syntax Highlighting", wxYES_NO | wxICON_QUESTION,
                       this) == wxYES)
        resetAll();
    });
    detail_ = new wxStaticText(this, wxID_ANY, "");
    outer->Add(detail_, 0, wxEXPAND | wxALL, 5);
    preview_ = new SyntaxEditor(this, wxID_ANY);
    preview_->SetMinSize(wxSize(-1, 150));
    preview_->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    preview_->SetMarginWidth(0, 40);
    outer->Add(preview_, 1, wxEXPAND | wxALL, 5);
    key_ = {"", "*", "keyword", ""};
    read();
    updatePreview();
    for (auto c : {scope_, language_, token_, word_})
      c->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { switchKey(); });
    for (auto c : {foregroundOn_, backgroundOn_, sizeOn_})
      c->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        enableFields();
        commit();
        updatePreview();
      });
    for (auto c : {weight_, italic_, underline_, strike_})
      c->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        commit();
        updatePreview();
      });
    for (auto c : {foreground_, background_})
      c->Bind(wxEVT_COLOURPICKER_CHANGED, [this](wxColourPickerEvent&) {
        commit();
        updatePreview();
      });
    size_->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent&) {
      commit();
      updatePreview();
    });
  }
  Json value() {
    commit();
    return draft_;
  }
  void resetAll() {
    draft_ = Json::object();
    read();
    updatePreview();
  }
  void refreshPreview() {
    commit();
    updatePreview();
  }
  void setPreviewTheme(const std::string& name) {
    commit();
    activeTheme_ = name;
    read();
    updatePreview();
  }
};
}  // namespace wxui
