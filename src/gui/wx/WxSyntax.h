#pragma once
#include <chrono>
#include <map>
#include <set>

#include "WxTheme.h"
#include "dodev/editor/SyntaxRules.h"
namespace wxui {
namespace syn = dodev::syntax;
using dodev::Json;
struct LexerSpec {
  int lexer = wxSTC_LEX_NULL;
  std::map<int, std::string> tokens;
};
inline LexerSpec lexerSpec(const std::string& lang) {
  LexerSpec s;
  auto add = [&](const char* category, std::initializer_list<int> ids) {
    for (int id : ids) s.tokens[id] = category;
  };
  if (lang == "c" || lang == "cpp" || lang == "java" || lang == "kotlin" ||
      lang == "csharp" || lang == "javascript" || lang == "typescript" ||
      lang == "go") {
    s.lexer = wxSTC_LEX_CPP;
    add("keyword", {wxSTC_C_WORD, wxSTC_C_WORD2});
    add("comment", {wxSTC_C_COMMENT, wxSTC_C_COMMENTLINE, wxSTC_C_COMMENTDOC,
                    wxSTC_C_COMMENTLINEDOC, wxSTC_C_COMMENTDOCKEYWORD,
                    wxSTC_C_COMMENTDOCKEYWORDERROR});
    add("string", {wxSTC_C_STRING, wxSTC_C_CHARACTER, wxSTC_C_STRINGEOL,
                   wxSTC_C_VERBATIM, wxSTC_C_REGEX, wxSTC_C_STRINGRAW});
    add("number", {wxSTC_C_NUMBER});
    add("operator", {wxSTC_C_OPERATOR});
    add("identifier", {wxSTC_C_IDENTIFIER});
    add("preprocessor", {wxSTC_C_PREPROCESSOR});
  } else if (lang == "python") {
    s.lexer = wxSTC_LEX_PYTHON;
    add("keyword", {wxSTC_P_WORD, wxSTC_P_WORD2});
    add("comment", {wxSTC_P_COMMENTLINE, wxSTC_P_COMMENTBLOCK});
    add("string", {wxSTC_P_STRING, wxSTC_P_CHARACTER, wxSTC_P_TRIPLE,
                   wxSTC_P_TRIPLEDOUBLE, wxSTC_P_STRINGEOL});
    add("number", {wxSTC_P_NUMBER});
    add("operator", {wxSTC_P_OPERATOR});
    add("identifier", {wxSTC_P_IDENTIFIER});
    add("type", {wxSTC_P_CLASSNAME});
    add("function", {wxSTC_P_DEFNAME});
    add("preprocessor", {wxSTC_P_DECORATOR});
  } else if (lang == "json") {
    s.lexer = wxSTC_LEX_JSON;
    add("keyword", {wxSTC_JSON_KEYWORD, wxSTC_JSON_LDKEYWORD});
    add("property", {wxSTC_JSON_PROPERTYNAME});
    add("string", {wxSTC_JSON_STRING, wxSTC_JSON_ESCAPESEQUENCE});
    add("number", {wxSTC_JSON_NUMBER});
    add("operator", {wxSTC_JSON_OPERATOR});
    add("comment", {wxSTC_JSON_LINECOMMENT, wxSTC_JSON_BLOCKCOMMENT});
    add("error", {wxSTC_JSON_ERROR, wxSTC_JSON_STRINGEOL});
  } else if (lang == "shell") {
    s.lexer = wxSTC_LEX_BASH;
    add("keyword", {wxSTC_SH_WORD});
    add("comment", {wxSTC_SH_COMMENTLINE});
    add("number", {wxSTC_SH_NUMBER});
    add("string", {wxSTC_SH_STRING, wxSTC_SH_CHARACTER, wxSTC_SH_BACKTICKS,
                   wxSTC_SH_HERE_Q});
    add("operator", {wxSTC_SH_OPERATOR});
    add("identifier", {wxSTC_SH_IDENTIFIER, wxSTC_SH_SCALAR, wxSTC_SH_PARAM});
    add("error", {wxSTC_SH_ERROR});
  } else if (lang == "sql") {
    s.lexer = wxSTC_LEX_SQL;
    add("keyword", {wxSTC_SQL_WORD, wxSTC_SQL_WORD2});
    add("comment",
        {wxSTC_SQL_COMMENT, wxSTC_SQL_COMMENTLINE, wxSTC_SQL_COMMENTDOC});
    add("string", {wxSTC_SQL_STRING, wxSTC_SQL_CHARACTER});
    add("number", {wxSTC_SQL_NUMBER});
    add("operator", {wxSTC_SQL_OPERATOR});
    add("identifier", {wxSTC_SQL_IDENTIFIER});
  } else if (lang == "xml" || lang == "html") {
    s.lexer = lang == "xml" ? wxSTC_LEX_XML : wxSTC_LEX_HTML;
    add("tag", {wxSTC_H_TAG, wxSTC_H_TAGUNKNOWN, wxSTC_H_TAGEND});
    add("property", {wxSTC_H_ATTRIBUTE, wxSTC_H_ATTRIBUTEUNKNOWN});
    add("string", {wxSTC_H_DOUBLESTRING, wxSTC_H_SINGLESTRING, wxSTC_H_VALUE});
    add("number", {wxSTC_H_NUMBER});
    add("comment", {wxSTC_H_COMMENT});
    add("preprocessor", {wxSTC_H_XMLSTART, wxSTC_H_XMLEND});
    add("keyword", {wxSTC_H_ENTITY});
  } else if (lang == "css") {
    s.lexer = wxSTC_LEX_CSS;
    add("tag", {wxSTC_CSS_TAG});
    add("identifier", {wxSTC_CSS_CLASS, wxSTC_CSS_ID, wxSTC_CSS_PSEUDOCLASS});
    add("property", {wxSTC_CSS_IDENTIFIER, wxSTC_CSS_UNKNOWN_IDENTIFIER});
    add("operator", {wxSTC_CSS_OPERATOR});
    add("comment", {wxSTC_CSS_COMMENT});
    add("string",
        {wxSTC_CSS_DOUBLESTRING, wxSTC_CSS_SINGLESTRING, wxSTC_CSS_VALUE});
    add("keyword", {wxSTC_CSS_DIRECTIVE, wxSTC_CSS_IMPORTANT});
  } else if (lang == "yaml") {
    s.lexer = wxSTC_LEX_YAML;
    add("keyword", {wxSTC_YAML_KEYWORD});
    add("comment", {wxSTC_YAML_COMMENT});
    add("number", {wxSTC_YAML_NUMBER});
    add("property", {wxSTC_YAML_IDENTIFIER});
    add("operator", {wxSTC_YAML_OPERATOR});
    add("error", {wxSTC_YAML_ERROR});
    add("preprocessor", {wxSTC_YAML_REFERENCE, wxSTC_YAML_DOCUMENT});
    add("string", {wxSTC_YAML_TEXT});
  } else if (lang == "cmake") {
    s.lexer = wxSTC_LEX_CMAKE;
    add("comment", {wxSTC_CMAKE_COMMENT});
    add("string",
        {wxSTC_CMAKE_STRINGDQ, wxSTC_CMAKE_STRINGLQ, wxSTC_CMAKE_STRINGRQ});
    add("keyword",
        {wxSTC_CMAKE_COMMANDS, wxSTC_CMAKE_MACRODEF, wxSTC_CMAKE_WHILEDEF,
         wxSTC_CMAKE_FOREACHDEF, wxSTC_CMAKE_IFDEFINEDEF});
    add("identifier",
        {wxSTC_CMAKE_VARIABLE, wxSTC_CMAKE_USERDEFINED, wxSTC_CMAKE_STRINGVAR});
    add("number", {wxSTC_CMAKE_NUMBER});
  } else if (lang == "make") {
    s.lexer = wxSTC_LEX_MAKEFILE;
    add("comment", {wxSTC_MAKE_COMMENT});
    add("preprocessor", {wxSTC_MAKE_PREPROCESSOR});
    add("identifier", {wxSTC_MAKE_IDENTIFIER});
    add("operator", {wxSTC_MAKE_OPERATOR});
    add("function", {wxSTC_MAKE_TARGET});
  } else if (lang == "lua") {
    s.lexer = wxSTC_LEX_LUA;
    add("keyword",
        {wxSTC_LUA_WORD, wxSTC_LUA_WORD2, wxSTC_LUA_WORD3, wxSTC_LUA_WORD4});
    add("comment",
        {wxSTC_LUA_COMMENT, wxSTC_LUA_COMMENTLINE, wxSTC_LUA_COMMENTDOC});
    add("string", {wxSTC_LUA_STRING, wxSTC_LUA_CHARACTER,
                   wxSTC_LUA_LITERALSTRING, wxSTC_LUA_STRINGEOL});
    add("number", {wxSTC_LUA_NUMBER});
    add("operator", {wxSTC_LUA_OPERATOR});
    add("identifier", {wxSTC_LUA_IDENTIFIER});
  } else if (lang == "rust") {
    s.lexer = wxSTC_LEX_RUST;
    add("keyword", {wxSTC_RUST_WORD, wxSTC_RUST_WORD2, wxSTC_RUST_WORD3});
    add("comment", {wxSTC_RUST_COMMENTBLOCK, wxSTC_RUST_COMMENTLINE,
                    wxSTC_RUST_COMMENTBLOCKDOC, wxSTC_RUST_COMMENTLINEDOC});
    add("string",
        {wxSTC_RUST_STRING, wxSTC_RUST_STRINGR, wxSTC_RUST_CHARACTER});
    add("number", {wxSTC_RUST_NUMBER});
    add("operator", {wxSTC_RUST_OPERATOR});
    add("identifier", {wxSTC_RUST_IDENTIFIER});
  } else if (lang == "markdown") {
    s.lexer = wxSTC_LEX_MARKDOWN;
    add("keyword", {wxSTC_MARKDOWN_HEADER1, wxSTC_MARKDOWN_HEADER2,
                    wxSTC_MARKDOWN_HEADER3, wxSTC_MARKDOWN_HEADER4,
                    wxSTC_MARKDOWN_HEADER5, wxSTC_MARKDOWN_HEADER6});
    add("string",
        {wxSTC_MARKDOWN_CODE, wxSTC_MARKDOWN_CODE2, wxSTC_MARKDOWN_CODEBK});
    add("property", {wxSTC_MARKDOWN_LINK});
    add("operator", {wxSTC_MARKDOWN_STRONG1, wxSTC_MARKDOWN_STRONG2,
                     wxSTC_MARKDOWN_EM1, wxSTC_MARKDOWN_EM2});
  }
  return s;
}
inline void nativeLexer(wxStyledTextCtrl* e, const std::string& lang) {
  auto spec = lexerSpec(lang);
  e->SetLexer(spec.lexer);
  for (int i = 0; i < 9; ++i) e->SetKeyWords(i, wxEmptyString);
  e->SetKeyWords(0, wxString::FromUTF8(syn::language(lang).keywords));
  e->SetProperty("fold", "1");
  e->SetProperty("lexer.json.allow.comments", "1");
}
inline wxColour tokenColor(const Theme& t, const std::string& c) {
  if (c == "keyword" || c == "type" || c == "preprocessor" || c == "property" ||
      c == "tag" || c == "function")
    return t.keyword;
  if (c == "comment") return t.comment;
  if (c == "string" || c == "number") return t.string;
  if (c == "error") return wxColour(220, 80, 80);
  return t.text;
}
// Native lexer remains active for category-only rules. Exact keyword styles use
// an invisible native lexer and a container-style visible document, avoiding
// corruption of native lexer state by custom style IDs. Text and undo history
// are never rewritten.
class SyntaxEditor : public wxStyledTextCtrl {
  std::string lang_ = "text", theme_;
  Json rules_;
  Theme palette_ = Theme::dark();
  wxFont font_;
  LexerSpec spec_;
  bool dirty_ = true, overlay_ = false;
  std::chrono::steady_clock::time_point changed_{};
  std::map<std::string, int> custom_;
  std::set<int> strikes_;
  std::map<int, std::string> categories_;
  static constexpr int strikeIndicator = 24, maxBytes = 1024 * 1024,
                       firstCustom = 160, lastCustom = 255;
  void style(int id, const std::string& category,
             const std::string& word = {}) {
    auto r = syn::resolve(rules_, theme_, lang_, category, word);
    StyleSetFont(id, font_);
    StyleSetForeground(id, tokenColor(palette_, category));
    StyleSetBackground(id, palette_.background);
    if (r["foreground"].isString())
      StyleSetForeground(id, Theme::color(r["foreground"].string()));
    if (r["background"].isString())
      StyleSetBackground(id, Theme::color(r["background"].string()));
    if (r["fontSize"].isNumber()) StyleSetSize(id, r["fontSize"].integer());
    if (r["fontWeight"].isNumber())
      StyleSetWeight(id, r["fontWeight"].integer());
    StyleSetItalic(id, r["italic"].boolean());
    StyleSetUnderline(id, r["underline"].boolean());
    if (r["strike"].boolean()) strikes_.insert(id);
  }
  void setupStyles() {
    strikes_.clear();
    categories_ = spec_.tokens;
    for (int i = 0; i < 160; ++i) {
      if (i >= 32 && i <= 39) continue;
      auto it = categories_.find(i);
      style(i, it == categories_.end() ? "default" : it->second);
    }
    custom_.clear();
    int id = firstCustom;
    for (const auto& word : syn::keywordOverrides(rules_, theme_, lang_)) {
      if (id > lastCustom) break;
      custom_[word] = id;
      style(id, "keyword", word);
      ++id;
    }
    IndicatorSetStyle(strikeIndicator, wxSTC_INDIC_STRIKE);
    IndicatorSetForeground(strikeIndicator, palette_.text);
  }
  static bool wordChar(unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '$' || c >= 128;
  }
  void markStrikes() {
    SetIndicatorCurrent(strikeIndicator);
    IndicatorClearRange(0, GetLength());
    if (strikes_.empty() || GetLength() > maxBytes) return;
    for (int i = 0, n = GetLength(); i < n;) {
      int st = GetStyleAt(i) & 255;
      int end = i + 1;
      while (end < n && (GetStyleAt(end) & 255) == st) ++end;
      if (strikes_.count(st)) IndicatorFillRange(i, end - i);
      i = end;
    }
  }
  void render() {
    dirty_ = false;
    bool useOverlay = !custom_.empty() && GetLength() <= maxBytes;
    if (useOverlay != overlay_) {
      overlay_ = useOverlay;
      if (overlay_)
        SetLexer(wxSTC_LEX_CONTAINER);
      else
        nativeLexer(this, lang_);
      ClearDocumentStyle();
    }
    if (overlay_) {
      wxStyledTextCtrl shadow(GetParent(), wxID_ANY, wxDefaultPosition,
                              wxSize(1, 1));
      shadow.Hide();
      shadow.SetCodePage(wxSTC_CP_UTF8);
      nativeLexer(&shadow, lang_);
      shadow.SetText(GetText());
      shadow.Colourise(0, -1);
      auto raw = GetText().ToUTF8();
      const char* bytes = raw.data();
      int n = GetLength();
      if (!bytes || raw.length() != static_cast<size_t>(n)) return;
      StartStyling(0);
      for (int i = 0; i < n;) {
        int st = shadow.GetStyleAt(i) & 255;
        int end = i + 1;
        while (end < n && (shadow.GetStyleAt(end) & 255) == st) ++end;
        auto cat = categories_.find(st);
        if (cat != categories_.end() && cat->second == "keyword") {
          for (int p = i; p < end;) {
            int q = p + 1;
            if (wordChar(static_cast<unsigned char>(bytes[p]))) {
              while (q < end && wordChar(static_cast<unsigned char>(bytes[q])))
                ++q;
              std::string word(bytes + p, bytes + q);
              if (lang_ == "sql" || lang_ == "cmake") word = syn::lower(word);
              auto found = custom_.find(word);
              SetStyling(q - p, found == custom_.end() ? st : found->second);
            } else
              SetStyling(1, st);
            p = q;
          }
        } else
          SetStyling(end - i, st);
        i = end;
      }
      for (int i = 0; i < GetLineCount(); ++i)
        SetFoldLevel(i, shadow.GetFoldLevel(i));
    } else
      Colourise(0, -1);
    markStrikes();
    Refresh();
  }

