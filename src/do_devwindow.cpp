#include "do_devwindow.h"
#include "menubar.h"
#include "callbacks.h"
#include "utils.h"
#include "callbacks.h"
#include "do_devwindow.h"
#include "tabs.h"
#include "explorer_panel.h"
#include "exec_dialog.h"
#include "terminal_emulator.h"
#include "symbol_panel.h"
#include "git_panel.h"
#include "argc_handler.h"
do_devwindow::do_devwindow() : wxFrame(nullptr, wxID_ANY, wxEmptyString)
{

  auiManager.SetManagedWindow(this);
  auto menubar = create_default_menubar();
  SetMenuBar(menubar);
  Maximize();
  Bind(wxEVT_MENU, &do_devwindow::on_exit, this, wxID_EXIT);

  Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, wxID_NEW);
  Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, wxID_OPEN);
  Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, wxID_SAVE);
  Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, SaveAs);
  Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, OpenFolder);
  Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, CloseFolder);
  Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, ActionMenuCommands);
  Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, ViewFileExplorer);
  Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, ViewGitExplorer);
  Bind(wxEVT_MENU, &do_devwindow::on_key_entered, this, ViewExecPanel);

  Bind(wxEVT_CONTEXT_MENU, &do_devwindow::on_context_menu, this);
}
do_devwindow::~do_devwindow() {}

void do_devwindow::update_components()
{
  auiManager.Update();
}

void do_devwindow::setup_default_panels()
{
   auto frame = this;
           auto editortabs = new editor_tab(frame);
        auto explorerpanel = new explorer_panel(frame);
        auto exec_panel = new exec_dialog(frame);
        auto symbolpanel = new symbol_panel(frame);
        auto gitpanel = new git_panel(frame);

        panel_info editorPanel;
        editorPanel.panel = editortabs;
        editorPanel.info = wxAuiPaneInfo().Name(editor_panel_name).CenterPane();

        panel_info explorerPanel;
        explorerPanel.panel = explorerpanel;
        explorerPanel.info = wxAuiPaneInfo().Name(file_explorer_panel_name).Left().Caption("Explorer").BestSize(300, 800).MinSize(200, 200);

        panel_info execPanel;
        execPanel.panel = exec_panel;
        execPanel.info = wxAuiPaneInfo().Name(exec_panel_name).Bottom().Caption("exec").BestSize(300, 400).MinSize(200, 100);

        panel_info symbolPanel;
        symbolPanel.panel = symbolpanel;
        symbolPanel.info = wxAuiPaneInfo().Name(symbol_panel_name).Left().Caption("symbol").BestSize(300, 400).MinSize(200, 100);

        panel_info gitPanel;
        gitPanel.panel = gitpanel;
        gitPanel.info = wxAuiPaneInfo().Name("git_panel_name").Right().Caption("GIT").BestSize(200, 400).MinSize(100, 100);

        set_base_window(
            frame,
            editortabs,
            explorerpanel,
            gitpanel);
        frame->add_panel(&explorerPanel);
        frame->add_panel(&editorPanel);
        frame->add_panel(&execPanel);
       /// frame->add_panel(&symbolPanel);
        frame->add_panel(&gitPanel);

        frame->update_components();
}



void do_devwindow::on_exit(wxCommandEvent &event)
{
  auiManager.UnInit();
  Close();
}

void do_devwindow::add_panel(panel_info *panel)
{
  auiManager.AddPane(panel->panel, panel->info);
}
void do_devwindow::on_context_menu(wxContextMenuEvent &event)
{
  std::cout << "EVent has been triggered: " << event.GetId() << "\n";
  // Create the context menu
  wxMenu menu;
  menu.Append(wxID_OPEN, "Open");
  menu.Append(OpenFolder, "Open folder");
  menu.Append(CloseFolder, "Close Open folder");
  menu.Append(wxID_SAVE, "Save Current");
  menu.AppendSeparator();
  menu.Append(wxID_EXIT, "Exit");
  // Show the menu at the current mouse position
  PopupMenu(&menu, ScreenToClient(wxGetMousePosition()));
}

void do_devwindow::on_key_entered(wxCommandEvent &ev)
{
  call_by_event(ev);
}

void do_devwindow::show_explorer() {
  wxAuiPaneInfo& pane = auiManager.GetPane(file_explorer_panel_name);

  if (!pane.IsShown()) {
      pane.Show(true);  // Show the pane
      auiManager.Update();  // Apply changes
  }
}
void do_devwindow::show_editor() 
{
  wxAuiPaneInfo& pane = auiManager.GetPane(editor_panel_name);

  if (!pane.IsShown()) {
      pane.Show(true);  // Show the pane
      auiManager.Update();  // Apply changes
  }
}
void do_devwindow::show_git_panel() {
    wxAuiPaneInfo& pane = auiManager.GetPane(git_panel_name);

  if (!pane.IsShown()) {
      pane.Show(true);  // Show the pane
      auiManager.Update();  // Apply changes
  }
}
void do_devwindow::show_exec() {
    wxAuiPaneInfo& pane = auiManager.GetPane(exec_panel_name);

  if (!pane.IsShown()) {
      pane.Show(true);  // Show the pane
      auiManager.Update();  // Apply changes
  }
}