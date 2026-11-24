#pragma once
#include "SourceSymbols.h"
#include <filesystem>
namespace dodev::symbols {
struct LocatedCall { Call call; std::filesystem::path path; };
struct WorkspaceIndex {
    std::vector<LocatedCall> calls;
    std::filesystem::path root;
    std::vector<Symbol> rows;
    std::vector<std::filesystem::path> paths;
    std::size_t hits=0, parsed=0, skipped=0;
    std::string diagnostic;
};
WorkspaceIndex indexWorkspace(const std::filesystem::path& root,const Options& options);
}
