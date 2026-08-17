#include "symbols/ManualSymbolParser.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace dodev::symbols
{
namespace
{

enum class TokenKind
{
    Identifier,
    Number,
    StringLiteral,
    CharacterLiteral,
    Preprocessor,
    Symbol
};

struct Token
{
    TokenKind kind = TokenKind::Symbol;
    std::string text;
    unsigned line = 1;
    unsigned column = 1;
    std::size_t start = 0;
    std::size_t end = 0;
};

struct Scope
{
    std::size_t open = 0;
    std::size_t close = 0;
    std::string name;
    SymbolKind kind = SymbolKind::Other;
};

struct CallableRange
{
    std::string name;
    std::string qualifiedName;
    SymbolKind kind = SymbolKind::Function;
    std::size_t nameToken = 0;
    std::size_t bodyOpen = 0;
    std::size_t bodyClose = 0;
    SourceLocation location;
};

bool IsIdentifierStart(char c)
{
    const unsigned char u = static_cast<unsigned char>(c);
    return std::isalpha(u) != 0 || c == '_' || c == '$';
}

bool IsIdentifierContinue(char c)
{
    const unsigned char u = static_cast<unsigned char>(c);
    return std::isalnum(u) != 0 || c == '_' || c == '$';
}

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
    {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string ExtensionOf(const std::string& path)
{
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return std::string();
    return Lower(path.substr(dot + 1));
}

std::vector<Token> Tokenize(const std::string& source)
{
    std::vector<Token> out;
    std::size_t i = 0;
    unsigned line = 1;
    unsigned column = 1;
    bool lineOnlyWhitespace = true;

    auto advance = [&](char c)
    {
        if (c == '\n')
        {
            ++line;
            column = 1;
            lineOnlyWhitespace = true;
        }
        else
        {
            ++column;
            if (!std::isspace(static_cast<unsigned char>(c)))
                lineOnlyWhitespace = false;
        }
    };

    auto append = [&](TokenKind kind, std::size_t start, std::size_t end,
                      unsigned tokenLine, unsigned tokenColumn)
    {
        Token token;
        token.kind = kind;
        token.text = source.substr(start, end - start);
        token.line = tokenLine;
        token.column = tokenColumn;
        token.start = start;
        token.end = end;
        out.push_back(std::move(token));
    };

    while (i < source.size())
    {
        const char c = source[i];
        if (std::isspace(static_cast<unsigned char>(c)))
        {
            advance(c);
            ++i;
            continue;
        }

        const unsigned tokenLine = line;
        const unsigned tokenColumn = column;
        const std::size_t start = i;

        if (c == '#' && lineOnlyWhitespace)
        {
            while (i < source.size() && source[i] != '\n')
            {
                advance(source[i]);
                ++i;
            }
            append(TokenKind::Preprocessor, start, i, tokenLine, tokenColumn);
            continue;
        }

        if (c == '/' && i + 1 < source.size() && source[i + 1] == '/')
        {
            while (i < source.size() && source[i] != '\n')
            {
                advance(source[i]);
                ++i;
            }
            continue;
        }

        if (c == '/' && i + 1 < source.size() && source[i + 1] == '*')
        {
            advance(source[i++]);
            advance(source[i++]);
            while (i < source.size())
            {
                if (i + 1 < source.size() && source[i] == '*' && source[i + 1] == '/')
                {
                    advance(source[i++]);
                    advance(source[i++]);
                    break;
                }
                advance(source[i]);
                ++i;
            }
            continue;
        }

        // Kotlin triple-quoted strings.
        if (c == '"' && i + 2 < source.size() && source[i + 1] == '"' && source[i + 2] == '"')
        {
            for (int n = 0; n < 3; ++n)
                advance(source[i++]);
            while (i < source.size())
            {
                if (i + 2 < source.size() && source[i] == '"' && source[i + 1] == '"' && source[i + 2] == '"')
                {
                    for (int n = 0; n < 3; ++n)
                        advance(source[i++]);
                    break;
                }
                advance(source[i]);
                ++i;
            }
            append(TokenKind::StringLiteral, start, i, tokenLine, tokenColumn);
            continue;
        }

        // Common C++ raw strings: R"delimiter(... )delimiter".
        if (c == 'R' && i + 1 < source.size() && source[i + 1] == '"')
        {
            std::size_t openParen = source.find('(', i + 2);
            if (openParen != std::string::npos && openParen - (i + 2) <= 16)
            {
                const std::string delimiter = source.substr(i + 2, openParen - (i + 2));
                const std::string terminator = ")" + delimiter + "\"";
                std::size_t finish = source.find(terminator, openParen + 1);
                finish = finish == std::string::npos ? source.size() : finish + terminator.size();
                while (i < finish)
                {
                    advance(source[i]);
                    ++i;
                }
                append(TokenKind::StringLiteral, start, i, tokenLine, tokenColumn);
                continue;
            }
        }

        if (c == '"' || c == '\'')
        {
            const char quote = c;
            advance(source[i++]);
            bool escaped = false;
            while (i < source.size())
            {
                const char value = source[i];
                advance(value);
                ++i;
                if (escaped)
                {
                    escaped = false;
                    continue;
                }
                if (value == '\\')
                {
                    escaped = true;
                    continue;
                }
                if (value == quote)
                    break;
            }
            append(quote == '"' ? TokenKind::StringLiteral : TokenKind::CharacterLiteral,
                   start, i, tokenLine, tokenColumn);
            continue;
        }

        if (IsIdentifierStart(c))
        {
            advance(source[i++]);
            while (i < source.size() && IsIdentifierContinue(source[i]))
                advance(source[i++]);
            append(TokenKind::Identifier, start, i, tokenLine, tokenColumn);
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)))
        {
            advance(source[i++]);
            while (i < source.size())
            {
                const char n = source[i];
                if (!std::isalnum(static_cast<unsigned char>(n)) && n != '.' && n != '_' && n != '\'')
                    break;
                advance(source[i++]);
            }
            append(TokenKind::Number, start, i, tokenLine, tokenColumn);
            continue;
        }

        static const std::set<std::string> twoChar = {
            "::", "->", "=>", "==", "!=", "<=", ">=", "++", "--", "&&", "||",
            "+=", "-=", "*=", "/=", "%=", "<<", ">>", "?.", "?:"
        };
        if (i + 1 < source.size())
        {
            const std::string pair = source.substr(i, 2);
            if (twoChar.find(pair) != twoChar.end())
            {
                advance(source[i++]);
                advance(source[i++]);
                append(TokenKind::Symbol, start, i, tokenLine, tokenColumn);
                continue;
            }
        }

        advance(source[i++]);
        append(TokenKind::Symbol, start, i, tokenLine, tokenColumn);
    }

    return out;
}

std::vector<std::size_t> BuildMatching(const std::vector<Token>& tokens,
                                       const std::string& open,
                                       const std::string& close)
{
    std::vector<std::size_t> match(tokens.size(), tokens.size());
    std::vector<std::size_t> stack;
    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].text == open)
            stack.push_back(i);
        else if (tokens[i].text == close && !stack.empty())
        {
            const std::size_t begin = stack.back();
            stack.pop_back();
            match[begin] = i;
            match[i] = begin;
        }
    }
    return match;
}

