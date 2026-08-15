#include "EditorConfigManager.h"

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/filefn.h>
#include <wx/stdpaths.h>
#include <wx/stc/stc.h>

#include <fstream>
#include <algorithm>
#include <utility>
#include <string>

namespace
{
wxString JoinPath(const wxString& left, const wxString& right)
{
    wxFileName path(left, right);
    return path.GetFullPath();
}

wxString LowerExtension(const wxString& path)
{
    return wxFileName(path).GetExt().Lower();
}
}

wxString EditorConfigManager::ConfigRoot()
{
    const wxString executable = wxStandardPaths::Get().GetExecutablePath();
    if (!executable.IsEmpty())
    {
        wxFileName executableFile(executable);
        const wxString nextToExecutable = JoinPath(executableFile.GetPath(), "config");
        if (wxDirExists(nextToExecutable))
            return nextToExecutable;
    }

    wxFileName cwdConfig(wxGetCwd(), "config");
    if (wxDirExists(cwdConfig.GetFullPath()))
        return cwdConfig.GetFullPath();

    return wxString("config");
}

wxString EditorConfigManager::ThemesDirectory()
{
    return JoinPath(ConfigRoot(), "themes");
}

wxString EditorConfigManager::SyntaxDirectory()
{
    return JoinPath(ConfigRoot(), "syntax");
}

bool EditorConfigManager::LoadJsonFile(const wxString& path,
                                       Json::Value& value,
                                       wxString* error)
{
    std::ifstream stream(path.ToStdString(), std::ios::binary);
    if (!stream.is_open())
    {
        if (error)
            *error = wxString("Unable to open JSON configuration: ") + path;
        return false;
    }

    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, stream, &value, &errors))
    {
        if (error)
            *error = wxString("Invalid JSON configuration: ") + path + "\n" + wxString::FromUTF8(errors.c_str());
        return false;
    }
    return true;
}

wxArrayString EditorConfigManager::ThemeIds()
{
    wxArrayString ids;
    const wxString directory = ThemesDirectory();
    wxDir dir(directory);
    if (!dir.IsOpened())
        return ids;

    wxString filename;
    bool cont = dir.GetFirst(&filename, "*.json", wxDIR_FILES);
    while (cont)
    {
        Json::Value theme;
        if (LoadJsonFile(JoinPath(directory, filename), theme, nullptr))
        {
            wxString id = wxString::FromUTF8(theme.get("id", "").asCString());
            if (id.IsEmpty())
                id = wxFileName(filename).GetName();
            ids.Add(id);
        }
        cont = dir.GetNext(&filename);
    }
    ids.Sort();
    return ids;
}

wxString EditorConfigManager::ThemeDisplayName(const wxString& themeId)
{
    Json::Value theme;
    if (!LoadTheme(themeId, theme, nullptr))
        return themeId;
    const wxString name = wxString::FromUTF8(theme.get("name", "").asCString());
    return name.IsEmpty() ? themeId : name;
}

bool EditorConfigManager::LoadTheme(const wxString& themeId,
                                    Json::Value& theme,
                                    wxString* error)
{
    wxString id = themeId;
    if (id.IsEmpty())
        id = "vscode";

    wxString path = JoinPath(ThemesDirectory(), id + ".json");
    if (LoadJsonFile(path, theme, error))
        return true;

    if (id != "vscode")
    {
        path = JoinPath(ThemesDirectory(), "vscode.json");
        return LoadJsonFile(path, theme, error);
    }
    return false;
}

int EditorConfigManager::LexerFromName(const wxString& lexerName)
{
    const wxString lexer = lexerName.Lower();
    if (lexer == "cpp" || lexer == "c" || lexer == "c++" || lexer == "java" || lexer == "kotlin" || lexer == "javascript")
        return wxSTC_LEX_CPP;
    if (lexer == "python")
        return wxSTC_LEX_PYTHON;
    if (lexer == "json")
        return wxSTC_LEX_JSON;
    if (lexer == "html")
        return wxSTC_LEX_HTML;
    if (lexer == "xml")
        return wxSTC_LEX_XML;
    if (lexer == "css")
        return wxSTC_LEX_CSS;
    if (lexer == "bash" || lexer == "shell")
        return wxSTC_LEX_BASH;
    if (lexer == "cmake")
        return wxSTC_LEX_CMAKE;
    if (lexer == "markdown")
        return wxSTC_LEX_MARKDOWN;
    if (lexer == "yaml")
        return wxSTC_LEX_YAML;
    if (lexer == "sql")
        return wxSTC_LEX_SQL;
    if (lexer == "lua")
        return wxSTC_LEX_LUA;
    if (lexer == "rust")
        return wxSTC_LEX_RUST;
    return wxSTC_LEX_NULL;
}

