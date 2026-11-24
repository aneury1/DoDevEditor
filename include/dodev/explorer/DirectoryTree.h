#pragma once
#include <filesystem>
#include <set>
#include <vector>
namespace dodev::explorer {
struct TreeRow { std::filesystem::path path; unsigned depth; bool directory; bool expanded; };
std::vector<TreeRow> directoryContents(const std::filesystem::path& path,bool showHidden);
class DirectoryTree {
public:
 void root(std::filesystem::path p);
 void toggle(const std::filesystem::path& p);
 void reveal(const std::filesystem::path& p);
 void collapse();
 void hidden(bool value){showHidden_=value;}
 bool hidden() const{return showHidden_;}
 std::vector<TreeRow> rows() const;
private:
 std::filesystem::path root_;
 std::set<std::filesystem::path> expanded_;
 bool showHidden_=true;
};
}
