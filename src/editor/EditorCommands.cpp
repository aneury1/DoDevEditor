#include "dodev/editor/EditorCommands.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace dodev::editor {
namespace {
using S=std::size_t;
S clamp(S v,const std::string& t){return std::min(v,t.size());}
bool word(unsigned char c){return std::isalnum(c)||c=='_'||c>=128;}
S lineAfter(const std::string& s,S p){auto n=s.find('\n',std::min(s.size(),p));return n==std::string::npos?s.size():n+1;}
S lastLineExclusive(const std::string& s,S a,S b){
    const S last=std::max(a,b);
    const S effective=last>0&&last>s.size()?s.size():last;
    const S selectedLast=(effective>std::min(a,b)&&effective>0&&s[effective-1]=='\n')?effective-1:effective;
    return lineAfter(s,selectedLast);
}
Result selectResult(S a,S b){Result r;r.handled=true;r.selectBegin=a;r.selectEnd=b;return r;}
Result replaceResult(S start,S end,std::string value,S a,S b){
    Result r;r.handled=true;r.changed=true;r.replaceBegin=start;r.replaceEnd=end;r.replacement=std::move(value);
    r.selectBegin=a;r.selectEnd=b;return r;
}
std::vector<std::string> linesOf(const std::string& data){
    std::vector<std::string> out;S cursor=0;
    while(cursor<data.size()){auto i=data.find('\n',cursor);if(i==std::string::npos){out.push_back(data.substr(cursor));break;}
        out.push_back(data.substr(cursor,i-cursor));cursor=i+1;
    }
    if(data.empty()||data.back()=='\n')out.emplace_back();
    return out;
}
std::string trimRight(std::string s){while(!s.empty()&&(s.back()==' '||s.back()=='\t'||s.back()=='\r'))s.pop_back();return s;}
} // namespace
S lineStart(const std::string& s,S p){p=clamp(p,s);if(p==0)return 0;auto i=s.rfind('\n',p-1);return i==std::string::npos?0:i+1;}
S lineEnd(const std::string& s,S p){auto i=s.find('\n',clamp(p,s));return i==std::string::npos?s.size():i;}
S previousCharacter(const std::string& s,S p){p=clamp(p,s);if(!p)return 0;--p;while(p>0&&(static_cast<unsigned char>(s[p])&0xC0)==0x80)--p;return p;}
S nextCharacter(const std::string& s,S p){p=clamp(p,s);if(p==s.size())return p;++p;while(p<s.size()&&(static_cast<unsigned char>(s[p])&0xC0)==0x80)++p;return p;}
Result execute(std::string_view action,const std::string& text,S anchor,S caret,const Options& opt){
    anchor=clamp(anchor,text);caret=clamp(caret,text);const S lo=std::min(anchor,caret),hi=std::max(anchor,caret);
    const S start=lineStart(text,lo),end=lastLineExclusive(text,anchor,caret);
    const auto lineText=text.substr(start,lineEnd(text,lo)-start);
    if(action=="selection.all")return selectResult(0,text.size());
    if(action=="selection.line")return selectResult(start,lineAfter(text,lo));
    if(action=="selection.word"){
        S a=lo,b=hi;
        if(a==b){if(a<text.size()&&word(static_cast<unsigned char>(text[a])))b=nextCharacter(text,a);
            else if(a&&word(static_cast<unsigned char>(text[previousCharacter(text,a)])))a=previousCharacter(text,a);
            else return selectResult(a,b);
        }
        while(a>0&&word(static_cast<unsigned char>(text[previousCharacter(text,a)])))a=previousCharacter(text,a);
        while(b<text.size()&&word(static_cast<unsigned char>(text[b])))b=nextCharacter(text,b);
        return selectResult(a,b);
    }
    if(action=="selection.to_start")return selectResult(caret,0);
    if(action=="selection.to_end")return selectResult(caret,text.size());
    if(action=="selection.line_start")return selectResult(caret,lineStart(text,caret));
    if(action=="selection.line_end")return selectResult(caret,lineEnd(text,caret));
    if(action=="selection.swap_anchor")return selectResult(caret,anchor);
    if(action=="selection.cursor_up"||action=="selection.cursor_down"){
        const S col=caret-lineStart(text,caret);S target=caret;
        if(action=="selection.cursor_up"){
            if(caret>0&&lineStart(text,caret)>0){const S prev=lineStart(text,lineStart(text,caret)-1);target=std::min(prev+col,lineEnd(text,prev));}
        }else{const S next=lineAfter(text,caret);if(next<text.size())target=std::min(next+col,lineEnd(text,next));}
        return selectResult(target,target);
    }
    if(action=="selection.next_occurrence"||action=="selection.prev_occurrence"){
        if(lo==hi)return selectResult(lo,hi);
        const auto needle=text.substr(lo,hi-lo);if(needle.empty())return selectResult(lo,hi);
        S pos=std::string::npos;
        if(action=="selection.next_occurrence"){
            pos=text.find(needle,hi);if(pos==std::string::npos)pos=text.find(needle,0);
        }else{
            if(lo>0)pos=text.rfind(needle,lo-1);
            if(pos==std::string::npos)pos=text.rfind(needle);
        }
        return pos==std::string::npos?selectResult(anchor,caret):selectResult(pos,pos+needle.size());
    }
    if(action=="selection.expand"){
        if(lo==hi){auto r=execute("selection.word",text,lo,hi,opt);if(r.selectBegin!=r.selectEnd)return r;}
        if(lo!=start||hi!=end)return selectResult(start,end);
        return selectResult(0,text.size());
    }
    if(action=="selection.shrink"){
        if(lo==0&&hi==text.size())return selectResult(lineStart(text,caret),lineAfter(text,caret));
        if(lo!=hi){const S v=lo;return selectResult(v,v);}
        return selectResult(caret,caret);
    }
    if(action=="edit.delete"){
        S e=hi;if(lo==hi)e=nextCharacter(text,lo);
        return replaceResult(lo,e,"",lo,lo);
    }
    if(action=="edit.insert_above"||action=="edit.insert_below"){
        if(action=="edit.insert_above")return replaceResult(start,start,"\n",start,start);
        const S e=lineEnd(text,hi);
        const std::string insertion="\n";
        return replaceResult(e,e,insertion,e+insertion.size(),e+insertion.size());
    }
    if(action=="edit.toggle_block_comment"){
        S a=lo,b=hi;if(a==b){a=start;b=lineEnd(text,lo);}
        const std::string part=text.substr(a,b-a);
        if(part.size()>=opt.blockStart.size()+opt.blockEnd.size() &&
           part.compare(0,opt.blockStart.size(),opt.blockStart)==0 &&
           part.compare(part.size()-opt.blockEnd.size(),opt.blockEnd.size(),opt.blockEnd)==0){
            const auto inner=part.substr(opt.blockStart.size(),part.size()-opt.blockStart.size()-opt.blockEnd.size());
            return replaceResult(a,b,inner,a,a+inner.size());
        }
        const std::string replacement=opt.blockStart+part+opt.blockEnd;
        return replaceResult(a,b,replacement,a+opt.blockStart.size(),a+replacement.size()-opt.blockEnd.size());
    }
    if(action=="edit.uppercase"||action=="edit.lowercase"||action=="edit.titlecase"){
        S a=lo,b=hi;if(a==b){a=start;b=lineEnd(text,lo);}
        std::string value=text.substr(a,b-a);bool capitalize=true;
        for(char& c:value){unsigned char u=static_cast<unsigned char>(c);
            if(u<128){if(action=="edit.uppercase")c=static_cast<char>(std::toupper(u));
                else if(action=="edit.lowercase")c=static_cast<char>(std::tolower(u));
                else {c=static_cast<char>(capitalize?std::toupper(u):std::tolower(u));capitalize=!word(u);}}
        }
        return replaceResult(a,b,value,a,a+value.size());
    }
    if(action=="edit.join_lines"){
        const S a=start,b=end;std::string value=text.substr(a,b-a);
        for(S k=0;k<value.size();++k)if(value[k]=='\n'&&k+1<value.size())value[k]=' ';
        return replaceResult(a,b,value,a,a+value.size());
    }
    if(action=="edit.duplicate"){
        if(lo!=hi){const std::string val=text.substr(lo,hi-lo);return replaceResult(hi,hi,val,hi,hi+val.size());}
        const auto val=text.substr(start,lineAfter(text,lo)-start);const auto at=start+val.size();
        const std::string append=val.empty()?"\n":(val.back()=='\n'?val:"\n"+val);
        return replaceResult(at,at,append,at,at+append.size());
    }
    if(action=="edit.copy_line_up"||action=="edit.copy_line_down"||action=="edit.move_line_up"||action=="edit.move_line_down"){
        const bool down=action=="edit.copy_line_down"||action=="edit.move_line_down";
        const bool copy=action=="edit.copy_line_up"||action=="edit.copy_line_down";
        S a=start,b=end;
        if(copy){const std::string block=text.substr(a,b-a);
            if(down){const std::string insert=(b==text.size()&&!block.empty()&&block.back()!='\n'?"\n":"")+block;
                return replaceResult(b,b,insert,b,b+insert.size());}
            const std::string insert=block+(b==text.size()&&!block.empty()&&block.back()!='\n'?"\n":"");
            return replaceResult(a,a,insert,a,a+insert.size());
        }
        if(down){if(b>=text.size())return selectResult(anchor,caret);
            const S next=lineAfter(text,b);const std::string block=text.substr(a,b-a),other=text.substr(b,next-b);
            const std::string combined=(!other.empty()&&other.back()!='\n'&&!block.empty()&&block.back()=='\n')
                ? other+"\n"+block.substr(0,block.size()-1) : other+block;
            return replaceResult(a,next,combined,a,a+combined.size());
        }
        if(a==0)return selectResult(anchor,caret);
        const S prev=lineStart(text,a-1);const std::string block=text.substr(a,b-a),other=text.substr(prev,a-prev);
        const std::string combined=(!block.empty()&&block.back()!='\n'&&!other.empty()&&other.back()=='\n')
            ? block+"\n"+other.substr(0,other.size()-1) : block+other;
        return replaceResult(prev,b,combined,prev,prev+combined.size());
    }
    if(action=="edit.toggle_line_comment"||action=="edit.indent"||action=="edit.outdent"||
       action=="edit.sort_asc"||action=="edit.sort_desc"||action=="edit.trim_whitespace"){
        S a=start,b=end;std::string source=text.substr(a,b-a);const bool newline=!source.empty()&&source.back()=='\n';
        auto lines=linesOf(source);if(newline&&!lines.empty()&&lines.back().empty())lines.pop_back();
        bool allCommented=true;
        for(const auto& line:lines){auto first=line.find_first_not_of(" \t");if(first!=std::string::npos&&line.compare(first,opt.lineComment.size(),opt.lineComment)!=0)allCommented=false;}
        for(auto& line:lines){auto first=line.find_first_not_of(" \t");
            if(action=="edit.toggle_line_comment"){
                if(first==std::string::npos)continue;
                if(allCommented){if(line.compare(first,opt.lineComment.size(),opt.lineComment)==0){line.erase(first,opt.lineComment.size());if(first<line.size()&&line[first]==' ')line.erase(first,1);}}
                else line.insert(first,opt.lineComment+" ");
            }else if(action=="edit.indent")line.insert(0,opt.useSpaces?std::string(std::min(16u,std::max(1u,opt.tabSize)),' '):"\t");
            else if(action=="edit.outdent"){
                if(!line.empty()&&line[0]=='\t')line.erase(0,1);
                else {S k=0;while(k<line.size()&&k<opt.tabSize&&line[k]==' ')++k;line.erase(0,k);}
            }else if(action=="edit.trim_whitespace")line=trimRight(std::move(line));
        }
        if(action=="edit.sort_asc")std::stable_sort(lines.begin(),lines.end());
        if(action=="edit.sort_desc")std::stable_sort(lines.begin(),lines.end(),std::greater<std::string>{});
        std::string result;for(S k=0;k<lines.size();++k){if(k)result+='\n';result+=lines[k];}if(newline)result+='\n';
        return replaceResult(a,b,result,a,a+result.size());
    }
    if(action=="edit.delete_line")return replaceResult(start,lineAfter(text,lo),"",start,start);
    return {};
}
} // namespace dodev::editor