SourceLocation LocationOf(const std::string& file, const Token& token)
{
    SourceLocation location;
    location.file = file;
    location.line = token.line;
    location.column = token.column;
    location.offset = token.start;
    return location;
}

std::string JoinTokens(const std::vector<Token>& tokens, std::size_t begin, std::size_t end)
{
    std::string result;
    for (std::size_t i = begin; i < end && i < tokens.size(); ++i)
    {
        if (!result.empty())
        {
            const std::string& previous = tokens[i - 1].text;
            const std::string& current = tokens[i].text;
            const bool glue = current == "," || current == ")" || current == "]" || current == ";" ||
                              current == "::" || current == "." || current == "->" || current == "?." ||
                              previous == "(" || previous == "[" || previous == "::" || previous == "." ||
                              previous == "->" || previous == "?.";
            if (!glue)
                result += ' ';
        }
        result += tokens[i].text;
    }
    return result;
}

bool IsCallableKind(SymbolKind kind)
{
    return kind == SymbolKind::Function || kind == SymbolKind::Method ||
           kind == SymbolKind::Constructor || kind == SymbolKind::Destructor;
}

bool IsTypeKind(SymbolKind kind)
{
    return kind == SymbolKind::Class || kind == SymbolKind::Struct || kind == SymbolKind::Union ||
           kind == SymbolKind::Interface || kind == SymbolKind::Object || kind == SymbolKind::Enum ||
           kind == SymbolKind::Namespace || kind == SymbolKind::Package || kind == SymbolKind::TypeAlias;
}

bool ShouldEmit(SymbolKind kind, const RuntimeSettings& settings)
{
    if (!settings.symbolsEnabled)
        return false;
    if (IsCallableKind(kind))
        return settings.functionsEnabled;
    if (IsTypeKind(kind))
        return settings.typesEnabled;
    if (kind == SymbolKind::Variable || kind == SymbolKind::Field || kind == SymbolKind::Property)
        return settings.variablesEnabled;
    if (kind == SymbolKind::Macro)
        return settings.macrosEnabled;
    return true;
}

std::string ScopePrefixAt(const std::vector<Scope>& scopes, std::size_t tokenIndex)
{
    std::vector<const Scope*> containing;
    for (const Scope& scope : scopes)
    {
        if (scope.open < tokenIndex && tokenIndex < scope.close && !scope.name.empty())
            containing.push_back(&scope);
    }
    std::sort(containing.begin(), containing.end(), [](const Scope* a, const Scope* b)
    {
        return a->open < b->open;
    });
    std::string prefix;
    for (const Scope* scope : containing)
    {
        if (!prefix.empty())
            prefix += "::";
        prefix += scope->name;
    }
    return prefix;
}

const Scope* InnermostTypeScopeAt(const std::vector<Scope>& scopes, std::size_t tokenIndex)
{
    const Scope* best = nullptr;
    for (const Scope& scope : scopes)
    {
        if (scope.open < tokenIndex && tokenIndex < scope.close &&
            (scope.kind == SymbolKind::Class || scope.kind == SymbolKind::Struct ||
             scope.kind == SymbolKind::Union || scope.kind == SymbolKind::Interface ||
             scope.kind == SymbolKind::Object))
        {
            if (!best || scope.open > best->open)
                best = &scope;
        }
    }
    return best;
}

bool InsideCallable(const std::vector<CallableRange>& callables, std::size_t tokenIndex)
{
    for (const CallableRange& callable : callables)
    {
        if (callable.bodyOpen < tokenIndex && tokenIndex < callable.bodyClose)
            return true;
    }
    return false;
}

std::size_t PreviousBoundary(const std::vector<Token>& tokens, std::size_t index)
{
    while (index > 0)
    {
        --index;
        const std::string& t = tokens[index].text;
        if (t == ";" || t == "{" || t == "}")
            return index + 1;
    }
    return 0;
}

