#pragma once
#include <filesystem>
#include <vector>
#include <algorithm>
#include <system_error>
namespace wxui::explorer {
namespace fs = std::filesystem;
struct Entry { fs::path path; bool directory=false, expandable=false; };
struct Listing { std::vector<Entry> entries; std::error_code error; };
inline bool within(const fs::path& p,const fs::path& root) {
    auto rel=p.lexically_relative(root);
    return p==root || (!rel.empty() && *rel.begin()!="..");
}
// A workspace opened through a symlink is valid. Nested directory links may
// be followed only within the canonical workspace and never back to ancestors.
inline bool canExpand(const fs::path& p,const fs::path& root) {
    std::error_code ec;
    if(!fs::is_directory(p,ec)||ec) return false;
    auto actual=fs::canonical(p,ec); if(ec)return false;
    auto base=fs::canonical(root,ec); if(ec||!within(actual,base))return false;
    if(p==root)return true;
    for(auto a=p.parent_path();!a.empty()&&within(a,root);a=a.parent_path()){
        auto parent=fs::canonical(a,ec);if(ec||parent==actual)return false;
        if(a==root||a==a.parent_path())break;
    }
    return true;
}
inline Listing list(const fs::path& folder,const fs::path& root,bool hidden) {
    Listing result;
    // Do not suppress permission errors: expose them in the explorer.
    for(fs::directory_iterator it(folder,result.error),end;it!=end&&!result.error;it.increment(result.error)){
        const auto p=it->path();const auto name=p.filename().u8string();
        if(!hidden&&!name.empty()&&name.front()=='.')continue;
        std::error_code ec;bool directory=it->is_directory(ec);
        // Retain even broken symlinks and entries whose metadata is inaccessible.
        result.entries.push_back({p,directory,directory&&canExpand(p,root)});
    }
    std::sort(result.entries.begin(),result.entries.end(),[](const Entry& a,const Entry& b){return a.directory!=b.directory?a.directory>b.directory:a.path.filename()<b.path.filename();});
    return result;
}
}
