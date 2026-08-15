#pragma once

#include <wx/string.h>
#include <wx/colour.h>
#include <wx/arrstr.h>
#include <json/json.h>
#include <vector>

struct SyntaxDefinition
{
    wxString id;
    wxString name;
    wxString lexerName;
    int lexer = 0;
    int priority = 0;
    wxString keywords0;
    wxString keywords1;
    std::vector<wxString> extensions;
    Json::Value properties;
};

class EditorConfigManager
{
public:
    static wxString ConfigRoot();
    static wxString ThemesDirectory();
    static wxString SyntaxDirectory();

    static wxArrayString ThemeIds();
    static wxString ThemeDisplayName(const wxString& themeId);
    static bool LoadTheme(const wxString& themeId, Json::Value& theme, wxString* error = nullptr);
    static bool LoadSyntaxForFile(const wxString& filePath, SyntaxDefinition& syntax, wxString* error = nullptr);

    static int LexerFromName(const wxString& lexerName);
    static wxColour ReadColour(const Json::Value& object,
                               const char* key,
                               const wxColour& fallback);
    static wxString ReadString(const Json::Value& object,
                               const char* key,
                               const wxString& fallback = wxString());
    static int ReadInt(const Json::Value& object, const char* key, int fallback);

private:
    static bool LoadJsonFile(const wxString& path, Json::Value& value, wxString* error);
    static bool ParseHexColour(const wxString& text, wxColour& colour);
};