std::size_t NextAfterFunctionSuffix(const std::vector<Token>& tokens, std::size_t index)
{
    // Skip common C++ post-parameter qualifiers and Kotlin return types.
    int angleDepth = 0;
    while (index < tokens.size())
    {
        const std::string& t = tokens[index].text;
        if (t == "<") ++angleDepth;
        if (t == ">" && angleDepth > 0) --angleDepth;
        if (angleDepth == 0 && (t == "{" || t == ";" || t == "=" || t == ":"))
            return index;
        ++index;
    }
    return tokens.size();
}

bool IsControlName(const std::string& value)
{
    static const std::set<std::string> blocked = {
        "if", "for", "while", "switch", "catch", "sizeof", "alignof", "decltype",
        "return", "new", "delete", "throw", "static_cast", "dynamic_cast",
        "reinterpret_cast", "const_cast", "typeid", "assert", "when", "try",
        "synchronized"
    };
    return blocked.find(value) != blocked.end();
}

void AddSymbol(std::vector<Symbol>& symbols,
               const RuntimeSettings& settings,
               Symbol symbol)
{
    if (ShouldEmit(symbol.kind, settings))
        symbols.push_back(std::move(symbol));
}

void CollectPreprocessorSymbols(const std::string& file,
                                const std::vector<Token>& tokens,
                                const RuntimeSettings& settings,
                                std::vector<Symbol>& symbols)
{
    if (!settings.symbolsEnabled || !settings.macrosEnabled)
        return;
    for (const Token& token : tokens)
    {
        if (token.kind != TokenKind::Preprocessor)
            continue;
        std::string text = token.text;
        std::size_t pos = text.find("#define");
        if (pos == std::string::npos)
            continue;
        pos += 7;
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
        const std::size_t begin = pos;
        while (pos < text.size() && IsIdentifierContinue(text[pos])) ++pos;
        if (pos == begin)
            continue;
        Symbol symbol;
        symbol.name = text.substr(begin, pos - begin);
        symbol.qualifiedName = symbol.name;
        symbol.signature = token.text;
        symbol.kind = SymbolKind::Macro;
        symbol.location = LocationOf(file, token);
        symbol.definition = true;
        symbols.push_back(std::move(symbol));
    }
}

void CollectCppScopes(const std::string& file,
                      const std::vector<Token>& tokens,
                      const std::vector<std::size_t>& braceMatch,
                      const RuntimeSettings& settings,
                      std::vector<Scope>& scopes,
                      std::vector<Symbol>& symbols)
{
    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        SymbolKind kind = SymbolKind::Other;
        std::size_t nameIndex = i + 1;
        const std::string& t = tokens[i].text;
        if (t == "namespace") kind = SymbolKind::Namespace;
        else if (t == "class") kind = SymbolKind::Class;
        else if (t == "struct") kind = SymbolKind::Struct;
        else if (t == "union") kind = SymbolKind::Union;
        else if (t == "enum")
        {
            kind = SymbolKind::Enum;
            if (nameIndex < tokens.size() && (tokens[nameIndex].text == "class" || tokens[nameIndex].text == "struct"))
                ++nameIndex;
        }
        else continue;

        std::size_t open = tokens.size();
        if (nameIndex < tokens.size() && tokens[nameIndex].kind == TokenKind::Identifier)
        {
            open = nameIndex + 1;
            while (open < tokens.size() && tokens[open].text != "{" && tokens[open].text != ";")
                ++open;
        }
        else if (nameIndex < tokens.size() && tokens[nameIndex].text == "{" &&
                 i > 0 && tokens[i - 1].text == "typedef")
        {
            open = nameIndex;
            if (braceMatch[open] < tokens.size())
            {
                const std::size_t close = braceMatch[open];
                std::size_t alias = close + 1;
                while (alias < tokens.size() && tokens[alias].text != ";" &&
                       tokens[alias].kind != TokenKind::Identifier)
                    ++alias;
                if (alias < tokens.size() && tokens[alias].kind == TokenKind::Identifier)
                    nameIndex = alias;
            }
        }
        if (nameIndex >= tokens.size() || tokens[nameIndex].kind != TokenKind::Identifier ||
            open >= tokens.size() || tokens[open].text != "{" || braceMatch[open] >= tokens.size())
            continue;

        const std::string prefix = ScopePrefixAt(scopes, i);
        const std::string qualified = prefix.empty() ? tokens[nameIndex].text : prefix + "::" + tokens[nameIndex].text;
        scopes.push_back(Scope{open, braceMatch[open], tokens[nameIndex].text, kind});

        Symbol symbol;
        symbol.name = tokens[nameIndex].text;
        symbol.qualifiedName = qualified;
        symbol.signature = JoinTokens(tokens, i, open);
        symbol.kind = kind;
        symbol.location = LocationOf(file, tokens[nameIndex]);
        symbol.end = LocationOf(file, tokens[braceMatch[open]]);
        symbol.definition = true;
        AddSymbol(symbols, settings, std::move(symbol));
    }
}

std::string QualifiedFunctionName(const std::vector<Token>& tokens,
                                  std::size_t nameIndex,
                                  const std::vector<Scope>& scopes)
{
    std::string explicitQualifier;
    std::size_t cursor = nameIndex;
    std::vector<std::string> parts;
    parts.push_back(tokens[nameIndex].text);
    while (cursor >= 2 && tokens[cursor - 1].text == "::" && tokens[cursor - 2].kind == TokenKind::Identifier)
    {
        parts.push_back(tokens[cursor - 2].text);
        cursor -= 2;
    }
    if (parts.size() > 1)
    {
        std::reverse(parts.begin(), parts.end());
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            if (i) explicitQualifier += "::";
            explicitQualifier += parts[i];
        }
        return explicitQualifier;
    }

    const std::string prefix = ScopePrefixAt(scopes, nameIndex);
    return prefix.empty() ? tokens[nameIndex].text : prefix + "::" + tokens[nameIndex].text;
}

