#pragma once

#include <wx/string.h>
#include <vector>

struct CppSourceLocation
{
    wxString file;
    unsigned line = 0;
    unsigned column = 0;

    bool IsValid() const { return !file.IsEmpty() && line > 0; }
};

enum class CppSymbolKind
{
    Function,
    Method,
    Constructor,
    Destructor,
    Class,
    Struct,
    Enum,
    Namespace,
    Variable,
    Field,
    Typedef,
    Macro,
    Other
};

struct CppSymbol
{
    wxString name;
    wxString displayName;
    CppSymbolKind kind = CppSymbolKind::Other;
    CppSourceLocation location;
    bool definition = false;
};

struct CppCallNode
{
    wxString name;
    CppSourceLocation callSite;
    CppSourceLocation target;
    std::vector<CppCallNode> children;
};

struct CppAnalysisOptions
{
    wxString projectRoot;
    wxString standard = "c++17";
    std::vector<wxString> defines;
    std::vector<wxString> includeDirs;
    std::vector<wxString> extraArgs;
};

struct CppAnalysisResult
{
    bool success = false;
    wxString status;
    std::vector<CppSymbol> symbols;
    std::vector<CppCallNode> callTree;
    std::vector<wxString> diagnostics;
};

struct CppCompileResult
{
    bool success = false;
    long exitCode = -1;
    wxString command;
    wxString outputFile;
    std::vector<wxString> output;
};

class CppAnalysisEngine
{
public:
    static bool HasLibClang();
    static wxString LibClangVersion();
    static bool IsCppFile(const wxString& path);
    static bool IsCFile(const wxString& path);
    static bool IsCOrCppFile(const wxString& path);

    CppAnalysisResult Parse(const wxString& path,
                            const wxString& unsavedContents,
                            const CppAnalysisOptions& options) const;

    bool FindDefinition(const wxString& path,
                        const wxString& unsavedContents,
                        unsigned line,
                        unsigned column,
                        const CppAnalysisOptions& options,
                        CppSourceLocation& definition,
                        wxString& diagnostic) const;

    CppCompileResult Compile(const wxString& path,
                             const CppAnalysisOptions& options,
                             bool syntaxOnly = false) const;

private:
    static wxString Quote(const wxString& value);
};
