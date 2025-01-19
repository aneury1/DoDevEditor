#include "tabs.h"
#include "text_editor.h"

#include <iostream>

editor_tab::editor_tab(do_devwindow *parent) : wxPanel(parent, wxID_ANY)
{
    current_text_editor = nullptr;

    SetBackgroundColour(wxColour(255, 0, 0, 255));

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    editorTabs = new wxAuiNotebook(this, wxID_LAST + 2, wxDefaultPosition, wxDefaultSize,
                                   wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ACTIVE_TAB);

    newTab = new wxButton(this, wxID_LAST + 1, wxString("Add empty text"));

    Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &editor_tab::on_close_tab , this, CloseTab);

    Bind(wxEVT_BUTTON, [this](wxCommandEvent &event)
         { add_empty_page(); }, wxID_LAST + 1);

    sizer->Add(newTab, 0);

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

void editor_tab::set_title_current_page(const wxString &title)
{
    int currentPage = editorTabs->GetSelection(); // Get the active page index
    if (currentPage != wxNOT_FOUND)
    { // Check if a page is selected
        editorTabs->SetPageText(currentPage, title);
    }
}

void editor_tab::on_close_tab(wxAuiNotebookEvent &event)
{
   int pageIndex = event.GetSelection();
  /// std::cout <<"Page index: "<< pageIndex <<"\n";
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

bool editor_tab::is_file_already_in_the_editor(const wxString& path){
       int pageCount = editorTabs->GetPageCount();

    for (int i = 0; i < pageCount; ++i) {
        // Get the text of each tab
        wxString tabText = editorTabs->GetPageText(i);
        text_editor* pageWindow = static_cast<text_editor*>(editorTabs->GetPage(i));
        pageWindow->debug();
        if(pageWindow->get_from_file()){
           if(pageWindow->get_path() == path){
            return true;
           }
        }
        // Perform an action with the tab text
        // wxLogMessage("Tab %d: %s", i + 1, tabText);
    }
    return false;
}