void CollectCppCallables(const std::string& file,
                         Language language,
                         const std::vector<Token>& tokens,
                         const std::vector<std::size_t>& parenMatch,
                         const std::vector<std::size_t>& braceMatch,
                         const std::vector<Scope>& scopes,
                         const RuntimeSettings& settings,
                         std::vector<CallableRange>& callables,
                         std::vector<Symbol>& symbols)
{
    for (std::size_t openParen = 0; openParen < tokens.size(); ++openParen)
    {
        if (tokens[openParen].text != "(" || openParen == 0 || parenMatch[openParen] >= tokens.size())
            continue;

        std::size_t nameIndex = openParen - 1;
        if (tokens[nameIndex].kind != TokenKind::Identifier)
            continue;
        if (IsControlName(tokens[nameIndex].text))
            continue;
        if (nameIndex > 0 && (tokens[nameIndex - 1].text == "." || tokens[nameIndex - 1].text == "->" || tokens[nameIndex - 1].text == "?."))
            continue;

        const std::size_t closeParen = parenMatch[openParen];
        const std::size_t suffix = NextAfterFunctionSuffix(tokens, closeParen + 1);
        if (suffix >= tokens.size())
            continue;

        const bool definition = tokens[suffix].text == "{" && braceMatch[suffix] < tokens.size();
        const bool declaration = tokens[suffix].text == ";" || tokens[suffix].text == "=";
        if (!definition && !declaration)
            continue;

        // Calls inside another function are not declarations.
        if (InsideCallable(callables, nameIndex))
            continue;

        const Scope* typeScope = InnermostTypeScopeAt(scopes, nameIndex);
        if (language == Language::C && typeScope && typeScope->kind != SymbolKind::Struct && typeScope->kind != SymbolKind::Union)
            continue;

        // Top-level call statements such as foo(); have no plausible return/declaration prefix.
        const std::size_t headerStart = PreviousBoundary(tokens, nameIndex);
        bool hasDeclarationPrefix = false;
        for (std::size_t j = headerStart; j < nameIndex; ++j)
        {
            if (tokens[j].kind == TokenKind::Identifier || tokens[j].text == "*" || tokens[j].text == "&" || tokens[j].text == "::" || tokens[j].text == "~")
            {
                hasDeclarationPrefix = true;
                break;
            }
        }
        const bool qualifiedName = nameIndex >= 2 && tokens[nameIndex - 1].text == "::";
        const bool constructorLike = typeScope && tokens[nameIndex].text == typeScope->name;
        if (!hasDeclarationPrefix && !qualifiedName && !constructorLike)
            continue;

        SymbolKind kind = typeScope ? SymbolKind::Method : SymbolKind::Function;
        if (constructorLike)
            kind = SymbolKind::Constructor;
        if (nameIndex > 0 && tokens[nameIndex - 1].text == "~")
            kind = SymbolKind::Destructor;

        const std::string qualified = QualifiedFunctionName(tokens, nameIndex, scopes);
        CallableRange callable;
        callable.name = tokens[nameIndex].text;
        callable.qualifiedName = qualified;
        callable.kind = kind;
        callable.nameToken = nameIndex;
        callable.location = LocationOf(file, tokens[nameIndex]);
        if (definition)
        {
            callable.bodyOpen = suffix;
            callable.bodyClose = braceMatch[suffix];
            callables.push_back(callable);
        }

        Symbol symbol;
        symbol.name = callable.name;
        symbol.qualifiedName = qualified;
        symbol.signature = JoinTokens(tokens, headerStart, closeParen + 1);
        symbol.kind = kind;
        symbol.location = callable.location;
        symbol.definition = definition;
        if (definition)
            symbol.end = LocationOf(file, tokens[braceMatch[suffix]]);
        AddSymbol(symbols, settings, std::move(symbol));
    }
}

void CollectCppAliases(const std::string& file,
                       const std::vector<Token>& tokens,
                       const std::vector<std::size_t>& braceMatch,
                       const RuntimeSettings& settings,
                       std::vector<Symbol>& symbols)
{
    if (!settings.symbolsEnabled || !settings.typesEnabled)
        return;
    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].text == "using" && i + 2 < tokens.size() &&
            tokens[i + 1].kind == TokenKind::Identifier && tokens[i + 2].text == "=")
        {
            Symbol symbol;
            symbol.name = tokens[i + 1].text;
            symbol.qualifiedName = symbol.name;
            symbol.signature = "using " + symbol.name;
            symbol.kind = SymbolKind::TypeAlias;
            symbol.location = LocationOf(file, tokens[i + 1]);
            symbol.definition = true;
            symbols.push_back(std::move(symbol));
        }
        if (tokens[i].text == "typedef")
        {
            std::size_t semicolon = i + 1;
            std::size_t lastIdentifier = tokens.size();
            while (semicolon < tokens.size() && tokens[semicolon].text != ";")
            {
                if (tokens[semicolon].text == "{" && braceMatch[semicolon] < tokens.size())
                {
                    semicolon = braceMatch[semicolon] + 1;
                    continue;
                }
                if (tokens[semicolon].kind == TokenKind::Identifier)
                    lastIdentifier = semicolon;
                ++semicolon;
            }
            if (lastIdentifier < tokens.size())
            {
                Symbol symbol;
                symbol.name = tokens[lastIdentifier].text;
                symbol.qualifiedName = symbol.name;
                symbol.signature = JoinTokens(tokens, i, semicolon);
                symbol.kind = SymbolKind::TypeAlias;
                symbol.location = LocationOf(file, tokens[lastIdentifier]);
                symbol.definition = true;
                symbols.push_back(std::move(symbol));
            }
        }
    }
}

