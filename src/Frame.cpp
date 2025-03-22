
#include <mutex>
#include <thread>
#include "frame.h"
#include "TabContainer.h"
#include "MenubarOptions.h"
#include "constant.h"
#include "utils.h"
#include "DLTViewer.h"
#include "CSVEditor.h"
#include "DataServerEditor.h"
#include "Dialogs.h"
#include "Settings.h"


WindowFrame::WindowFrame() : wxFrame(nullptr, wxID_ANY, wxT("DoDevEditor"))
{
  ///SetBackgroundColour(*wxBLACK);
  auiManager.SetManagedWindow(this);
 // auiManager.SetBackgroundColour(defaultSettings.getPanelBG());
  AddDefaultEvent();
  SetDefaultPanel();
  SetUpMenu();
  Maximize();
 
  SetBackgroundColour(defaultSettings.getPanelBG());
  auto ptr = auiManager.GetManagedWindow();
  ptr->SetBackgroundColour(*wxBLACK);
  ptr->Update();
  ptr->Refresh();
  auiManager.Update();
  Bind(wxEVT_MENU, &WindowFrame::OnExit, this, wxID_EXIT);
  Bind(wxEVT_MENU, &WindowFrame::OnEventHappened, this, wxID_NEW);
  Bind(wxEVT_MENU, &WindowFrame::OnEventHappened, this, wxID_OPEN);
  Bind(wxEVT_MENU, &WindowFrame::OnEventHappened, this, wxID_SAVE);
  Bind(wxEVT_MENU, &WindowFrame::OnEventHappened, this, SaveAs);
  Bind(wxEVT_MENU, &WindowFrame::OnEventHappened, this, OpenFolder);
  Bind(wxEVT_MENU, &WindowFrame::OnEventHappened, this, CloseFolder);
  Bind(wxEVT_MENU, &WindowFrame::OnEventHappened, this, ActionMenuCommands);
  Bind(wxEVT_MENU, &WindowFrame::OnEventHappened, this, ViewFileExplorer);
  Bind(wxEVT_MENU, &WindowFrame::OnEventHappened, this, ViewGitExplorer);
  Bind(wxEVT_MENU, &WindowFrame::OnEventHappened, this, ViewExecPanel);
  Bind(wxEVT_MENU, &WindowFrame::OnEventHappened, this, AddDLTViewer);
  Bind(wxEVT_MENU, &WindowFrame::OnEventHappened, this, AddCSVViewer);
  Bind(wxEVT_MENU, &WindowFrame::OnEventHappened, this, AddDataServerEditor);
  Bind(wxEVT_MENU, &WindowFrame::OnEventHappened, this, OpenFileAccelerator);
}

