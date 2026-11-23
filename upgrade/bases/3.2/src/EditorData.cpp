#include "EditorData.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace dodev {
CsvResult parseCsv(const std::string& source, char separator) {
    CsvResult out;
    std::vector<std::string> row;
    std::string cell;
    enum class State { Start, Bare, Quoted, AfterQuote } state = State::Start;
    bool active = false;
    for (std::size_t i=0; i<source.size(); ++i) {
        const char ch=source[i];
        if (state == State::Quoted) {
            if (ch=='"') {
                if (i+1<source.size() && source[i+1]=='"') { cell+='"'; ++i; }
                else state=State::AfterQuote;
            } else { cell+=ch; }
            active=true;
            continue;
        }
        if (ch==separator) {
            row.push_back(std::move(cell)); cell.clear(); state=State::Start; active=true;
        } else if (ch=='\r' || ch=='\n') {
            if (ch=='\r' && i+1<source.size() && source[i+1]=='\n') ++i;
            row.push_back(std::move(cell)); cell.clear();
            out.rows.push_back(std::move(row)); row.clear(); state=State::Start; active=false;
        } else if (state==State::Start && ch=='"') {
            state=State::Quoted; active=true;
        } else if (state==State::AfterQuote) {
            out.valid=false;
            out.error="Caracter inesperado despues de comillas en CSV (byte " + std::to_string(i) + ")";
            return out;
        } else if (ch=='"') {
            out.valid=false;
            out.error="Comillas sin escapar en celda CSV (byte " + std::to_string(i) + ")";
            return out;
        } else { cell+=ch; state=State::Bare; active=true; }
    }
    if (state==State::Quoted) { out.valid=false; out.error="Celda CSV con comillas sin cerrar"; return out; }
    if (active || !row.empty()) { row.push_back(std::move(cell)); out.rows.push_back(std::move(row)); }
    return out;
}
std::string writeCsv(const std::vector<std::vector<std::string>>& rows, char separator) {
    std::string text;
    for (const auto& row : rows) {
        for (std::size_t i=0; i<row.size(); ++i) {
            if (i) text+=separator;
            const auto& v=row[i];
            const bool quote=v.find_first_of(std::string("\"\r\n")+separator)!=std::string::npos;
            if (quote) text+='"';
            for (char c : v) { if (c=='"') text+='"'; text+=c; }
            if (quote) text+='"';
        }
        text+='\n';
    }
    return text;
}
static std::uint32_t le32(const std::vector<std::uint8_t>& d, std::size_t p) {
    return static_cast<std::uint32_t>(d[p]) | (static_cast<std::uint32_t>(d[p+1])<<8) |
           (static_cast<std::uint32_t>(d[p+2])<<16) | (static_cast<std::uint32_t>(d[p+3])<<24);
}