void CollectCppVariables(const std::string& file,
                         const std::vector<Token>& tokens,
                         const std::vector<Scope>& scopes,
                         const std::vector<CallableRange>& callables,
                         const RuntimeSettings& settings,
                         std::vector<Symbol>& symbols)
{
    if (!settings.symbolsEnabled || !settings.variablesEnabled)
        return;

    for (std::size_t i = 1; i + 1 < tokens.size(); ++i)
    {
        if (tokens[i].kind != TokenKind::Identifier)
            continue;
        if (InsideCallable(callables, i))
            continue;
        if (tokens[i + 1].text != ";" && tokens[i + 1].text != "=" && tokens[i + 1].text != "[")
            continue;
        const std::string& previous = tokens[i - 1].text;
        if (previous == "class" || previous == "struct" || previous == "enum" || previous == "namespace" ||
            previous == "typedef" || previous == "using" || previous == "." || previous == "->" || previous == "::")
            continue;
        if (tokens[i - 1].kind != TokenKind::Identifier && previous != "*" && previous != "&" && previous != ">")
            continue;

        const Scope* typeScope = InnermostTypeScopeAt(scopes, i);
        Symbol symbol;
        symbol.name = tokens[i].text;
        const std::string prefix = ScopePrefixAt(scopes, i);
        symbol.qualifiedName = prefix.empty() ? symbol.name : prefix + "::" + symbol.name;
        symbol.signature = symbol.name;
        symbol.kind = typeScope ? SymbolKind::Field : SymbolKind::Variable;
        symbol.location = LocationOf(file, tokens[i]);
        symbol.definition = true;
        symbols.push_back(std::move(symbol));
    }
}

std::string ClassPrefix(const std::string& qualified)
{
    const std::size_t pos = qualified.rfind("::");
    if (pos == std::string::npos)
        return std::string();
    return qualified.substr(0, pos);
}

void ResolveCalls(std::vector<CallReference>& calls,
                  const std::vector<Symbol>& emittedSymbols,
                  const std::vector<CallableRange>& callables)
{
    struct Candidate
    {
        std::string name;
        std::string qualifiedName;
        SourceLocation location;
    };
    std::unordered_map<std::string, std::vector<Candidate>> byName;
    for (const CallableRange& callable : callables)
        byName[callable.name].push_back(Candidate{callable.name, callable.qualifiedName, callable.location});
    for (const Symbol& symbol : emittedSymbols)
    {
        if (!IsCallableKind(symbol.kind))
            continue;
        auto& values = byName[symbol.name];
        const bool exists = std::any_of(values.begin(), values.end(), [&](const Candidate& c)
        {
            return c.qualifiedName == symbol.qualifiedName && c.location.line == symbol.location.line;
        });
        if (!exists)
            values.push_back(Candidate{symbol.name, symbol.qualifiedName, symbol.location});
    }

    for (CallReference& call : calls)
    {
        auto found = byName.find(call.callee);
        if (found == byName.end() || found->second.empty())
        {
            call.resolution = CallResolution::Unresolved;
            continue;
        }
        const std::string callerClass = ClassPrefix(call.caller);
        const Candidate* selected = nullptr;
        for (const Candidate& candidate : found->second)
        {
            if (!callerClass.empty() && ClassPrefix(candidate.qualifiedName) == callerClass)
            {
                selected = &candidate;
                call.resolution = CallResolution::ClassResolved;
                break;
            }
        }
        if (!selected && found->second.size() == 1)
        {
            selected = &found->second.front();
            call.resolution = CallResolution::FullyResolved;
        }
        if (!selected)
        {
            call.resolution = CallResolution::NameOnly;
            continue;
        }
        call.target = selected->location;
        call.displayCallee = selected->qualifiedName;
    }
}

void CollectCallsForRanges(const std::string& file,
                           const std::vector<Token>& tokens,
                           const std::vector<std::size_t>& parenMatch,
                           const std::vector<CallableRange>& callables,
                           std::vector<CallReference>& calls)
{
    for (const CallableRange& callable : callables)
    {
        for (std::size_t i = callable.bodyOpen + 1; i < callable.bodyClose && i + 1 < tokens.size(); ++i)
        {
            if (tokens[i].kind != TokenKind::Identifier || tokens[i + 1].text != "(")
                continue;
            if (IsControlName(tokens[i].text) || i == callable.nameToken)
                continue;
            if (parenMatch[i + 1] >= tokens.size())
                continue;
            if (i > 0 && (tokens[i - 1].text == "fun" || tokens[i - 1].text == "class" || tokens[i - 1].text == "struct"))
                continue;

            CallReference call;
            call.caller = callable.qualifiedName;
            call.callee = tokens[i].text;
            call.displayCallee = call.callee;
            call.callSite = LocationOf(file, tokens[i]);
            call.indirect = false;

            if (i > 0 && (tokens[i - 1].text == "." || tokens[i - 1].text == "->" || tokens[i - 1].text == "?."))
            {
                std::size_t qualifier = i - 1;
                if (qualifier > 0 && tokens[qualifier - 1].kind == TokenKind::Identifier)
                    call.displayCallee = tokens[qualifier - 1].text + tokens[i - 1].text + call.callee;
            }
            calls.push_back(std::move(call));
        }
    }
}

