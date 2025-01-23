#include <wx/fontenum.h>
#include <wx/choice.h>
#include <wx/stattext.h>
#include <iostream>

#include "tabs.h"
#include "text_editor.h"


editor_tab::editor_tab(do_devwindow *parent) : wxPanel(parent, wxID_ANY)
{
   current_text_editor = nullptr;

   SetBackgroundColour(wxColour(255, 0, 0, 255));

   wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

   editorTabs = new wxAuiNotebook(this, wxID_LAST + 2, wxDefaultPosition, wxDefaultSize,
                                  wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ACTIVE_TAB);

   wxPanel *action_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition);
   
   action_panel->SetBackgroundColour(wxColour(0, 255, 0, 244));

   newTab = new wxButton(action_panel, wxID_LAST + 1, wxString("Add empty text"));

   wxStaticText *fontChooseText = new wxStaticText(action_panel, wxID_ANY, wxString(" Select Font"));

   fontChoice = new wxChoice(action_panel, wxID_ANY);
   {
      wxFontEnumerator fontEnum;
      fontEnum.EnumerateFacenames();
      const wxArrayString &fonts = fontEnum.GetFacenames();
      fontChoice->Append(fonts);
   }
   wxBoxSizer *panel_tool_sizer = new wxBoxSizer(wxHORIZONTAL);
   panel_tool_sizer->Add(newTab, 0, wxSHRINK, 15);
   panel_tool_sizer->Add(fontChooseText, 0, wxSHRINK, 15);
   panel_tool_sizer->Add(fontChoice, 0, wxSHRINK, 15);

   action_panel->SetSizer(panel_tool_sizer);

   Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &editor_tab::on_close_tab, this, CloseTab);
   
   fontChoice->Bind(wxEVT_CHOICE, &editor_tab::on_font_change, this);
   
   Bind(wxEVT_BUTTON, [this](wxCommandEvent &event)
        { add_empty_page(); }, wxID_LAST + 1);

   sizer->Add(action_panel, 0, wxEXPAND);

   sizer->Add(editorTabs, 1, wxEXPAND);
   SetSizer(sizer);
}

editor_tab::~editor_tab()
{
}

void editor_tab::add_empty_page()
{
   auto panel = new text_editor(this);
   editorTabs->AddPage(panel, "Untitled", true);
   current_text_editor = panel;
}

void editor_tab::on_font_change(wxCommandEvent &event)
{
   wxString selectedFont = fontChoice->GetStringSelection();
   if (!selectedFont.IsEmpty())
   {
      wxFont font(18, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, selectedFont);
      if (font.IsOk())
      {

         if (current_text_editor)
         {
            current_text_editor->set_font(font);
         }
      }
      else
      {
         wxMessageBox("Failed to apply the selected font.", "Error", wxICON_ERROR);
      }
   }
}

text_editor *editor_tab::get_current_editor()
{
      return current_text_editor;
}

void editor_tab::set_title_current_page(const wxString &title)
{
   int currentPage = editorTabs->GetSelection(); // Get the active page index
   if (currentPage != wxNOT_FOUND)
   { 
      editorTabs->SetPageText(currentPage, title);
   }
}

void editor_tab::on_close_tab(wxAuiNotebookEvent &event)
{
   int pageIndex = event.GetSelection();
#if 0
   auto textEditor = get_current_text_editor();

   if (textEditor && !textEditor->has_changed() && !textEditor->is_untitle())
   {
      event.Veto();
      return;
   }

   wxString prompt = "¿Do you want to close?";
   if (textEditor->has_changed())
   {
      prompt = "File has been changed, ¿Do you want to close?";
   }

   int query = wxMessageBox(prompt,
                            "Confirm close: " + prompt,
                            wxYES_NO | wxCANCEL | wxICON_QUESTION);
   if (query != wxYES)
   {
      event.Veto();
      return;
   }
   event.Skip();

#endif
   return;
}

bool editor_tab::is_file_already_in_the_editor(const wxString &path)
{
   int pageCount = editorTabs->GetPageCount();

   for (int i = 0; i < pageCount; ++i)
   {
      wxString tabText = editorTabs->GetPageText(i);
    
      text_editor *pageWindow = static_cast<text_editor *>(editorTabs->GetPage(i));
    
      pageWindow->debug();
    
      if (pageWindow->get_from_file())
      {
         if (pageWindow->get_path() == path)
         {
            return true;
         }
      }
   
   }
   
   return false;
}