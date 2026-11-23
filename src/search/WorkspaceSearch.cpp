#include "dodev/search/WorkspaceSearch.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <system_error>

namespace dodev::search {
namespace {
std::string lower(std::string_view s){std::string o(s);for(auto& c:o)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return o;}
std::string normal(std::string s){std::replace(s.begin(),s.end(),'\\','/');return s;}
std::vector<std::string> patterns(std::string_view s){
    std::vector<std::string> out;std::size_t pos=0;
    while(pos<s.size()){
        auto end=s.find_first_of(",;\n",pos);if(end==std::string_view::npos)end=s.size();
        auto item=std::string(s.substr(pos,end-pos));
        const auto left=item.find_first_not_of(" \t");
        if(left!=std::string::npos){item=item.substr(left,item.find_last_not_of(" \t")-left+1);out.push_back(normal(item));}
        pos=end+1;
    }
    return out;
}
std::regex glob(std::string_view s){
    std::string pattern="^";
    for(std::size_t i=0;i<s.size();++i){const char c=s[i];
        if(c=='*'){
            if(i+1<s.size()&&s[i+1]=='*') {pattern+=".*";++i;if(i+1<s.size()&&s[i+1]=='/') {++i;}}
            else pattern+="[^/]*";
        }else if(c=='?')pattern+="[^/]";
        else if(std::string_view(".()[]{}+^$|\\").find(c)!=std::string_view::npos){pattern+='\\';pattern+=c;}
        else pattern+=c;
    }
    return std::regex(pattern+"$",std::regex::ECMAScript|std::regex::icase);
}
bool globMatch(const fs::path& rel,const std::vector<std::string>& values){
    const auto path=normal(rel.generic_string());const auto basename=rel.filename().string();
    for(const auto& v:values){
        try{const auto re=glob(v);if(std::regex_match(path,re)||std::regex_match(basename,re))return true;}
        catch(const std::regex_error&){}
    }
    return false;
}
bool excludedDirectory(std::string_view v){
    return v==".git"||v==".svn"||v==".hg"||v=="node_modules"||v=="build"||v==".cache"||v=="dist"||v==".venv"||v=="vendor";
}
std::string getContent(const fs::path& file,std::size_t limit,bool& ok){
    std::error_code ec;const auto size=fs::file_size(file,ec);
    if(ec||size>limit){ok=false;return {};}
    std::ifstream stream(file,std::ios::binary);if(!stream){ok=false;return {};}
    std::string data((std::istreambuf_iterator<char>(stream)),std::istreambuf_iterator<char>());
    ok=!data.empty() || size==0;
    if(data.find('\0')!=std::string::npos)ok=false;
    return data;
}
bool isWord(unsigned char c){return std::isalnum(c)||c=='_';}
bool boundary(std::string_view text,std::size_t off,std::size_t length){
    return (off==0||!isWord(static_cast<unsigned char>(text[off-1])))&&
           (off+length>=text.size()||!isWord(static_cast<unsigned char>(text[off+length])));
}
std::string errorOf(const std::regex_error& err){return std::string("Invalid regular expression: ")+err.what();}
std::regex makeRegex(const Options& opts){
    auto flags=std::regex::ECMAScript;
    if(!opts.caseSensitive)flags|=std::regex::icase;
    return std::regex(opts.regex?opts.query:std::string(),flags);
}
std::string safeSnippet(std::string_view line){
    std::string s(line.substr(0,300));for(char& c:s)if(c=='\t'||c=='\r')c=' ';
    return s;
}
void scanData(const fs::path& path,std::string_view data,const Options& opts,Results& result,const std::regex* re){
    const std::string folded=opts.caseSensitive?std::string():lower(data);
    const auto needle=opts.caseSensitive?opts.query:lower(opts.query);
    std::size_t pos=0,line=1;
    while(pos<data.size()&&result.matches.size()<opts.maxHits){
        const auto end=data.find('\n',pos);const auto e=end==std::string::npos?data.size():end;
        const std::string_view textLine(data.data()+pos,e-pos);
        std::size_t from=0;
        while(from<textLine.size()&&result.matches.size()<opts.maxHits){
            std::size_t start=std::string::npos,length=0;
            if(re){
                const auto begin=textLine.cbegin()+static_cast<std::ptrdiff_t>(from);
                std::match_results<std::string_view::const_iterator> match;
                if(!std::regex_search(begin,textLine.cend(),match,*re,from?std::regex_constants::match_prev_avail:std::regex_constants::match_default))break;
                start=from+static_cast<std::size_t>(match.position());length=static_cast<std::size_t>(match.length());
            }else{
                start=(opts.caseSensitive?textLine:std::string_view(folded).substr(pos,e-pos)).find(needle,from);
                if(start==std::string::npos)break;length=needle.size();
            }
            if((!opts.wholeWord||boundary(textLine,start,length))&&length>0)
                result.matches.push_back({path,line,start+1,pos+start,length,safeSnippet(textLine)});
            from=start+std::max<std::size_t>(length,1);
        }
        if(end==std::string::npos)break;
        pos=end+1;++line;
    }
}
}
Results scan(const fs::path& root,const Options& opts,const Overlays& overlays){
    Results out;
    if(opts.query.empty()){out.error="Search query cannot be empty";return out;}
    std::error_code ec;
    if(!fs::is_directory(root,ec)){out.error="Workspace folder is unavailable";return out;}
    std::regex re;
    try{if(opts.regex)re=makeRegex(opts);}catch(const std::regex_error& e){out.error=errorOf(e);return out;}
    const auto includes=patterns(opts.include),excludes=patterns(opts.exclude);
    for(fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;
        !ec&&it!=end;it.increment(ec)){
        if(out.filesScanned>=opts.maxFiles||out.matches.size()>=opts.maxHits){out.truncated=true;break;}
        const auto current=it->path();
        auto rel=fs::relative(current,root,ec);if(ec){ec.clear();continue;}
        if(it->is_directory(ec)){
            if(excludedDirectory(current.filename().string())||globMatch(rel,excludes))it.disable_recursion_pending();
            continue;
        }
        if(ec){ec.clear();continue;}
        if(!it->is_regular_file(ec)||ec){ec.clear();continue;}
        if((!includes.empty()&&!globMatch(rel,includes))||globMatch(rel,excludes))continue;
        std::string data;
        const auto normalized=fs::weakly_canonical(current,ec);
        const auto found=overlays.find(ec?current:normalized);ec.clear();
        if(found!=overlays.end()) {data=found->second;if(data.size()>opts.maxFileBytes||data.find('\0')!=std::string::npos)continue;}
        else {bool ok=false;data=getContent(current,opts.maxFileBytes,ok);if(!ok)continue;}
        ++out.filesScanned;scanData(current,data,opts,out,opts.regex?&re:nullptr);
    }
    if(ec)out.error=ec.message();
    if(out.matches.size()>=opts.maxHits)out.truncated=true;
    return out;
}
bool replaceText(std::string_view source,const Options& opts,std::string_view replacement,
                 std::string& output,std::size_t& count,std::string* error){
    count=0;output.clear();if(error)error->clear();
    if(opts.query.empty()){if(error)*error="Empty query";return false;}
    try{
        // Regex replacement supports $1 captures. Ordinary fixed-text replacement is literal.
        if(opts.regex){
            const auto re=makeRegex(opts);
            std::string data(source);
            for(std::sregex_iterator it(data.begin(),data.end(),re),end;it!=end;++it){
                const auto& m=*it;
                if(m.length()&&(!opts.wholeWord||boundary(source,static_cast<std::size_t>(m.position()),static_cast<std::size_t>(m.length()))))++count;
            }
            if(!opts.wholeWord){output=std::regex_replace(data,re,std::string(replacement));return true;}
            // Boundary-sensitive regex replacement, preserving capture expansion.
            std::size_t last=0;
            for(std::sregex_iterator it(data.begin(),data.end(),re),end;it!=end;++it){
                const auto& m=*it;auto pos=static_cast<std::size_t>(m.position());
                if(!m.length()||!boundary(source,pos,static_cast<std::size_t>(m.length())))continue;
                output.append(source.substr(last,pos-last));output+=m.format(std::string(replacement));last=pos+m.length();
            }
            output.append(source.substr(last));return true;
        }
        const std::string folded=opts.caseSensitive?std::string(source):lower(source);
        const std::string needle=opts.caseSensitive?opts.query:lower(opts.query);
        std::size_t pos=0,at=0;
        while((at=(opts.caseSensitive?source:std::string_view(folded)).find(needle,pos))!=std::string::npos){
            if(!opts.wholeWord||boundary(source,at,needle.size())){
                output.append(source.substr(pos,at-pos));output.append(replacement);++count;pos=at+needle.size();
            }else {output.append(source.substr(pos,at+needle.size()-pos));pos=at+needle.size();}
        }
        output.append(source.substr(pos));return true;
    }catch(const std::regex_error& e){if(error)*error=errorOf(e);return false;}
}
bool replaceFile(const fs::path& file,const Options& opts,std::string_view replacement,std::size_t& count,std::string* error){
    count=0;
    std::error_code ec;
    if(!fs::is_regular_file(file,ec)||fs::is_symlink(file,ec)){if(error)*error="Not a regular file or symlink disallowed";return false;}
    const auto originalSize=fs::file_size(file,ec);
    const auto modified=fs::last_write_time(file,ec);
    bool ok=false;const auto original=getContent(file,opts.maxFileBytes,ok);
    if(!ok){if(error)*error="Unreadable, binary or oversized file";return false;}
    std::string result;
    if(!replaceText(original,opts,replacement,result,count,error))return false;
    if(!count)return true;
    if(fs::file_size(file,ec)!=originalSize||fs::last_write_time(file,ec)!=modified){if(error)*error="File changed during replacement";return false;}
    auto temporary=file;temporary+=".dodev-replace-tmp";
    if(fs::exists(temporary,ec)){if(error)*error="Temporary output path exists";return false;}
    {std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);if(!stream){if(error)*error="Cannot create temporary file";return false;}stream.write(result.data(),static_cast<std::streamsize>(result.size()));stream.close();if(!stream){fs::remove(temporary,ec);if(error)*error="Temporary write failed";return false;}}
    fs::permissions(temporary,fs::status(file,ec).permissions(),fs::perm_options::replace,ec);
    if(ec){fs::remove(temporary,ec);if(error)*error="Cannot preserve file permissions";return false;}
    fs::rename(temporary,file,ec);
    if(ec){fs::remove(temporary,ec);if(error)*error="Atomic rename failed";return false;}
    return true;
}
}