void ParseCppOrC(const std::string& file,
                 Language language,
                 const std::vector<Token>& tokens,
                 const RuntimeSettings& settings,
                 AnalysisResult& result)
{
    const std::vector<std::size_t> parenMatch = BuildMatching(tokens, "(", ")");
    const std::vector<std::size_t> braceMatch = BuildMatching(tokens, "{", "}");
    std::vector<Scope> scopes;
    std::vector<CallableRange> callables;

    if (language == Language::Cpp)
        CollectPreprocessorSymbols(file, tokens, settings, result.symbols);
    else
        CollectPreprocessorSymbols(file, tokens, settings, result.symbols);

    CollectCppScopes(file, tokens, braceMatch, settings, scopes, result.symbols);
    // Re-sort scopes so ScopePrefixAt is deterministic even when nested scopes
    // were discovered before their parent had been appended.
    std::sort(scopes.begin(), scopes.end(), [](const Scope& a, const Scope& b)
    {
        return a.open < b.open;
    });
    CollectCppCallables(file, language, tokens, parenMatch, braceMatch, scopes,
                        settings, callables, result.symbols);
    CollectCppAliases(file, tokens, braceMatch, settings, result.symbols);
    CollectCppVariables(file, tokens, scopes, callables, settings, result.symbols);

    if (settings.callsEnabled)
    {
        CollectCallsForRanges(file, tokens, parenMatch, callables, result.calls);
        ResolveCalls(result.calls, result.symbols, callables);
    }
}

std::size_t FindNextToken(const std::vector<Token>& tokens,
                          std::size_t from,
                          const std::string& text)
{
    for (std::size_t i = from; i < tokens.size(); ++i)
    {
        if (tokens[i].text == text)
            return i;
    }
    return tokens.size();
}

bool IsInsideKotlinPrimaryConstructor(const std::vector<Token>& tokens,
                                      const std::vector<std::size_t>& parenMatch,
                                      std::size_t tokenIndex)
{
    for (std::size_t open = 0; open < tokenIndex; ++open)
    {
        if (tokens[open].text != "(" || parenMatch[open] >= tokens.size() ||
            parenMatch[open] <= tokenIndex)
            continue;
        if (open == 0 || tokens[open - 1].kind != TokenKind::Identifier)
            continue;
        std::size_t cursor = open - 1;
        while (cursor > 0 && cursor + 6 >= open)
        {
            --cursor;
            if (tokens[cursor].text == "class")
                return true;
            if (tokens[cursor].text == "fun" || tokens[cursor].text == "{" || tokens[cursor].text == "}")
                break;
        }
    }
    return false;
}

