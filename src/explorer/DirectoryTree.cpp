#include "dodev/explorer/DirectoryTree.h"
#include <algorithm>
#include <functional>
namespace dodev::explorer {
namespace fs=std::filesystem;
std::vector<TreeRow> directoryContents(const fs::path& path,bool showHidden){
 std::vector<TreeRow> out;std::error_code ec;
 for(fs::directory_iterator it(path,fs::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec)){
  auto name=it->path().filename().u8string();if(!showHidden&&!name.empty()&&name.front()=='.')continue;
  std::error_code e;auto st=it->symlink_status(e);if(!e)out.push_back({it->path(),0,fs::is_directory(st),false});
 }
 std::sort(out.begin(),out.end(),[](const TreeRow& a,const TreeRow& b){return a.directory!=b.directory?a.directory>b.directory:a.path.filename()<b.path.filename();});return out;
}
void DirectoryTree::root(fs::path p){p=p.lexically_normal();if(p==root_)return;root_=std::move(p);collapse();}
void DirectoryTree::collapse(){expanded_.clear();if(!root_.empty())expanded_.insert(root_);}
void DirectoryTree::toggle(const fs::path& p){if(!expanded_.erase(p))expanded_.insert(p);}
void DirectoryTree::reveal(const fs::path& p){auto rel=p.lexically_relative(root_);if(rel.empty()||*rel.begin()=="..")return;for(auto q=p.parent_path();!q.empty();q=q.parent_path()){expanded_.insert(q);if(q==root_||q==q.parent_path())break;}}
std::vector<TreeRow> DirectoryTree::rows() const{
 std::vector<TreeRow> out;if(root_.empty())return out;
 std::function<void(const fs::path&,unsigned,bool)> visit=[&](const fs::path& p,unsigned depth,bool dir){
 if(out.size()>=20000)return;bool open=dir&&expanded_.count(p);out.push_back({p,depth,dir,open});if(!open||depth>=128)return;
 std::error_code ec;std::vector<std::pair<fs::path,bool>> children;
 for(fs::directory_iterator it(p,fs::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec)){
 auto name=it->path().filename().u8string();if(!showHidden_&&!name.empty()&&name.front()=='.')continue;
 std::error_code e;auto st=it->symlink_status(e);if(e)continue;children.emplace_back(it->path(),fs::is_directory(st));}
 std::sort(children.begin(),children.end(),[](const auto& a,const auto& b){return a.second!=b.second?a.second>b.second:a.first.filename()<b.first.filename();});
 for(const auto& c:children)visit(c.first,depth+1,c.second);
 };visit(root_,0,true);return out;
}
}
