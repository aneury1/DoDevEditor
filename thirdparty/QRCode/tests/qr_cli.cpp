#include "QRMatrix.h"
#include <fstream>
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    if(argc<4){std::cerr<<"Usage: qr_cli <text-file> <output.png> <ecc:L|M|Q|H>\n";return 2;}
    std::ifstream in(argv[1],std::ios::binary);
    if(!in)return 3;
    const std::string text{std::istreambuf_iterator<char>(in),{}};
    const std::string opt=argv[3];
    const auto ecc=opt=="L"?dodevqr::ErrorCorrection::Low:opt=="Q"?dodevqr::ErrorCorrection::Quartile:opt=="H"?dodevqr::ErrorCorrection::High:dodevqr::ErrorCorrection::Medium;
    try{
        auto qr=dodevqr::encode(text,ecc);
        auto png=dodevqr::toPng(qr,8);
        std::ofstream out(argv[2],std::ios::binary);out.write(reinterpret_cast<const char*>(png.data()),static_cast<std::streamsize>(png.size()));
        std::cerr<<"version="<<qr.version<<" size="<<qr.size<<" mask="<<qr.mask<<" bytes="<<text.size()<<" png_bytes="<<png.size()<<"\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 4;}
}
