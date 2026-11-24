#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace dodev::symbols {
struct Symbol {
    std::string kind;
    std::string name;
    std::string signature;
    std::size_t line=0; // one-based line
    std::size_t column=1; // one-based byte column
};
enum class ParserMode { Auto, ClangOnly, RegexOnly };
struct Options {
    ParserMode mode=ParserMode::Auto;
    std::string clangExecutable; // empty: discover clang++ / clang in PATH
    int timeoutMs=2500;
};
struct Call {
    std::string caller, callee;
    std::size_t callerLine=0, line=0;
};
struct Result {
    std::vector<Call> calls;
    std::string language;
    std::vector<Symbol> rows;
    bool truncated=false;
    std::string parser="Regex";
    std::string diagnostics;
};
// The active, unsaved buffer is passed directly to Clang via stdin. If Clang is
// unavailable/invalid/times out, Auto mode falls back to the lexical parser.
Result extract(std::string_view path,std::string_view contents,const Options& options={});
std::string toCsv(const std::vector<Symbol>& rows);
std::vector<Symbol> filter(const std::vector<Symbol>& rows,std::string_view needle);
}
