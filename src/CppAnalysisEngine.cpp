#include "CppAnalysisEngine.h"

#include <wx/filename.h>
#include <wx/utils.h>
#include <wx/filefn.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <set>
#include <sstream>
#include <string>
#include <utility>

#ifndef DODEV_HAS_LIBCLANG
#define DODEV_HAS_LIBCLANG 0
#endif

bool UseCppDriver(const wxString& path, const wxString& standard)
{
    const wxString ext = wxFileName(path).GetExt().Lower();
    if (ext == "c")
        return false;
    if (ext == "h")
        return standard.Lower().StartsWith("c++");
    if (CppAnalysisEngine::IsCppFile(path))
        return true;
    return standard.Lower().StartsWith("c++");
}

wxString EffectiveStandard(bool cpp, const wxString& configured)
{
    const wxString lower = configured.Lower();
    if (cpp)
        return lower.StartsWith("c++") ? configured : wxString("c++17");
    return lower.StartsWith("c++") || configured.IsEmpty() ? wxString("c17") : configured;
}

#if DODEV_HAS_LIBCLANG
#include <clang-c/Index.h>
#endif

namespace
{
wxString NormalizePath(const wxString& path, const wxString& projectRoot)
{
    if (path.IsEmpty())
        return path;

    wxFileName fn(path);
    if (fn.IsRelative() && !projectRoot.IsEmpty())
        fn.MakeAbsolute(projectRoot);
    fn.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    return fn.GetFullPath();
}

void AppendDefaultIncludeDirs(std::vector<wxString>& includeDirs,
                              const wxString& projectRoot,
                              const wxString& sourcePath)
{
    std::set<wxString> seen;
    for (const auto& dir : includeDirs)
        seen.insert(NormalizePath(dir, projectRoot));

    auto appendIfDirectory = [&](const wxString& path)
    {
        if (path.IsEmpty())
            return;
        const wxString normalized = NormalizePath(path, projectRoot);
        if (wxDirExists(normalized) && seen.insert(normalized).second)
            includeDirs.push_back(normalized);
    };

    wxFileName source(sourcePath);
    appendIfDirectory(source.GetPath());
    appendIfDirectory(projectRoot);
    if (!projectRoot.IsEmpty())
    {
        appendIfDirectory(projectRoot + wxFileName::GetPathSeparator() + "include");
        appendIfDirectory(projectRoot + wxFileName::GetPathSeparator() + "includes");
        appendIfDirectory(projectRoot + wxFileName::GetPathSeparator() + "src");
    }
}

#if DODEV_HAS_LIBCLANG
wxString FromCXString(CXString value)
{
    const char* c = clang_getCString(value);
    wxString result = c ? wxString::FromUTF8(c) : wxString();
    clang_disposeString(value);
    return result;
}

CppSourceLocation GetLocation(CXCursor cursor)
{
    CppSourceLocation result;
    CXSourceLocation location = clang_getCursorLocation(cursor);
    CXFile file = nullptr;
    unsigned line = 0;
    unsigned column = 0;
    unsigned offset = 0;
    clang_getExpansionLocation(location, &file, &line, &column, &offset);
    if (file)
        result.file = FromCXString(clang_getFileName(file));
    result.line = line;
    result.column = column;
    return result;
}

bool IsFromFile(CXCursor cursor, const wxString& path)
{
    const CppSourceLocation location = GetLocation(cursor);
    if (!location.IsValid())
        return false;

    wxFileName a(location.file);
    wxFileName b(path);
    a.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    b.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
#ifdef __WXMSW__
    return a.GetFullPath().CmpNoCase(b.GetFullPath()) == 0;
#else
    return a.GetFullPath() == b.GetFullPath();
#endif
}

CppSymbolKind MapSymbolKind(CXCursorKind kind)
{
    switch (kind)
    {
    case CXCursor_FunctionDecl:
    case CXCursor_FunctionTemplate: return CppSymbolKind::Function;
    case CXCursor_CXXMethod: return CppSymbolKind::Method;
    case CXCursor_Constructor: return CppSymbolKind::Constructor;
    case CXCursor_Destructor: return CppSymbolKind::Destructor;
    case CXCursor_ClassDecl:
    case CXCursor_ClassTemplate: return CppSymbolKind::Class;
    case CXCursor_StructDecl: return CppSymbolKind::Struct;
    case CXCursor_EnumDecl: return CppSymbolKind::Enum;
    case CXCursor_Namespace: return CppSymbolKind::Namespace;
    case CXCursor_VarDecl: return CppSymbolKind::Variable;
    case CXCursor_FieldDecl: return CppSymbolKind::Field;
    case CXCursor_TypedefDecl:
    case CXCursor_TypeAliasDecl: return CppSymbolKind::Typedef;
    case CXCursor_MacroDefinition: return CppSymbolKind::Macro;
    default: return CppSymbolKind::Other;
    }
}

bool IsInterestingSymbol(CXCursorKind kind)
{
    return MapSymbolKind(kind) != CppSymbolKind::Other;
}

CppSourceLocation DefinitionLocation(CXCursor cursor)
{
    CXCursor referenced = clang_getCursorReferenced(cursor);
    if (clang_Cursor_isNull(referenced))
        referenced = cursor;

    CXCursor definition = clang_getCursorDefinition(referenced);
    if (!clang_Cursor_isNull(definition))
        return GetLocation(definition);

    return GetLocation(referenced);
}

wxString CursorDisplayName(CXCursor cursor)
{
    wxString value = FromCXString(clang_getCursorDisplayName(cursor));
    if (value.IsEmpty())
        value = FromCXString(clang_getCursorSpelling(cursor));
    return value;
}

struct ParseContext
{
    wxString mainFile;
    CppAnalysisResult* result = nullptr;
};

CXChildVisitResult CollectSymbolsVisitor(CXCursor cursor, CXCursor, CXClientData data)
{
    auto* context = static_cast<ParseContext*>(data);
    const CXCursorKind kind = clang_getCursorKind(cursor);

    if (IsInterestingSymbol(kind) && IsFromFile(cursor, context->mainFile))
    {
        CppSymbol symbol;
        symbol.name = FromCXString(clang_getCursorSpelling(cursor));
        symbol.displayName = CursorDisplayName(cursor);
        if (symbol.displayName.IsEmpty())
            symbol.displayName = symbol.name;
        symbol.kind = MapSymbolKind(kind);
        symbol.location = GetLocation(cursor);
        symbol.definition = clang_isCursorDefinition(cursor) != 0;
        if (!symbol.displayName.IsEmpty())
            context->result->symbols.push_back(std::move(symbol));
    }

    return CXChildVisit_Recurse;
}

struct CallVisitContext
{
    CppCallNode* parent = nullptr;
};

CXChildVisitResult CollectCallsVisitor(CXCursor cursor, CXCursor, CXClientData data)
{
    auto* context = static_cast<CallVisitContext*>(data);
    if (clang_getCursorKind(cursor) == CXCursor_CallExpr)
    {
        CppCallNode call;
        call.name = CursorDisplayName(cursor);
        CXCursor referenced = clang_getCursorReferenced(cursor);
        if (!clang_Cursor_isNull(referenced))
        {
            wxString referencedName = CursorDisplayName(referenced);
            if (!referencedName.IsEmpty())
                call.name = referencedName;
        }
        if (call.name.IsEmpty())
            call.name = "<call>";

        call.callSite = GetLocation(cursor);
        call.target = DefinitionLocation(cursor);
        context->parent->children.push_back(std::move(call));
        CppCallNode& inserted = context->parent->children.back();
        CallVisitContext nested{&inserted};
        clang_visitChildren(cursor, CollectCallsVisitor, &nested);
        return CXChildVisit_Continue;
    }

    return CXChildVisit_Recurse;
}

struct FunctionVisitContext
{
    wxString mainFile;
    CppAnalysisResult* result = nullptr;
};

bool IsCallableDefinition(CXCursor cursor)
{
    const CXCursorKind kind = clang_getCursorKind(cursor);
    const bool callable = kind == CXCursor_FunctionDecl ||
                          kind == CXCursor_CXXMethod ||
                          kind == CXCursor_Constructor ||
                          kind == CXCursor_Destructor ||
                          kind == CXCursor_FunctionTemplate;
    return callable && clang_isCursorDefinition(cursor) != 0;
}

CXChildVisitResult CollectFunctionCallsVisitor(CXCursor cursor, CXCursor, CXClientData data)
{
    auto* context = static_cast<FunctionVisitContext*>(data);
    if (IsCallableDefinition(cursor) && IsFromFile(cursor, context->mainFile))
    {
        CppCallNode root;
        root.name = CursorDisplayName(cursor);
        if (root.name.IsEmpty())
            root.name = FromCXString(clang_getCursorSpelling(cursor));
        root.callSite = GetLocation(cursor);
        root.target = root.callSite;

        CallVisitContext callContext{&root};
        clang_visitChildren(cursor, CollectCallsVisitor, &callContext);
        context->result->callTree.push_back(std::move(root));
        return CXChildVisit_Continue;
    }
    return CXChildVisit_Recurse;
}

std::vector<std::string> BuildUtf8Args(const wxString& path,
                                       const CppAnalysisOptions& options)
{
    std::vector<std::string> args;
    const bool cpp = UseCppDriver(path, options.standard);
    const wxString standard = EffectiveStandard(cpp, options.standard);

    args.emplace_back(cpp ? "-xc++" : "-xc");
    if (!standard.IsEmpty())
        args.emplace_back(("-std=" + standard).ToStdString());

    for (const auto& define : options.defines)
    {
        if (!define.IsEmpty())
            args.emplace_back(("-D" + define).ToStdString());
    }

    std::vector<wxString> includes = options.includeDirs;
    AppendDefaultIncludeDirs(includes, options.projectRoot, path);
    for (const auto& includeDir : includes)
    {
        const wxString normalized = NormalizePath(includeDir, options.projectRoot);
        if (!normalized.IsEmpty())
            args.emplace_back(("-I" + normalized).ToStdString());
    }

    for (const auto& arg : options.extraArgs)
    {
        if (!arg.IsEmpty())
            args.emplace_back(arg.ToStdString());
    }
    return args;
}

struct TranslationUnitHandle
{
    CXIndex index = nullptr;
    CXTranslationUnit tu = nullptr;