void ParseKotlin(const std::string& file,
                 const std::vector<Token>& tokens,
                 const RuntimeSettings& settings,
                 AnalysisResult& result)
{
    const std::vector<std::size_t> parenMatch = BuildMatching(tokens, "(", ")");
    const std::vector<std::size_t> braceMatch = BuildMatching(tokens, "{", "}");
    std::vector<Scope> scopes;
    std::vector<CallableRange> callables;

    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].text == "package" && i + 1 < tokens.size())
        {
            std::size_t end = i + 1;
            while (end + 1 < tokens.size() && tokens[end + 1].line == tokens[i].line)
                ++end;
            std::string packageName;
            for (std::size_t j = i + 1; j <= end; ++j)
            {
                if (tokens[j].kind == TokenKind::Identifier || tokens[j].text == ".")
                    packageName += tokens[j].text;
                else
                    break;
            }
            if (!packageName.empty())
            {
                Symbol symbol;
                symbol.name = packageName;
                symbol.qualifiedName = packageName;
                symbol.signature = "package " + packageName;
                symbol.kind = SymbolKind::Package;
                symbol.location = LocationOf(file, tokens[i + 1]);
                symbol.definition = true;
                AddSymbol(result.symbols, settings, std::move(symbol));
            }
        }

        SymbolKind kind = SymbolKind::Other;
        std::size_t nameIndex = i + 1;
        if (tokens[i].text == "class") kind = SymbolKind::Class;
        else if (tokens[i].text == "interface") kind = SymbolKind::Interface;
        else if (tokens[i].text == "object") kind = SymbolKind::Object;
        else if (tokens[i].text == "enum" && i + 1 < tokens.size() && tokens[i + 1].text == "class")
        {
            kind = SymbolKind::Enum;
            nameIndex = i + 2;
        }
        else continue;

        if (nameIndex >= tokens.size() || tokens[nameIndex].kind != TokenKind::Identifier)
            continue;
        std::size_t open = nameIndex + 1;
        int parenDepth = 0;
        int angleDepth = 0;
        while (open < tokens.size())
        {
            const std::string& value = tokens[open].text;
            if (value == "(") ++parenDepth;
            else if (value == ")" && parenDepth > 0) --parenDepth;
            else if (value == "<") ++angleDepth;
            else if (value == ">" && angleDepth > 0) --angleDepth;
            if (parenDepth == 0 && angleDepth == 0)
            {
                if (value == "{" || value == "=" || value == ";")
                    break;
                if (open > nameIndex + 1 && tokens[open].line > tokens[nameIndex].line &&
                    (value == "class" || value == "interface" || value == "object" ||
                     value == "fun" || value == "typealias" || value == "package"))
                    break;
            }
            ++open;
        }
        const std::string prefix = ScopePrefixAt(scopes, i);
        const std::string qualified = prefix.empty() ? tokens[nameIndex].text : prefix + "::" + tokens[nameIndex].text;
        const bool hasBody = open < tokens.size() && tokens[open].text == "{" && braceMatch[open] < tokens.size();
        if (hasBody)
            scopes.push_back(Scope{open, braceMatch[open], tokens[nameIndex].text, kind});

        Symbol symbol;
        symbol.name = tokens[nameIndex].text;
        symbol.qualifiedName = qualified;
        symbol.signature = JoinTokens(tokens, i, open < tokens.size() ? open : nameIndex + 1);
        symbol.kind = kind;
        symbol.location = LocationOf(file, tokens[nameIndex]);
        if (hasBody)
            symbol.end = LocationOf(file, tokens[braceMatch[open]]);
        symbol.definition = true;
        AddSymbol(result.symbols, settings, std::move(symbol));

        // Kotlin primary-constructor val/var parameters are properties even
        // when the class has no explicit body (common for data classes).
        std::size_t ctorOpen = nameIndex + 1;
        while (ctorOpen < tokens.size() && ctorOpen < open && tokens[ctorOpen].text != "(")
            ++ctorOpen;
        if (settings.symbolsEnabled && settings.variablesEnabled &&
            ctorOpen < tokens.size() && ctorOpen < open && tokens[ctorOpen].text == "(" &&
            parenMatch[ctorOpen] < tokens.size())
        {
            const std::size_t ctorClose = parenMatch[ctorOpen];
            for (std::size_t j = ctorOpen + 1; j + 1 < ctorClose; ++j)
            {
                if ((tokens[j].text == "val" || tokens[j].text == "var") &&
                    tokens[j + 1].kind == TokenKind::Identifier)
                {
                    Symbol property;
                    property.name = tokens[j + 1].text;
                    property.qualifiedName = qualified + "::" + property.name;
                    property.signature = tokens[j].text + " " + property.name;
                    property.kind = SymbolKind::Property;
                    property.location = LocationOf(file, tokens[j + 1]);
                    property.definition = true;
                    result.symbols.push_back(std::move(property));
                }
            }
        }
    }

    std::sort(scopes.begin(), scopes.end(), [](const Scope& a, const Scope& b)
    {
        return a.open < b.open;
    });

    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].text == "typealias" && i + 1 < tokens.size() && tokens[i + 1].kind == TokenKind::Identifier)
        {
            Symbol symbol;
            symbol.name = tokens[i + 1].text;
            const std::string prefix = ScopePrefixAt(scopes, i);
            symbol.qualifiedName = prefix.empty() ? symbol.name : prefix + "::" + symbol.name;
            symbol.signature = "typealias " + symbol.name;
            symbol.kind = SymbolKind::TypeAlias;
            symbol.location = LocationOf(file, tokens[i + 1]);
            symbol.definition = true;
            AddSymbol(result.symbols, settings, std::move(symbol));
            continue;
        }

        if ((tokens[i].text == "val" || tokens[i].text == "var") &&
            i + 1 < tokens.size() && tokens[i + 1].kind == TokenKind::Identifier)
        {
            if (!InsideCallable(callables, i) && !IsInsideKotlinPrimaryConstructor(tokens, parenMatch, i))
            {
                Symbol symbol;
                symbol.name = tokens[i + 1].text;
                const std::string prefix = ScopePrefixAt(scopes, i);
                symbol.qualifiedName = prefix.empty() ? symbol.name : prefix + "::" + symbol.name;
                symbol.signature = tokens[i].text + " " + symbol.name;
                symbol.kind = InnermostTypeScopeAt(scopes, i) ? SymbolKind::Property : SymbolKind::Variable;
                symbol.location = LocationOf(file, tokens[i + 1]);
                symbol.definition = true;
                AddSymbol(result.symbols, settings, std::move(symbol));
            }
            continue;
        }

        if (tokens[i].text != "fun")
            continue;

        std::size_t openParen = FindNextToken(tokens, i + 1, "(");
        if (openParen >= tokens.size() || openParen == i + 1 || parenMatch[openParen] >= tokens.size())
            continue;
        std::size_t nameIndex = openParen - 1;
        if (tokens[nameIndex].kind != TokenKind::Identifier)
            continue;

        std::string receiver;
        if (nameIndex >= 2 && tokens[nameIndex - 1].text == ".")
        {
            std::size_t begin = i + 1;
            receiver = JoinTokens(tokens, begin, nameIndex - 1);
        }

        const Scope* typeScope = InnermostTypeScopeAt(scopes, i);
        const std::string prefix = ScopePrefixAt(scopes, i);
        std::string qualified = prefix.empty() ? tokens[nameIndex].text : prefix + "::" + tokens[nameIndex].text;
        if (!receiver.empty())
            qualified = receiver + "." + tokens[nameIndex].text;

        const std::size_t closeParen = parenMatch[openParen];
        std::size_t bodyOpen = closeParen + 1;
        while (bodyOpen < tokens.size() && tokens[bodyOpen].text != "{" && tokens[bodyOpen].text != "=" && tokens[bodyOpen].text != ";")
            ++bodyOpen;

        CallableRange callable;
        callable.name = tokens[nameIndex].text;
        callable.qualifiedName = qualified;
        callable.kind = typeScope ? SymbolKind::Method : SymbolKind::Function;
        callable.nameToken = nameIndex;
        callable.location = LocationOf(file, tokens[nameIndex]);
        bool definition = false;
        if (bodyOpen < tokens.size() && tokens[bodyOpen].text == "{" && braceMatch[bodyOpen] < tokens.size())
        {
            callable.bodyOpen = bodyOpen;
            callable.bodyClose = braceMatch[bodyOpen];
            callables.push_back(callable);
            definition = true;
        }

        Symbol symbol;
        symbol.name = callable.name;
        symbol.qualifiedName = qualified;
        symbol.signature = JoinTokens(tokens, i, closeParen + 1);
        symbol.kind = callable.kind;
        symbol.location = callable.location;
        symbol.definition = definition;
        if (definition)
            symbol.end = LocationOf(file, tokens[braceMatch[bodyOpen]]);
        AddSymbol(result.symbols, settings, std::move(symbol));
    }

    if (settings.callsEnabled)
    {
        CollectCallsForRanges(file, tokens, parenMatch, callables, result.calls);
        ResolveCalls(result.calls, result.symbols, callables);
    }
}

} // namespace

