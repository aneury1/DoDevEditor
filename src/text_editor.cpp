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
   textEditor->SetBackgroundColour(wxColour(30, 30, 30));
   textEditor->SetForegroundColour(wxColour(255, 255, 255));
   textEditor->SetBackgroundColour("#434343");
   textEditor->StyleSetSize(wxSTC_STYLE_DEFAULT, normal_font_size);
   std::cout << "panel id=" << id << "\n";
   wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
   sizer->Add(textEditor, 1, wxEXPAND);
   this->SetSizer(sizer);
   Bind(wxEVT_PAINT, &text_editor::OnPaint, this);
   Bind(wxEVT_STC_CHARADDED, &text_editor::OnCharAdded, this);
   Bind(wxEVT_STC_CHANGE, &text_editor::OnCharAdded, this);
   Bind(wxEVT_STC_MODIFIED, &text_editor::OnCharAdded, this);
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
      font.SetPointSize(font.GetPointSize()+1); 
      textEditor->SetFont(font);
   }
}
void text_editor::decrease_font_size_by_one()
{
   if (textEditor && normal_font_size >= 9)
   {
      textEditor->StyleSetSize(wxSTC_STYLE_DEFAULT, --normal_font_size);
      wxFont font = textEditor->GetFont();
      font.SetPointSize(font.GetPointSize()+1); 
      textEditor->SetFont(font);
   }
}

void text_editor::OnPaint(wxPaintEvent &event)
{
   wxPaintDC dc(this);
   dc.SetBrush(*wxBLUE_BRUSH);
   dc.DrawRectangle(10, 10, 100, 50); // Dibuja un rectángulo azul
}

void text_editor::OnCharAdded(wxStyledTextEvent &event)
{
   changed = true;
}