    ~TranslationUnitHandle()
    {
        if (tu)
            clang_disposeTranslationUnit(tu);
        if (index)
            clang_disposeIndex(index);
    }
};

bool CreateTranslationUnit(const wxString& path,
                           const wxString& unsavedContents,
                           const CppAnalysisOptions& options,
                           TranslationUnitHandle& handle,
                           wxString& error)
{
    handle.index = clang_createIndex(0, 0);
    if (!handle.index)
    {
        error = "clang_createIndex() failed";
        return false;
    }

    std::vector<std::string> utf8Args = BuildUtf8Args(path, options);
    std::vector<const char*> argPointers;
    argPointers.reserve(utf8Args.size());
    for (const auto& arg : utf8Args)
        argPointers.push_back(arg.c_str());

    const wxCharBuffer pathBuffer = path.ToUTF8();
    if (!pathBuffer.data())
    {
        error = "Could not convert source path to UTF-8";
        return false;
    }

    std::string contents;
    CXUnsavedFile unsaved{};
    CXUnsavedFile* unsavedPtr = nullptr;
    unsigned unsavedCount = 0;
    {
        const wxCharBuffer contentBuffer = unsavedContents.ToUTF8();
        if (contentBuffer.data())
        {
            contents.assign(contentBuffer.data(), std::strlen(contentBuffer.data()));
            unsaved.Filename = pathBuffer.data();
            unsaved.Contents = contents.data();
            unsaved.Length = static_cast<unsigned long>(contents.size());
            unsavedPtr = &unsaved;
            unsavedCount = 1;
        }
    }

    const unsigned flags = CXTranslationUnit_DetailedPreprocessingRecord |
                           CXTranslationUnit_KeepGoing;

    const CXErrorCode code = clang_parseTranslationUnit2(
        handle.index,
        pathBuffer.data(),
        argPointers.empty() ? nullptr : argPointers.data(),
        static_cast<int>(argPointers.size()),
        unsavedPtr,
        unsavedCount,
        flags,
        &handle.tu);

    if (code != CXError_Success || !handle.tu)
    {
        error = wxString::Format("libclang could not parse the translation unit (error %d)",
                                 static_cast<int>(code));
        return false;
    }
    return true;
}
#endif
}

