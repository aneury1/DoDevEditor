// Optional native rendering regression test; run under a desktop or Xvfb.
#include <iostream>
#include <stdexcept>

#include "WxSyntax.h"
class SyntaxTestApp : public wxApp {
 public:
  bool OnInit() override { return true; }
};
wxIMPLEMENT_APP_NO_MAIN(SyntaxTestApp);
static void check(bool condition, const char* text) {
  if (!condition) throw std::runtime_error(text);
}
int main(int argc, char** argv) {
  if (!wxEntryStart(argc, argv)) {
    std::cerr << "wxEntryStart failed (display required)\n";
    return 1;
  }
  if (!wxTheApp->CallOnInit()) {
    wxEntryCleanup();
    return 1;
  }
  int result = 0;
  auto* frame = new wxFrame(nullptr, wxID_ANY, "Syntax test");
  try {
    using dodev::Json;
    namespace syn = dodev::syntax;
    auto* e = new wxui::SyntaxEditor(frame, wxID_ANY);
    auto font = wxFont(wxFontInfo(14).Family(wxFONTFAMILY_TELETYPE));
    auto dark = wxui::Theme::dark();
    e->setLanguage("cpp");
    const std::string code =
        "int main() { return 42; } // return\nconst char* s = \"return\";\n";
    e->SetText(wxString::FromUTF8(code.c_str()));
    e->SetSavePoint();
    Json rules = Json::object();
    syn::setRule(rules, "", "cpp", "keyword", "",
                 Json::Object{{"foreground", "#00ff00"}});
    syn::setRule(rules, "Dark", "cpp", "keyword", "return",
                 Json::Object{{"fontSize", 20},
                              {"fontWeight", 700},
                              {"foreground", "#ff0000"},
                              {"background", "#112233"},
                              {"strike", true}});
    e->applySyntax(dark, "Dark", rules, font);
    const int at = static_cast<int>(code.find("return"));
    const int comment = static_cast<int>(code.find("return", at + 6));
    const int quoted = static_cast<int>(code.rfind("return"));
    const int style = e->GetStyleAt(at) & 255;
    check(style >= 160, "keyword-specific style");
    check(e->StyleGetSize(style) == 20, "keyword font size");
    check(e->StyleGetWeight(style) == 700, "keyword font weight");
    check(e->StyleGetForeground(style) == wxColour("#ff0000"),
          "keyword foreground");
    check(e->StyleGetBackground(style) == wxColour("#112233"),
          "keyword background");
    check(e->IndicatorValueAt(24, at) != 0, "strikethrough indicator");
    check((e->GetStyleAt(comment) & 255) != style &&
              (e->GetStyleAt(quoted) & 255) != style,
          "comment/string isolation");
    check(!e->GetModify() && e->GetText() == wxString::FromUTF8(code.c_str()),
          "source unchanged");
    e->applySyntax(dark, "Other", rules, font);
    check((e->GetStyleAt(at) & 255) == wxSTC_C_WORD, "native lexer restored");
    check(e->StyleGetForeground(wxSTC_C_WORD) == wxColour("#00ff00"),
          "general language category retained");
    check(e->IndicatorValueAt(24, at) == 0, "strike cleared");
    std::cout << "Native syntax rendering checks passed\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    result = 1;
  }
  delete frame;
  wxTheApp->OnExit();
  wxEntryCleanup();
  return result;
}
