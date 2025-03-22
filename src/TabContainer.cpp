#include "Settings.h"
#include "TabContainer.h"
#include "TextEditor.h"
#include "utils.h"
#include "frame.h"
#include "Dialogs.h"
 

TabContainer::TabContainer(wxWindow *parent) : wxPanel(parent, wxID_ANY)
{
   wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
   editorTabs = new wxAuiNotebook(this, CloseTab, wxDefaultPosition, wxDefaultSize,
                                  wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ACTIVE_TAB);
 
   editorTabs->SetBackgroundColour(defaultSettings.getPanelBG());
   editorTabs->SetForegroundColour(defaultSettings.getPanelFG());
   toolPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition);
   toolPanel->SetBackgroundColour(defaultSettings.getPanelBG());
   auto newTab = new wxButton(toolPanel, newEmptyTab, wxString("Add empty text"));
   wxBoxSizer *panel_tool_sizer = new wxBoxSizer(wxHORIZONTAL);
   panel_tool_sizer->Add(newTab, 0, wxEXPAND | wxALL);
   toolPanel->SetSizer(panel_tool_sizer);
   SetBackgroundColour(defaultSettings.getPanelBG());
   toolPanel->SetBackgroundColour(defaultSettings.getPanelBG());
   Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &TabContainer::OnCloseTabEvent, this, CloseTab);
   Bind(wxEVT_BUTTON, [this](wxCommandEvent &event)
        { AddEmptyTextPage(event); }, newEmptyTab);
   sizer->Add(toolPanel, 0, wxEXPAND);
   sizer->Add(editorTabs, 1, wxEXPAND);
   SetSizer(sizer);
   
}

void TabContainer::AddEmptyTextPage()
{
   auto panel = new TextEditor(this);
   panel->SetBackgroundColour(defaultSettings.getPanelBG());
   panel->SetForegroundColour(defaultSettings.getPanelFG());
   editorTabs->AddPage(panel, "Untitled", true);
   currentTab = panel;
}

void TabContainer::AddPage(const std::string file)
{
  
   std::string extension = ExtractFileExtension(file);
    
   if(extension=="txt"){
      auto panel = new TextEditor(this);
      editorTabs->AddPage(panel, "Untitled", true);
      currentTab = panel;
      return ;
   }
   throw std::runtime_error("Is not implemented");
}

void TabContainer::AddEmptyTextPage(wxCommandEvent &ev)
{
   auto panel = new TextEditor(this);

   editorTabs->AddPage(panel, "Untitled", true);
   currentTab = panel;
}

void TabContainer::OnCloseTabEvent(wxAuiNotebookEvent &ev)
{
   if (currentTab)
   {
      bool queryOnClose = true;

      if (currentTab->wasChanged())
      {
         auto v = currentTab->getData();
         auto s = toString(v);
         auto response = QuestionYesOrNo("File Has Changed", "Do you want to save this document?", this);

         if (response == Response::Success)
         {
            auto rs = currentTab->saveDocument();
            if (rs == Response::Cancel)
            {
               ev.Veto();
               return;
            }
         }

         queryOnClose = response != Response::Success;
      }

      if (queryOnClose)
      {
         auto response = QuestionYesOrNo("File Has Changed", "Do you want to close without save?", this);
         if (response != Response::Success)
         {
            ev.Veto();
            return;
         }
      }
      currentTab = nullptr;
      return;
   }
}

void TabContainer::SetTitleToCurrentPage(const std::string &title)
{
   int currentPage = editorTabs->GetSelection(); // Get the active page index
   if (currentPage != wxNOT_FOUND)
   {
      editorTabs->SetPageText(currentPage, title);
   }
}

void TabContainer::SetDataToCurrentContainer(std::vector<uint8_t> da)
{ 
   if(currentTab!=nullptr){
      currentTab->setData(da);
   }else{
      AddEmptyTextPage();
      currentTab->setData(da);
   }
}

void TabContainer::addCustomEditorTab(EditorTab *tab){
   auto panel = tab;
   if(tab){
      editorTabs->AddPage(panel, "  ", true);
      currentTab = panel;
   }
}

 