bool CppAnalysisEngine::HasLibClang()
{
#if DODEV_HAS_LIBCLANG
    return true;
#else
    return false;
#endif
}

wxString CppAnalysisEngine::LibClangVersion()
{
#if DODEV_HAS_LIBCLANG
    return FromCXString(clang_getClangVersion());
#else
    return "libclang not linked";
#endif
}

bool CppAnalysisEngine::IsCppFile(const wxString& path)
{
    const wxString ext = wxFileName(path).GetExt().Lower();
    return ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "c++" ||
           ext == "hpp" || ext == "hxx" || ext == "hh" || ext == "inl" ||
           ext == "ipp" || ext == "tpp";
}

bool CppAnalysisEngine::IsCFile(const wxString& path)
{
    const wxString ext = wxFileName(path).GetExt().Lower();
    return ext == "c" || ext == "h";
}

bool CppAnalysisEngine::IsCOrCppFile(const wxString& path)
{
    return IsCFile(path) || IsCppFile(path);
}

CppAnalysisResult CppAnalysisEngine::Parse(const wxString& path,
                                           const wxString& unsavedContents,
                                           const CppAnalysisOptions& options) const
{
    CppAnalysisResult result;
    if (!IsCOrCppFile(path))
    {
        result.status = "Current document is not C/C++";
        return result;
    }

#if !DODEV_HAS_LIBCLANG
    result.status = "LLVM analysis is not available in this build (libclang was not found by CMake).";
    return result;
#else
    TranslationUnitHandle handle;
    wxString error;
    if (!CreateTranslationUnit(path, unsavedContents, options, handle, error))
    {
        result.status = error;
        return result;
    }

    const unsigned diagnosticCount = clang_getNumDiagnostics(handle.tu);
    for (unsigned i = 0; i < diagnosticCount; ++i)
    {
        CXDiagnostic diagnostic = clang_getDiagnostic(handle.tu, i);
        const CXDiagnosticSeverity severity = clang_getDiagnosticSeverity(diagnostic);
        if (severity >= CXDiagnostic_Warning)
        {
            const unsigned formatOptions = clang_defaultDiagnosticDisplayOptions();
            result.diagnostics.push_back(
                FromCXString(clang_formatDiagnostic(diagnostic, formatOptions)));
        }
        clang_disposeDiagnostic(diagnostic);
    }

    CXCursor root = clang_getTranslationUnitCursor(handle.tu);
    ParseContext symbolContext{path, &result};
    clang_visitChildren(root, CollectSymbolsVisitor, &symbolContext);

    FunctionVisitContext functionContext{path, &result};
    clang_visitChildren(root, CollectFunctionCallsVisitor, &functionContext);

    std::sort(result.symbols.begin(), result.symbols.end(), [](const CppSymbol& a, const CppSymbol& b)
    {
        if (a.location.line != b.location.line)
            return a.location.line < b.location.line;
        if (a.location.column != b.location.column)
            return a.location.column < b.location.column;
        return a.displayName < b.displayName;
    });

    result.success = true;
    result.status = wxString::Format("Parsed %zu symbols, %zu call roots",
                                     result.symbols.size(), result.callTree.size());
    return result;
#endif
}

