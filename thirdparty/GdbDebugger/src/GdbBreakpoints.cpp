#include "GdbBreakpoints.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
namespace dodev::gdb {
namespace {
fs::path store(const Context& c){return c.workspace/".vscode"/"dodev-breakpoints.json";}
fs::path full(const Context& c,const std::string& file){auto p=fs::u8path(file);return (p.is_absolute()?p:c.workspace/p).lexically_normal();}
}
std::string breakpointLocation(const fs::path& file,int line){return quoteGdb(file.u8string())+":"+std::to_string(line);}
std::vector<Breakpoint> loadBreakpoints(const Context& c){
    std::vector<Breakpoint> result;if(!fs::exists(store(c)))return result;
    const auto root=readJsonc(store(c));if(!root["breakpoints"].isArray())throw std::runtime_error("Invalid breakpoint store");
    for(const auto& item:root["breakpoints"].asArray()){
        const auto name=item["file"].string();const int line=item["line"].integer();
        if(name.empty()||line<1||item["line"].number()!=line)throw std::runtime_error("Invalid breakpoint entry");
        if(result.size()>=1000)throw std::runtime_error("Too many breakpoints");
        result.push_back({full(c,name).u8string(),line});
    }return result;
}
bool toggleBreakpoint(const Context& c,int line){
    if(c.file.empty()||line<1)throw std::runtime_error("Open a saved source file and select a line");
    const auto file=full(c,c.file.u8string()).u8string();auto list=loadBreakpoints(c);
    auto same=[&](const Breakpoint& b){return b.file==file&&b.line==line;};
    bool added=std::none_of(list.begin(),list.end(),same);
    if(added){if(list.size()>=1000)throw std::runtime_error("Breakpoint limit reached");list.push_back({file,line});}
    else list.erase(std::remove_if(list.begin(),list.end(),same),list.end());
    Json::Array entries;for(const auto& b:list){auto p=fs::u8path(b.file);auto rel=p.lexically_relative(c.workspace);auto name=(!rel.empty()&&*rel.begin()!="..")?rel.u8string():b.file;entries.push_back(Json::Object{{"file",name},{"line",b.line}});}
    auto output=store(c);fs::create_directories(output.parent_path());auto temporary=output;temporary+=".tmp";
    {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("Cannot write breakpoint store");out<<Json(Json::Object{{"version",1},{"breakpoints",entries}}).dump(2)<<'\n';out.close();if(!out)throw std::runtime_error("Cannot save breakpoint store");}
    std::error_code ec;fs::rename(temporary,output,ec);
#ifdef _WIN32
    if(ec){ // Windows rename cannot replace a destination. Keep original on failure.
        auto backup=output;backup+=".bak";if(fs::exists(backup))throw std::runtime_error("Resolve existing breakpoint backup first");
        fs::rename(output,backup);fs::rename(temporary,output,ec);if(ec){fs::rename(backup,output);throw std::runtime_error(ec.message());}fs::remove(backup);
    }
#endif
    if(ec)throw std::runtime_error("Cannot replace breakpoint store: "+ec.message());return added;
}
}
