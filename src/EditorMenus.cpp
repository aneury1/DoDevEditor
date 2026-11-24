#include "EditorApp.h"
#include "dodev/editor/EditorCommands.h"
#include "dodev/terminal/TerminalTasks.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>

namespace dodev {
namespace {
std::string data(Rid editor){const char* text=Rtext_editor_get_text(editor);return text?text:"";}
std::size_t offset(const std::string& text,int line,int column){
    std::size_t index=0;for(int n=0;n<line&&index<text.size();++n){
        const auto end=text.find('\n',index);if(end==std::string::npos)return text.size();index=end+1;
    }
    const auto e=text.find('\n',index);
    return std::min(index+static_cast<std::size_t>(std::max(0,column)),e==std::string::npos?text.size():e);
}
std::pair<int,int> lineCol(const std::string& s,std::size_t off){
    int line=0,col=0;
    for(std::size_t i=0;i<std::min(off,s.size());++i){if(s[i]=='\n'){++line;col=0;}else ++col;}
    return {line,col};
}
void selectOffsets(Rid widget,const std::string& text,std::size_t a,std::size_t b){
    auto l=lineCol(text,a),r=lineCol(text,b);
    Rtext_editor_select(widget,l.first,l.second,r.first,r.second);
    Rtext_editor_scroll_to_line(widget,r.first);
    Rwidget_focus(widget);
}
std::string selectedOrLine(Rid widget){
    const char* selection=Rtext_editor_get_selected_text(widget);
    if(selection&&*selection)return selection;
    int l=0,c=0;Rtext_editor_get_cursor(widget,&l,&c);
    const char* line=Rtext_editor_get_line(widget,l);
    return line?std::string(line)+"\n":"";
}
std::string suitableComment(const std::filesystem::path& path){
    const auto e=path.extension().string();
    if(e==".py"||e==".sh"||e==".rb"||e==".pl"||e==".cmake"||e==".yml"||e==".yaml"||e==".toml")return "#";
    if(e==".sql")return "--";
    if(e==".ini")return ";";
    return "//";
}
}
bool EditorApp::dispatchEditSelection(const std::string& action){
    if(action=="edit.find_workspace"){findWorkspace();return true;}
    auto* d=active();if(!d||d->editor==R_INVALID_ID){setStatus("No active text editor");return true;}
    const auto id=d->editor;
    if(action=="edit.copy"||action=="edit.cut"){
        const auto value=selectedOrLine(id);
        Rclipboard_set_text(value.c_str());
        if(action=="edit.cut"&&!d->read_only){
            int a=0,b=0,c=0,e=0;
            if(Rtext_editor_get_selection(id,&a,&b,&c,&e))Rtext_editor_insert_text(id,"");
            else {const auto text=data(id);
                int row=0,col=0;Rtext_editor_get_cursor(id,&row,&col);
                const auto pos=offset(text,row,col);
                selectOffsets(id,text,editor::lineStart(text,pos),std::min(text.size(),editor::lineEnd(text,pos)+1));
                Rtext_editor_insert_text(id,"");
            }
        }
        return true;
    }
    if(action=="edit.paste"){
        if(d->read_only)return true;
        if(const char* clip=Rclipboard_get_text();clip&&*clip)Rtext_editor_insert_text(id,clip);
        return true;
    }
    if(d->read_only&&action.rfind("edit.",0)==0){setStatus("Read-only document");return true;}
    const std::string text=data(id);
    int al=0,ac=0,bl=0,bc=0;
    if(!Rtext_editor_get_selection(id,&al,&ac,&bl,&bc)){
        Rtext_editor_get_cursor(id,&bl,&bc);al=bl;ac=bc;
    }
    editor::Options options;
    options.tabSize=static_cast<unsigned>(std::clamp(settings_.getInt("editor.tabSize",4),1,16));
    options.useSpaces=settings_.getBool("editor.insertSpaces",true);
    options.lineComment=suitableComment(d->path);
    if(options.lineComment=="#"){options.blockStart="\"\"\"";options.blockEnd="\"\"\"";}
    const auto edit=editor::execute(action,text,offset(text,al,ac),offset(text,bl,bc),options);
    if(!edit.handled){setStatus("Edit/Selection command not available: "+action);return false;}
    if(edit.changed){
        selectOffsets(id,text,edit.replaceBegin,edit.replaceEnd);
        Rtext_editor_insert_text(id,edit.replacement.c_str()); // preserve undo history
        const auto after=data(id);
        selectOffsets(id,after,edit.selectBegin,edit.selectEnd);
    }else selectOffsets(id,text,edit.selectBegin,edit.selectEnd);
    return true;
}
void EditorApp::findWorkspace(){showWorkspaceSearch();}
void EditorApp::refreshTasks(){
    tasks_.clear();
    const fs::path project=folder_.empty()?fs::path{}:folder_/".vscode"/"tasks.json";
    std::string error;
    if(!project.empty())tasks_=terminal::loadTasks(project,&error);
    if(tasks_.empty())tasks_=terminal::loadTasks(settings_.directory()/"tasks.json",&error);
    if(!error.empty())setStatus(error);
    if(window_==R_INVALID_ID)return;
    const Rid combo=Rwidget_find(window_,"terminal_task_select");if(combo==R_INVALID_ID)return;
    const int selected=Rcombo_box_get_selected(combo);
    Rcombo_box_clear_items(combo);
    for(const auto& t:tasks_)Rcombo_box_add_item(combo,t.label.c_str());
    if(!tasks_.empty())Rcombo_box_set_selected(combo,selected>=0&&selected<static_cast<int>(tasks_.size())?selected:0);
}
bool EditorApp::executeTask(const terminal::Task& task){
    if(!activeTerminal()||activeTerminal()->debugger)newTerminal();
    auto* t=activeTerminal();if(!t||!t->session.running()){setStatus("No running terminal session");return false;}
#ifdef _WIN32
    constexpr bool windows=true;
#else
    constexpr bool windows=false;
#endif
    if(!t->session.sendLine(terminal::commandLine(task,windows))){setStatus("Failed to send task: "+task.label);return false;}
    Rdock_area_open_panel(dock_,output_pane_);focusTerminal();
    setStatus("Running task: "+task.label);return true;
}
bool EditorApp::dispatchTerminalMenu(const std::string& action){
    if(action=="terminal.new")newTerminal();
    else if(action=="terminal.split")newTerminal(true);
    else if(action=="terminal.close")closeTerminal();
    else if(action=="terminal.clear")clearTerminal();
    else if(action=="terminal.copy")copyTerminal();
    else if(action=="terminal.export")exportTerminal();
    else if(action=="terminal.export_all")exportAllTerminals();
    else if(action=="terminal.paste")pasteTerminal();
    else if(action=="terminal.interrupt"){if(auto* t=activeTerminal())t->session.interrupt();}
    else if(action=="terminal.focus")focusTerminal();
    else if(action=="terminal.next"||action=="terminal.previous"){
        const int count=Rtab_view_get_tab_count(terminal_tabs_);
        if(count>0){const int idx=Rtab_view_get_active(terminal_tabs_);
            Rtab_view_set_active(terminal_tabs_,(idx+count+(action=="terminal.next"?1:-1))%count);focusTerminal();}
    }else if(action=="terminal.rename"){
        auto* t=activeTerminal();if(!t)return true;
        const auto field=Rwidget_find(window_,"terminal_name");
        const char* name=field==R_INVALID_ID?nullptr:Rtext_input_get_text(field);
        if(!name||!*name){setStatus("Enter a name in the terminal name field");return true;}
        t->title=name;Rtab_view_set_tab_title(terminal_tabs_,t->group,name);
        setStatus("Terminal renamed to "+t->title);
    }else if(action=="terminal.restart"){
        auto* t=activeTerminal();if(!t)return true;
        const auto group=t->group;
        t->session.stop();
        closeTerminal();
        // Restart uses the same workspace and profile in a fresh terminal tab.
        (void)group;newTerminal();
    }else if(action=="terminal.run_selected"){
        auto* d=active();if(!d||d->editor==R_INVALID_ID){setStatus("No active text selection");return true;}
        const char* value=Rtext_editor_get_selected_text(d->editor);
        if(!value||!*value){setStatus("Select text to run in the terminal");return true;}
        const std::string command(value);
        if(command.find('\n')!=std::string::npos){
            if(Rmessage_box(window_,"Run selected text?","The selection contains multiple lines. Send all lines to the shell?",R_MESSAGE_YES_NO)!=R_MESSAGE_RESULT_YES)return true;
        }
        if(!activeTerminal()||activeTerminal()->debugger)newTerminal();
        if(auto* t=activeTerminal())t->session.sendLine(command);
        focusTerminal();
    }else if(action=="terminal.run_file"){
        auto* d=active();if(!d||d->path.empty()){setStatus("Save the active file before running it");return true;}
        if(d->dirty&&!save(*d))return true;
#ifdef _WIN32
        const auto command=terminal::launchFileCommand(d->path,true);
#else
        const auto command=terminal::launchFileCommand(d->path,false);
#endif
        if(command.empty()){setStatus("No run configuration for this extension; configure a task");return true;}
        if(!activeTerminal()||activeTerminal()->debugger)newTerminal();
        if(auto* t=activeTerminal())t->session.sendLine(command);
        focusTerminal();
    }else if(action=="terminal.configure_tasks"||action=="terminal.configure_build"){
        if(folder_.empty()){setStatus("Open a project folder to configure tasks");return true;}
        const auto file=folder_/".vscode"/"tasks.json";
        if(!fs::exists(file)){
            std::error_code error;fs::create_directories(file.parent_path(),error);
            if(error){setStatus("Cannot create .vscode folder: "+error.message());return true;}
            std::ofstream output(file,std::ios::binary);
            if(!output){setStatus("Cannot create tasks.json");return true;}
            output<<terminal::defaultTasksJson();output.close();
        }
        opened(file);refreshTasks();
        if(action=="terminal.configure_build")setStatus("Choose a task with group.kind = build in tasks.json");
    }else if(action=="terminal.run_task"||action=="terminal.run_build"){
        refreshTasks();
        if(tasks_.empty()){setStatus("No tasks defined. Use Terminal > Configure Tasks");return true;}
        std::size_t index=0;
        if(action=="terminal.run_build"){
            const auto it=std::find_if(tasks_.begin(),tasks_.end(),[](const auto& t){return t.group=="build";});
            if(it==tasks_.end()){setStatus("No build task. Set group: build in .vscode/tasks.json");return true;}
            index=static_cast<std::size_t>(it-tasks_.begin());
        }else{
            const auto combo=Rwidget_find(window_,"terminal_task_select");
            const int chosen=combo==R_INVALID_ID?-1:Rcombo_box_get_selected(combo);
            if(chosen>=0&&chosen<static_cast<int>(tasks_.size()))index=static_cast<std::size_t>(chosen);
        }
        executeTask(tasks_[index]);
    }else return false;
    return true;
}
} // namespace dodev
