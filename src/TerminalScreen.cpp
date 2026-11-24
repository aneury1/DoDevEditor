#include "TerminalScreen.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace dodev {
namespace {
int bound(int n,int lo,int hi){return std::max(lo,std::min(n,hi));}
std::vector<int> parseArgs(std::string p) {
    if(!p.empty() && (p[0]=='?'||p[0]=='>'||p[0]=='!'))p.erase(0,1);
    std::vector<int> result;
    std::istringstream input(p);
    std::string item;
    while(std::getline(input,item,';')) {
        try {result.push_back(item.empty()?0:std::stoi(item));}
        catch(...) {result.push_back(0);}
        if(result.size()>48)break;
    }
    if(result.empty())result.push_back(0);
    return result;
}
}
TerminalScreen::TerminalScreen(int columns,int rows):cols_(bound(columns,10,400)),rows_(bound(rows,4,180)) {
    scroll_bottom_=rows_-1;
    main_.assign(rows_,blankLine());alt_.assign(rows_,blankLine());
}
TerminalScreen::Line TerminalScreen::blankLine() const {return Line(static_cast<std::size_t>(cols_),blank_);}
std::vector<TerminalScreen::Line>& TerminalScreen::buffer(){return alternate_?alt_:main_;}
const std::vector<TerminalScreen::Line>& TerminalScreen::buffer() const{return alternate_?alt_:main_;}
void TerminalScreen::resize(int columns,int rows){
    columns=bound(columns,10,400);rows=bound(rows,4,180);
    if(columns==cols_&&rows==rows_)return;
    const int old_rows=rows_;
    cols_=columns;rows_=rows;
    for(auto* list:{&main_,&alt_}){
        for(auto& line:*list)line.resize(static_cast<std::size_t>(cols_),blank_);
        list->resize(static_cast<std::size_t>(rows_),blankLine());
    }
    for(auto& line:history_)line.resize(static_cast<std::size_t>(cols_),blank_);
    x_=bound(x_,0,cols_-1);y_=bound(y_,0,rows_-1);
    if(scroll_top_==0&&scroll_bottom_==old_rows-1) scroll_bottom_=rows_-1;
    else {scroll_top_=0;scroll_bottom_=rows_-1;}
    scroll_offset_=std::min(scroll_offset_,static_cast<int>(history_.size()));
}
void TerminalScreen::scroll(int n){
    if(alternate_)return;
    scroll_offset_=bound(scroll_offset_+n,0,static_cast<int>(history_.size()));
}
void TerminalScreen::toBottom(){scroll_offset_=0;}
void TerminalScreen::setScrollbackLimit(std::size_t lines){
    max_history_=std::clamp<std::size_t>(lines,100,200000);
    while(history_.size()>max_history_)history_.pop_front();
    scroll_offset_=std::min(scroll_offset_,static_cast<int>(history_.size()));
}
const TerminalCell& TerminalScreen::cell(int row,int col) const {
    row=bound(row,0,rows_-1);col=bound(col,0,cols_-1);
    if(alternate_)return alt_[row][col];
    const std::size_t top=history_.size()-static_cast<std::size_t>(scroll_offset_);
    const std::size_t index=top+static_cast<std::size_t>(row);
    return index<history_.size()?history_[index][col]:main_[index-history_.size()][col];
}
std::string TerminalScreen::lineText(int row) const {
    std::string out;
    for(int c=0;c<cols_;++c)out+=cell(row,c).glyph;
    const auto e=out.find_last_not_of(' ');
    return e==std::string::npos?"":out.substr(0,e+1);
}
std::string TerminalScreen::visibleText() const {
    std::string s;
    for(int r=0;r<rows_;++r){if(r)s+='\n';s+=lineText(r);}
    return s;
}
std::string TerminalScreen::fullText() const {
    // Export the currently retained VT screen, including off-screen scrollback.
    // Alternate-screen applications are intentionally not serialized as history.
    std::string text;
    const auto append=[&text](const Line& line){
        std::string content;
        for(const auto& cell:line)content+=cell.glyph;
        const auto last=content.find_last_not_of(' ');
        text+=(last==std::string::npos?std::string{}:content.substr(0,last+1));
        text+='\n';
    };
    if(!alternate_)for(const auto& line:history_)append(line);
    for(const auto& line:buffer())append(line);
    while(!text.empty() && text.back()=='\n')text.pop_back();
    text+='\n';
    return text;
}
void TerminalScreen::clear() {
    main_.assign(rows_,blankLine());alt_.assign(rows_,blankLine());history_.clear();
    x_=y_=save_x_=save_y_=scroll_offset_=0;wrapping_=false;
}
void TerminalScreen::reset(){
    clear();scroll_top_=0;scroll_bottom_=rows_-1;cursor_visible_=true;
    alternate_=false;style_=blank_;state_=Parse::Normal;application_cursor_=bracketed_paste_=false;pending_utf8_.clear();utf8_remaining_=0;params_.clear();
}
void TerminalScreen::put(std::string_view glyph){
    if(wrapping_){x_=0;index();wrapping_=false;}
    auto& target=buffer()[y_][x_];
    target=style_;target.glyph.assign(glyph.data(),glyph.size());
    if(x_==cols_-1)wrapping_=true;else ++x_;
}
void TerminalScreen::carriageReturn(){x_=0;wrapping_=false;}
void TerminalScreen::newline(){index();}
void TerminalScreen::index(){
    wrapping_=false;
    if(y_==scroll_bottom_)scrollUp(1);
    else if(y_<rows_-1)++y_;
}
void TerminalScreen::reverseIndex(){
    wrapping_=false;
    if(y_==scroll_top_)scrollDown(1);
    else if(y_>0)--y_;
}
void TerminalScreen::scrollUp(int n){
    auto& b=buffer();
    for(int t=0;t<std::min(n,rows_);++t){
        if(!alternate_&&scroll_top_==0&&scroll_bottom_==rows_-1){
            if(history_.size()>=max_history_){history_.pop_front();if(scroll_offset_)--scroll_offset_;}
            history_.push_back(b.front());
            if(scroll_offset_>0)++scroll_offset_;
        }
        for(int j=scroll_top_;j<scroll_bottom_;++j)b[j]=std::move(b[j+1]);
        b[scroll_bottom_]=blankLine();
    }
}
void TerminalScreen::scrollDown(int n){
    auto& b=buffer();
    for(int t=0;t<std::min(n,rows_);++t){
        for(int j=scroll_bottom_;j>scroll_top_;--j)b[j]=std::move(b[j-1]);
        b[scroll_top_]=blankLine();
    }
}
void TerminalScreen::eraseLine(int how){
    auto& line=buffer()[y_];
    const int start=how==0?x_:0;
    const int end=how==1?x_:cols_-1;
    for(int c=start;c<=end;++c)line[c]=blank_;
}
void TerminalScreen::eraseDisplay(int how){
    if(how==3){history_.clear();scroll_offset_=0;return;}
    if(how==2){for(auto& line:buffer())line=blankLine();return;}
    if(how==0){eraseLine(0);for(int r=y_+1;r<rows_;++r)buffer()[r]=blankLine();}
    if(how==1){eraseLine(1);for(int r=0;r<y_;++r)buffer()[r]=blankLine();}
}
TerminalColor TerminalScreen::palette(int n){
    static const TerminalColor basic[16]={
        {15,21,34},{244,88,94},{72,190,127},{245,190,80},
        {100,151,248},{187,120,237},{65,202,210},{223,229,238},
        {106,118,141},{255,111,116},{98,225,151},{255,217,120},
        {141,180,255},{213,157,255},{109,227,236},{255,255,255}
    };
    if(n<16)return basic[bound(n,0,15)];
    if(n>=232){auto v=static_cast<std::uint8_t>(8+(n-232)*10);return {v,v,v};}
    n=bound(n,16,231)-16;
    const int rr=n/36,gg=(n/6)%6,bb=n%6;
    const auto cv=[](int value){return static_cast<std::uint8_t>(value==0?0:value*40+55);};
    return {cv(rr),cv(gg),cv(bb)};
}
void TerminalScreen::sgr(const std::vector<int>& a){
    for(std::size_t i=0;i<a.size();++i){
        const int code=a[i];
        if(code==0)style_=blank_;
        else if(code==1)style_.bold=true;
        else if(code==22)style_.bold=false;
        else if(code==39)style_.foreground=blank_.foreground;
        else if(code==49)style_.background=blank_.background;
        else if(code>=30&&code<=37)style_.foreground=palette(code-30);
        else if(code>=90&&code<=97)style_.foreground=palette(code-90+8);
        else if(code>=40&&code<=47)style_.background=palette(code-40);
        else if(code>=100&&code<=107)style_.background=palette(code-100+8);
        else if((code==38||code==48)&&i+2<a.size()){
            TerminalColor col{}; bool got=false;
            if(a[i+1]==5){col=palette(bound(a[i+2],0,255));i+=2;got=true;}
            else if(a[i+1]==2&&i+4<a.size()){
                col={static_cast<std::uint8_t>(bound(a[i+2],0,255)),
                     static_cast<std::uint8_t>(bound(a[i+3],0,255)),
                     static_cast<std::uint8_t>(bound(a[i+4],0,255))};
                i+=4;got=true;
            }
            if(got){if(code==38)style_.foreground=col;else style_.background=col;}
        }
    }
}
void TerminalScreen::switchAlternate(bool on){
    if(alternate_==on)return;
    alternate_=on; x_=y_=0;scroll_top_=0;scroll_bottom_=rows_-1;wrapping_=false;
    if(on)alt_.assign(rows_,blankLine());
}
void TerminalScreen::csi(char final){
    const bool priv=!params_.empty()&&params_[0]=='?';
    const auto a=parseArgs(params_);
    const auto arg=[&](std::size_t n,int fallback){return n>=a.size()||a[n]==0?fallback:a[n];};
    auto& b=buffer();
    switch(final){
        case 'A':y_=bound(y_-arg(0,1),0,rows_-1);break;
        case 'B':y_=bound(y_+arg(0,1),0,rows_-1);break;
        case 'C':x_=bound(x_+arg(0,1),0,cols_-1);break;
        case 'D':x_=bound(x_-arg(0,1),0,cols_-1);break;
        case 'E':y_=bound(y_+arg(0,1),0,rows_-1);x_=0;break;
        case 'F':y_=bound(y_-arg(0,1),0,rows_-1);x_=0;break;
        case 'G':x_=bound(arg(0,1)-1,0,cols_-1);break;
        case 'd':y_=bound(arg(0,1)-1,0,rows_-1);break;
        case 'H':case 'f':y_=bound(arg(0,1)-1,0,rows_-1);x_=bound(arg(1,1)-1,0,cols_-1);break;
        case 'J':eraseDisplay(a[0]);break;
        case 'K':eraseLine(a[0]);break;
        case 'X':for(int i=x_;i<std::min(cols_,x_+arg(0,1));++i)b[y_][i]=blank_;break;
        case 'P':{const int n=bound(arg(0,1),0,cols_-x_);for(int i=x_;i<cols_-n;++i)b[y_][i]=b[y_][i+n];for(int i=cols_-n;i<cols_;++i)b[y_][i]=blank_;break;}
        case '@':{const int n=bound(arg(0,1),0,cols_-x_);for(int i=cols_-1;i>=x_+n;--i)b[y_][i]=b[y_][i-n];for(int i=x_;i<x_+n;++i)b[y_][i]=blank_;break;}
        case 'L':{const int old_top=scroll_top_;scroll_top_=y_;scrollDown(arg(0,1));scroll_top_=old_top;break;}
        case 'M':{const int old_top=scroll_top_;scroll_top_=y_;scrollUp(arg(0,1));scroll_top_=old_top;break;}
        case 'S':scrollUp(arg(0,1));break;
        case 'T':scrollDown(arg(0,1));break;
        case 'm':sgr(a);break;
        case 's':save_x_=x_;save_y_=y_;break;
        case 'u':x_=save_x_;y_=save_y_;break;
        case 'r':scroll_top_=bound(arg(0,1)-1,0,rows_-1);scroll_bottom_=bound(arg(1,rows_)-1,scroll_top_,rows_-1);x_=y_=0;break;
        case 'h':case 'l':
            if(priv){for(int option:a){if(option==25)cursor_visible_=final=='h';
                if(option==1)application_cursor_=final=='h';
                if(option==2004)bracketed_paste_=final=='h';
                if(option==1049||option==1047||option==47)switchAlternate(final=='h');}}
            break;
        default:break;
    }
    wrapping_=false;
}
void TerminalScreen::feed(std::string_view bytes){
    for(unsigned char c:bytes){
        if(state_==Parse::Esc){
            state_=Parse::Normal;
            if(c=='['){state_=Parse::Csi;params_.clear();}
            else if(c==']'||c=='P'||c=='_'){state_=Parse::Osc;}
            else if(c=='7'){save_x_=x_;save_y_=y_;}
            else if(c=='8'){x_=save_x_;y_=save_y_;}
            else if(c=='D')index();
            else if(c=='M')reverseIndex();
            else if(c=='E'){index();carriageReturn();}
            else if(c=='c')reset();
            continue;
        }
        if(state_==Parse::Csi){
            if(c>=0x40&&c<=0x7e){csi(static_cast<char>(c));state_=Parse::Normal;}
            else if(params_.size()<256)params_+=static_cast<char>(c);
            else state_=Parse::Normal;
            continue;
        }
        if(state_==Parse::Osc){if(c==7)state_=Parse::Normal;else if(c==27)state_=Parse::OscEsc;continue;}
        if(state_==Parse::OscEsc){state_=c=='\\'?Parse::Normal:Parse::Osc;continue;}
        if(c==27){pending_utf8_.clear();utf8_remaining_=0;state_=Parse::Esc;continue;}
        if(c=='\r'){carriageReturn();continue;}
        if(c=='\n'){newline();continue;}
        if(c=='\b'){x_=std::max(0,x_-1);wrapping_=false;continue;}
        if(c=='\t'){const int next=(x_+8)&~7;x_=std::min(cols_-1,next);continue;}
        if(c<32||c==127)continue;
        if(utf8_remaining_>0){
            if((c&0xC0)==0x80){pending_utf8_+=static_cast<char>(c);--utf8_remaining_;
                if(utf8_remaining_==0){put(pending_utf8_);pending_utf8_.clear();}
                continue;
            }
            put("?");pending_utf8_.clear();utf8_remaining_=0;
        }
        if(c<128)put(std::string_view(reinterpret_cast<const char*>(&c),1));
        else {
            utf8_remaining_=(c&0xE0)==0xC0?1:(c&0xF0)==0xE0?2:(c&0xF8)==0xF0?3:0;
            if(utf8_remaining_==0)put("?");
            else pending_utf8_.assign(1,static_cast<char>(c));
        }
    }
}
} // namespace dodev
