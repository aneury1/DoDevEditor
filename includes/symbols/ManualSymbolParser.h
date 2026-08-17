#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace dodev::symbols
{

enum class Language
{
    Unknown,
    C,
    Cpp,
    Kotlin
};

enum class SymbolKind
{
    Function,
    Method,
    Constructor,
    Destructor,
    Class,
    Struct,
    Union,
    Interface,
    Object,
    Enum,
    Namespace,
    Package,
    Variable,
    Field,
    Property,
    TypeAlias,
    Macro,
    Other
};

enum class CallResolution
{
    Unresolved,
    NameOnly,
    ClassResolved,
    FullyResolved
};

struct SourceLocation
{
    std::string file;
    unsigned line = 0;
    unsigned column = 0;
    std::size_t offset = 0;

    bool IsValid() const { return !file.empty() && line > 0; }
};

struct Symbol
{
    std::string name;
    std::string qualifiedName;
    std::string signature;
    SymbolKind kind = SymbolKind::Other;
    SourceLocation location;
    SourceLocation end;
    bool definition = false;
};

struct CallReference
{
    std::string caller;
    std::string callee;
    std::string displayCallee;
    SourceLocation callSite;
    SourceLocation target;
    CallResolution resolution = CallResolution::Unresolved;
    bool indirect = false;
};

struct RuntimeSettings
{
    bool enabled = true;
    bool cEnabled = true;
    bool cppEnabled = true;
    bool kotlinEnabled = true;

    bool symbolsEnabled = true;
    bool callsEnabled = true;
    bool typesEnabled = true;
    bool functionsEnabled = true;
    bool variablesEnabled = true;
    bool macrosEnabled = true;
    bool autoParse = true;
};

struct AnalysisResult
{
    bool success = false;
    Language language = Language::Unknown;
    std::string languageName;
    std::string status;
    std::vector<Symbol> symbols;
    std::vector<CallReference> calls;
};

class ParserRegistry
{
public:
    ParserRegistry();

    // Extension registration is intentionally public so a future Java/Rust/etc.
    // parser can reuse the same editor/index/UI framework. Extensions may be
    // supplied with or without a leading dot.
    void RegisterExtension(const std::string& extension, Language language);
    void RegisterExtensions(Language language, const std::vector<std::string>& extensions);

    Language LanguageForPath(const std::string& path) const;
    bool SupportsFile(const std::string& path) const;

    static Language DetectLanguage(const std::string& path);
    static bool IsSupportedFile(const std::string& path);
    static bool IsLanguageEnabled(Language language, const RuntimeSettings& settings);

    AnalysisResult Parse(const std::string& path,
                         const std::string& source,
                         const RuntimeSettings& settings) const;

private:
    std::unordered_map<std::string, Language> m_extensions;
};

const char* SymbolKindName(SymbolKind kind);
const char* LanguageName(Language language);

} // namespace dodev::symbols