ParserRegistry::ParserRegistry()
{
    RegisterExtensions(Language::C, {"c"});
    RegisterExtensions(Language::Cpp, {"cpp", "cc", "cxx", "c++", "hpp", "hh", "hxx", "h++",
                                      "ipp", "inl", "tpp", "h"});
    RegisterExtensions(Language::Kotlin, {"kt", "kts"});
}

void ParserRegistry::RegisterExtension(const std::string& extension, Language language)
{
    std::string normalized = Lower(extension);
    if (!normalized.empty() && normalized[0] == '.')
        normalized.erase(normalized.begin());
    if (!normalized.empty())
        m_extensions[normalized] = language;
}

void ParserRegistry::RegisterExtensions(Language language, const std::vector<std::string>& extensions)
{
    for (const std::string& extension : extensions)
        RegisterExtension(extension, language);
}

Language ParserRegistry::LanguageForPath(const std::string& path) const
{
    const std::string extension = ExtensionOf(path);
    const auto found = m_extensions.find(extension);
    return found == m_extensions.end() ? Language::Unknown : found->second;
}

bool ParserRegistry::SupportsFile(const std::string& path) const
{
    return LanguageForPath(path) != Language::Unknown;
}

Language ParserRegistry::DetectLanguage(const std::string& path)
{
    const std::string ext = ExtensionOf(path);
    if (ext == "c")
        return Language::C;
    if (ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "c++" ||
        ext == "hpp" || ext == "hh" || ext == "hxx" || ext == "h++" ||
        ext == "ipp" || ext == "inl" || ext == "tpp")
        return Language::Cpp;
    // Headers default to C++ because the editor cannot infer the including
    // translation unit cheaply; users can disable the C++ parser at runtime.
    if (ext == "h")
        return Language::Cpp;
    if (ext == "kt" || ext == "kts")
        return Language::Kotlin;
    return Language::Unknown;
}

bool ParserRegistry::IsSupportedFile(const std::string& path)
{
    return DetectLanguage(path) != Language::Unknown;
}

bool ParserRegistry::IsLanguageEnabled(Language language, const RuntimeSettings& settings)
{
    if (!settings.enabled)
        return false;
    switch (language)
    {
    case Language::C: return settings.cEnabled;
    case Language::Cpp: return settings.cppEnabled;
    case Language::Kotlin: return settings.kotlinEnabled;
    default: return false;
    }
}

AnalysisResult ParserRegistry::Parse(const std::string& path,
                                     const std::string& source,
                                     const RuntimeSettings& settings) const
{
    AnalysisResult result;
    result.language = LanguageForPath(path);
    result.languageName = LanguageName(result.language);
    if (!IsLanguageEnabled(result.language, settings))
    {
        result.status = result.language == Language::Unknown
            ? "No manual parser registered for this extension"
            : result.languageName + " parser disabled in Settings";
        return result;
    }

    const std::vector<Token> tokens = Tokenize(source);
    if (result.language == Language::Kotlin)
        ParseKotlin(path, tokens, settings, result);
    else
        ParseCppOrC(path, result.language, tokens, settings, result);

    result.success = true;
    std::ostringstream status;
    status << "Manual " << result.languageName << " parser: " << result.symbols.size()
           << " symbol(s), " << result.calls.size() << " call(s)";
    result.status = status.str();
    return result;
}

const char* SymbolKindName(SymbolKind kind)
{
    switch (kind)
    {
    case SymbolKind::Function: return "function";
    case SymbolKind::Method: return "method";
    case SymbolKind::Constructor: return "constructor";
    case SymbolKind::Destructor: return "destructor";
    case SymbolKind::Class: return "class";
    case SymbolKind::Struct: return "struct";
    case SymbolKind::Union: return "union";
    case SymbolKind::Interface: return "interface";
    case SymbolKind::Object: return "object";
    case SymbolKind::Enum: return "enum";
    case SymbolKind::Namespace: return "namespace";
    case SymbolKind::Package: return "package";
    case SymbolKind::Variable: return "variable";
    case SymbolKind::Field: return "field";
    case SymbolKind::Property: return "property";
    case SymbolKind::TypeAlias: return "type alias";
    case SymbolKind::Macro: return "macro";
    default: return "symbol";
    }
}

const char* LanguageName(Language language)
{
    switch (language)
    {
    case Language::C: return "C";
    case Language::Cpp: return "C++";
    case Language::Kotlin: return "Kotlin";
    default: return "Unknown";
    }
}

} // namespace dodev::symbols
