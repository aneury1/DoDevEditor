#include "TextEditor.h"
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

   textEditor->StyleSetSize(wxSTC_C_PREPROCESSOR, 40);
   wxFont dfont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
   wxFont pfont(18, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
   // Fonts

   textEditor->StyleSetFont(wxSTC_C_DEFAULT, dfont);
   textEditor->StyleSetFont(wxSTC_C_PREPROCESSOR, pfont);

   // Enable line numbers
   textEditor->SetMarginType(0, wxSTC_MARGIN_NUMBER);
   textEditor->SetMarginWidth(0, 50);
   textEditor->SetCaretPeriod(500);
   // textEditor->StyleSetBackground(wxSTC_STYLE_LINENUMBER, wxColour(230, 230, 230)); // Light gray background
   textEditor->StyleSetForeground(wxSTC_STYLE_LINENUMBER, wxColour(0, 0, 0));
   textEditor->SetMarginBackground(0, *wxRED);
   textEditor->SetCaretForeground(wxColour(255, 0, 0)); // Red caret
   textEditor->SetCaretWidth(100);

   textEditor->StyleSetItalic(wxSTC_C_PREPROCESSOR, true);
   textEditor->StyleSetUnderline(wxSTC_C_PREPROCESSOR, true);
   
   textEditor->StyleSetBackground(wxSTC_STYLE_DEFAULT, wxColour(0x43,0x43,0x43,255));
   textEditor->StyleSetBackground(wxSTC_STYLE_LINENUMBER, wxColour(0x70,0x73,0x73,255));
   textEditor->SetBackgroundColour(*wxBLACK);
   textEditor->SetWhitespaceBackground(false,wxColour(0x43,0x43,0x43,255));


///Experiments.
   textEditor->StyleSetBackground( wxSTC_MARK_BACKGROUND , wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground(wxSTC_MARK_DOTDOTDOT, wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground(wxSTC_MARK_ARROWS , wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground(wxSTC_MARK_PIXMAP , wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground(wxSTC_MARK_FULLRECT , wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground(wxSTC_MARK_LEFTRECT , wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground(wxSTC_MARK_AVAILABLE , wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground(wxSTC_MARK_UNDERLINE , wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground( wxSTC_MARK_RGBAIMAGE , wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground( wxSTC_MARK_BOOKMARK, wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground( wxSTC_STYLE_LINENUMBER, wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground( wxSTC_STYLE_BRACELIGHT , wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground( wxSTC_STYLE_BRACEBAD , wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground( wxSTC_STYLE_CONTROLCHAR , wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground( wxSTC_STYLE_INDENTGUIDE, wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground( wxSTC_STYLE_CALLTIP, wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground( wxSTC_STYLE_FOLDDISPLAYTEXT , wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground( wxSTC_STYLE_LASTPREDEFINED, wxColour(0x70,0x73,0x73,255));
   textEditor->StyleSetBackground( wxSTC_STYLE_MAX, wxColour(0x70,0x73,0x73,255));
}

TextEditor::TextEditor(wxWindow *parent) : EditorTab(parent)
{
   auto panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(0, 10));
   // panel->SetBackgroundColour(wxColour(255,0,0,255));
   textEditor = new wxStyledTextCtrl(this, wxID_ANY);
   textEditor->SetMinSize(wxSize(800, 600));
   textEditor->StyleSetSize(wxSTC_STYLE_DEFAULT, 12);
   wxBoxSizer *Ssizer = new wxBoxSizer(wxVERTICAL);
   Ssizer->Add(panel, 0, wxEXPAND);
   Ssizer->Add(textEditor, 1, wxEXPAND | wxALL, 2);
   SetSizer(Ssizer);

   cpp(textEditor);
   Bind(wxEVT_STC_CHARADDED, &TextEditor::OnCharAdded, this);
   
   //@Todo: decide and fix position, Add Accelerator for showing up on CTRL-<KEY>
   //findIn = new FindDialogInCurrentTextEditor(this);
   //findIn->ShowModal();
}

int TextEditor::getTypeOfEditor()
{
   return 0;
}

std::vector<uint8_t> TextEditor::getData()
{
   std::vector<uint8_t> ret;
   if (textEditor)
   {
      auto value = textEditor->GetValue();
      for (auto it : value)
      {
         ret.emplace_back(static_cast<uint8_t>(it));
      }
   }
   return ret;
}

void TextEditor::OnCharAdded(wxStyledTextEvent &event)
{
   changed = true;
}

void TextEditor::SetLineNumber(bool value)
{
}

Response TextEditor::setData(std::vector<uint8_t> d)
{
   auto s = toString(d);
   this->textEditor->SetValue(s.c_str());
   return Response::Success;
}

Response TextEditor::openFile(std::string filepath)
{
   auto file = ReadFile(filepath);
   if(file.size()>0)
   {
      SetPath(filepath);
      this->textEditor->SetValue(file.c_str());
      return Response::Success;
   }
   return Response::Error;
}

Response TextEditor::saveDocument()
{
   if (is_file == false)
   {
      filepath = CreateFileBySelectingIt(this, this->textEditor->GetValue().ToStdString());
      is_file = filepath.size() > 0;
      if(is_file){
         changed =false;
         return Response::Success;
      }
      else
      {
         return Response::Cancel;
      }
     
   }
   else
   {
      auto response = WriteContentToFile(filepath, this->textEditor->GetValue().ToStdString());
      changed = (response == Response::Success) ? false : true;
      return response;
   }
}