 public:
  SyntaxEditor(wxWindow* parent, wxWindowID id) : wxStyledTextCtrl(parent, id) {
    SetCodePage(wxSTC_CP_UTF8);
    Bind(wxEVT_STC_MODIFIED, [this](wxStyledTextEvent& e) {
      if (e.GetModificationType() &
          (wxSTC_MOD_INSERTTEXT | wxSTC_MOD_DELETETEXT)) {
        dirty_ = true;
        changed_ = std::chrono::steady_clock::now();
      }
      e.Skip();
    });
  }
  void setLanguage(const std::string& lang) {
    lang_ = lang;
    spec_ = lexerSpec(lang);
    overlay_ = false;
    nativeLexer(this, lang);
    dirty_ = true;
  }
  const std::string& languageId() const { return lang_; }
  void applySyntax(const Theme& p, const std::string& name, const Json& r,
                   const wxFont& font) {
    palette_ = p;
    theme_ = name;
    rules_ = r;
    font_ = font;
    p.editor(this);
    setupStyles();
    render();
  }
  void refreshSyntax() { render(); }
  void pollSyntax() {
    if (dirty_ && std::chrono::steady_clock::now() - changed_ >
                      std::chrono::milliseconds(200)) {
      if (!custom_.empty() || !strikes_.empty() || overlay_)
        render();
      else
        dirty_ = false;
    }
  }
};
}  // namespace wxui
