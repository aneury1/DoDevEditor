#include "EditorApp.h"
#include <algorithm>
#include <iostream>

namespace dodev {
namespace {
constexpr const char* kPlugin="com.dodev.aiassistant";
void button(Rid row,const char* title,const char* action){
    const auto id=Rbutton_create(title);Rwidget_set_class(id,"chip");
    Rwidget_set_action(id,action);Rwidget_add_child(row,id);
}
Rid input(Rid row,const char* placeholder,const std::string& initial={}){
    auto id=Rwidget_create(R_WIDGET_TEXT_INPUT);
    Rtext_input_set_placeholder(id,placeholder);
    Rtext_input_set_text(id,initial.c_str());Rwidget_add_child(row,id);
    Rslot_set_fill(id,1);return id;
}
Rid combo(Rid row,const std::vector<std::string>& items){
    auto id=Rwidget_create(R_WIDGET_COMBO_BOX);
    for(const auto& item:items)Rcombo_box_add_item(id,item.c_str());
    if(!items.empty())Rcombo_box_set_selected(id,0);
    Rwidget_add_child(row,id);return id;
}
std::string value(Rid id){
    if(id==R_INVALID_ID||!Rwidget_is_valid(id))return {};
    const auto* str=Rtext_input_get_text(id);return str?str:"";
}
void set(Rid id,const std::string& text){
    if(id!=R_INVALID_ID&&Rwidget_is_valid(id))Rtext_input_set_text(id,text.c_str());
}
void status(Rid id,const std::string& text){
    if(id!=R_INVALID_ID&&Rwidget_is_valid(id))Rlabel_set_text(id,text.c_str());
}
std::string setting(const PluginManager& manager,const char* key,const char* fallback){
    const auto text=manager.pluginSetting(kPlugin,key);
    return text.empty()?fallback:text;
}
int selected(Rid widget){return widget==R_INVALID_ID?-1:Rcombo_box_get_selected(widget);}
}
void EditorApp::aiLoadProfiles(){
    if(aiProfilesLoaded_)return;
    aiProfilesLoaded_=true;
    const auto stored=pluginManager_.pluginSettingJson(kPlugin,"profiles");
    std::string why;
    if(!stored.isArray()||!ai::profilesFromJson(stored,aiProfiles_,&why)){
        aiProfiles_=ai::defaultProfiles();
        // Import the older single-provider settings if they actually existed.
        const auto legacy=pluginManager_.pluginSetting(kPlugin,"endpoint");
        if(!legacy.empty() &&
           (legacy!="http://127.0.0.1:11434/v1/chat/completions" ||
            pluginManager_.pluginSetting(kPlugin,"model")!="llama3.2")){
            ai::ProviderProfile p;
            p.id="legacy-provider";p.name="Previously configured provider";
            p.kind=(legacy.find("127.0.0.1")!=std::string::npos||legacy.find("localhost")!=std::string::npos)
                ?(legacy.find(":11434/")!=std::string::npos?"ollama":"llama.cpp"):"remote";
            p.config.endpoint=legacy;
            p.config.model=pluginManager_.pluginSetting(kPlugin,"model");
            p.config.keyEnvironment=setting(pluginManager_,"keyEnvironment","OPENAI_API_KEY");
            p.config.authMode=p.kind=="remote"?"bearer":"none";
            if(ai::validateProfile(p))aiProfiles_.push_back(std::move(p));
        }
    }
    const auto active=setting(pluginManager_,"activeProfileId","");
    auto it=std::find_if(aiProfiles_.begin(),aiProfiles_.end(),[&](const auto& p){return p.id==active;});
    aiProfileIndex_=it==aiProfiles_.end()?0:static_cast<std::size_t>(it-aiProfiles_.begin());
}
void EditorApp::aiStoreProfileFields(){
    if(aiUpdatingUi_||aiProfileIndex_>=aiProfiles_.size()||aiEndpoint_==R_INVALID_ID)return;
    auto& p=aiProfiles_[aiProfileIndex_];
    const auto name=value(aiProfileName_);
    if(!name.empty())p.name=name;
    switch(selected(aiProviderKind_)){
        case 0:p.kind="llama.cpp";break;
        case 1:p.kind="ollama";break;
        default:p.kind="remote";break;
    }
    p.config.endpoint=value(aiEndpoint_);
    p.modelsEndpoint=value(aiModelsEndpoint_);
    p.config.model=value(aiModel_);
    p.config.keyEnvironment=value(aiKeyEnv_);
    p.config.authMode=selected(aiAuthMode_)==0?"none":"bearer";
    try{p.config.timeoutSeconds=std::clamp(std::stoi(value(aiTimeout_)),5,180);}
    catch(...){p.config.timeoutSeconds=45;}
}
void EditorApp::aiShowProfile(std::size_t index){
    if(index>=aiProfiles_.size())return;
    aiUpdatingUi_=true;aiProfileIndex_=index;
    const auto& profile=aiProfiles_[index];
    if(aiProfileCombo_!=R_INVALID_ID){
        Rcombo_box_clear_items(aiProfileCombo_);
        for(const auto& p:aiProfiles_)Rcombo_box_add_item(aiProfileCombo_,p.name.c_str());
        Rcombo_box_set_selected(aiProfileCombo_,static_cast<int>(index));
    }
    set(aiProfileName_,profile.name);
    Rcombo_box_set_selected(aiProviderKind_,profile.kind=="llama.cpp"?0:profile.kind=="ollama"?1:2);
    set(aiEndpoint_,profile.config.endpoint);set(aiModelsEndpoint_,profile.modelsEndpoint);
    set(aiModel_,profile.config.model);set(aiKeyEnv_,profile.config.keyEnvironment);
    set(aiTimeout_,std::to_string(profile.config.timeoutSeconds));
    Rcombo_box_set_selected(aiAuthMode_,profile.config.authMode=="none"?0:1);
    if(aiModelCombo_!=R_INVALID_ID){
        Rcombo_box_clear_items(aiModelCombo_);
        const auto found=aiModelCache_.find(profile.id);
        if(found!=aiModelCache_.end()){
            for(const auto& model:found->second)Rcombo_box_add_item(aiModelCombo_,model.c_str());
            auto it=std::find(found->second.begin(),found->second.end(),profile.config.model);
            Rcombo_box_set_selected(aiModelCombo_,it==found->second.end()?-1:static_cast<int>(it-found->second.begin()));
        }
    }
    aiUpdatingUi_=false;
    status(aiStatus_,"Profile: "+profile.name+". Refresh Models to query its server; custom model IDs are supported.");
}
void EditorApp::aiProfileChanged(Rid,int index,void* user){
    auto* app=static_cast<EditorApp*>(user);
    if(app->aiUpdatingUi_||index<0||static_cast<std::size_t>(index)>=app->aiProfiles_.size()||
       static_cast<std::size_t>(index)==app->aiProfileIndex_)return;
    app->aiStoreProfileFields();app->aiShowProfile(static_cast<std::size_t>(index));
    // Local model lists are queried without blocking the Raven UI thread.
    if(app->aiProfiles_[app->aiProfileIndex_].kind!="remote")app->aiRefreshModels();
}
void EditorApp::aiModelChanged(Rid,int index,void* user){
    auto* app=static_cast<EditorApp*>(user);
    if(app->aiUpdatingUi_||index<0)return;
    const char* model=Rcombo_box_get_item_text(app->aiModelCombo_,index);
    if(model)set(app->aiModel_,model);
}
bool EditorApp::aiSaveProfiles(){
    aiStoreProfileFields();
    for(const auto& profile:aiProfiles_){
        std::string reason;
        if(!ai::validateProfile(profile,&reason)){
            setStatus("AI profile '"+profile.name+"': "+reason);return false;
        }
    }
    if(!pluginManager_.updatePluginSettingJson(kPlugin,"profiles",ai::profilesToJson(aiProfiles_))||
       !pluginManager_.updatePluginSetting(kPlugin,"activeProfileId",aiProfiles_[aiProfileIndex_].id)){
        setStatus("Could not save AI provider profiles to plugin settings.json");return false;
    }
    std::clog<<"[DoDevEditor][AI] Provider profiles saved: "<<aiProfiles_.size()<<" (credentials not stored)\n";
    return true;
}
void EditorApp::aiRefreshModels(){
    if(aiModelsPending_){status(aiStatus_,"Model discovery is already running");return;}
    aiStoreProfileFields();
    if(aiProfileIndex_>=aiProfiles_.size())return;
    const auto& profile=aiProfiles_[aiProfileIndex_];
    if(profile.config.endpoint.empty()){
        status(aiStatus_,"Configure the provider endpoint before refreshing models");return;
    }
    if(!profile.modelsEndpoint.empty()&&!ai::validEndpoint(profile.modelsEndpoint)){
        status(aiStatus_,"Invalid Models endpoint; remote URLs must use HTTPS");return;
    }
    aiModelsTarget_=profile.id;
    aiModelsCancel_.store(false);aiModelsPending_=true;aiModelsReady_=false;
    status(aiStatus_,"Loading available models from provider...");
    if(aiModelsWorker_.joinable())aiModelsWorker_.join();
    const auto config=profile.config;
    const auto url=profile.modelsEndpoint;
    aiModelsWorker_=std::thread([this,config,url]{
        auto result=ai::listModels(config,url,&aiModelsCancel_);
        std::lock_guard<std::mutex> lock(aiModelsMutex_);
        aiModelsReply_=std::move(result);aiModelsReady_=true;
    });
}
bool EditorApp::openAiAssistant(){
    if(aiPanel_!=R_INVALID_ID&&Rwidget_is_valid(aiPanel_)){
        Rdock_area_open_panel(dock_,aiPanel_);return true;
    }
    aiLoadProfiles();
    aiPanel_=Rwidget_create(R_WIDGET_VBOX);Rwidget_set_class(aiPanel_,"music-surface surface");
    auto header=Rwidget_create(R_WIDGET_HBOX);Rwidget_add_child(aiPanel_,header);
    Rwidget_add_child(header,Rlabel_create("AI ASSISTANT · PROVIDER SETTINGS"));
    button(header,"Save Profile","ai.save");button(header,"Clear Chat","ai.clear");
    auto profiles=Rwidget_create(R_WIDGET_HBOX);Rwidget_add_child(aiPanel_,profiles);
    aiProfileCombo_=combo(profiles,{});
    Rslot_set_fill(aiProfileCombo_,1);
    button(profiles,"+ Remote","ai.new_remote");
    button(profiles,"+ Local","ai.new_local");
    button(profiles,"Delete","ai.delete_profile");
    auto details=Rwidget_create(R_WIDGET_HBOX);Rwidget_add_child(aiPanel_,details);
    aiProfileName_=input(details,"Profile name");
    aiProviderKind_=combo(details,{"llama.cpp","Ollama","Remote (OpenAI-compatible)"});
    auto endpoints=Rwidget_create(R_WIDGET_HBOX);Rwidget_add_child(aiPanel_,endpoints);
    aiEndpoint_=input(endpoints,"Chat endpoint, e.g. http://127.0.0.1:8080/v1/chat/completions");
    auto discover=Rwidget_create(R_WIDGET_HBOX);Rwidget_add_child(aiPanel_,discover);
    aiModelsEndpoint_=input(discover,"Optional custom Models endpoint (/v1/models)");
    button(discover,"Refresh Models","ai.refresh_models");
    auto models=Rwidget_create(R_WIDGET_HBOX);Rwidget_add_child(aiPanel_,models);
    aiModelCombo_=combo(models,{});
    Rslot_set_fill(aiModelCombo_,1);
    aiModel_=input(models,"Model ID (editable / manually entered)");
    auto security=Rwidget_create(R_WIDGET_HBOX);Rwidget_add_child(aiPanel_,security);
    aiAuthMode_=combo(security,{"No authentication","Bearer token from environment"});
    aiKeyEnv_=input(security,"API key environment variable (not the key)");
    aiTimeout_=input(security,"Timeout in seconds (5-180)");
    aiStatus_=Rlabel_create("Ready. No source is sent unless explicitly attached.");
    Rwidget_add_child(aiPanel_,aiStatus_);
    aiChat_=Rtext_editor_create(aiTranscript_.c_str());
    Rtext_editor_set_read_only(aiChat_,R_TRUE);Rwidget_set_class(aiChat_,"music-editor");
    Rwidget_add_child(aiPanel_,aiChat_);Rslot_set_fill(aiChat_,1);
    Rwidget_add_child(aiPanel_,Rlabel_create("Message (sent only when you press Ask)"));
    aiPrompt_=Rtext_editor_create("");Rwidget_set_class(aiPrompt_,"music-editor");
    Rwidget_add_child(aiPanel_,aiPrompt_);
    auto actions=Rwidget_create(R_WIDGET_HBOX);Rwidget_add_child(aiPanel_,actions);
    button(actions,"Ask","ai.ask");button(actions,"Explain File","ai.explain");
    button(actions,"Explain Selection","ai.selection");button(actions,"Review File","ai.review");
    button(actions,"Cancel","ai.cancel");
    Rcombo_box_set_changed_callback(aiProfileCombo_,&EditorApp::aiProfileChanged,this);
    Rcombo_box_set_changed_callback(aiModelCombo_,&EditorApp::aiModelChanged,this);
    aiShowProfile(aiProfileIndex_);
    Rdock_area_add_panel(dock_,aiPanel_,"AI Assistant",R_DOCK_RIGHT,R_INVALID_ID,0.40f);
    Rdock_area_open_panel(dock_,aiPanel_);
    if(aiProfiles_[aiProfileIndex_].kind!="remote")aiRefreshModels();
    return true;
}
void EditorApp::stopAiWorker(){
    aiCancel_.store(true);aiModelsCancel_.store(true);
    if(aiWorker_.joinable())aiWorker_.join();
    if(aiModelsWorker_.joinable())aiModelsWorker_.join();
}
void EditorApp::aiAction(const std::string& action){
    if(action=="ai.open"||action=="ai.configure"){openAiAssistant();return;}
    if(action=="ai.cancel"){
        aiCancel_.store(true);aiModelsCancel_.store(true);
        setStatus("AI request/model refresh cancellation requested");return;
    }
    if(!openAiAssistant())return;
    if(action=="ai.clear"){
        if(aiPending_){setStatus("Cancel the request before clearing chat");return;}
        aiHistory_.clear();aiTranscript_.clear();
        if(Rwidget_is_valid(aiChat_))Rtext_editor_set_text(aiChat_,"");return;
    }
    if(action=="ai.new_remote"||action=="ai.new_local"){
        if(aiProfiles_.size()>=32){setStatus("AI provider profile limit is 32");return;}
        aiStoreProfileFields();
        ai::ProviderProfile p;
        if(action=="ai.new_remote"){
            p.kind="remote";p.id=ai::nextProfileId(aiProfiles_,"remote");
            p.name="Remote "+std::to_string(aiProfiles_.size()+1);
            p.config.endpoint="";p.config.model="";p.config.authMode="bearer";
            p.config.keyEnvironment="OPENAI_API_KEY";
        }else{
            p=ai::defaultProfiles().front();p.id=ai::nextProfileId(aiProfiles_,"llamacpp");
            p.name="Local llama.cpp "+std::to_string(aiProfiles_.size()+1);
        }
        aiProfiles_.push_back(std::move(p));aiShowProfile(aiProfiles_.size()-1);
        status(aiStatus_,"New profile created. Configure it, then click Save Profile.");return;
    }
    if(action=="ai.delete_profile"){
        if(aiProfiles_.size()<=1){setStatus("Cannot delete the last AI provider profile");return;}
        aiProfiles_.erase(aiProfiles_.begin()+static_cast<std::ptrdiff_t>(aiProfileIndex_));
        aiShowProfile(std::min(aiProfileIndex_,aiProfiles_.size()-1));
        if(aiSaveProfiles())status(aiStatus_,"Provider profile removed and saved");return;
    }
    if(action=="ai.save"){
        if(aiSaveProfiles()){
            aiShowProfile(aiProfileIndex_);
            status(aiStatus_,"Provider configuration saved as JSON (no secrets stored)");
        }
        return;
    }
    if(action=="ai.refresh_models"){aiRefreshModels();return;}
    if(action!="ai.ask"&&action!="ai.explain"&&action!="ai.review"&&action!="ai.selection")return;
    if(aiPending_){setStatus("AI request still running");return;}
    aiStoreProfileFields();
    const char* prompt=Rtext_editor_get_text(aiPrompt_);
    std::string question=prompt?prompt:"";
    std::string document;
    if(action!="ai.ask"){
        auto* doc=active();
        if(!doc||doc->kind!=DocumentKind::Text||!Rwidget_is_valid(doc->editor)){
            setStatus("Open a source document first");return;
        }
        const char* txt=action=="ai.selection"?Rtext_editor_get_selected_text(doc->editor):Rtext_editor_get_text(doc->editor);
        const auto text=txt?std::string(txt):std::string();
        if(action=="ai.selection"&&text.empty()){
            setStatus("Select source text before choosing Explain Selection");return;
        }
        document="File: "+doc->title+"\n"+text;
        question=(action=="ai.review"?"Review this code for bugs, safety and correctness. Suggest concrete improvements.":
                 action=="ai.selection"?"Explain the selected source code and its behavior.":
                 "Explain the main functions, control flow and design of this code.")+
                 (question.empty()?std::string():"\nAdditional question: "+question);
    }
    if(question.empty()){setStatus("Type a question before sending it");return;}
    const auto config=aiProfiles_[aiProfileIndex_].config;
    std::string why;
    if(!ai::validEndpoint(config.endpoint,&why)){setStatus(why);return;}
    if(config.model.empty()){setStatus("Select a detected model or enter a model ID");return;}
    if(aiWorker_.joinable())aiWorker_.join();
    aiCancel_.store(false);aiPending_=true;aiReady_=false;
    aiTranscript_+="\nYOU: "+question+"\n";
    if(!document.empty())aiTranscript_+="[Active file explicitly attached: "+std::to_string(document.size())+" bytes]\n";
    if(Rwidget_is_valid(aiChat_))Rtext_editor_set_text(aiChat_,aiTranscript_.c_str());
    Rtext_editor_set_text(aiPrompt_,"");
    status(aiStatus_,"Contacting "+aiProfiles_[aiProfileIndex_].name+"...");
    const auto history=aiHistory_;
    aiWorker_=std::thread([this,config,history,question,document]{
        auto result=ai::complete(config,history,question,document,&aiCancel_);
        std::lock_guard<std::mutex> lock(aiMutex_);aiReply_=std::move(result);aiReady_=true;
    });
    aiHistory_.push_back({"user",question});
}
void EditorApp::refreshAiPanel(){
    if(aiModelsPending_){
        ai::ModelList list;bool ready=false;
        {
            std::lock_guard<std::mutex> lock(aiModelsMutex_);
            if(aiModelsReady_){list=aiModelsReply_;aiModelsReady_=false;ready=true;}
        }
        if(ready){
            if(aiModelsWorker_.joinable())aiModelsWorker_.join();
            aiModelsPending_=false;
            if(list.success){
                aiModelCache_[aiModelsTarget_]=std::move(list.models);
                if(aiProfileIndex_<aiProfiles_.size()&&aiProfiles_[aiProfileIndex_].id==aiModelsTarget_){
                    const auto& models=aiModelCache_[aiModelsTarget_];
                    aiUpdatingUi_=true;
                    Rcombo_box_clear_items(aiModelCombo_);
                    for(const auto& model:models)Rcombo_box_add_item(aiModelCombo_,model.c_str());
                    const auto current=value(aiModel_);
                    auto it=std::find(models.begin(),models.end(),current);
                    if(it!=models.end())Rcombo_box_set_selected(aiModelCombo_,static_cast<int>(it-models.begin()));
                    else if(current.empty()&&!models.empty()){
                        Rcombo_box_set_selected(aiModelCombo_,0);
                        set(aiModel_,models.front());
                        aiProfiles_[aiProfileIndex_].config.model=models.front();
                    }else Rcombo_box_set_selected(aiModelCombo_,-1);
                    aiUpdatingUi_=false;
                    status(aiStatus_,std::to_string(models.size())+" model(s) detected. Select a model or enter its ID.");
                }
            }else{
                status(aiStatus_,"Model discovery: "+list.error+". You can enter a model ID manually.");
                std::clog<<"[DoDevEditor][AI][models] "<<list.error<<"\n";
            }
        }
    }
    if(!aiPending_)return;
    ai::Reply reply;
    {
        std::lock_guard<std::mutex> lock(aiMutex_);
        if(!aiReady_)return;
        reply=aiReply_;aiReady_=false;
    }
    if(aiWorker_.joinable())aiWorker_.join();
    aiPending_=false;
    const auto line=reply.success?reply.answer:"ERROR: "+reply.error;
    if(reply.success)aiHistory_.push_back({"assistant",reply.answer});
    aiTranscript_+="\nASSISTANT: "+line+"\n";
    if(aiTranscript_.size()>180000)aiTranscript_.erase(0,aiTranscript_.size()-100000);
    if(aiChat_!=R_INVALID_ID&&Rwidget_is_valid(aiChat_))Rtext_editor_set_text(aiChat_,aiTranscript_.c_str());
    status(aiStatus_,reply.success?"Completed":"Request failed; check provider settings");
    if(!reply.success)std::clog<<"[DoDevEditor][AI] "<<reply.error<<"\n";
}
}