bool CppAnalysisEngine::FindDefinition(const wxString& path,
                                       const wxString& unsavedContents,
                                       unsigned line,
                                       unsigned column,
                                       const CppAnalysisOptions& options,
                                       CppSourceLocation& definition,
                                       wxString& diagnostic) const
{
    definition = {};
#if !DODEV_HAS_LIBCLANG
    diagnostic = "Go to Definition requires a build with libclang enabled.";
    return false;
#else
    TranslationUnitHandle handle;
    if (!CreateTranslationUnit(path, unsavedContents, options, handle, diagnostic))
        return false;

    const wxCharBuffer pathBuffer = path.ToUTF8();
    CXFile file = clang_getFile(handle.tu, pathBuffer.data());
    if (!file)
    {
        diagnostic = "libclang could not resolve the current file in the translation unit.";
        return false;
    }

    CXSourceLocation location = clang_getLocation(handle.tu, file, line, column);
    CXCursor cursor = clang_getCursor(handle.tu, location);
    if (clang_Cursor_isNull(cursor))
    {
        diagnostic = "No C/C++ symbol at the caret.";
        return false;
    }

    CXCursor referenced = clang_getCursorReferenced(cursor);
    if (clang_Cursor_isNull(referenced))
        referenced = cursor;

    CXCursor target = clang_getCursorDefinition(referenced);
    if (clang_Cursor_isNull(target))
        target = referenced;

    definition = GetLocation(target);
    if (!definition.IsValid())
    {
        diagnostic = "Definition location was not available.";
        return false;
    }
    diagnostic.clear();
    return true;
#endif
}

