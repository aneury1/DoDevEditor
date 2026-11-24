#include "EditorApp.h"
namespace dodev {
TerminalTab* EditorApp::debugTerminal(){
    for(auto it=terminals_.rbegin();it!=terminals_.rend();++it)if((*it)->debugger&&(*it)->session.running())return it->get();
    return nullptr;
}
bool EditorApp::debuggerContext(std::string& workspace,std::string& file,int& line){
    file.clear();line=0;auto root=folder_;
    if(auto* doc=active();doc&&doc->kind==DocumentKind::Text&&!doc->path.empty()){
        auto p=fs::absolute(doc->path).lexically_normal();file=p.u8string();
        if(doc->editor!=R_INVALID_ID&&Rwidget_is_valid(doc->editor)){int col=0;Rtext_editor_get_cursor(doc->editor,&line,&col);++line;}
        if(root.empty()){
            root=p.parent_path();for(auto dir=root;!dir.empty();dir=dir.parent_path()){
                if(fs::exists(dir/".vscode"/"launch.json")){root=dir;break;}
                if(dir==dir.parent_path())break;
            }
        }
    }
    if(root.empty()){setStatus("Open a workspace folder before configuring GDB");return false;}
    workspace=fs::absolute(root).lexically_normal().u8string();return true;
}
bool EditorApp::launchDebugger(const std::string& cwd,const std::vector<std::string>& argv){
    if(debugTerminal()){setStatus("A GDB session is already active; stop it first");return false;}
    if(argv.empty())return false;
    newTerminal(false,argv,fs::u8path(cwd));auto* tab=activeTerminal();
    if(!tab||!tab->debugger||!tab->session.running())return false;
    setStatus("GDB started in its own terminal; waiting for debugger output");return true;
}
int EditorApp::controlDebugger(const std::string& command){
    auto* tab=debugTerminal();if(!tab)return 0;
    if(command=="@state")return 1;
    if(command.empty()||command.find_first_of("\r\n")!=std::string::npos||command.find('\0')!=std::string::npos)return -1;
    Rdock_area_open_panel(dock_,output_pane_);Rtab_view_show_tab(terminal_tabs_,tab->group);active_terminal_canvas_=tab->canvas;
    if(command=="@pause"){
        if(tab->debugReady)return 1;
        tab->debugTail.clear();return tab->session.interrupt()?1:-1;
    }
    if(command=="@stop"&&!tab->debugReady){
        tab->debugQuitPending=true;tab->debugTail.clear();
        setStatus("Interrupting GDB; will quit at the next debugger prompt");return tab->session.interrupt()?1:-1;
    }
    if(!tab->debugReady||tab->debugQuitPending){setStatus("GDB busy or console input pending; pause or finish the command first");return -1;}
    tab->debugReady=false;tab->debugTail.clear();
    const bool ok=tab->session.sendLine(command=="@stop"?"quit":command);
    tab->screen.toBottom();return ok?1:-1;
}
}
