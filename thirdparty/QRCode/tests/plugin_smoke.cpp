#include "dodev/sdk/dodev_plugin_api.h"
#include <stdexcept>
void require(bool condition,const char* reason){if(!condition)throw std::runtime_error(reason);}
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <dlfcn.h>

namespace fs=std::filesystem;
struct Command{DodevCommandFn fn;void* data;};
struct Mock{
    std::map<std::string,std::string> settings;
    std::map<std::string,Command> commands;
    std::vector<std::string> logs;
    std::string scratch;
    std::string docPath;
    std::string text;
    std::string output;
    int prompts=0;
    int menus=0;
    int tabs=0;
    DodevHostAPI* host=nullptr;
};
void log(void* ctx,int,const char* msg){static_cast<Mock*>(ctx)->logs.push_back(msg?msg:"");}
int command(void* ctx,const char* id,const char*,DodevCommandFn fn,void* user){
    static_cast<Mock*>(ctx)->commands[id]={fn,user};return 1;
}
int menu(void* ctx,const char*,const char*,const char*){static_cast<Mock*>(ctx)->menus++;return 1;}
int tab(void* ctx,const char*,const char*,DodevViewFn,void*){static_cast<Mock*>(ctx)->tabs++;return 1;}
int panel(void*,const char*,const char*,const char*,DodevViewFn,void*){return 1;}
int other(void*,const char*,const char*,const char*,const char*){return 1;}
int theme(void*,const char*){return 1;}
int setting(void* ctx,const char* key,const char* def,const char*){auto* m=static_cast<Mock*>(ctx);if(!m->settings.count(key))m->settings[key]=def;return 1;}
const char* get(void* ctx,const char* key){auto* m=static_cast<Mock*>(ctx);auto it=m->settings.find(key);m->scratch=it==m->settings.end()?"":it->second;return m->scratch.c_str();}
int set(void* ctx,const char* key,const char* val){static_cast<Mock*>(ctx)->settings[key]=val;return 1;}
int ev(void*,const char*,DodevEventFn,void*){return 1;}
int doc(void* ctx,const char** path,const char** text){auto* m=static_cast<Mock*>(ctx);*path=m->docPath.c_str();*text=m->text.c_str();return 1;}
int replace(void*,const char*){return 0;}
int terminal(void*,const char*){return 0;}
int prompt(void* ctx,const char*,const char*,const char* initial,DodevPromptFn cb,void* user){
    auto* m=static_cast<Mock*>(ctx);++m->prompts;m->output=initial;cb(m->host,m->output.c_str(),user);return 1;
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    const auto temp=fs::temp_directory_path()/"dodev_qrcode_smoke";
    fs::create_directories(temp);
    Mock mock;
    mock.docPath=(temp/"example.cpp").string();
    mock.text="#include <iostream>\nint main(){std::cout << \"QR CODE\";}\n";
    DodevHostAPI host{sizeof(DodevHostAPI),DODEV_PLUGIN_API_VERSION,&mock,
      log,command,menu,tab,panel,other,theme,setting,get,set,ev,doc,replace,terminal,prompt};
    mock.host=&host;
    void* lib=dlopen(argv[1],RTLD_NOW);
    if(!lib){std::cerr<<dlerror()<<'\n';return 3;}
    auto init=reinterpret_cast<DodevPluginInitFn>(dlsym(lib,"dodev_plugin_init"));
    DodevPluginAPI plugin{sizeof(DodevPluginAPI),DODEV_PLUGIN_API_VERSION,nullptr};
    if(!init||!init(&host,&plugin))return 4;
    require(mock.commands.count("qrcode.generate")&&mock.commands.count("qrcode.png")&&mock.commands.count("qrcode.svg"),"missing QR commands");
    require(mock.menus>=2&&mock.tabs>=1,"missing menu or tab registration");
    auto invoke=[&](const std::string& id){const auto cmd=mock.commands.at(id);cmd.fn(&host,cmd.data);};
    invoke("qrcode.png");
    const auto png=temp/"example.qr.png";
    require(fs::exists(png)&&fs::file_size(png)>300,"PNG export missing");
    {
      std::ifstream in(png,std::ios::binary);char header[8]{};in.read(header,8);
      const char required[8]={static_cast<char>(137),'P','N','G','\r','\n',static_cast<char>(26),'\n'};
      require(std::memcmp(header,required,8)==0,"invalid PNG signature");
    }
    invoke("qrcode.svg");
    const auto svg=temp/"example.qr.svg";
    require(fs::exists(svg),"SVG export missing");
    {
      std::ifstream in(svg);std::string value((std::istreambuf_iterator<char>(in)),{});
      require(value.find("<svg")!=std::string::npos&&value.find("<path")!=std::string::npos,"SVG content missing");
    }
    mock.text=std::string(4000,'X');
    invoke("qrcode.generate");
    bool found=false;
    for(auto& message:mock.logs)if(message.find("capacity of one QR code")!=std::string::npos)found=true;
    require(found,"oversize QR text should be rejected with explicit error");
    require(mock.prompts==3,"prompt count mismatch");
    if(plugin.shutdown)plugin.shutdown();
    dlclose(lib);
    fs::remove_all(temp);
    std::cout<<"PASS: generated valid PNG+SVG from active document, checked oversize errors\n";
}