wxString CppAnalysisEngine::Quote(const wxString& value)
{
#ifdef __WXMSW__
    wxString escaped = value;
    escaped.Replace("\"", "\\\"");
    return "\"" + escaped + "\"";
#else
    wxString escaped = value;
    escaped.Replace("'", "'\"'\"'");
    return "'" + escaped + "'";
#endif
}

CppCompileResult CppAnalysisEngine::Compile(const wxString& path,
                                            const CppAnalysisOptions& options,
                                            bool syntaxOnly) const
{
    CppCompileResult result;
    if (!IsCOrCppFile(path))
    {
        result.output.push_back("Current document is not a C/C++ source file.");
        return result;
    }

    const bool cpp = UseCppDriver(path, options.standard);
    const wxString standard = EffectiveStandard(cpp, options.standard);
    const wxString compiler = cpp ? "clang++" : "clang";
    wxString command = compiler;

    if (!standard.IsEmpty())
        command += " -std=" + standard;

    for (const auto& define : options.defines)
    {
        if (!define.IsEmpty())
            command += " -D" + Quote(define);
    }

    std::vector<wxString> includes = options.includeDirs;
    AppendDefaultIncludeDirs(includes, options.projectRoot, path);
    for (const auto& includeDir : includes)
    {
        const wxString normalized = NormalizePath(includeDir, options.projectRoot);
        if (!normalized.IsEmpty())
            command += " -I" + Quote(normalized);
    }

    for (const auto& arg : options.extraArgs)
    {
        if (!arg.IsEmpty())
            command += " " + arg;
    }

    command += " -Wall -Wextra";

    const wxString ext = wxFileName(path).GetExt().Lower();
    const bool header = ext == "h" || ext == "hpp" || ext == "hxx" || ext == "hh";
    if (syntaxOnly || header)
    {
        command += " -fsyntax-only " + Quote(path);
    }
    else
    {
        wxString outputRoot = options.projectRoot;
        if (outputRoot.IsEmpty())
            outputRoot = wxFileName(path).GetPath();
        outputRoot += wxFileName::GetPathSeparator();
        outputRoot += ".dodev";
        wxFileName::Mkdir(outputRoot, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        outputRoot += wxFileName::GetPathSeparator();
        outputRoot += "llvm-obj";
        wxFileName::Mkdir(outputRoot, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

        wxString base = wxFileName(path).GetName();
        const unsigned long hash = static_cast<unsigned long>(std::hash<std::string>{}(path.ToStdString()));
        result.outputFile = outputRoot + wxFileName::GetPathSeparator() +
                            wxString::Format("%s_%08lx.o", base.c_str(), hash & 0xffffffffUL);
        command += " -c " + Quote(path) + " -o " + Quote(result.outputFile);
    }

    result.command = command;
    wxArrayString stdoutLines;
    wxArrayString stderrLines;
    result.exitCode = wxExecute(command, stdoutLines, stderrLines, wxEXEC_SYNC);

    result.output.push_back("$ " + command);
    for (const auto& line : stdoutLines)
        result.output.push_back(line);
    for (const auto& line : stderrLines)
        result.output.push_back(line);

    result.success = result.exitCode == 0;
    if (result.success)
    {
        if (syntaxOnly || header)
            result.output.push_back("LLVM/Clang syntax check succeeded.");
        else
            result.output.push_back("Compiled object: " + result.outputFile);
    }
    else if (result.exitCode == -1)
    {
        result.output.push_back("Could not execute Clang. Make sure clang/clang++ is installed and on PATH.");
    }
    return result;
}
