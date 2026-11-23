#include "dodev/search/WorkspaceSearch.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
using namespace dodev::search;
namespace fs=std::filesystem;
int main(){
 const fs::path root=fs::temp_directory_path()/("dodev_search_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::create_directories(root/"src");fs::create_directories(root/"build");
 {std::ofstream(root/"src"/"hello.cpp")<<"Alpha beta alpha\nALPHA\nAlphaBeta\n";
  std::ofstream(root/"build"/"hidden.cpp")<<"Alpha";
  std::ofstream(root/"src"/"other.h")<<"Alpha HERE\n";
  std::ofstream(root/"src"/"binary.dat",std::ios::binary)<<"Alpha\0Alpha";}
 Options opt;opt.query="alpha";opt.include="**/*.cpp,**/*.h";opt.wholeWord=true;
 const auto found=scan(root,opt); assert(found.error.empty());assert(found.filesScanned==2);
 assert(found.matches.size()==4);
 assert(found.matches[0].line==1&&found.matches[0].column==1);
 opt.caseSensitive=true;assert(scan(root,opt).matches.size()==1); // lowercase 'alpha' appears once
 opt.regex=true;opt.query="Alpha|HERE";opt.caseSensitive=true;opt.wholeWord=false;
 assert(scan(root,opt).matches.size()==4);
 opt.query="[";assert(!scan(root,opt).error.empty());
 opt.query="(Alpha)";
 std::string output,error;std::size_t count=0;
 assert(replaceText("Alpha beta Alpha",opt,"[$1]",output,count,&error));
 assert(count==2&&output=="[Alpha] beta [Alpha]");
 opt.regex=false;opt.query="Alpha";opt.caseSensitive=false;
 assert(replaceFile(root/"src"/"other.h",opt,"Replaced",count,&error));assert(count==1);
 {std::ifstream in(root/"src"/"other.h");std::string line;std::getline(in,line);assert(line=="Replaced HERE");}
 // Overlay unsaved editor buffers without changing the files on disk.
 Overlays overlays;overlays[fs::weakly_canonical(root/"src"/"hello.cpp")]="Alpha overlay\n";
 assert(scan(root,opt,overlays).matches.size()==1);
 opt.include="*.py";assert(scan(root,opt).matches.empty());
 std::error_code ec;fs::remove_all(root,ec);
 std::cout<<"Workspace search/replace passed\n";
}
