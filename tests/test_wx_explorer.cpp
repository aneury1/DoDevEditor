#include "WxExplorerModel.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace ex=wxui::explorer;
namespace fs=std::filesystem;
static void check(bool v,const char* msg){if(!v)throw std::runtime_error(msg);}
int main(){
 auto tmp=fs::temp_directory_path()/("dodev-wx-tree-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 auto root=tmp/"project";
 try{
  fs::create_directories(root/"src"/"nested"/"deep");fs::create_directories(root/".vscode");fs::create_directories(root/"build");fs::create_directories(tmp/"external");
  std::ofstream(root/"src"/"nested"/"deep"/"test.cpp")<<"int x;";
  std::ofstream(root/"README.md")<<"readme";std::ofstream(root/".hidden")<<"hidden";
  for(int i=0;i<350;++i)std::ofstream(root/"build"/(std::to_string(i)+".o"))<<i;
  auto l=ex::list(root,root,true);check(!l.error&&l.entries.size()==5,"root entries");check(l.entries.front().directory,"directories first");
  check(ex::list(root,root,false).entries.size()==3,"hidden filtering");
  check(ex::list(root/"build",root,true).entries.size()==350,"no arbitrary entry cap");
  check(ex::list(root/"src"/"nested"/"deep",root,true).entries.at(0).path.filename()=="test.cpp","deep file");
  check(ex::canExpand(root,root),"root expandable");check(bool(ex::list(root/"absent",root,true).error),"missing directory error");
  std::error_code ec;fs::create_directory_symlink(root,tmp/"link-root",ec);
  if(!ec){check(ex::canExpand(tmp/"link-root",tmp/"link-root"),"symlink workspace root");check(ex::list(tmp/"link-root",tmp/"link-root",true).entries.size()==5,"symlink workspace entries");
   fs::create_directory_symlink(root/"src",root/"alias",ec);check(!ec&&ex::canExpand(root/"alias",root),"internal symlink expandable");
   fs::create_directory_symlink(root,root/"src"/"loop",ec);check(!ec&&!ex::canExpand(root/"src"/"loop",root),"ancestor loop blocked");
   fs::create_directory_symlink(tmp/"external",root/"outside",ec);check(!ec&&!ex::canExpand(root/"outside",root),"external symlink not followed");
   fs::create_symlink(root/"missing",root/"broken",ec);check(!ec,"create broken symlink");auto all=ex::list(root,root,true);check(all.entries.size()==8,"broken and external links visible");
  }
  fs::remove_all(tmp);std::cout<<"wx explorer filesystem tests passed\n";return 0;
 }catch(const std::exception& e){fs::remove_all(tmp);std::cerr<<e.what()<<'\n';return 1;}
}
