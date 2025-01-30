#include "text_editor.h"
#include "id_handler.h"
#include "utils.h"


void cpp(wxStyledTextCtrl *textEditor)
{
   // Set the lexer for C++
   textEditor->SetLexer(wxSTC_LEX_CPP);

   // Set keywords for C++
   textEditor->SetKeyWords(0,
                         "alignas alignof asm auto bool break case catch char char16_t char32_t class const constexpr "
                         "const_cast continue decltype default delete do double dynamic_cast else enum explicit export extern "
                         "false float for friend goto if inline int long mutable namespace new noexcept nullptr operator "
                         "private protected public register reinterpret_cast return short signed sizeof static static_assert "
                         "static_cast struct switch template this thread_local throw true try typedef typeid typename union "
                         "unsigned using virtual void volatile wchar_t while");


      textEditor->SetKeyWords(1,
                         "std::string std::cout");


   static wxColor green("#123b28");
   static wxColor blue("#080073");
   static wxColor red("#7d0c1a");
   static wxColor purple("#970d9e");
   static wxColor otherBlue("#0d0f78");
   // Default styles
   textEditor->StyleSetForeground(wxSTC_C_DEFAULT, *wxBLACK);
   textEditor->StyleSetForeground(wxSTC_C_COMMENT, *wxGREEN);
   textEditor->StyleSetForeground(wxSTC_C_COMMENTLINE, green);
   textEditor->StyleSetForeground(wxSTC_C_COMMENTDOC, green);
   textEditor->StyleSetForeground(wxSTC_C_NUMBER, red);
   textEditor->StyleSetForeground(wxSTC_C_WORD, purple); // Keywords
   textEditor->StyleSetForeground(wxSTC_C_STRING, red);
   textEditor->StyleSetForeground(wxSTC_C_CHARACTER, red);
   textEditor->StyleSetForeground(wxSTC_C_OPERATOR, *wxBLACK);
   textEditor->StyleSetForeground(wxSTC_C_IDENTIFIER, *wxBLACK);
   textEditor->StyleSetForeground(wxSTC_C_PREPROCESSOR, otherBlue);

   // Fonts
   textEditor->StyleSetFont(wxSTC_C_DEFAULT, wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
   textEditor->StyleSetFont(wxSTC_C_PREPROCESSOR, wxFont(13, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

   // Enable line numbers
   textEditor->SetMarginType(0, wxSTC_MARGIN_NUMBER);
   textEditor->SetMarginWidth(0, 50);
   textEditor->SetCaretPeriod(500);
   textEditor->StyleSetBackground(wxSTC_STYLE_LINENUMBER, wxColour(230, 230, 230)); // Light gray background
   textEditor->StyleSetForeground(wxSTC_STYLE_LINENUMBER, wxColour(0, 0, 0)); 
   textEditor->SetMarginBackground(0, *wxRED);
   textEditor->SetCaretForeground(wxColour(255, 0, 0)); // Red caret
   textEditor->SetCaretWidth(100);
}


void set_style_for_default_language(const wxString& languange, wxStyledTextCtrl *ctrl){
   cpp(ctrl);
}
text_editor::~text_editor(){
   std::cout << "Destroy one\n";
}


text_editor::text_editor(wxWindow *parent) : wxPanel(parent, wxID_ANY)
{
    internal_id = get_next_id();
    fromfile = false;
    changed = false;
    ///SetBackgroundColour(wxColour(0, 0, 255, 255));
    textEditor = new wxStyledTextCtrl(this, internal_id);
    textEditor->SetMinSize(wxSize(800, 600));
    textEditor->SetBackgroundColour(wxColour(255, 30, 30));
    textEditor->SetForegroundColour(wxColour(255, 255, 255));
    textEditor->SetBackgroundColour("#000000FF");
   /// textEditor->StyleSetBackground(wxSTC_STYLE_DEFAULT, wxColor(255,0,0,255)); //before that you need define some style for all content
   /// textEditor->StyleSetBackground(wxSTC_STYLE_DEFAULT, wxColor(255,0,0,255)); //before that you need define some style for all content
   // textEditor->SetSelBackground(wxColour(255,255,255,255)); //before that you need to select all content
    textEditor->StyleSetSize(wxSTC_STYLE_DEFAULT, 12);
    wxBoxSizer *Ssizer = new wxBoxSizer(wxVERTICAL);
    Ssizer->Add(textEditor, 1, wxEXPAND);
    set_style_for_default_language("cpp",textEditor);
    SetSizer(Ssizer);


   Bind(wxEVT_STC_CHARADDED, &text_editor::on_char_added, this);
   //Bind(wxEVT_STC_CHANGE, &text_editor::on_char_added, this);
   //Bind(wxEVT_STC_MODIFIED, &text_editor::on_char_added, this);
}

void text_editor::set_filepath(const wxString& path){
   fromfile = true;
   this->path=path;  
}

   void text_editor::set_font(const wxFont& font){
      if(textEditor)
        textEditor->SetFont(font);
        textEditor->StyleSetFont(wxSTC_C_DEFAULT, font);
        ///textEditor->Layout();
        textEditor->StyleClearAll();
   }

void text_editor::set_text(const wxString &text)
{
    if (textEditor)
    {
        textEditor->SetText(text);
    }
}

wxString text_editor::get_text()
{
   if (textEditor)
      return textEditor->GetText();
   return wxEmptyString;
}

void text_editor::on_char_added(wxStyledTextEvent &event)
{
  /// std::cout <<"EVent triggerd\n";
   changed = true;
}
