#pragma once


#if 0 
#include <clang-c/Index.h>
#include <vector>
#include <string>



struct Symbol
{
    std::string name;
    std::string type;   // function, class, variable
    std::string scope;  // namespace/class scope
    std::string returnType;
};

class CppIndexer
{
public:
    std::vector<Symbol> symbols;

    void IndexFile(const std::string& file)
    {
        CXIndex index = clang_createIndex(0, 0);

        const char* args[] = {
            "-std=c++17",
            "-I/usr/include",
        };

        CXTranslationUnit unit = clang_parseTranslationUnit(
            index,
            file.c_str(),
            args, 2,
            nullptr, 0,
            CXTranslationUnit_None
        );

        if (!unit)
            return;

        CXCursor cursor = clang_getTranslationUnitCursor(unit);

        clang_visitChildren(
            cursor,
            [](CXCursor c, CXCursor parent, CXClientData data)
            {
                auto* self = static_cast<CppIndexer*>(data);

                CXString spelling = clang_getCursorSpelling(c);
                CXString kindSpelling = clang_getCursorKindSpelling(clang_getCursorKind(c));

                std::string name = clang_getCString(spelling);
                std::string kind = clang_getCString(kindSpelling);

                clang_disposeString(spelling);
                clang_disposeString(kindSpelling);

                if (name.empty())
                    return CXChildVisit_Continue;

                Symbol s;
                s.name = name;
                s.type = kind;

                self->symbols.push_back(s);

                return CXChildVisit_Recurse;
            },
            this
        );

        clang_disposeTranslationUnit(unit);
        clang_disposeIndex(index);
    }
};

class IntelliSenseEngine
{
public:
    std::vector<Symbol> symbols;

    std::string Query(const std::string& prefix)
    {
        std::string result;

        for (auto& s : symbols)
        {
            if (s.name.find(prefix) == 0)
            {
                if (!result.empty())
                    result += " ";

                result += s.name;
            }
        }

        return result;
    }
};
#endif
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <vector>

static std::string Trim(std::string s)
{
    size_t b = s.find_first_not_of(" \t");
    size_t e = s.find_last_not_of(" \t");
    return (b == std::string::npos) ? "" : s.substr(b, e - b + 1);
}

static bool StartsWith(const std::string& s, const std::string& p)
{
    return s.rfind(p, 0) == 0;
}

enum class SymbolType
{
    Function,
    Class,
    Struct,
    Variable,
    Member
};

struct Symbol
{
    std::string name;
    std::string scope;      // global / class name
    SymbolType type;
    std::string returnType; // optional
};
static std::string ExtractName(const std::string& line, const std::string& keyword)
{
    // Find keyword position
    size_t pos = line.find(keyword);
    if (pos == std::string::npos)
        return "";

    // Move after keyword
    pos += keyword.length();

    // Skip spaces / tabs
    while (pos < line.size() && isspace(line[pos]))
        pos++;

    // Extract identifier until space, '{', ':', or end
    size_t start = pos;

    while (pos < line.size())
    {
        char c = line[pos];

        if (isspace(c) || c == '{' || c == ':' || c == ';')
            break;

        pos++;
    }

    if (pos <= start)
        return "";

    return line.substr(start, pos - start);
}
class SymbolIndex
{
public:
    // prefix -> symbols
    std::unordered_map<std::string, std::vector<Symbol>> buckets;

    void Add(const Symbol& s)
    {
        for (size_t i = 1; i <= s.name.size(); i++)
        {
            std::string prefix = s.name.substr(0, i);
            buckets[prefix].push_back(s);
        }
    }

    std::vector<Symbol> Query(const std::string& prefix)
    {
        return buckets[prefix];
    }
};
Symbol ExtractFunction(const std::string& line)
{
    Symbol s;
    s.type = SymbolType::Function;

    size_t nameEnd = line.find('(');
    if (nameEnd == std::string::npos)
        return s;

    size_t nameStart = line.find_last_of(" \t", nameEnd);
    if (nameStart == std::string::npos)
        nameStart = 0;
    else
        nameStart++;

    s.name = line.substr(nameStart, nameEnd - nameStart);

    return s;
}

bool IsVariable(const std::string& line)
{
    if (line.find('(') != std::string::npos)
        return false;

    if (line.find('{') != std::string::npos)
        return false;

    return line.find(';') != std::string::npos;
}

std::string ExtractVariableName(const std::string& line)
{
    size_t semi = line.find(';');
    size_t eq = line.find('=');

    size_t end = (eq != std::string::npos) ? eq : semi;

    size_t start = line.find_last_of(" \t", end);
    if (start == std::string::npos)
        start = 0;
    else
        start++;

    return line.substr(start, end - start);
}

class LightCppParser
{
public:
    SymbolIndex index;

    void ParseFile(const std::string& path)
    {
        std::ifstream file(path);
        std::string line;

        std::string currentScope;

        while (std::getline(file, line))
        {
            ParseLine(line, currentScope);
        }
    }

private:
    void ParseLine(const std::string& line, std::string& scope)
    {
        std::string trimmed = Trim(line);

        // CLASS
        if (StartsWith(trimmed, "class "))
        {
            std::string name = ExtractName(trimmed, "class");
            scope = name;

            index.Add({name, "global", SymbolType::Class, ""});
            return;
        }

        // STRUCT
        if (StartsWith(trimmed, "struct "))
        {
            std::string name = ExtractName(trimmed, "struct");
            scope = name;

            index.Add({name, "global", SymbolType::Struct, ""});
            return;
        }

        // FUNCTION (very simplified)
        if (trimmed.find('(') != std::string::npos &&
            trimmed.find(')') != std::string::npos &&
            trimmed.find(';') == std::string::npos)
        {
            auto func = ExtractFunction(trimmed);
            if (!func.name.empty())
            {
                func.scope = scope;
                index.Add(func);
            }
        }

        // VARIABLE (basic heuristic)
        if (IsVariable(trimmed))
        {
            Symbol s;
            s.name = ExtractVariableName(trimmed);
            s.type = SymbolType::Variable;
            s.scope = scope;

            index.Add(s);
        }
    }

};

class IntelliSense
{
public:
    SymbolIndex* index;

    std::string Query(const std::string& prefix)
    {
        auto symbols = index->Query(prefix);

        std::string result;

        for (auto& s : symbols)
        {
            if (s.name.rfind(prefix, 0) == 0)
            {
                if (!result.empty())
                    result += " ";

                result += s.name;
            }
        }

        return result;
    }
};

void BuildIndex(const std::vector<std::string>& files)
{
    LightCppParser parser;

    for (auto& f : files)
        parser.ParseFile(f);

   /// engine.index = &parser.index;
}