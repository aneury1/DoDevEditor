#include "text_editor.h"
#include <iostream>
#include <wx/wx.h>
#include <wx/stc/stc.h>
text_editor::text_editor(wxWindow *parent, int id) : wxPanel(parent, id)
{
   changed = false;
   internal_editor_id = get_next_id();
   textEditor = new wxStyledTextCtrl(this, internal_editor_id);
   textEditor->SetMinSize(wxSize(800, 600));
   textEditor->SetBackgroundColour(wxColour(255, 30, 30));
   textEditor->SetForegroundColour(wxColour(255, 255, 255));
   textEditor->SetBackgroundColour("#FFFFFF");
   textEditor->StyleSetSize(wxSTC_STYLE_DEFAULT, normal_font_size);
   //textEditor->StyleClearAll();
   //textEditor->StyleSetBackground(wxSTC_STYLE_DEFAULT, (255, 0, 0));
   //textEditor->StyleSetForeground(wxSTC_STYLE_DEFAULT, (255, 244, 255));
   //textEditor->StyleClearAll();
   configure_cpp_style();
   std::cout << "panel id=" << id << "\n";
   wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
   sizer->Add(textEditor, 1, wxEXPAND);
   this->SetSizer(sizer);
   Bind(wxEVT_PAINT, &text_editor::on_paint, this);
   Bind(wxEVT_STC_CHARADDED, &text_editor::on_char_added, this);
   Bind(wxEVT_STC_CHANGE, &text_editor::on_char_added, this);
   Bind(wxEVT_STC_MODIFIED, &text_editor::on_char_added, this);
}

text_editor::~text_editor()
{
}

void text_editor::set_text(const wxString &str)
{
   if (str.length() > 0 && textEditor)
      textEditor->SetText(str);
}

wxString text_editor::get_text()
{
   if (textEditor)
      return textEditor->GetText();
   return wxEmptyString;
}

bool text_editor::load_text_file(const wxString &path)
{
   wxFile file(path);
   wxString content;
   if (file.IsOpened())
   {
      file.ReadAll(&content);
      textEditor->SetText(content);
      filepath = path;
      file.Close();
      return true;
   }
   return false;
}

void text_editor::increase_font_size_by_one()
{
   if (textEditor)
   {
      textEditor->StyleSetSize(wxSTC_STYLE_DEFAULT, ++normal_font_size);
      wxFont font = textEditor->GetFont();
      font.SetPointSize(font.GetPointSize() + 1);
      textEditor->SetFont(font);
   }
}
void text_editor::decrease_font_size_by_one()
{
   if (textEditor && normal_font_size >= 9)
   {
      textEditor->StyleSetSize(wxSTC_STYLE_DEFAULT, --normal_font_size);
      wxFont font = textEditor->GetFont();
      font.SetPointSize(font.GetPointSize() + 1);
      textEditor->SetFont(font);
   }
}

void text_editor::on_paint(wxPaintEvent &event)
{
   wxPaintDC dc(this);
   dc.SetBrush(*wxBLUE_BRUSH);
   dc.DrawRectangle(10, 10, 100, 50); // Dibuja un rectángulo azul
}

void text_editor::on_char_added(wxStyledTextEvent &event)
{
   changed = true;
}

void text_editor::configure_cpp_style()
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
   textEditor->StyleSetFont(wxSTC_C_PREPROCESSOR, wxFont(17, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

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