static std::string dltId(const std::vector<std::uint8_t>& d, std::size_t p) {
    std::string s;
    for (int i=0;i<4;++i) if(d[p+i]) s+=static_cast<char>(d[p+i]);
    return s;
}
static std::string makePayload(const std::vector<std::uint8_t>& d, std::size_t p, std::size_t end) {
    std::string s;
    bool printable=true;
    for (auto i=p;i<end;++i) {
        if (d[i]==0 || (d[i]<32 && d[i]!='\n' && d[i]!='\r' && d[i]!='\t')) { printable=false; break; }
    }
    if(printable) {
        for (auto i=p;i<end && s.size()<16000;++i) s+=static_cast<char>(d[i]);
    } else {
        std::ostringstream os;
        os << "hex: ";
        for (auto i=p;i<end && i-p<256;++i) os << std::hex << std::setfill('0') << std::setw(2) << unsigned(d[i]) << ' ';
        if (end-p>256) os << "...";
        s=os.str();
    }
    return s;
}
DltResult parseDlt(const std::vector<std::uint8_t>& data, std::size_t max_messages) {
    DltResult out;
    std::size_t pos=0;
    while (pos<data.size() && out.entries.size()<max_messages) {
        if (data.size()-pos<20) { out.skipped_bytes+=data.size()-pos; break; }
        if (data[pos]!='D'||data[pos+1]!='L'||data[pos+2]!='T'||data[pos+3]!=1) {++pos; ++out.skipped_bytes; continue;}
        const std::uint32_t sec=le32(data,pos+4), micros=le32(data,pos+8);
        const std::string ecu=dltId(data,pos+12);
        const std::size_t h=pos+16;
        const auto flags=data[h], counter=data[h+1];
        const std::size_t len=static_cast<std::size_t>((data[h+2]<<8)|data[h+3]);
        // DLT standard-header length includes the standard header but not the storage header.
        if (len<4 || len>data.size()-h) {++pos; ++out.skipped_bytes; continue;}
        const std::size_t end=h+len;
        std::size_t p=h+4;
        const bool msbf=(flags&0x02)!=0;
        if(flags&0x04) { if(p+4>end) {pos=end;continue;} p+=4; } // with ECU id
        if(flags&0x08) { if(p+4>end) {pos=end;continue;} p+=4; } // session id
        if(flags&0x10) { if(p+4>end) {pos=end;continue;} p+=4; } // timestamp
        DltEntry e; e.counter=counter; e.ecu=ecu;
        { std::ostringstream os; os << sec << "." << std::setfill('0') << std::setw(6) << micros; e.timestamp=os.str(); }
        if(flags&0x01) { // extended header: msin, noar, APID, CTID
            if(p+10>end) {pos=end;continue;}
            e.apid=dltId(data,p+2); e.ctid=dltId(data,p+6); p+=10;
        }
        (void)msbf;
        e.payload=makePayload(data,p,end);
        out.entries.push_back(std::move(e));
        pos=end;
    }
    if(out.entries.empty()) out.error="No se encontraron registros DLT validos (storage header DLT\\x01).";
    return out;
}
std::string pathUtf8(const fs::path& p) { return p.u8string(); }
std::vector<std::uint8_t> readBytes(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if(!file) throw std::runtime_error("No se pudo abrir: " + pathUtf8(path));
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
std::string readText(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if(!file) throw std::runtime_error("No se pudo abrir: " + pathUtf8(path));
    std::ostringstream out; out<<file.rdbuf();
    if(file.bad()) throw std::runtime_error("Fallo de lectura: " + pathUtf8(path));
    return out.str();
}
void writeText(const fs::path& path, const std::string& data) {
    std::ofstream file(path, std::ios::binary|std::ios::trunc);
    if(!file) throw std::runtime_error("No se pudo escribir: " + pathUtf8(path));
    file.write(data.data(),static_cast<std::streamsize>(data.size()));
    file.flush();
    if(!file) throw std::runtime_error("Escritura incompleta: " + pathUtf8(path));
}
bool looksBinary(const std::vector<std::uint8_t>& bytes) {
    const std::size_t size=std::min<std::size_t>(bytes.size(),4096);
    std::size_t unusual=0;
    for(std::size_t i=0;i<size;++i) {
        const auto ch=bytes[i];
        if(ch==0) return true;
        if(ch<9 || (ch>13 && ch<32)) ++unusual;
    }
    return size>0 && unusual>size/10;
}
std::string hexDump(const std::vector<std::uint8_t>& bytes, std::size_t max_bytes) {
    std::ostringstream os;
    const auto n=std::min(bytes.size(),max_bytes);
    for(std::size_t i=0;i<n;i+=16) {
        os << std::hex << std::setfill('0') << std::setw(8) << i << "  ";
        for(std::size_t j=0;j<16;++j) {
            if(i+j<n) os << std::setw(2) << static_cast<unsigned>(bytes[i+j]) << ' ';
            else os << "   ";
        }
        os << " |";
        for(std::size_t j=i;j<std::min(n,i+16);++j) {
            const auto c=bytes[j]; os << ((c>=32 && c<127)?static_cast<char>(c):'.');
        }
        os << "|\n";
    }
    if(bytes.size()>n) os << "... vista limitada a " << max_bytes << " bytes ...\n";
    return os.str();
}
std::string fileExtension(const fs::path& p) {
    std::string s=pathUtf8(p.extension());
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return s;
}
std::string quoteShell(const std::string& s) {
#ifdef _WIN32
    std::string out="\"";
    for(char c:s) { if(c=='\"') out+="\\\""; else if(c=='%') out+="%%"; else out+=c; }
    return out+'"';
#else
    std::string out="'";
    for(char c:s) { if(c=='\'') out+="'\\''"; else out+=c; }
    return out+'\'';
#endif
}
std::string captureCommand(const std::string& cmd, std::size_t max_bytes) {
#ifdef _WIN32
    FILE* pipe=_popen(cmd.c_str(),"r");
#else
    FILE* pipe=popen(cmd.c_str(),"r");
#endif
    if(!pipe) return "ERROR: no se pudo crear proceso\n";
    std::string out;
    std::array<char,4096> buf{};
    while(fgets(buf.data(),static_cast<int>(buf.size()),pipe) && out.size()<max_bytes) out+=buf.data();
#ifdef _WIN32
    const int code=_pclose(pipe);
#else
    const int code=pclose(pipe);
#endif
    if(out.size()>max_bytes) {out.resize(max_bytes);out+="\n[Salida truncada]";}
    if(code!=0) out+="\n[Comando finalizo con estado no cero: " + std::to_string(code) + "]\n";
    return out;
}
std::vector<GitCommit> gitHistory(const fs::path& folder, std::string* err) {
    const auto command="git -C " + quoteShell(pathUtf8(folder)) +
        " --no-pager log -n 60 --date=short --pretty=format:" + quoteShell("%h%x1f%an%x1f%ad%x1f%s") + " 2>&1";
    const auto text=captureCommand(command);
    std::vector<GitCommit> commits;
    std::istringstream lines(text); std::string line;
    while(std::getline(lines,line)) {
        std::array<std::string,4> fields;
        std::size_t start=0; bool good=true;
        for(int i=0;i<3;++i) {
            const auto n=line.find('\x1f',start);
            if(n==std::string::npos) {good=false;break;}
            fields[i]=line.substr(start,n-start); start=n+1;
        }
        if(!good) continue;
        fields[3]=line.substr(start);
        commits.push_back({fields[0],fields[1],fields[2],fields[3]});
    }
    if(commits.empty() && err) *err=text;
    return commits;
}
} // namespace dodev
