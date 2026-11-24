#include "QRMatrix.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dodevqr {
namespace {
struct BlockSpec {int countA,totalA,dataA,countB,totalB,dataB;};
// Standard QR RS block layout for versions 1..40, ECC L/M/Q/H (ISO/IEC 18004).
// Constants cross-checked against the python-qrcode RS block tables (BSD license).
constexpr BlockSpec kBlocks[40][4] = {
    {
        {1,26,19,0,0,0},
        {1,26,16,0,0,0},
        {1,26,13,0,0,0},
        {1,26,9,0,0,0},
    },
    {
        {1,44,34,0,0,0},
        {1,44,28,0,0,0},
        {1,44,22,0,0,0},
        {1,44,16,0,0,0},
    },
    {
        {1,70,55,0,0,0},
        {1,70,44,0,0,0},
        {2,35,17,0,0,0},
        {2,35,13,0,0,0},
    },
    {
        {1,100,80,0,0,0},
        {2,50,32,0,0,0},
        {2,50,24,0,0,0},
        {4,25,9,0,0,0},
    },
    {
        {1,134,108,0,0,0},
        {2,67,43,0,0,0},
        {2,33,15,2,34,16},
        {2,33,11,2,34,12},
    },
    {
        {2,86,68,0,0,0},
        {4,43,27,0,0,0},
        {4,43,19,0,0,0},
        {4,43,15,0,0,0},
    },
    {
        {2,98,78,0,0,0},
        {4,49,31,0,0,0},
        {2,32,14,4,33,15},
        {4,39,13,1,40,14},
    },
    {
        {2,121,97,0,0,0},
        {2,60,38,2,61,39},
        {4,40,18,2,41,19},
        {4,40,14,2,41,15},
    },
    {
        {2,146,116,0,0,0},
        {3,58,36,2,59,37},
        {4,36,16,4,37,17},
        {4,36,12,4,37,13},
    },
    {
        {2,86,68,2,87,69},
        {4,69,43,1,70,44},
        {6,43,19,2,44,20},
        {6,43,15,2,44,16},
    },
    {
        {4,101,81,0,0,0},
        {1,80,50,4,81,51},
        {4,50,22,4,51,23},
        {3,36,12,8,37,13},
    },
    {
        {2,116,92,2,117,93},
        {6,58,36,2,59,37},
        {4,46,20,6,47,21},
        {7,42,14,4,43,15},
    },
    {
        {4,133,107,0,0,0},
        {8,59,37,1,60,38},
        {8,44,20,4,45,21},
        {12,33,11,4,34,12},
    },
    {
        {3,145,115,1,146,116},
        {4,64,40,5,65,41},
        {11,36,16,5,37,17},
        {11,36,12,5,37,13},
    },
    {
        {5,109,87,1,110,88},
        {5,65,41,5,66,42},
        {5,54,24,7,55,25},
        {11,36,12,7,37,13},
    },
    {
        {5,122,98,1,123,99},
        {7,73,45,3,74,46},
        {15,43,19,2,44,20},
        {3,45,15,13,46,16},
    },
    {
        {1,135,107,5,136,108},
        {10,74,46,1,75,47},
        {1,50,22,15,51,23},
        {2,42,14,17,43,15},
    },
    {
        {5,150,120,1,151,121},
        {9,69,43,4,70,44},
        {17,50,22,1,51,23},
        {2,42,14,19,43,15},
    },
    {
        {3,141,113,4,142,114},
        {3,70,44,11,71,45},
        {17,47,21,4,48,22},
        {9,39,13,16,40,14},
    },
    {
        {3,135,107,5,136,108},
        {3,67,41,13,68,42},
        {15,54,24,5,55,25},
        {15,43,15,10,44,16},
    },
    {
        {4,144,116,4,145,117},
        {17,68,42,0,0,0},
        {17,50,22,6,51,23},
        {19,46,16,6,47,17},
    },
    {
        {2,139,111,7,140,112},
        {17,74,46,0,0,0},
        {7,54,24,16,55,25},
        {34,37,13,0,0,0},
    },
    {
        {4,151,121,5,152,122},
        {4,75,47,14,76,48},
        {11,54,24,14,55,25},
        {16,45,15,14,46,16},
    },
    {
        {6,147,117,4,148,118},
        {6,73,45,14,74,46},
        {11,54,24,16,55,25},
        {30,46,16,2,47,17},
    },
    {
        {8,132,106,4,133,107},
        {8,75,47,13,76,48},
        {7,54,24,22,55,25},
        {22,45,15,13,46,16},
    },
    {
        {10,142,114,2,143,115},
        {19,74,46,4,75,47},
        {28,50,22,6,51,23},
        {33,46,16,4,47,17},
    },
    {
        {8,152,122,4,153,123},
        {22,73,45,3,74,46},
        {8,53,23,26,54,24},
        {12,45,15,28,46,16},
    },
    {
        {3,147,117,10,148,118},
        {3,73,45,23,74,46},
        {4,54,24,31,55,25},
        {11,45,15,31,46,16},
    },
    {
        {7,146,116,7,147,117},
        {21,73,45,7,74,46},
        {1,53,23,37,54,24},
        {19,45,15,26,46,16},
    },
    {
        {5,145,115,10,146,116},
        {19,75,47,10,76,48},
        {15,54,24,25,55,25},
        {23,45,15,25,46,16},
    },
    {
        {13,145,115,3,146,116},
        {2,74,46,29,75,47},
        {42,54,24,1,55,25},
        {23,45,15,28,46,16},
    },
    {
        {17,145,115,0,0,0},
        {10,74,46,23,75,47},
        {10,54,24,35,55,25},
        {19,45,15,35,46,16},
    },
    {
        {17,145,115,1,146,116},
        {14,74,46,21,75,47},
        {29,54,24,19,55,25},
        {11,45,15,46,46,16},
    },
    {
        {13,145,115,6,146,116},
        {14,74,46,23,75,47},
        {44,54,24,7,55,25},
        {59,46,16,1,47,17},
    },
    {
        {12,151,121,7,152,122},
        {12,75,47,26,76,48},
        {39,54,24,14,55,25},
        {22,45,15,41,46,16},
    },
    {
        {6,151,121,14,152,122},
        {6,75,47,34,76,48},
        {46,54,24,10,55,25},
        {2,45,15,64,46,16},
    },
    {
        {17,152,122,4,153,123},
        {29,74,46,14,75,47},
        {49,54,24,10,55,25},
        {24,45,15,46,46,16},
    },
    {
        {4,152,122,18,153,123},
        {13,74,46,32,75,47},
        {48,54,24,14,55,25},
        {42,45,15,32,46,16},
    },
    {
        {20,147,117,4,148,118},
        {40,75,47,7,76,48},
        {43,54,24,22,55,25},
        {10,45,15,67,46,16},
    },
    {
        {19,148,118,6,149,119},
        {18,75,47,31,76,48},
        {34,54,24,34,55,25},
        {20,45,15,61,46,16},
    },
};
// Alignment pattern coordinates for each version; zeros mean unused positions.
constexpr int kAlign[40][7] = {
    {0,0,0,0,0,0,0},
    {6,18,0,0,0,0,0},
    {6,22,0,0,0,0,0},
    {6,26,0,0,0,0,0},
    {6,30,0,0,0,0,0},
    {6,34,0,0,0,0,0},
    {6,22,38,0,0,0,0},
    {6,24,42,0,0,0,0},
    {6,26,46,0,0,0,0},
    {6,28,50,0,0,0,0},
    {6,30,54,0,0,0,0},
    {6,32,58,0,0,0,0},
    {6,34,62,0,0,0,0},
    {6,26,46,66,0,0,0},
    {6,26,48,70,0,0,0},
    {6,26,50,74,0,0,0},
    {6,30,54,78,0,0,0},
    {6,30,56,82,0,0,0},
    {6,30,58,86,0,0,0},
    {6,34,62,90,0,0,0},
    {6,28,50,72,94,0,0},
    {6,26,50,74,98,0,0},
    {6,30,54,78,102,0,0},
    {6,28,54,80,106,0,0},
    {6,32,58,84,110,0,0},
    {6,30,58,86,114,0,0},
    {6,34,62,90,118,0,0},
    {6,26,50,74,98,122,0},
    {6,30,54,78,102,126,0},
    {6,26,52,78,104,130,0},
    {6,30,56,82,108,134,0},
    {6,34,60,86,112,138,0},
    {6,30,58,86,114,142,0},
    {6,34,62,90,118,146,0},
    {6,30,54,78,102,126,150},
    {6,24,50,76,102,128,154},
    {6,28,54,80,106,132,158},
    {6,32,58,84,110,136,162},
    {6,26,54,82,110,138,166},
    {6,30,58,86,114,142,170},
};

using Bits = std::vector<uint8_t>;
void addBits(Bits& bits, unsigned value, int count) {
    for (int i=count-1;i>=0;--i)bits.push_back(static_cast<uint8_t>((value>>i)&1u));
}
uint8_t mul(uint8_t a,uint8_t b) {
    unsigned z=0;
    for (int i=7;i>=0;--i) {
        z = (z<<1u) ^ ((z & 0x80u) ? 0x11Du : 0u);
        z ^= ((b>>i)&1u) ? a : 0u;
    }
    return static_cast<uint8_t>(z);
}
std::vector<uint8_t> reedSolomon(const std::vector<uint8_t>& data,int degree) {
    std::vector<uint8_t> gen{1};
    uint8_t root=1;
    for (int i=0;i<degree;++i) {
        std::vector<uint8_t> next(gen.size()+1);
        for(size_t j=0;j<gen.size();++j){ next[j]^=gen[j]; next[j+1]^=mul(gen[j],root); }
        gen.swap(next);
        root=mul(root,2);
    }
    std::vector<uint8_t> buf(data);
    buf.resize(data.size()+static_cast<size_t>(degree));
    for (size_t i=0;i<data.size();++i) {
        const uint8_t coefficient=buf[i];
        if(coefficient!=0)for(size_t j=0;j<gen.size();++j)buf[i+j]^=mul(gen[j],coefficient);
    }
    return std::vector<uint8_t>(buf.begin()+static_cast<std::ptrdiff_t>(data.size()),buf.end());
}
int capacity(int version,ErrorCorrection ecc) {
    const auto& s=kBlocks[version-1][static_cast<int>(ecc)];
    return s.countA*s.dataA+s.countB*s.dataB;
}
int sizeFor(int ver){return ver*4+17;}

struct Grid {
    int size;
    std::vector<uint8_t> cell;
    std::vector<uint8_t> function;
    explicit Grid(int size_):size(size_),cell(static_cast<size_t>(size_*size_)),function(cell.size()){}
    size_t at(int x,int y)const{return static_cast<size_t>(y*size+x);}
    bool inside(int x,int y)const{return x>=0&&y>=0&&x<size&&y<size;}
    void put(int x,int y,bool black,bool fn=true) {
        if(!inside(x,y))return;
        const size_t k=at(x,y);cell[k]=static_cast<uint8_t>(black);if(fn)function[k]=1;
    }
    bool get(int x,int y)const{return cell[at(x,y)]!=0;}
    void finder(int centerX,int centerY) {
        for(int y=-4;y<=4;++y)for(int x=-4;x<=4;++x){
            int radius=std::max(std::abs(x),std::abs(y));
            put(centerX+x,centerY+y,radius!=2&&radius!=4);
        }
    }
    void alignment(int cx,int cy){
        for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x)
            put(cx+x,cy+y,std::max(std::abs(x),std::abs(y))!=1);
    }
    void drawFormat(ErrorCorrection ecc,int mask) {
        const unsigned eccBits[4]={1,0,3,2};
        const unsigned data=(eccBits[static_cast<int>(ecc)]<<3u)|static_cast<unsigned>(mask);
        unsigned bits=data<<10u;
        for(int i=14;i>=10;--i)if((bits>>i)&1u)bits^=0x537u<<(i-10);
        bits=((data<<10u)|bits)^0x5412u;
        auto b=[&](int i)->bool{return ((bits>>i)&1u)!=0;};
        for(int i=0;i<=5;++i)put(8,i,b(i));
        put(8,7,b(6));put(8,8,b(7));put(7,8,b(8));
        for(int i=9;i<15;++i)put(14-i,8,b(i));
        for(int i=0;i<8;++i)put(size-1-i,8,b(i));
        for(int i=8;i<15;++i)put(8,size-15+i,b(i));
        put(8,size-8,true);
    }
    void drawVersion(int version) {
        if(version<7)return;
        unsigned bits=static_cast<unsigned>(version)<<12u;
        for(int i=17;i>=12;--i)if(((bits>>i)&1u)!=0)bits^=0x1F25u<<(i-12);
        bits|=static_cast<unsigned>(version)<<12u;
        for(int i=0;i<18;++i){
            const bool bit=((bits>>i)&1u)!=0;
            const int x=size-11+(i%3),y=i/3;
            put(x,y,bit);put(y,x,bit);
        }
    }
    void drawPatterns(int version,ErrorCorrection ecc) {
        for(int i=0;i<size;++i){put(6,i,(i%2)==0);put(i,6,(i%2)==0);}
        const int* align=kAlign[version-1];
        int count=0;while(count<7&&align[count]>0)++count;
        for(int yi=0;yi<count;++yi)for(int xi=0;xi<count;++xi){
            // Three finder corners never receive an alignment pattern.
            if((xi==0&&yi==0)||(xi==0&&yi==count-1)||(xi==count-1&&yi==0))continue;
            alignment(align[xi],align[yi]);
        }
        finder(3,3);finder(size-4,3);finder(3,size-4);
        drawFormat(ecc,0); // Reserve format positions before placing data.
        drawVersion(version);
    }
    void placeData(const std::vector<uint8_t>& words){
        int bitIndex=0;
        const int bitCount=static_cast<int>(words.size()*8u);
        for(int right=size-1;right>=1;right-=2){
            if(right==6)--right;
            bool upward=(((right+1)&2)==0);
            for(int vert=0;vert<size;++vert){
                const int y=upward?size-1-vert:vert;
                for(int dx=0;dx<2;++dx){
                    const int x=right-dx;
                    if(function[at(x,y)])continue;
                    bool value=false;
                    if(bitIndex<bitCount)value=((words[static_cast<size_t>(bitIndex/8)]>>(7-(bitIndex%8)))&1u)!=0;
                    put(x,y,value,false);
                    ++bitIndex;
                }
            }
        }
        if(bitIndex<bitCount)throw std::logic_error("QR bit placement overflow");
    }
    static bool maskBit(int x,int y,int m){
        switch(m){
            case 0:return ((x+y)%2)==0;
            case 1:return (y%2)==0;
            case 2:return (x%3)==0;
            case 3:return ((x+y)%3)==0;
            case 4:return (((y/2)+(x/3))%2)==0;
            case 5:return (((x*y)%2)+((x*y)%3))==0;
            case 6:return (((x*y)%2+(x*y)%3)%2)==0;
            default:return (((x+y)%2+(x*y)%3)%2)==0;
        }
    }
    void applyMask(int m){
        for(int y=0;y<size;++y)for(int x=0;x<size;++x){
            size_t k=at(x,y);
            if(!function[k]&&maskBit(x,y,m))cell[k]^=1;
        }
    }
    int penalty()const{
        int score=0;
        // Identical consecutive modules in horizontal and vertical runs.
        for(int dir=0;dir<2;++dir)for(int outer=0;outer<size;++outer){
            int len=1;
            for(int inner=1;inner<size;++inner){
                const bool a=dir==0?get(inner,outer):get(outer,inner);
                const bool b=dir==0?get(inner-1,outer):get(outer,inner-1);
                if(a==b)++len;
                else{if(len>=5)score+=3+len-5;len=1;}
            }
            if(len>=5)score+=3+len-5;
            // Finder-like patterns with minimum 4 light modules on either side.
            for(int i=0;i+10<size;++i){
                static constexpr int p1[11]={1,0,1,1,1,0,1,0,0,0,0};
                static constexpr int p2[11]={0,0,0,0,1,0,1,1,1,0,1};
                bool match1=true,match2=true;
                for(int j=0;j<11;++j){
                    const bool bit=dir==0?get(i+j,outer):get(outer,i+j);
                    match1&=(bit==static_cast<bool>(p1[j]));
                    match2&=(bit==static_cast<bool>(p2[j]));
                }
                if(match1||match2)score+=40;
            }
        }
        for(int y=0;y<size-1;++y)for(int x=0;x<size-1;++x){
            const auto v=get(x,y);
            if(get(x+1,y)==v&&get(x,y+1)==v&&get(x+1,y+1)==v)score+=3;
        }
        int black=0;for(auto c:cell)black+=c!=0;
        const int total=size*size;
        score+=(std::abs(black*20-total*10)/total)*10;
        return score;
    }
};
std::vector<uint8_t> codewords(std::string_view text,int version,ErrorCorrection ecc){
    Bits bits;
    addBits(bits,4,4); // byte mode
    addBits(bits,static_cast<unsigned>(text.size()),version<=9?8:16);
    for(unsigned char ch:text)addBits(bits,ch,8);
    const int bitCap=capacity(version,ecc)*8;
    const int remaining=bitCap-static_cast<int>(bits.size());
    if(remaining<0)throw std::logic_error("Invalid QR version capacity");
    for(int i=0;i<std::min(4,remaining);++i)bits.push_back(0);
    while(bits.size()%8!=0)bits.push_back(0);
    std::vector<uint8_t> data;
    for(size_t i=0;i<bits.size();i+=8){
        uint8_t b=0;for(int j=0;j<8;++j)b=static_cast<uint8_t>((b<<1u)|bits[i+static_cast<size_t>(j)]);
        data.push_back(b);
    }
    for(int i=0;static_cast<int>(data.size())<capacity(version,ecc);++i)
        data.push_back(static_cast<uint8_t>((i%2==0)?0xEC:0x11));
    const auto& spec=kBlocks[version-1][static_cast<int>(ecc)];
    std::vector<std::vector<uint8_t>> blocks,check;
    size_t offset=0;
    auto append=[&](int count,int total,int payload){
        for(int b=0;b<count;++b){
            blocks.emplace_back(data.begin()+static_cast<std::ptrdiff_t>(offset),data.begin()+static_cast<std::ptrdiff_t>(offset+payload));
            check.push_back(reedSolomon(blocks.back(),total-payload));
            offset+=static_cast<size_t>(payload);
        }
    };
    append(spec.countA,spec.totalA,spec.dataA);
    append(spec.countB,spec.totalB,spec.dataB);
    std::vector<uint8_t> result;
    for(size_t j=0;j<255;++j){
        bool used=false;
        for(const auto& block:blocks)if(j<block.size()){result.push_back(block[j]);used=true;}
        if(!used)break;
    }
    for(size_t j=0;j<255;++j){
        bool used=false;
        for(const auto& block:check)if(j<block.size()){result.push_back(block[j]);used=true;}
        if(!used)break;
    }
    return result;
}
void put32(std::vector<uint8_t>& dst,uint32_t val){for(int shift=24;shift>=0;shift-=8)dst.push_back(static_cast<uint8_t>(val>>shift));}
uint32_t crc32(const uint8_t* data,size_t n){
    uint32_t crc=0xFFFFFFFFu;
    for(size_t i=0;i<n;++i){crc^=data[i];for(int b=0;b<8;++b)crc=(crc>>1u)^(0xEDB88320u&static_cast<uint32_t>(-static_cast<int>(crc&1u)));}
    return crc^0xFFFFFFFFu;
}
void chunk(std::vector<uint8_t>& dst,const char* type,const std::vector<uint8_t>& bytes){
    put32(dst,static_cast<uint32_t>(bytes.size()));
    const size_t start=dst.size();
    for(int i=0;i<4;++i)dst.push_back(static_cast<uint8_t>(type[i]));
    dst.insert(dst.end(),bytes.begin(),bytes.end());
    put32(dst,crc32(dst.data()+start,4+bytes.size()));
}
} // namespace