bool EditorConfigManager::LoadSyntaxForFile(const wxString& filePath,
                                            SyntaxDefinition& syntax,
                                            wxString* error)
{
    const wxString extension = LowerExtension(filePath);
    if (extension.IsEmpty())
        return false;

    const wxString directory = SyntaxDirectory();
    wxDir dir(directory);
    if (!dir.IsOpened())
    {
        if (error)
            *error = wxString("Syntax configuration directory not found: ") + directory;
        return false;
    }

    struct Candidate
    {
        SyntaxDefinition syntax;
        wxString source;
    };
    std::vector<Candidate> candidates;

    wxString filename;
    bool cont = dir.GetFirst(&filename, "*.json", wxDIR_FILES);
    while (cont)
    {
        Json::Value json;
        wxString loadError;
        if (LoadJsonFile(JoinPath(directory, filename), json, &loadError))
        {
            const Json::Value& extensions = json["extensions"];
            bool matches = false;
            std::vector<wxString> parsedExtensions;
            if (extensions.isArray())
            {
                for (const auto& item : extensions)
                {
                    if (!item.isString())
                        continue;
                    wxString ext = wxString::FromUTF8(item.asCString()).Lower();
                    if (ext.StartsWith("."))
                        ext = ext.Mid(1);
                    parsedExtensions.push_back(ext);
                    if (ext == extension)
                        matches = true;
                }
            }

            if (matches)
            {
                SyntaxDefinition def;
                def.id = wxString::FromUTF8(json.get("id", "").asCString());
                def.name = wxString::FromUTF8(json.get("name", "").asCString());
                def.priority = json.get("priority", 0).asInt();
                def.lexerName = wxString::FromUTF8(json.get("lexer", "").asCString());
                def.lexer = LexerFromName(def.lexerName);
                def.extensions = std::move(parsedExtensions);
                if (json["keywords"].isObject())
                {
                    def.keywords0 = wxString::FromUTF8(json["keywords"].get("primary", "").asCString());
                    def.keywords1 = wxString::FromUTF8(json["keywords"].get("types", "").asCString());
                }
                if (json["properties"].isObject())
                    def.properties = json["properties"];
                candidates.push_back({def, filename});
            }
        }
        cont = dir.GetNext(&filename);
    }

    if (candidates.empty())
        return false;

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b)
    {
        if (a.syntax.priority != b.syntax.priority)
            return a.syntax.priority > b.syntax.priority;
        return a.source.CmpNoCase(b.source) < 0;
    });
    syntax = candidates.front().syntax;
    return true;
}

bool EditorConfigManager::ParseHexColour(const wxString& text, wxColour& colour)
{
    wxString value = text;
    value.Trim(true).Trim(false);
    if (value.StartsWith("#"))
        value = value.Mid(1);
    if (value.length() != 6)
        return false;

    unsigned long rgb = 0;
    if (!value.ToULong(&rgb, 16))
        return false;
    colour = wxColour(static_cast<unsigned char>((rgb >> 16) & 0xff),
                      static_cast<unsigned char>((rgb >> 8) & 0xff),
                      static_cast<unsigned char>(rgb & 0xff));
    return colour.IsOk();
}

wxColour EditorConfigManager::ReadColour(const Json::Value& object,
                                         const char* key,
                                         const wxColour& fallback)
{
    if (!object.isObject() || !object.isMember(key))
        return fallback;

    const Json::Value& value = object[key];
    if (value.isString())
    {
        wxColour colour;
        if (ParseHexColour(wxString::FromUTF8(value.asCString()), colour))
            return colour;
    }
    else if (value.isArray() && value.size() >= 3)
    {
        return wxColour(value[0].asInt(), value[1].asInt(), value[2].asInt());
    }
    return fallback;
}

wxString EditorConfigManager::ReadString(const Json::Value& object,
                                         const char* key,
                                         const wxString& fallback)
{
    if (!object.isObject() || !object.isMember(key) || !object[key].isString())
        return fallback;
    return wxString::FromUTF8(object[key].asCString());
}

int EditorConfigManager::ReadInt(const Json::Value& object, const char* key, int fallback)
{
    if (!object.isObject() || !object.isMember(key) || !object[key].isInt())
        return fallback;
    return object[key].asInt();
}
