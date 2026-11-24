#include "dodev/explorer/ExplorerOps.h"
#include <fstream>

namespace dodev::explorer {
bool validEntryName(const std::string& name){
    if(name.empty()||name=="."||name==".."||name.size()>255||name.back()==' '||name.back()=='.')return false;
    for(unsigned char c:name)if(c<32||c==127||c=='/'||c=='\\'||c==':'||c=='\0')return false;
    return true;
}
Result createEntry(const fs::path& parent,const std::string& name,bool directory){
    if(!validEntryName(name))return {false,"Invalid name (must be a single file/folder name)",{}};
    std::error_code ec;
    if(!fs::is_directory(parent,ec))return {false,"Parent directory does not exist",{}};
    const auto target=parent/fs::u8path(name);
    if(fs::exists(target,ec)||fs::is_symlink(target,ec))return {false,"An entry with this name already exists",target};
    if(directory){if(!fs::create_directory(target,ec)||ec)return {false,"Could not create directory: "+ec.message(),target};}
    else {std::ofstream out(target,std::ios::binary|std::ios::app);if(!out)return {false,"Could not create file",target};}
    return {true,{},target};
}
Result renameEntry(const fs::path& source,const std::string& newName){
    if(!validEntryName(newName))return {false,"Invalid new entry name",{}};
    std::error_code ec;
    if(!fs::exists(source,ec)&&!fs::is_symlink(source,ec))return {false,"Source does not exist",source};
    const auto dest=source.parent_path()/fs::u8path(newName);
    if(source==dest)return {true,{},source};
    if(fs::exists(dest,ec)||fs::is_symlink(dest,ec))return {false,"Destination already exists",dest};
    fs::rename(source,dest,ec);
    if(ec)return {false,"Rename failed: "+ec.message(),source};
    return {true,{},dest};
}
Result removeEntry(const fs::path& entry,const fs::path& protectedWorkspace){
    std::error_code ec;
    if(entry.empty()||entry==entry.root_path())return {false,"Cannot delete filesystem root",entry};
    if(!protectedWorkspace.empty()&&fs::weakly_canonical(entry,ec)==fs::weakly_canonical(protectedWorkspace,ec))
        return {false,"Cannot delete the currently opened workspace",entry};
    if(!fs::exists(entry,ec)&&!fs::is_symlink(entry,ec))return {false,"Entry no longer exists",entry};
    fs::remove_all(entry,ec); // Does not follow directory symlinks.
    if(ec)return {false,"Deletion failed: "+ec.message(),entry};
    return {true,{},entry};
}
}