void WindowFrame::AddDefaultEvent()
{
  auto newEmptyFile=[this](WindowFrame *frame){
    tabContainer->AddEmptyTextPage();
  };

  auto openExistingFile = [this](WindowFrame *frame){ 
    std::string path;
    if(OpenFileDialog(this, path)==Response::Success && path.size()>0){
      std::string content;
      content = ReadFile(path);
      if(content.size()>0){
        auto vc = fromStrTo8Vec(content);
        tabContainer->SetDataToCurrentContainer(vc);
        int lastSlashOnPath = path.find_last_of(FileSeparator);
        if(lastSlashOnPath==std::string::npos)
        {
         tabContainer->SetTitleToCurrentPage(path);
         tabContainer->GetCurrentTab()->SetPath(path);
        } 
        
        else
        {
          tabContainer->GetCurrentTab()->SetPath(path); 
          path = path.substr(lastSlashOnPath+1, path.size()-lastSlashOnPath);
          tabContainer->SetTitleToCurrentPage(path);
        }
      }
    }
  };

  std::mutex mu;
  auto openFolder = [this](WindowFrame *frame){
   
     if(explorerTabContainer)
      explorerTabContainer->OpenFolder();
  };

  auto saveCurrentFile = [this](WindowFrame *frame){
      tabContainer->GetCurrentTab()->saveDocument();
      tabContainer->SetTitleToCurrentPage(tabContainer->GetCurrentTab()->GetFileName());
  };


  auto openTerminal =[this](WindowFrame *frame){
     GetTerminal();
  };

  auto openDLTViewer=[this](WindowFrame *frame){
     if(tabContainer){
      auto dlt = new DLTViewerTab(tabContainer);
      tabContainer->addCustomEditorTab(dlt);
      tabContainer->SetTitleToCurrentPage("--DLT File viewer:--");
     }
  };

  auto openCSVViewer=[this](WindowFrame *frame){
     if(tabContainer){
      auto csv = new CSVEditor(tabContainer);
      tabContainer->addCustomEditorTab(csv);
      tabContainer->SetTitleToCurrentPage("--CSV Editor--");
     }
  };

    auto openDataServerEditor=[this](WindowFrame *frame){
     if(tabContainer){
      auto csv = new DataServerEditor(tabContainer);
      tabContainer->addCustomEditorTab(csv);
      tabContainer->SetTitleToCurrentPage("--DataServer Editor--");
     }
  };


  auto openFileByAccelerator=[this](WindowFrame *frame){
      auto files = this->explorerTabContainer->GetFiles();
      OpenExistingFile file(frame, files);
      if (file.ShowModal() == wxID_CANCEL){

      }
  };

  AddCMDCallback(wxID_NEW     , newEmptyFile);
  AddCMDCallback(wxID_OPEN    , openExistingFile);
  AddCMDCallback(OpenFolder   , openFolder);
  AddCMDCallback(wxID_SAVE    , saveCurrentFile);
  AddCMDCallback(ViewExecPanel, openTerminal);
  AddCMDCallback(AddDLTViewer , openDLTViewer);
  AddCMDCallback(AddCSVViewer , openCSVViewer);
  AddCMDCallback(AddDataServerEditor, openDataServerEditor);
  AddCMDCallback(OpenFileAccelerator, openFileByAccelerator);


  wxAcceleratorEntry entries[1];
  entries[0].Set(wxACCEL_CTRL, (int)'P', OpenFileAccelerator);

  wxAcceleratorTable accel(1, entries);
  SetAcceleratorTable(accel);


}

void WindowFrame::SetDefaultPanel()
{
  panel_info tabpanel;
  panel_info explorerpanel;
  
  tabContainer =  new TabContainer(this);
  tabpanel.panel = tabContainer;
  tabpanel.info = wxAuiPaneInfo().Name(tabcontainer).Centre().Caption(" ").BestSize(300, 400).MinSize(100, 100);
  add_panel(&tabpanel);

  explorerTabContainer = new FileExplorerTabContainer(this); 
  explorerpanel.panel = explorerTabContainer;
  explorerpanel.info = wxAuiPaneInfo().Name(explorercontainer).Left().Caption(" ").BestSize(300, 400).MinSize(300, 100);
  add_panel(&explorerpanel);

  auiManager.Update();
  auto ptr = auiManager.GetManagedWindow();
  ptr->SetBackgroundColour(*wxBLACK);
  ptr->Update();
  ptr->Refresh();
  auiManager.Update();


}

void WindowFrame::add_panel(panel_info *panel)
{
  auiManager.AddPane(panel->panel, panel->info);
}

void WindowFrame::SetUpMenu()
{
  auto menubar = CreateMenuBarAndOptions();
  SetMenuBar(menubar);
}

void WindowFrame::OnEventHappened(wxCommandEvent &ev)
{
  std::cout <<"DEBUG: Event not found: "<<ev.GetId()<<"\n"; 
    auto find = commandEventFunctions.find(ev.GetId());
    if(find != commandEventFunctions.end()){
      find->second(this);
    }else{
      
    }
}

void WindowFrame::OnExit(wxCommandEvent &ev)
{
   exit(1);
}

EditorTab *WindowFrame::GetCurrentEditorTab(){
   return tabContainer->GetCurrentTab();
}

void WindowFrame::AddCMDCallback(int id, std::function<void(WindowFrame*)> cb)
{
    this->commandEventFunctions[id]= cb;
}

EditorTab* WindowFrame::AddNewPage(){
  if(tabContainer){
    tabContainer->AddEmptyTextPage();
    EditorTab *tab = tabContainer->GetCurrentTab();
    return tab;
  }
  return nullptr;
}

EditorTab* WindowFrame::AddNewPage(std::string filename){
  if(tabContainer){
    tabContainer->AddPage(filename);
    EditorTab *tab = tabContainer->GetCurrentTab();
    return tab;
  }
  return nullptr;
}


TabContainer* WindowFrame::GetTabContainer(){
   return tabContainer;
}