#include "EditorApp.h"
#include "EmbeddedUi.h"
#include "EmbeddedSettingsUi.h"
#include "EmbeddedPluginPromptUi.h"
#include "dodev/plugins/PluginPaths.h"
#include <set>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
namespace dodev {
namespace {
Rid find(Rid win,const char* name){return win==R_INVALID_ID?R_INVALID_ID:Rwidget_find(win,name);}
void setInput(Rid win,const char* name,const std::string& value){if(const auto id=find(win,name);id!=R_INVALID_ID)Rtext_input_set_text(id,value.c_str());}
std::string input(Rid win,const char* name){auto id=find(win,name);if(id==R_INVALID_ID)return {};const char* c=Rtext_input_get_text(id);return c?c:"";}
void check(Rid win,const char* name,bool enabled){if(auto id=find(win,name);id!=R_INVALID_ID)Rcheckbox_set_checked(id,enabled?R_TRUE:R_FALSE);}
bool checked(Rid win,const char* name){auto id=find(win,name);return id!=R_INVALID_ID&&Rcheckbox_is_checked(id)!=R_FALSE;}
void label(Rid win,const char* name,const std::string& value){if(auto id=find(win,name);id!=R_INVALID_ID)Rlabel_set_text(id,value.c_str());}
int number(Rid win,const char* key,int def){try{return std::stoi(input(win,key));}catch(...){return def;}}
std::string className(std::string stem){if(stem.empty())return "NewClass";for(char& c:stem)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='_')c='_';if(std::isdigit(static_cast<unsigned char>(stem[0])))stem='_'+stem;return stem;}
std::string isoDate(){ // stable placeholder without depending on wall-clock/timezone configuration
    std::time_t now=std::time(nullptr);std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm,&now);
#else
    localtime_r(&now,&tm);
#endif
    char buf[32];std::strftime(buf,sizeof(buf),"%Y-%m-%d",&tm);return buf;
}
std::filesystem::path defaultThemeDir(const SettingsManager& s){const auto selected=s.getString("appearance.customThemesDirectory");return selected.empty()?s.directory()/"themes":std::filesystem::u8path(selected);}
}
void EditorApp::loadUserThemes(){
    std::vector<std::string> warnings;
    themeRegistry_.importDirectory(defaultThemeDir(settings_),&warnings);
    for(const auto& warning:warnings)std::cerr<<"Theme: "<<warning<<'\n';
}
void EditorApp::refreshThemeChoices(){
    if(themes_==R_INVALID_ID)return;
    themeNames_.clear();Rcombo_box_clear_items(themes_);
    // Preserve embedded theme order, then append custom/plugin themes.
    for(const auto& theme:ui::kThemes){
        if(themeRegistry_.find(std::string(theme.name))){themeNames_.push_back(std::string(theme.name));Rcombo_box_add_item(themes_,std::string(theme.name).c_str());}
    }
    for(const auto& name:themeRegistry_.names()){
        if(std::find(themeNames_.begin(),themeNames_.end(),name)==themeNames_.end()){themeNames_.push_back(name);Rcombo_box_add_item(themes_,name.c_str());}
    }
    auto it=std::find(themeNames_.begin(),themeNames_.end(),theme_);
    Rcombo_box_set_selected(themes_,it==themeNames_.end()?0:static_cast<int>(it-themeNames_.begin()));
}
void EditorApp::applyFonts(){
    auto readFont=[](const std::string& path)->Rid{
        if(path.empty())return R_INVALID_ID;
        std::error_code ec;if(!fs::is_regular_file(fs::u8path(path),ec))return R_INVALID_ID;
        return Rfont_create_from_file(path.c_str());
    };
    const Rid nextEditor=readFont(settings_.getString("editor.fontFile"));
    const Rid nextUi=readFont(settings_.getString("appearance.uiFontFile"));
    const Rid nextTerminal=readFont(settings_.getString("terminal.fontFile"));
    const Rid oldEditor=editorFont_,oldUi=uiFont_,oldTerminal=terminalFont_;
    editorFont_=nextEditor;uiFont_=nextUi;terminalFont_=nextTerminal;
    Rtheme_set_font(uiFont_!=R_INVALID_ID?uiFont_:Rfont_get_default());
    Rtheme_set_metric(R_METRIC_FONT_SIZE,static_cast<float>(settings_.getInt("appearance.uiFontSize",13)));
    if(!docs_.empty())applyEditorOptions();
    for(const auto& t:terminals_)if(Rwidget_is_valid(t->canvas))Rwidget_invalidate(t->canvas);
    if(oldEditor!=R_INVALID_ID&&oldEditor!=editorFont_)Rfont_destroy(oldEditor);
    if(oldUi!=R_INVALID_ID&&oldUi!=uiFont_)Rfont_destroy(oldUi);
    if(oldTerminal!=R_INVALID_ID&&oldTerminal!=terminalFont_)Rfont_destroy(oldTerminal);
}
void EditorApp::applyEditorOptions(){
    for(const auto& d:docs_){
        if(d->editor==R_INVALID_ID||!Rwidget_is_valid(d->editor))continue;
        Rtext_editor_set_font(d->editor,editorFont_,static_cast<float>(settings_.getInt("editor.fontSize",14)));
        Rtext_editor_set_tab_size(d->editor,settings_.getInt("editor.tabSize",4));
        Rtext_editor_set_line_numbers(d->editor,settings_.getBool("editor.lineNumbers",true)?R_TRUE:R_FALSE);
        Rtext_editor_set_tab_inserts_spaces(d->editor,settings_.getBool("editor.insertSpaces",true)?R_TRUE:R_FALSE);
        Rtext_editor_set_word_wrap(d->editor,settings_.getBool("editor.wordWrap",false)?R_TRUE:R_FALSE);
    }
}
void EditorApp::openSettings(){
    if(settingsWindow_!=R_INVALID_ID&&Rwindow_is_valid(settingsWindow_)){Rwindow_show(settingsWindow_);return;}
    settingsWindow_=Rwindow_create_from_xml(ui::kSettingsWindow.data(),ui::kSettingsWindow.size());
    if(settingsWindow_==R_INVALID_ID){setStatus("Cannot create settings window");return;}
    Rcombo_box_add_item(find(settingsWindow_,"settings_theme_format"),"json");
    Rcombo_box_add_item(find(settingsWindow_,"settings_theme_format"),"xml");
    Rtext_editor_set_text(find(settingsWindow_,"settings_font_preview"),"#include <string>\nint main() {\n    std::string message = \"DoDevEditor Raven 3\";\n    return 0;\n}\n");
    Rtext_editor_set_read_only(find(settingsWindow_,"settings_font_preview"),R_TRUE);
    Rlist_view_set_selection_changed_callback(find(settingsWindow_,"settings_plugin_list"),&EditorApp::pluginListCb,this);
    Rcombo_box_set_changed_callback(find(settingsWindow_,"settings_plugin_key"),&EditorApp::pluginKeyCb,this);
    Rlist_view_set_selection_changed_callback(find(settingsWindow_,"kb_command_list"),&EditorApp::keybindingSelectCb,this);
    Rtext_input_set_changed_callback(find(settingsWindow_,"kb_filter"),&EditorApp::keybindingFilterCb,this);
    Rwidget_set_event_callback(find(settingsWindow_,"kb_shortcut"),&EditorApp::keybindingEventCb,this);
    fillSettings();refreshKeybindingRows();Rwindow_show(settingsWindow_);
}
void EditorApp::fillSettings(){if(settingsWindow_==R_INVALID_ID||!Rwindow_is_valid(settingsWindow_))return;
    label(settingsWindow_,"settings_config_directory","Configuration: "+settings_.directory().string());
    check(settingsWindow_,"settings_restore",settings_.getBool("general.restoreSession",true));
    check(settingsWindow_,"settings_auto_save",settings_.getBool("general.autoSave"));
    setInput(settingsWindow_,"settings_editor_font",settings_.getString("editor.fontFile"));
    setInput(settingsWindow_,"settings_editor_size",std::to_string(settings_.getInt("editor.fontSize",14)));
    setInput(settingsWindow_,"settings_tab_size",std::to_string(settings_.getInt("editor.tabSize",4)));
    check(settingsWindow_,"settings_wrap",settings_.getBool("editor.wordWrap"));
    check(settingsWindow_,"settings_line_numbers",settings_.getBool("editor.lineNumbers",true));
    check(settingsWindow_,"settings_insert_spaces",settings_.getBool("editor.insertSpaces",true));
    setInput(settingsWindow_,"settings_ui_font",settings_.getString("appearance.uiFontFile"));
    setInput(settingsWindow_,"settings_ui_size",std::to_string(settings_.getInt("appearance.uiFontSize",13)));
    Rcombo_box_set_selected(find(settingsWindow_,"settings_theme_format"),settings_.getString("appearance.preferredThemeFormat")=="xml"?1:0);
    setInput(settingsWindow_,"settings_theme_directory",settings_.getString("appearance.customThemesDirectory",(settings_.directory()/"themes").string()));
    setInput(settingsWindow_,"settings_terminal_font",settings_.getString("terminal.fontFile"));
    setInput(settingsWindow_,"settings_terminal_size",std::to_string(settings_.getInt("terminal.fontSize",14)));
    setInput(settingsWindow_,"settings_scrollback",std::to_string(settings_.getInt("terminal.scrollbackLines",3000)));
    setInput(settingsWindow_,"settings_shell",settings_.getString("terminal.defaultShell"));
    setInput(settingsWindow_,"settings_templates_directory",settings_.getString("templates.directory",(settings_.directory()/"templates").string()));
    setInput(settingsWindow_,"settings_plugin_directory",settings_.getString("plugins.directory",(settings_.directory()/"plugins").string()));
    check(settingsWindow_,"settings_plugins_enabled",settings_.getBool("plugins.enabled",true));
    check(settingsWindow_,"settings_safe_mode",settings_.getBool("plugins.safeMode"));
    populatePluginSettings();
    refreshKeybindingRows();
}
void EditorApp::applySettings(bool reset){
    if(settingsWindow_==R_INVALID_ID||!Rwindow_is_valid(settingsWindow_))return;
    if(reset){settings_.data()=SettingsManager::defaultSettings();theme_="Music Nocturne";}
    else {
        settings_.set("general.restoreSession",checked(settingsWindow_,"settings_restore"));
        settings_.set("general.autoSave",checked(settingsWindow_,"settings_auto_save"));
        settings_.set("editor.fontFile",input(settingsWindow_,"settings_editor_font"));
        settings_.set("editor.fontSize",number(settingsWindow_,"settings_editor_size",14));
        settings_.set("editor.tabSize",number(settingsWindow_,"settings_tab_size",4));
        settings_.set("editor.wordWrap",checked(settingsWindow_,"settings_wrap"));
        settings_.set("editor.lineNumbers",checked(settingsWindow_,"settings_line_numbers"));
        settings_.set("editor.insertSpaces",checked(settingsWindow_,"settings_insert_spaces"));
        settings_.set("appearance.uiFontFile",input(settingsWindow_,"settings_ui_font"));
        settings_.set("appearance.uiFontSize",number(settingsWindow_,"settings_ui_size",13));
        settings_.set("appearance.preferredThemeFormat",Rcombo_box_get_selected(find(settingsWindow_,"settings_theme_format"))==1?"xml":"json");
        settings_.set("appearance.customThemesDirectory",input(settingsWindow_,"settings_theme_directory"));
        settings_.set("terminal.fontFile",input(settingsWindow_,"settings_terminal_font"));
        settings_.set("terminal.fontSize",number(settingsWindow_,"settings_terminal_size",14));
        settings_.set("terminal.scrollbackLines",number(settingsWindow_,"settings_scrollback",3000));
        settings_.set("terminal.defaultShell",input(settingsWindow_,"settings_shell"));
        settings_.set("templates.directory",input(settingsWindow_,"settings_templates_directory"));
        settings_.set("plugins.directory",input(settingsWindow_,"settings_plugin_directory"));
        settings_.set("plugins.enabled",checked(settingsWindow_,"settings_plugins_enabled"));
        settings_.set("plugins.safeMode",checked(settingsWindow_,"settings_safe_mode"));
    }
    // Bound the numeric settings to safe values even when entered manually.
    settings_.set("editor.fontSize",std::clamp(settings_.getInt("editor.fontSize",14),8,64));
    settings_.set("editor.tabSize",std::clamp(settings_.getInt("editor.tabSize",4),1,16));
    settings_.set("terminal.fontSize",std::clamp(settings_.getInt("terminal.fontSize",14),8,48));
    settings_.set("terminal.scrollbackLines",std::clamp(settings_.getInt("terminal.scrollbackLines",3000),100,200000));
    applyFonts();applyEditorOptions();
    for(const auto& terminal:terminals_)terminal->screen.setScrollbackLimit(static_cast<size_t>(settings_.getInt("terminal.scrollbackLines",3000)));
    const auto temDir=defaultThemeDir(settings_);std::vector<std::string> warnings;
    themeRegistry_.importDirectory(temDir,&warnings);
    for(const auto& name:themeRegistry_.names())if(const auto* theme=themeRegistry_.find(name)){
        const auto xml=theme->toXml();Rtheme_load_from_memory(xml.c_str(),xml.size());
    }
    refreshThemeChoices();
    if(reset){auto it=std::find(themeNames_.begin(),themeNames_.end(),theme_);if(it!=themeNames_.end())applyTheme(static_cast<int>(it-themeNames_.begin()));}
    const auto templateDir=settings_.getString("templates.directory");
    templateRegistry_.loadDirectory(templateDir.empty()?settings_.directory()/"templates":fs::u8path(templateDir),&warnings);
    savePrefs();
    if(!settings_.maySave())label(settingsWindow_,"settings_status","Settings file is malformed. Rename/repair settings.json before saving.");
    else label(settingsWindow_,"settings_status","Applied. Plugin activation changes require restart.");
    if(reset)fillSettings();
}
void EditorApp::browseSetting(const std::string& action){if(settingsWindow_==R_INVALID_ID)return;
    std::string control;bool folder=false;
    if(action=="settings.browse_editor_font")control="settings_editor_font";
    else if(action=="settings.browse_ui_font")control="settings_ui_font";
    else if(action=="settings.browse_terminal_font")control="settings_terminal_font";
    else if(action=="settings.browse_themes"){control="settings_theme_directory";folder=true;}
    else if(action=="settings.browse_templates"){control="settings_templates_directory";folder=true;}
    else if(action=="settings.browse_plugins"){control="settings_plugin_directory";folder=true;}
    if(control.empty())return;
    RFileDialogDesc desc{};desc.parent=settingsWindow_;desc.title=folder?"Choose directory":"Choose font file";
    const RFileFilter filters[]={{"TrueType/OpenType","*.ttf;*.otf"}};desc.filters=filters;desc.filter_count=folder?0:1;
    const char* result=folder?Rfile_dialog_select_folder(&desc):Rfile_dialog_open(&desc);
    if(result&&*result)setInput(settingsWindow_,control.c_str(),result);
}
void EditorApp::populatePluginSettings(){
    if(settingsWindow_==R_INVALID_ID||!Rwindow_is_valid(settingsWindow_))return;
    const auto list=find(settingsWindow_,"settings_plugin_list");if(list==R_INVALID_ID)return;
    Rlist_view_clear_items(list);pluginCandidates_.clear();
    const auto configured=input(settingsWindow_,"settings_plugin_directory");
    const fs::path userRoot=configured.empty()?settings_.directory()/"plugins":fs::u8path(configured);
    std::set<std::string> seen;
    // Include installed plugins, even when they aren't enabled or the user
    // has never created a plugin folder; make them available in Settings.
    for(const auto& record:pluginManager_.records()){
        const auto& info=record.manifest;
        if(!seen.insert(info.id).second)continue;
        Rlist_view_add_item(list,(info.name+"  ["+info.id+"]  ·  "+record.status).c_str());
        pluginCandidates_.push_back(record.status.rfind("Invalid",0)==0?"":info.id);
    }
    for(const auto& root:std::vector<fs::path>{userRoot,bundledPluginDirectory()}){
        std::error_code ec;
        if(!fs::is_directory(root,ec))continue;
        for(const auto& f:fs::directory_iterator(root,ec)){
            if(!f.is_directory()||!fs::exists(f.path()/"plugin.json"))continue;
            PluginManifest info;std::string error;
            if(!PluginManager::readManifest(f.path(),info,&error)){
                const auto id=f.path().filename().string();
                if(!seen.insert(id).second)continue;
                Rlist_view_add_item(list,("Invalid · "+id+" · "+error).c_str());
                pluginCandidates_.push_back("");
            }else if(seen.insert(info.id).second){
                const auto state=pluginManager_.isEnabled(info.id)?"Will load on restart":"Disabled";
                Rlist_view_add_item(list,(info.name+"  ["+info.id+"]  ·  "+state).c_str());
                pluginCandidates_.push_back(info.id);
            }
        }
    }
    if(!pluginCandidates_.empty()){Rlist_view_set_selected(list,0);updatePluginSettingChoice();}
}
void EditorApp::pluginListCb(Rid,int,void* ptr){static_cast<EditorApp*>(ptr)->updatePluginSettingChoice();}
void EditorApp::pluginKeyCb(Rid,int,void* ptr){
    auto* app=static_cast<EditorApp*>(ptr);
    const auto win=app->settingsWindow_;
    if(win==R_INVALID_ID)return;
    const int selected=Rlist_view_get_selected(find(win,"settings_plugin_list"));
    if(selected<0||selected>=static_cast<int>(app->pluginCandidates_.size()))return;
    const auto combo=find(win,"settings_plugin_key");
    const int keyIndex=Rcombo_box_get_selected(combo);
    if(keyIndex<0)return;
    const char* key=Rcombo_box_get_item_text(combo,keyIndex);
    setInput(win,"settings_plugin_value",app->pluginManager_.pluginSetting(app->pluginCandidates_[static_cast<size_t>(selected)],key?key:""));
}
void EditorApp::updatePluginSettingChoice(){
    const auto win=settingsWindow_;if(win==R_INVALID_ID)return;
    const int index=Rlist_view_get_selected(find(win,"settings_plugin_list"));if(index<0||index>=static_cast<int>(pluginCandidates_.size()))return;
    const auto& id=pluginCandidates_[static_cast<size_t>(index)];const auto combo=find(win,"settings_plugin_key");
    const std::string prev=Rcombo_box_get_selected(combo)>=0?Rcombo_box_get_item_text(combo,Rcombo_box_get_selected(combo)):"";
    Rcombo_box_clear_items(combo);
    for(const auto& spec:pluginManager_.settingSpecs())if(spec.owner==id)Rcombo_box_add_item(combo,spec.key.c_str());
    int selected=0;for(int i=0;i<Rcombo_box_get_item_count(combo);++i)if(prev==Rcombo_box_get_item_text(combo,i))selected=i;
    Rcombo_box_set_selected(combo,Rcombo_box_get_item_count(combo)?selected:-1);
    if(Rcombo_box_get_item_count(combo)){
        const auto* name=Rcombo_box_get_item_text(combo,std::max(0,Rcombo_box_get_selected(combo)));
        setInput(win,"settings_plugin_value",pluginManager_.pluginSetting(id,name?name:""));
    }else setInput(win,"settings_plugin_value","");
}
void EditorApp::pluginAction(const std::string& action){
    if(action.rfind("plugin.view.",0)==0){openPluginView(action.substr(12));return;}
    if(settingsWindow_==R_INVALID_ID||!Rwindow_is_valid(settingsWindow_))return;
    const int index=Rlist_view_get_selected(find(settingsWindow_,"settings_plugin_list"));
    const std::string id=index>=0&&index<static_cast<int>(pluginCandidates_.size())?pluginCandidates_[static_cast<size_t>(index)]:"";
    if(action=="plugin.rescan"){populatePluginSettings();return;}
    if(id.empty())return;
    if(action=="plugin.enable"||action=="plugin.disable"){
        pluginManager_.setEnabled(id,action=="plugin.enable");savePrefs();populatePluginSettings();
        label(settingsWindow_,"settings_status","Plugin "+id+" updated. Restart to apply.");
    }else if(action=="plugin.setting.save"){
        auto combo=find(settingsWindow_,"settings_plugin_key");int keyIndex=Rcombo_box_get_selected(combo);
        if(keyIndex<0)return;const char* key=Rcombo_box_get_item_text(combo,keyIndex);
        if(key&&pluginManager_.updatePluginSetting(id,key,input(settingsWindow_,"settings_plugin_value")))
            label(settingsWindow_,"settings_status","Plugin setting saved to JSON: "+id+" / "+key);
    }
}
void EditorApp::openTemplateWindow(){
    if(templateWindow_!=R_INVALID_ID&&Rwindow_is_valid(templateWindow_)){Rwindow_show(templateWindow_);return;}
    templateWindow_=Rwindow_create_from_xml(ui::kTemplateWindow.data(),ui::kTemplateWindow.size());
    if(templateWindow_==R_INVALID_ID)return;
    const auto combo=find(templateWindow_,"template_selection");templateIds_.clear();
    for(const auto& t:templateRegistry_.all()){templateIds_.push_back(t.id);Rcombo_box_add_item(combo,(t.name+" ("+t.extension+")").c_str());}
    Rcombo_box_set_changed_callback(combo,&EditorApp::templateSelectCb,this);
    Rtext_input_set_committed_callback(find(templateWindow_,"template_filename"),&EditorApp::templateFilenameCb,this);
    if(!templateIds_.empty()){Rcombo_box_set_selected(combo,0);templatePreview();}
    Rwindow_show(templateWindow_);
}
void EditorApp::templateSelectCb(Rid,int,void* ptr){static_cast<EditorApp*>(ptr)->templatePreview();}
void EditorApp::templateFilenameCb(Rid,const char*,void* ptr){
    auto* app=static_cast<EditorApp*>(ptr);
    const auto name=input(app->templateWindow_,"template_filename");
    const auto ext=fs::u8path(name).extension().string();
    const auto id=app->templateRegistry_.associatedId(ext,app->settings_.get("templates.associations"));
    auto it=std::find(app->templateIds_.begin(),app->templateIds_.end(),id);
    if(it!=app->templateIds_.end()){
        Rcombo_box_set_selected(find(app->templateWindow_,"template_selection"),static_cast<int>(it-app->templateIds_.begin()));
        app->templatePreview();
    }
}
void EditorApp::templatePreview(){
    if(templateWindow_==R_INVALID_ID)return;const auto idx=Rcombo_box_get_selected(find(templateWindow_,"template_selection"));
    if(idx<0||idx>=static_cast<int>(templateIds_.size()))return;
    if(const auto* t=templateRegistry_.find(templateIds_[static_cast<size_t>(idx)])){
        if(input(templateWindow_,"template_filename").empty())setInput(templateWindow_,"template_filename","NewFile"+t->extension);
        Rtext_editor_set_text(find(templateWindow_,"template_preview"),t->body.c_str());
    }
}
void EditorApp::createTemplateDocument(){
    if(templateWindow_==R_INVALID_ID)return;
    const auto idx=Rcombo_box_get_selected(find(templateWindow_,"template_selection"));
    if(idx<0||idx>=static_cast<int>(templateIds_.size()))return;
    const auto* t=templateRegistry_.find(templateIds_[static_cast<size_t>(idx)]);if(!t)return;
    auto filename=fs::u8path(input(templateWindow_,"template_filename")).filename();
    if(filename.empty())filename=fs::u8path("NewFile"+t->extension);
    const auto file=filename.string(),project=folder_.empty()?std::string("Project"):folder_.filename().string();
    const auto content=templateRegistry_.render(*t,{{"FILE_NAME",file},{"CLASS_NAME",className(filename.stem().string())},{"PROJECT_NAME",project},{"DATE",isoDate()},{"AUTHOR",""}});
    newDocument();if(auto* d=active()){
        d->title=file;Rtab_view_set_tab_title(tabs_,d->page,d->title.c_str());
        Rtext_editor_set_text(d->editor,content.c_str());
        if(const char* language=Rlanguage_for_file(file.c_str()))Rtext_editor_set_language(d->editor,language);
        changed(d->editor);
    }
    Rwindow_destroy(templateWindow_);templateWindow_=R_INVALID_ID;
}
void EditorApp::openThemeDesigner(){
    if(designerWindow_!=R_INVALID_ID&&Rwindow_is_valid(designerWindow_)){Rwindow_show(designerWindow_);return;}
    designerWindow_=Rwindow_create_from_xml(ui::kThemeDesignerWindow.data(),ui::kThemeDesignerWindow.size());
    if(designerWindow_==R_INVALID_ID)return;
    auto combo=find(designerWindow_,"designer_base");for(const auto* text:{"Dark","Light","GlassDark","GlassLight"})Rcombo_box_add_item(combo,text);
    ThemeDefinition theme;if(const auto* selected=themeRegistry_.find(theme_))theme=*selected;
    theme.name="Custom " + theme.name;theme.originalXml.clear();
    setInput(designerWindow_,"designer_name",theme.name);
    Rcombo_box_set_selected(combo,theme.base=="Light"?1:theme.base=="GlassDark"?2:theme.base=="GlassLight"?3:0);
    setInput(designerWindow_,"designer_accent",theme.accent);setInput(designerWindow_,"designer_window",theme.window);
    setInput(designerWindow_,"designer_panel",theme.panel);setInput(designerWindow_,"designer_control",theme.control);
    setInput(designerWindow_,"designer_input",theme.input);setInput(designerWindow_,"designer_text",theme.text);
    setInput(designerWindow_,"designer_separator",theme.separator);setInput(designerWindow_,"designer_radius",std::to_string(theme.cornerRadius));
    setInput(designerWindow_,"designer_keyword",theme.keyword);setInput(designerWindow_,"designer_string",theme.stringColor);
    setInput(designerWindow_,"designer_comment",theme.comment);
    Rtext_editor_set_text(find(designerWindow_,"designer_preview"),"// Preview with your custom widget and syntax colors\nint main() {\n    return 42;\n}\n");
    Rwindow_show(designerWindow_);
}
ThemeDefinition EditorApp::designerDefinition()const{
    ThemeDefinition t;const auto w=designerWindow_;t.name=input(w,"designer_name");
    const auto selected=Rcombo_box_get_selected(find(w,"designer_base"));t.base=selected==1?"Light":selected==2?"GlassDark":selected==3?"GlassLight":"Dark";
    t.accent=input(w,"designer_accent");t.window=input(w,"designer_window");t.panel=input(w,"designer_panel");t.control=input(w,"designer_control");
    t.input=input(w,"designer_input");t.text=input(w,"designer_text");t.separator=input(w,"designer_separator");t.cornerRadius=number(w,"designer_radius",12);
    t.keyword=input(w,"designer_keyword");t.stringColor=input(w,"designer_string");t.comment=input(w,"designer_comment");return t;
}
void EditorApp::themeDesignerAction(const std::string& action){
    if(action=="theme.import"){
        RFileDialogDesc desc{};desc.parent=settingsWindow_!=R_INVALID_ID?settingsWindow_:window_;desc.title="Import custom theme";
        const RFileFilter filter[]={{"Raven themes","*.json;*.xml"}};desc.filters=filter;desc.filter_count=1;
        const auto p=Rfile_dialog_open(&desc);if(!p||!*p)return;std::string error;
        if(!themeRegistry_.importFile(fs::u8path(p),&error)){setStatus(error);return;}
        for(const auto& name:themeRegistry_.names())if(const auto* t=themeRegistry_.find(name)){
            if(t->source==p){const auto xml=t->toXml();Rtheme_load_from_memory(xml.c_str(),xml.size());}
        }
        refreshThemeChoices();setStatus("Theme imported: "+std::string(p));return;
    }
    if(action=="theme.export"){
        RFileDialogDesc desc{};desc.parent=settingsWindow_!=R_INVALID_ID?settingsWindow_:window_;desc.title="Export current theme";
        const auto filename=theme_+(settings_.getString("appearance.preferredThemeFormat")=="xml"?".xml":".json");desc.file_name=filename.c_str();
        const auto chosen=Rfile_dialog_save(&desc);if(!chosen||!*chosen)return;
        fs::path target=fs::u8path(chosen);if(target.extension().empty())target+=settings_.getString("appearance.preferredThemeFormat")=="xml"?".xml":".json";
        std::string error;if(!themeRegistry_.exportFile(theme_,target,&error))setStatus(error);else setStatus("Exported to: "+target.string());return;
    }
    if(designerWindow_==R_INVALID_ID||!Rwindow_is_valid(designerWindow_))return;
    if(action=="designer.close"){Rwindow_destroy(designerWindow_);designerWindow_=R_INVALID_ID;Rtheme_use(theme_.c_str());applyFonts();return;}
    ThemeDefinition t=designerDefinition();std::string error;
    if(!t.valid(&error)){label(designerWindow_,"designer_status",error);return;}
    // Do not overwrite one of the built-in themes with a preview.
    for(const auto& builtin:ui::kThemes)if(t.name==builtin.name){label(designerWindow_,"designer_status","Choose a new name; embedded themes are read-only.");return;}
    if(action=="designer.preview"||action=="designer.save_json"||action=="designer.save_xml"){
        const auto xml=t.toXml();if(!Rtheme_load_from_memory(xml.c_str(),xml.size())||!Rtheme_use(t.name.c_str())){
            label(designerWindow_,"designer_status","Raven rejected this theme XML");return;
        }
        if(action=="designer.preview"){
            label(designerWindow_,"designer_status","Live preview active. Save to make it persistent.");return;
        }
        const bool asXml=action=="designer.save_xml";const auto ext=asXml?".xml":".json";
        RFileDialogDesc desc{};desc.parent=designerWindow_;desc.title=asXml?"Save Raven theme XML":"Save theme JSON";
        const auto defaultFile=t.name+ext;desc.file_name=defaultFile.c_str();
        const auto themeDirectory=defaultThemeDir(settings_).string();desc.directory=themeDirectory.c_str();
        const char* selected=Rfile_dialog_save(&desc);
        if(!selected||!*selected)return;fs::path destination=fs::u8path(selected);if(destination.extension().empty())destination+=ext;
        if(!themeRegistry_.exportFile(t,destination,&error)){label(designerWindow_,"designer_status",error);return;}
        t.source=destination.string();themeRegistry_.add(t);refreshThemeChoices();
        auto it=std::find(themeNames_.begin(),themeNames_.end(),t.name);if(it!=themeNames_.end())applyTheme(static_cast<int>(it-themeNames_.begin()));
        label(designerWindow_,"designer_status","Saved: "+destination.string());
    }
}
void EditorApp::installPluginHooks(){
    PluginManager::HostHooks hooks;
    hooks.activeDocument=[this](std::string& path,std::string& contents){
        auto* doc=active();
        if(!doc||doc->kind!=DocumentKind::Text||doc->editor==R_INVALID_ID||!Rwidget_is_valid(doc->editor))return false;
        path=doc->path.empty()?doc->title:doc->path.string();
        const auto* txt=Rtext_editor_get_text(doc->editor);contents=txt?txt:"";
        return true;
    };
    hooks.replaceDocument=[this](const std::string& contents){
        auto* doc=active();
        if(!doc||doc->kind!=DocumentKind::Text||doc->read_only||!Rwidget_is_valid(doc->editor))return false;
        const char* current=Rtext_editor_get_text(doc->editor);
        const std::string before=current?current:"";
        if(before==contents)return true;
        Rtext_editor_select_all(doc->editor);
        Rtext_editor_insert_text(doc->editor,contents.c_str()); // triggers changed callback, undoable
        setStatus("Plugin updated active document (Undo available)");return true;
    };
    hooks.terminalCommand=[this](const std::string& command){
        if(command.empty()||command.find('\0')!=std::string::npos||
           command.find('\n')!=std::string::npos||command.find('\r')!=std::string::npos)return false;
        auto* tab=activeTerminal();if(!tab||tab->debugger||!tab->session.running()){newTerminal();tab=activeTerminal();}
        if(!tab)return false;
        const bool sent=tab->session.sendLine(command);
        if(sent){focusTerminal();setStatus("Sent plugin command to terminal");}
        return sent;
    };
    hooks.promptInput=[this](const std::string& title,const std::string& label,const std::string& initial,
                             std::function<void(const std::string&)> completion){
        return showPluginPrompt(title,label,initial,std::move(completion));
    };
    hooks.openSerialMonitor=[this](){return openSerialMonitor();};
    hooks.openSourceSymbols=[this](){return openSymbolsPanel();};
    hooks.openRemoteJournal=[this](){return openRemoteJournal();};
    hooks.openAiAssistant=[this](){return openAiAssistant();};
    hooks.debugContext=[this](std::string& workspace,std::string& file,int& line){return debuggerContext(workspace,file,line);};
    hooks.debugLaunch=[this](const std::string& cwd,const std::vector<std::string>& args){return launchDebugger(cwd,args);};
    hooks.debugControl=[this](const std::string& command){return controlDebugger(command);};
    hooks.debugOpenFile=[this](const std::string& path){opened(fs::u8path(path));return true;};
    hooks.debugStatus=[this](const std::string& message){setStatus(message);log(message+"\n");if(message.rfind("GDB error: ",0)==0)Rmessage_box(window_,"GDB debugger error",message.c_str(),R_MESSAGE_OK);};
    pluginManager_.setHostHooks(std::move(hooks));
}
bool EditorApp::showPluginPrompt(const std::string& title,const std::string& prompt,
                                 const std::string& initial,std::function<void(const std::string&)> completion){
    if(pluginPromptWindow_!=R_INVALID_ID&&Rwindow_is_valid(pluginPromptWindow_))return false;
    pluginPromptWindow_=Rwindow_create_from_xml(ui::kPluginPromptWindow.data(),ui::kPluginPromptWindow.size());
    if(pluginPromptWindow_==R_INVALID_ID)return false;
    Rwindow_set_title(pluginPromptWindow_,title.c_str());
    auto label=Rwidget_find(pluginPromptWindow_,"plugin_prompt_label");
    if(label!=R_INVALID_ID)Rlabel_set_text(label,prompt.c_str());
    auto input=Rwidget_find(pluginPromptWindow_,"plugin_prompt_input");
    if(input!=R_INVALID_ID){Rtext_input_set_text(input,initial.c_str());Rwidget_focus(input);}
    pendingPluginPrompt_=std::move(completion);
    Rwindow_show(pluginPromptWindow_);return true;
}
void EditorApp::closePluginPrompt(bool accept){
    if(pluginPromptWindow_==R_INVALID_ID)return;
    std::string value;
    if(accept){const auto input=Rwidget_find(pluginPromptWindow_,"plugin_prompt_input");
        if(input!=R_INVALID_ID){const char* text=Rtext_input_get_text(input);if(text)value=text;}}
    if(Rwindow_is_valid(pluginPromptWindow_))Rwindow_destroy(pluginPromptWindow_);
    pluginPromptWindow_=R_INVALID_ID;
    auto callback=std::move(pendingPluginPrompt_);pendingPluginPrompt_={};
    if(accept&&callback)callback(value);
}
void EditorApp::registerPluginUi(){
    for(const auto& cmd:pluginManager_.commands())Raction_bind(cmd.id.c_str(),&EditorApp::actionCb,this);
    for(const auto& menu:pluginManager_.menus()){
        // QR capture is a permanent entry in Herramientas, so do not add a
        // second duplicate when the bundled QR plugin is enabled.
        if(menu.command=="qrcode.generate"&&menu.location=="Tools")continue;
        if(menu.command=="gdb.start"&&menu.location=="Run")continue;
        const auto name=menu.location=="File"?"file_menu":menu.location=="Edit"?"edit_menu":menu.location=="View"?"view_menu":menu.location=="Tools"?"tools_menu":menu.location=="Run"?"run_menu":"plugins_menu";
        auto parent=find(window_,name);if(parent==R_INVALID_ID)parent=find(window_,"plugins_menu");
        if(parent!=R_INVALID_ID){auto title=menu.title;auto key=keybindings_.get(menu.command);if(!key.empty())title+="    "+key;Rwidget_set_action(Rmenu_add_item(parent,title.c_str()),menu.command.c_str());}
    }
    const auto pluginsMenu=find(window_,"plugins_menu");
    for(const auto& view:pluginManager_.views()){
        const auto action="plugin.view."+view.id;
        Raction_bind(action.c_str(),&EditorApp::actionCb,this);
        if(pluginsMenu!=R_INVALID_ID)Rwidget_set_action(Rmenu_add_item(pluginsMenu,("Open "+view.title).c_str()),action.c_str());

    }
    pluginManager_.emit("app.ready","{}");
}
namespace {struct BuildCtx{Rid box;};}
void EditorApp::pluginUiLabel(void* context,const char* text){auto* ui=static_cast<BuildCtx*>(context);Rwidget_add_child(ui->box,Rlabel_create(text?text:""));}
void EditorApp::pluginUiButton(void* context,const char* title,const char* command){
    auto* ui=static_cast<BuildCtx*>(context);auto btn=Rbutton_create(title?title:"Button");Rwidget_add_child(ui->box,btn);
    if(command&&*command)Rwidget_set_action(btn,command);
}
void EditorApp::pluginUiText(void* context,const char* text){
    auto* ui=static_cast<BuildCtx*>(context);auto editor=Rtext_editor_create(text?text:"");Rtext_editor_set_read_only(editor,R_TRUE);
    Rwidget_add_child(ui->box,editor);Rslot_set_fill(editor,1.0f);
}
void EditorApp::openPluginView(const std::string& id){
    const PluginView* view=nullptr;for(const auto& v:pluginManager_.views())if(v.id==id){view=&v;break;}if(!view)return;
    auto& existing=view->isPanel?pluginPanels_:pluginTabs_;auto it=existing.find(id);
    if(it!=existing.end()&&Rwidget_is_valid(it->second)){
        if(view->isPanel)Rdock_area_open_panel(dock_,it->second);else Rtab_view_show_tab(tabs_,it->second);return;
    }
    const auto panel=Rwidget_create(R_WIDGET_VBOX);
    Rwidget_set_class(panel,"surface music-surface");
    if(view->factory){BuildCtx context{panel};DodevUiBuilder builder{sizeof(DodevUiBuilder),&context,&EditorApp::pluginUiLabel,&EditorApp::pluginUiButton,&EditorApp::pluginUiText};
        pluginManager_.buildView(*view,builder);
    }else Rwidget_add_child(panel,Rlabel_create(("Plugin: "+view->owner).c_str()));
    if(view->isPanel){
        const auto side=view->side=="Left"?R_DOCK_LEFT:view->side=="Bottom"?R_DOCK_BOTTOM:view->side=="Tab"?R_DOCK_TAB:R_DOCK_RIGHT;
        Rdock_area_add_panel(dock_,panel,view->title.c_str(),side,view->side=="Tab"?explorer_:R_INVALID_ID,0.25f);
        Rdock_area_open_panel(dock_,panel);
    }else{Rtab_view_add_tab(tabs_,view->title.c_str(),panel);Rtab_view_show_tab(tabs_,panel);}
    existing[id]=panel;
}
} // namespace dodev
