#include "AiProfiles.h"
#include <algorithm>
#include <set>

namespace dodev::ai {
namespace {
std::string property(const Json& obj,const char* key,const std::string& fallback={}){
    const auto* p=obj.find(key);return p?p->string(fallback):fallback;
}
int numeric(const Json& obj,const char* key,int fallback){const auto* p=obj.find(key);return p?p->integer(fallback):fallback;}
}
std::vector<ProviderProfile> defaultProfiles(){
    ProviderProfile llama;llama.id="local-llamacpp";llama.name="llama.cpp (local)";llama.kind="llama.cpp";
    llama.config.endpoint="http://127.0.0.1:8080/v1/chat/completions";
    llama.config.model="";llama.config.authMode="none";llama.config.keyEnvironment="";
    ProviderProfile ollama;ollama.id="local-ollama";ollama.name="Ollama (local)";ollama.kind="ollama";
    ollama.config.endpoint="http://127.0.0.1:11434/v1/chat/completions";
    ollama.config.model="";ollama.config.authMode="none";ollama.config.keyEnvironment="";
    return {llama,ollama};
}
bool validateProfile(const ProviderProfile& profile,std::string* reason){
    if(profile.id.empty()||profile.id.size()>80||profile.name.empty()||profile.name.size()>100){
        if(reason)*reason="Profile identifier/name is missing or too long";return false;
    }
    for(unsigned char c:profile.id)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_')){
        if(reason)*reason="Profile identifier has invalid characters";return false;
    }
    if(profile.kind!="llama.cpp"&&profile.kind!="ollama"&&profile.kind!="remote"){
        if(reason)*reason="Provider type must be llama.cpp, ollama or remote";return false;
    }
    if(!profile.config.endpoint.empty()&&!validEndpoint(profile.config.endpoint,reason))return false;
    if(!profile.modelsEndpoint.empty()&&!validEndpoint(profile.modelsEndpoint,reason))return false;
    if(profile.config.authMode!="none"&&profile.config.authMode!="bearer"&&profile.config.authMode!="auto"){
        if(reason)*reason="Authentication must be none, bearer, or auto";return false;
    }
    if(profile.config.keyEnvironment.find_first_of("\r\n")!=std::string::npos){
        if(reason)*reason="Invalid API key variable name";return false;
    }
    return true;
}
Json profilesToJson(const std::vector<ProviderProfile>& profiles){
    Json::Array array;
    for(const auto& p:profiles){
        Json item=Json::object();item["id"]=p.id;item["name"]=p.name;item["kind"]=p.kind;
        item["endpoint"]=p.config.endpoint;item["modelsEndpoint"]=p.modelsEndpoint;
        item["model"]=p.config.model;item["authMode"]=p.config.authMode;
        item["keyEnvironment"]=p.config.keyEnvironment;
        item["timeoutSeconds"]=p.config.timeoutSeconds;
        item["maxContextBytes"]=static_cast<double>(p.config.maxContextBytes);
        array.push_back(std::move(item));
    }
    return Json(std::move(array));
}
bool profilesFromJson(const Json& json,std::vector<ProviderProfile>& profiles,std::string* reason){
    if(!json.isArray()||json.asArray().size()>32){if(reason)*reason="Provider profiles must be a JSON array (max 32)";return false;}
    std::vector<ProviderProfile> parsed;std::set<std::string> seen;
    for(const auto& item:json.asArray()){
        if(!item.isObject()){if(reason)*reason="Provider profile must be an object";return false;}
        ProviderProfile p;p.id=property(item,"id");p.name=property(item,"name");
        p.kind=property(item,"kind","remote");p.config.endpoint=property(item,"endpoint");
        p.modelsEndpoint=property(item,"modelsEndpoint");p.config.model=property(item,"model");
        p.config.authMode=property(item,"authMode",p.kind=="remote"?"bearer":"none");
        p.config.keyEnvironment=property(item,"keyEnvironment","OPENAI_API_KEY");
        p.config.timeoutSeconds=std::clamp(numeric(item,"timeoutSeconds",45),5,180);
        p.config.maxContextBytes=static_cast<std::size_t>(std::clamp(numeric(item,"maxContextBytes",16000),1024,64000));
        if(!validateProfile(p,reason)||!seen.insert(p.id).second){
            if(reason&&reason->empty())*reason="Duplicate provider profile ID";return false;
        }
        parsed.push_back(std::move(p));
    }
    if(parsed.empty()){if(reason)*reason="At least one provider profile is required";return false;}
    profiles=std::move(parsed);return true;
}
std::string nextProfileId(const std::vector<ProviderProfile>& profiles,const std::string& prefix){
    for(int n=1;n<=9999;++n){
        auto id=prefix+"-"+std::to_string(n);
        if(std::none_of(profiles.begin(),profiles.end(),[&](const ProviderProfile& p){return p.id==id;}))return id;
    }
    return {};
}
}
