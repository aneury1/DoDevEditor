#include "EditorApp.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <sstream>

namespace dodev {
namespace {
std::string field(Rid window,const char* name){
    const Rid w=Rwidget_find(window,name);if(w==R_INVALID_ID)return {};
    const char* s=Rtext_input_get_text(w);return s?s:"";
}
bool checked(Rid window,const char* name){
    const Rid w=Rwidget_find(window,name);
    return w!=R_INVALID_ID && Rcheckbox_is_checked(w)!=R_FALSE;
}
std::string displayPath(const fs::path& path,const fs::path& root){
    std::error_code ec;auto relative=fs::relative(path,root,ec);
    return ec?pathUtf8(path):pathUtf8(relative);
}
std::pair<int,int> lineCol(std::string_view data,std::size_t pos){
    int line=0,col=0;
    for(std::size_t i=0;i<pos&&i<data.size();++i){if(data[i]=='\n'){++line;col=0;}else ++col;}
    return {line,col};
}
}
void EditorApp::showWorkspaceSearch(){
    Rdock_area_open_panel(dock_,search_pane_);
    if(const auto w=Rwidget_find(window_,"workspace_query");w!=R_INVALID_ID)Rwidget_focus(w);
}
void EditorApp::searchWorkspaceFiles(){
    if(folder_.empty()){setStatus("Abre una carpeta de proyecto para buscar en archivos");showWorkspaceSearch();return;}
    workspaceOptions_.query=field(window_,"workspace_query");
    workspaceOptions_.include=field(window_,"workspace_include");
    workspaceOptions_.exclude=field(window_,"workspace_exclude");
    workspaceOptions_.regex=checked(window_,"workspace_regex");
    workspaceOptions_.caseSensitive=checked(window_,"workspace_case");
    workspaceOptions_.wholeWord=checked(window_,"workspace_word");
    search::Overlays openBuffers;
    for(const auto& d:docs_){
        if(d->kind!=DocumentKind::Text||d->path.empty()||d->editor==R_INVALID_ID)continue;
        std::error_code ec;const auto canonical=fs::weakly_canonical(d->path,ec);
        if(!ec){const char* data=Rtext_editor_get_text(d->editor);openBuffers[canonical]=data?data:"";}
    }
    workspaceMatches_=search::scan(folder_,workspaceOptions_,openBuffers);
    Rtable_clear_rows(search_table_);
    for(const auto& m:workspaceMatches_.matches){
        const int row=Rtable_add_row(search_table_,nullptr);
        Rtable_set_cell(search_table_,row,0,m.snippet.c_str());
        Rtable_set_cell(search_table_,row,1,"Match");
        const auto location=displayPath(m.path,folder_)+":"+std::to_string(m.line)+":"+std::to_string(m.column);
        Rtable_set_cell(search_table_,row,2,location.c_str());
    }
    std::string message=std::to_string(workspaceMatches_.matches.size())+" results in "+
        std::to_string(workspaceMatches_.filesScanned)+" files";
    if(workspaceMatches_.truncated)message+=" (limit reached)";
    if(!workspaceMatches_.error.empty())message+=" · "+workspaceMatches_.error;
    Rlabel_set_text(search_status_,message.c_str());
    setStatus(message);
    Rdock_area_open_panel(dock_,search_pane_);
    std::clog<<"[DoDevEditor][WorkspaceSearch] "<<message<<'\n';
}
void EditorApp::navigateWorkspaceResult(int index){
    if(index<0||static_cast<std::size_t>(index)>=workspaceMatches_.matches.size())return;
    const auto result=workspaceMatches_.matches[static_cast<std::size_t>(index)];
    opened(result.path);
    for(const auto& d:docs_){
        if(d->path.empty()||d->editor==R_INVALID_ID||d->kind!=DocumentKind::Text)continue;
        std::error_code ec;
        if(fs::equivalent(d->path,result.path,ec)&&!ec){
            Rtab_view_show_tab(tabs_,d->page);
            const auto begin=static_cast<int>(result.column-1);
            Rtext_editor_select(d->editor,static_cast<int>(result.line-1),begin,
                                static_cast<int>(result.line-1),begin+static_cast<int>(result.matchLength));
            Rtext_editor_scroll_to_line(d->editor,static_cast<int>(result.line-1));
            Rwidget_focus(d->editor);
            return;
        }
    }
}
void EditorApp::workspaceSelectCb(Rid,int index,void* user){
    auto& app=*static_cast<EditorApp*>(user);
    if(index<0||static_cast<std::size_t>(index)>=app.workspaceMatches_.matches.size())return;
    const auto& m=app.workspaceMatches_.matches[static_cast<std::size_t>(index)];
    app.setStatus(displayPath(m.path,app.folder_)+":"+std::to_string(m.line)+":"+std::to_string(m.column));
}
void EditorApp::workspaceActivatedCb(Rid,int index,void* user){
    static_cast<EditorApp*>(user)->navigateWorkspaceResult(index);
}
void EditorApp::workspaceAction(const std::string& action){
    if(action=="workspace.search"){searchWorkspaceFiles();return;}
    if(action!="workspace.replace_selected"&&action!="workspace.replace_all")return;
    if(workspaceOptions_.query.empty()||workspaceMatches_.matches.empty()){
        setStatus("Run workspace search before replacing");return;
    }
    // Refuse applying stale results if filters or query changed after the search.
    if(workspaceOptions_.query!=field(window_,"workspace_query")||
       workspaceOptions_.include!=field(window_,"workspace_include")||
       workspaceOptions_.exclude!=field(window_,"workspace_exclude")||
       workspaceOptions_.regex!=checked(window_,"workspace_regex")||
       workspaceOptions_.caseSensitive!=checked(window_,"workspace_case")||
       workspaceOptions_.wholeWord!=checked(window_,"workspace_word")){
        setStatus("Search settings changed. Run Search again before replacing.");return;
    }
    const auto replacement=field(window_,"workspace_replacement");
    const bool replaceAll=action=="workspace.replace_all";
    const int selected=Rtable_get_selected(search_table_);
    if(!replaceAll&&(selected<0||static_cast<std::size_t>(selected)>=workspaceMatches_.matches.size())){
        setStatus("Select a search result to replace");return;
    }
    const auto prompt=replaceAll?
        "Replace all matches in the workspace? Unsaved editor buffers will be modified without saving; closed files will be changed on disk. Make a backup or commit first.":
        "Replace the selected match in the editor?";
    if(Rmessage_box(window_,"Confirm workspace replacement",prompt,R_MESSAGE_YES_NO)!=R_MESSAGE_RESULT_YES)return;
    if(!replaceAll){
        const auto m=workspaceMatches_.matches[static_cast<std::size_t>(selected)];
        // Always open the result into an editor for undoable single replacements.
        opened(m.path);
        for(const auto& d:docs_){
            std::error_code ec;
            if(d->kind!=DocumentKind::Text||d->path.empty()||!fs::equivalent(d->path,m.path,ec)||ec)continue;
            const char* p=Rtext_editor_get_text(d->editor);std::string source=p?p:"";
            if(source.size()<m.byteOffset+m.matchLength){setStatus("File changed; refresh results");return;}
            std::string replacementText;std::size_t count=0;std::string error;
            if(!search::replaceText(std::string_view(source).substr(m.byteOffset,m.matchLength),workspaceOptions_,
                                    replacement,replacementText,count,&error)||count!=1){
                setStatus(error.empty()?"Match changed; refresh search results":error);return;
            }
            const auto [l,c]=lineCol(source,m.byteOffset);
            Rtab_view_show_tab(tabs_,d->page);
            Rtext_editor_select(d->editor,l,c,l,c+static_cast<int>(m.matchLength));
            Rtext_editor_insert_text(d->editor,replacementText.c_str());
            setStatus("Replaced selected match (unsaved; Ctrl+S to save)");
            searchWorkspaceFiles();return;
        }
        setStatus("Selected file is not editable");return;
    }
    std::set<fs::path> files;
    for(const auto& m:workspaceMatches_.matches)files.insert(m.path);
    std::size_t changed=0,replaced=0,failed=0;
    for(const auto& path:files){
        Document* openedDoc=nullptr;
        for(auto& d:docs_){
            if(d->path.empty())continue;
            std::error_code ec;
            if(fs::equivalent(d->path,path,ec)&&!ec){openedDoc=d.get();break;}
        }
        if(openedDoc){
            if(openedDoc->kind!=DocumentKind::Text||openedDoc->read_only){++failed;continue;}
            const char* raw=Rtext_editor_get_text(openedDoc->editor);
            const std::string current=raw?raw:"";
            std::string output,error;std::size_t count=0;
            if(!search::replaceText(current,workspaceOptions_,replacement,output,count,&error)){++failed;continue;}
            if(count){Rtext_editor_select_all(openedDoc->editor);Rtext_editor_insert_text(openedDoc->editor,output.c_str());++changed;replaced+=count;}
        }else{
            std::size_t count=0;std::string error;
            if(search::replaceFile(path,workspaceOptions_,replacement,count,&error)){
                if(count){++changed;replaced+=count;}
            }else{++failed;std::clog<<"[WorkspaceSearch][Replace] "<<path<<": "<<error<<'\n';}
        }
    }
    searchWorkspaceFiles();
    setStatus("Replaced "+std::to_string(replaced)+" matches in "+std::to_string(changed)+
              " files ("+std::to_string(failed)+" skipped). Open buffers need saving.");
}
}