Matrix encode(std::string_view text,ErrorCorrection ecc){
    if(text.empty())throw std::invalid_argument("Open a document containing text before generating a QR code");
    if(text.find('\0')!=std::string_view::npos)throw std::invalid_argument("Binary documents with NUL bytes are not supported");
    int version=0;
    for(int v=1;v<=40;++v){
        const uint64_t required=4u+static_cast<uint64_t>(v<=9?8:16)+text.size()*8u;
        if(required<=static_cast<uint64_t>(capacity(v,ecc))*8u){version=v;break;}
    }
    if(version==0)throw std::length_error("Text exceeds the capacity of one QR code at the selected error correction level; use a shorter document or lower ECC");
    Grid base(sizeFor(version));
    base.drawPatterns(version,ecc);
    base.placeData(codewords(text,version,ecc));
    Grid chosen=base;
    int bestScore=std::numeric_limits<int>::max(),bestMask=0;
    for(int mask=0;mask<8;++mask){
        Grid candidate=base;
        candidate.applyMask(mask);
        candidate.drawFormat(ecc,mask);
        const int score=candidate.penalty();
        if(score<bestScore){bestScore=score;bestMask=mask;chosen=std::move(candidate);}
    }
    return {version,chosen.size,bestMask,std::move(chosen.cell)};
}
std::string toSvg(const Matrix& qr,int border){
    if(border<0||border>32)throw std::invalid_argument("SVG quiet zone must be between 0 and 32 modules");
    const int dim=qr.size+border*2;
    std::string result="<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 "+std::to_string(dim)+" "+std::to_string(dim)+"\" shape-rendering=\"crispEdges\">\n";
    result+="<rect width=\"100%\" height=\"100%\" fill=\"#fff\"/>\n";
    result+="<path d=\"";
    for(int y=0;y<qr.size;++y)for(int x=0;x<qr.size;){
        if(!qr.black(x,y)){++x;continue;}
        const int start=x;
        while(x<qr.size&&qr.black(x,y))++x;
        result+="M"+std::to_string(start+border)+" "+std::to_string(y+border)+"h"+std::to_string(x-start)+"v1h-"+std::to_string(x-start)+"z";
    }
    result+="\" fill=\"#000\"/>\n</svg>\n";
    return result;
}
std::vector<uint8_t> toPng(const Matrix& qr,int scale,int border){
    if(scale<1||scale>32||border<0||border>32)throw std::invalid_argument("PNG scale must be 1..32 and quiet zone 0..32");
    const int pixels=(qr.size+border*2)*scale;
    const size_t bytesPerRow=static_cast<size_t>((pixels+7)/8);
    std::vector<uint8_t> scanlines(static_cast<size_t>(pixels)*(1+bytesPerRow),0xFF);
    for(int y=0;y<pixels;++y){
        const size_t start=static_cast<size_t>(y)*(1+bytesPerRow);
        scanlines[start]=0; // PNG filter None.
        const int yy=y/scale-border;
        if(yy<0||yy>=qr.size)continue;
        for(int xx=0;xx<qr.size;++xx)if(qr.black(xx,yy)){
            const int begin=(xx+border)*scale;
            for(int dx=0;dx<scale;++dx){
                const int x=begin+dx;
                scanlines[start+1+static_cast<size_t>(x/8)]&=static_cast<uint8_t>(~(1u<<(7-(x%8))));
            }
        }
    }
    std::vector<uint8_t> zlib{0x78,0x01}; // DEFLATE uncompressed blocks, no external dependencies.
    for(size_t offset=0;offset<scanlines.size();){
        const size_t take=std::min<size_t>(65535,scanlines.size()-offset);
        const bool final=(offset+take==scanlines.size());
        zlib.push_back(final?1:0);
        const uint16_t len=static_cast<uint16_t>(take);
        const uint16_t inv=static_cast<uint16_t>(~len);
        zlib.push_back(static_cast<uint8_t>(len));zlib.push_back(static_cast<uint8_t>(len>>8));
        zlib.push_back(static_cast<uint8_t>(inv));zlib.push_back(static_cast<uint8_t>(inv>>8));
        zlib.insert(zlib.end(),scanlines.begin()+static_cast<std::ptrdiff_t>(offset),scanlines.begin()+static_cast<std::ptrdiff_t>(offset+take));
        offset+=take;
    }
    uint32_t a=1,b=0;
    for(uint8_t byte:scanlines){a=(a+byte)%65521u;b=(b+a)%65521u;}
    put32(zlib,(b<<16u)|a);
    std::vector<uint8_t> png{137,80,78,71,13,10,26,10};
    std::vector<uint8_t> ihdr;
    put32(ihdr,static_cast<uint32_t>(pixels));put32(ihdr,static_cast<uint32_t>(pixels));
    ihdr.insert(ihdr.end(),{1,0,0,0,0}); // grayscale 1-bit, no interlace
    chunk(png,"IHDR",ihdr);chunk(png,"IDAT",zlib);chunk(png,"IEND",{});
    return png;
}
} // namespace dodevqr
