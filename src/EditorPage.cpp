 
#include <wx/wx.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>
#include <wx/stc/stc.h>
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/imaglist.h>
#include <wx/artprov.h>
#include <wx/splitter.h>
#include <wx/statusbr.h>
#include <wx/fontdlg.h>
#include <wx/colordlg.h>
#include <wx/settings.h>
#include <wx/utils.h>
#include <wx/ffile.h>
#include <wx/msgdlg.h>
#include <wx/filedlg.h>
#include <wx/dirdlg.h>
#include <wx/stdpaths.h>
#include <wx/config.h>
#include <wx/log.h>

#include <map>
#include <vector>
#include <string>
#include "EditorPage.h"
#include "constant.h"
#include "JsonStyledTextCtrl.h"
#include "EditorConfigManager.h"
#include "Config.h"

EditorPage::EditorPage(wxWindow *parent, const wxString &path  )
    : JsonStyledTextCtrl(parent, wxID_ANY), filepath(path)
{
    ApplyTheme();
    if (!path.IsEmpty())
        LoadFile(path);

    Bind(wxEVT_STC_CHANGE, &EditorPage::OnChange, this);
    Bind(wxEVT_STC_UPDATEUI, &EditorPage::OnUpdateUI, this);
    Bind(wxEVT_STC_CHARADDED, &EditorPage::OnCharAdded, this);
}

// ── Tab title: filename or "Untitled" (with leading "●" if modified) ──
wxString EditorPage::GetTitle() const
{
    wxString base = filepath.IsEmpty() ? "Untitled" : wxFileName(filepath).GetFullName();
    return (modified ? wxString(L"\u25cf ") : wxString("")) + base;
}

// ── Load file from disk ──
bool EditorPage::LoadFile(const wxString &path)
{
    filepath = path;
    wxFFile f(path, "rb");
    if (!f.IsOpened())
        return false;
    wxString content;
    f.ReadAll(&content);
    f.Close();
    SetText(content);
    EmptyUndoBuffer();
    SetSavePoint();
    modified = false;
    ApplySyntax();
    return true;
}

// ── Save to disk ──
bool EditorPage::SaveFile(const wxString &path )
{
    wxString target = path.IsEmpty() ? filepath : path;
    if (target.IsEmpty())
        return false;
    wxFFile f(target, "wb");
    if (!f.IsOpened())
        return false;
    wxString content = GetText();
    f.Write(content);
    f.Close();
    if (!path.IsEmpty())
        filepath = path;
    SetSavePoint();
    modified = false;
    ApplySyntax();
    return true;
}

// ── Apply VSCode dark theme + syntax ──
void EditorPage::ApplyTheme()
{
    const std::string configuredTheme = AppEditorConfig::config.isObject()
                                            ? AppEditorConfig::GetEditorTheme()
                                            : std::string("vscode");
    wxString error;
    if (!EditorConfigManager::LoadTheme(wxString::FromUTF8(configuredTheme.c_str()), m_theme, &error))
        m_theme = Json::Value(Json::objectValue);

    const Json::Value& editor = m_theme["editor"];
    const wxColour bg = EditorConfigManager::ReadColour(editor, "background", Colors::BG);
    const wxColour fg = EditorConfigManager::ReadColour(editor, "foreground", Colors::FG);
    const wxColour panelBg = EditorConfigManager::ReadColour(editor, "panel_background", Colors::BG_PANEL);
    const wxColour lineBg = EditorConfigManager::ReadColour(editor, "line_number_background", Colors::LINENUMBER_BG);
    const wxColour lineFg = EditorConfigManager::ReadColour(editor, "line_number_foreground", Colors::LINENUMBER_FG);
    const wxColour caret = EditorConfigManager::ReadColour(editor, "caret", Colors::CARET_FG);
    const wxColour caretLine = EditorConfigManager::ReadColour(editor, "caret_line", Colors::CARET);
    const wxColour selection = EditorConfigManager::ReadColour(editor, "selection", Colors::SELECTION);
    const wxColour indentGuide = EditorConfigManager::ReadColour(editor, "indent_guide", wxColour(60, 60, 60));
    const wxColour edge = EditorConfigManager::ReadColour(editor, "edge", wxColour(60, 60, 60));
    const wxColour braceOk = EditorConfigManager::ReadColour(editor, "brace_ok", Colors::BRACE_OK);
    const wxColour braceBad = EditorConfigManager::ReadColour(editor, "brace_bad", Colors::BRACE_BAD);
    const wxString fontFace = EditorConfigManager::ReadString(editor, "font_face", "Monospace");
    const int fontSize = EditorConfigManager::ReadInt(editor, "font_size", 12);

    StyleSetBackground(wxSTC_STYLE_DEFAULT, bg);
    StyleSetForeground(wxSTC_STYLE_DEFAULT, fg);
    StyleSetFont(wxSTC_STYLE_DEFAULT,
                 wxFont(fontSize, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL,
                        wxFONTWEIGHT_NORMAL, false, fontFace));
    StyleClearAll();
    SetBackgroundColour(bg);
    SetForegroundColour(fg);

    SetMarginType(0, wxSTC_MARGIN_NUMBER);
    SetMarginWidth(0, TextWidth(wxSTC_STYLE_LINENUMBER, "_99999"));
    StyleSetBackground(wxSTC_STYLE_LINENUMBER, lineBg);
    StyleSetForeground(wxSTC_STYLE_LINENUMBER, lineFg);

    SetMarginType(1, wxSTC_MARGIN_SYMBOL);
    SetMarginMask(1, wxSTC_MASK_FOLDERS);
    SetMarginWidth(1, 16);
    SetMarginSensitive(1, true);
    SetMarginBackground(1, panelBg);
    SetProperty("fold", "1");
    SetProperty("fold.compact", "0");
    SetFoldFlags(wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED);
    MarkerDefine(wxSTC_MARKNUM_FOLDER, wxSTC_MARK_BOXPLUS, lineFg, panelBg);
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_BOXMINUS, lineFg, panelBg);
    MarkerDefine(wxSTC_MARKNUM_FOLDEREND, wxSTC_MARK_BOXPLUSCONNECTED, lineFg, panelBg);
    MarkerDefine(wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_VLINE, lineFg, panelBg);
    MarkerDefine(wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_LCORNER, lineFg, panelBg);
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_BOXMINUSCONNECTED, lineFg, panelBg);
    MarkerDefine(wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_TCORNER, lineFg, panelBg);

    SetCaretForeground(caret);
    SetCaretLineBackground(caretLine);
    SetCaretLineVisible(true);
    SetCaretLineVisibleAlways(true);
    SetSelBackground(true, selection);
    SetSelForeground(false, fg);

    StyleSetForeground(wxSTC_STYLE_BRACELIGHT, braceOk);
    StyleSetForeground(wxSTC_STYLE_BRACEBAD, braceBad);
    StyleSetBold(wxSTC_STYLE_BRACELIGHT, true);

    SetIndentationGuides(wxSTC_IV_LOOKBOTH);
    StyleSetBackground(wxSTC_STYLE_INDENTGUIDE, bg);
    StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, indentGuide);

    SetUseTabs(false);
    SetTabWidth(4);
    SetIndent(4);
    SetTabIndents(true);
    SetBackSpaceUnIndents(true);
    SetEdgeColumn(120);
    SetEdgeColour(edge);
    SetEdgeMode(wxSTC_EDGE_LINE);
    SetScrollWidth(1);
    SetScrollWidthTracking(true);
    SetEOLMode(wxSTC_EOL_LF);

    AutoCompSetAutoHide(false);
    AutoCompSetDropRestOfWord(true);

    ApplySyntax();
}

void EditorPage::ApplySyntax()
{
    LangInfo lang = DetectLanguage(filepath);
    SyntaxDefinition configuredSyntax;
    if (EditorConfigManager::LoadSyntaxForFile(filepath, configuredSyntax, nullptr))
    {
        lang.lexer = configuredSyntax.lexer;
        lang.keywords0 = configuredSyntax.keywords0;
        lang.keywords1 = configuredSyntax.keywords1;
        lang.name = configuredSyntax.name;
        if (configuredSyntax.properties.isObject())
        {
            for (const std::string& key : configuredSyntax.properties.getMemberNames())
            {
                const Json::Value& value = configuredSyntax.properties[key];
                if (value.isString())
                    SetProperty(wxString::FromUTF8(key.c_str()), wxString::FromUTF8(value.asCString()));
            }
        }
    }
    m_languageName = lang.name;
    SetLexer(lang.lexer);

    const Json::Value& editorTheme = m_theme["editor"];
    const Json::Value& syntaxTheme = m_theme["syntax"];
    const wxColour bg = EditorConfigManager::ReadColour(editorTheme, "background", Colors::BG);
    const wxColour fg = EditorConfigManager::ReadColour(editorTheme, "foreground", Colors::FG);
    const wxColour commentColour = EditorConfigManager::ReadColour(syntaxTheme, "comment", Colors::COMMENT);
    const wxColour keywordColour = EditorConfigManager::ReadColour(syntaxTheme, "keyword", Colors::KEYWORD);
    const wxColour typeColour = EditorConfigManager::ReadColour(syntaxTheme, "type", Colors::TYPE);
    const wxColour stringColour = EditorConfigManager::ReadColour(syntaxTheme, "string", Colors::STRING);
    const wxColour numberColour = EditorConfigManager::ReadColour(syntaxTheme, "number", Colors::NUMBER);
    const wxColour preprocColour = EditorConfigManager::ReadColour(syntaxTheme, "preprocessor", Colors::PREPROC);
    const wxColour operatorColour = EditorConfigManager::ReadColour(syntaxTheme, "operator", Colors::OPERATOR);
    const wxColour identifierColour = EditorConfigManager::ReadColour(syntaxTheme, "identifier", Colors::IDENTIFIER);
    const wxColour functionColour = EditorConfigManager::ReadColour(syntaxTheme, "function", Colors::FUNCTION);
    const wxColour errorColour = EditorConfigManager::ReadColour(syntaxTheme, "error", Colors::BRACE_BAD);

    for (int i = 0; i < wxSTC_STYLE_DEFAULT; ++i)
    {
        StyleSetBackground(i, bg);
        StyleSetForeground(i, fg);
    }

    switch (lang.lexer)
    {

    case wxSTC_LEX_CPP:
        SetKeyWords(0, lang.keywords0);
        SetKeyWords(1, lang.keywords1);
        StyleSetForeground(wxSTC_C_COMMENT, commentColour);
        StyleSetForeground(wxSTC_C_COMMENTLINE, commentColour);
        StyleSetForeground(wxSTC_C_COMMENTDOC, commentColour);
        StyleSetForeground(wxSTC_C_COMMENTLINEDOC, commentColour);
        StyleSetForeground(wxSTC_C_NUMBER, numberColour);
        StyleSetForeground(wxSTC_C_WORD, keywordColour);
        StyleSetBold(wxSTC_C_WORD, true);
        StyleSetForeground(wxSTC_C_WORD2, typeColour);
        StyleSetForeground(wxSTC_C_STRING, stringColour);
        StyleSetForeground(wxSTC_C_CHARACTER, stringColour);
        StyleSetForeground(wxSTC_C_STRINGEOL, stringColour);
        StyleSetForeground(wxSTC_C_PREPROCESSOR, preprocColour);
        StyleSetForeground(wxSTC_C_OPERATOR, operatorColour);
        StyleSetForeground(wxSTC_C_IDENTIFIER, identifierColour);
        StyleSetForeground(wxSTC_C_GLOBALCLASS, typeColour);
        StyleSetForeground(wxSTC_C_REGEX, stringColour);
        StyleSetForeground(wxSTC_C_VERBATIM, stringColour);
        StyleSetForeground(wxSTC_C_TRIPLEVERBATIM, stringColour);
        StyleSetItalic(wxSTC_C_COMMENT, true);
        StyleSetItalic(wxSTC_C_COMMENTLINE, true);
        break;

    case wxSTC_LEX_PYTHON:
        SetKeyWords(0, lang.keywords0);
        SetKeyWords(1, lang.keywords1);
        StyleSetForeground(wxSTC_P_COMMENTLINE, commentColour);
        StyleSetForeground(wxSTC_P_COMMENTBLOCK, commentColour);
        StyleSetForeground(wxSTC_P_NUMBER, numberColour);
        StyleSetForeground(wxSTC_P_WORD, keywordColour);
        StyleSetBold(wxSTC_P_WORD, true);
        StyleSetForeground(wxSTC_P_WORD2, typeColour);
        StyleSetForeground(wxSTC_P_STRING, stringColour);
        StyleSetForeground(wxSTC_P_CHARACTER, stringColour);
        StyleSetForeground(wxSTC_P_TRIPLE, stringColour);
        StyleSetForeground(wxSTC_P_TRIPLEDOUBLE, stringColour);
        StyleSetForeground(wxSTC_P_STRINGEOL, stringColour);
        StyleSetForeground(wxSTC_P_OPERATOR, operatorColour);
        StyleSetForeground(wxSTC_P_IDENTIFIER, fg);
        StyleSetForeground(wxSTC_P_CLASSNAME, typeColour);
        StyleSetForeground(wxSTC_P_DEFNAME, functionColour);
        StyleSetForeground(wxSTC_P_DECORATOR, preprocColour);
        StyleSetItalic(wxSTC_P_COMMENTLINE, true);
        break;

    case wxSTC_LEX_HTML:
    case wxSTC_LEX_XML:
        StyleSetForeground(wxSTC_H_TAG, keywordColour);
        StyleSetForeground(wxSTC_H_TAGUNKNOWN, keywordColour);
        StyleSetForeground(wxSTC_H_ATTRIBUTE, identifierColour);
        StyleSetForeground(wxSTC_H_ATTRIBUTEUNKNOWN, identifierColour);
        StyleSetForeground(wxSTC_H_NUMBER, numberColour);
        StyleSetForeground(wxSTC_H_DOUBLESTRING, stringColour);
        StyleSetForeground(wxSTC_H_SINGLESTRING, stringColour);
        StyleSetForeground(wxSTC_H_COMMENT, commentColour);
        StyleSetForeground(wxSTC_H_ENTITY, typeColour);
        StyleSetForeground(wxSTC_H_XMLSTART, preprocColour);
        StyleSetForeground(wxSTC_H_XMLEND, preprocColour);
        SetKeyWords(0, lang.keywords0);
        SetKeyWords(1, lang.keywords1);
        break;

    case wxSTC_LEX_JSON:
        StyleSetForeground(wxSTC_JSON_STRING, stringColour);
        StyleSetForeground(wxSTC_JSON_NUMBER, numberColour);
        StyleSetForeground(wxSTC_JSON_KEYWORD, keywordColour);
        StyleSetForeground(wxSTC_JSON_OPERATOR, operatorColour);
        StyleSetForeground(wxSTC_JSON_PROPERTYNAME, identifierColour);
        StyleSetForeground(wxSTC_JSON_LINECOMMENT, commentColour);
        StyleSetForeground(wxSTC_JSON_BLOCKCOMMENT, commentColour);
        StyleSetForeground(wxSTC_JSON_ERROR, errorColour);
        SetKeyWords(0, lang.keywords0);
        break;

    case wxSTC_LEX_RUST:
        StyleSetForeground(wxSTC_RUST_COMMENTLINE, commentColour);
        StyleSetForeground(wxSTC_RUST_COMMENTBLOCK, commentColour);
        StyleSetForeground(wxSTC_RUST_WORD, keywordColour);
        StyleSetForeground(wxSTC_RUST_WORD2, typeColour);
        StyleSetForeground(wxSTC_RUST_NUMBER, numberColour);
        StyleSetForeground(wxSTC_RUST_STRING, stringColour);
        StyleSetForeground(wxSTC_RUST_CHARACTER, stringColour);
        StyleSetForeground(wxSTC_RUST_OPERATOR, operatorColour);
        StyleSetForeground(wxSTC_RUST_IDENTIFIER, fg);
        StyleSetForeground(wxSTC_RUST_LIFETIME, typeColour);
        StyleSetForeground(wxSTC_RUST_MACRO, preprocColour);
        SetKeyWords(0, lang.keywords0);
        SetKeyWords(1, lang.keywords1);
        break;

    case wxSTC_LEX_BASH:
        StyleSetForeground(wxSTC_SH_COMMENTLINE, commentColour);
        StyleSetForeground(wxSTC_SH_NUMBER, numberColour);
        StyleSetForeground(wxSTC_SH_WORD, keywordColour);
        StyleSetForeground(wxSTC_SH_STRING, stringColour);
        StyleSetForeground(wxSTC_SH_CHARACTER, stringColour);
        StyleSetForeground(wxSTC_SH_OPERATOR, operatorColour);
        StyleSetForeground(wxSTC_SH_IDENTIFIER, fg);
        StyleSetForeground(wxSTC_SH_PARAM, identifierColour);
        SetKeyWords(0, lang.keywords0);
        break;

    case wxSTC_LEX_SQL:
        StyleSetForeground(wxSTC_SQL_COMMENT, commentColour);
        StyleSetForeground(wxSTC_SQL_COMMENTLINE, commentColour);
        StyleSetForeground(wxSTC_SQL_COMMENTDOC, commentColour);
        StyleSetForeground(wxSTC_SQL_NUMBER, numberColour);
        StyleSetForeground(wxSTC_SQL_WORD, keywordColour);
        StyleSetForeground(wxSTC_SQL_WORD2, typeColour);
        StyleSetForeground(wxSTC_SQL_STRING, stringColour);
        StyleSetForeground(wxSTC_SQL_CHARACTER, stringColour);
        StyleSetForeground(wxSTC_SQL_OPERATOR, operatorColour);
        StyleSetForeground(wxSTC_SQL_IDENTIFIER, fg);
        SetKeyWords(0, lang.keywords0);
        SetKeyWords(1, lang.keywords1);
        break;

    case wxSTC_LEX_CSS:
        StyleSetForeground(wxSTC_CSS_COMMENT, commentColour);
        StyleSetForeground(wxSTC_CSS_TAG, keywordColour);
        StyleSetForeground(wxSTC_CSS_CLASS, functionColour);
        StyleSetForeground(wxSTC_CSS_PSEUDOCLASS, typeColour);
        StyleSetForeground(wxSTC_CSS_UNKNOWN_PSEUDOCLASS, typeColour);
        StyleSetForeground(wxSTC_CSS_OPERATOR, operatorColour);
        StyleSetForeground(wxSTC_CSS_IDENTIFIER, identifierColour);
        StyleSetForeground(wxSTC_CSS_UNKNOWN_IDENTIFIER, fg);
        StyleSetForeground(wxSTC_CSS_VALUE, stringColour);
        StyleSetForeground(wxSTC_CSS_IMPORTANT, keywordColour);
        StyleSetForeground(wxSTC_CSS_DIRECTIVE, preprocColour);
        StyleSetForeground(wxSTC_CSS_DOUBLESTRING, stringColour);
        StyleSetForeground(wxSTC_CSS_SINGLESTRING, stringColour);
        StyleSetForeground(wxSTC_CSS_ID, functionColour);
        StyleSetForeground(wxSTC_CSS_ATTRIBUTE, identifierColour);
        SetKeyWords(0, lang.keywords0);
        break;

    case wxSTC_LEX_LUA:
        StyleSetForeground(wxSTC_LUA_COMMENT, commentColour);
        StyleSetForeground(wxSTC_LUA_COMMENTLINE, commentColour);
        StyleSetForeground(wxSTC_LUA_COMMENTDOC, commentColour);
        StyleSetForeground(wxSTC_LUA_NUMBER, numberColour);
        StyleSetForeground(wxSTC_LUA_WORD, keywordColour);
        StyleSetForeground(wxSTC_LUA_WORD2, typeColour);
        StyleSetForeground(wxSTC_LUA_STRING, stringColour);
        StyleSetForeground(wxSTC_LUA_CHARACTER, stringColour);
        StyleSetForeground(wxSTC_LUA_OPERATOR, operatorColour);
        StyleSetForeground(wxSTC_LUA_IDENTIFIER, fg);
        SetKeyWords(0, lang.keywords0);
        SetKeyWords(1, lang.keywords1);
        break;

    case wxSTC_LEX_YAML:
        StyleSetForeground(wxSTC_YAML_COMMENT, commentColour);
        StyleSetForeground(wxSTC_YAML_IDENTIFIER, identifierColour);
        StyleSetForeground(wxSTC_YAML_KEYWORD, keywordColour);
        StyleSetForeground(wxSTC_YAML_NUMBER, numberColour);
        StyleSetForeground(wxSTC_YAML_REFERENCE, typeColour);
        StyleSetForeground(wxSTC_YAML_DOCUMENT, preprocColour);
        StyleSetForeground(wxSTC_YAML_TEXT, stringColour);
        StyleSetForeground(wxSTC_YAML_ERROR, errorColour);
        SetKeyWords(0, lang.keywords0);
        break;

    case wxSTC_LEX_MARKDOWN:
        StyleSetForeground(wxSTC_MARKDOWN_STRONG1, keywordColour);
        StyleSetForeground(wxSTC_MARKDOWN_STRONG2, keywordColour);
        StyleSetForeground(wxSTC_MARKDOWN_EM1, typeColour);
        StyleSetForeground(wxSTC_MARKDOWN_EM2, typeColour);
        StyleSetForeground(wxSTC_MARKDOWN_HEADER1, functionColour);
        StyleSetForeground(wxSTC_MARKDOWN_HEADER2, functionColour);
        StyleSetForeground(wxSTC_MARKDOWN_HEADER3, functionColour);
        StyleSetForeground(wxSTC_MARKDOWN_HEADER4, functionColour);
        StyleSetForeground(wxSTC_MARKDOWN_HEADER5, functionColour);
        StyleSetForeground(wxSTC_MARKDOWN_HEADER6, functionColour);
        StyleSetForeground(wxSTC_MARKDOWN_ULIST_ITEM, identifierColour);
        StyleSetForeground(wxSTC_MARKDOWN_OLIST_ITEM, identifierColour);
        StyleSetForeground(wxSTC_MARKDOWN_BLOCKQUOTE, commentColour);
        StyleSetForeground(wxSTC_MARKDOWN_STRIKEOUT, preprocColour);
        StyleSetForeground(wxSTC_MARKDOWN_HRULE, operatorColour);
        StyleSetForeground(wxSTC_MARKDOWN_LINK, stringColour);
        StyleSetForeground(wxSTC_MARKDOWN_CODE, typeColour);
        StyleSetForeground(wxSTC_MARKDOWN_CODE2, typeColour);
        StyleSetForeground(wxSTC_MARKDOWN_CODEBK, typeColour);
        StyleSetBold(wxSTC_MARKDOWN_STRONG1, true);
        StyleSetBold(wxSTC_MARKDOWN_STRONG2, true);
        StyleSetBold(wxSTC_MARKDOWN_HEADER1, true);
        StyleSetBold(wxSTC_MARKDOWN_HEADER2, true);
        StyleSetItalic(wxSTC_MARKDOWN_EM1, true);
        StyleSetItalic(wxSTC_MARKDOWN_EM2, true);
        break;

    case wxSTC_LEX_CMAKE:
        StyleSetForeground(wxSTC_CMAKE_COMMENT, commentColour);
        StyleSetForeground(wxSTC_CMAKE_STRINGDQ, stringColour);
        StyleSetForeground(wxSTC_CMAKE_COMMANDS, keywordColour);
        StyleSetForeground(wxSTC_CMAKE_PARAMETERS, identifierColour);
        StyleSetForeground(wxSTC_CMAKE_VARIABLE, typeColour);
        StyleSetForeground(wxSTC_CMAKE_USERDEFINED, functionColour);
        SetKeyWords(0, lang.keywords0);
        break;

    default:
        break;
    }
}

void EditorPage::OnChange(wxStyledTextEvent &)
{
    modified = true;
}

void EditorPage::OnUpdateUI(wxStyledTextEvent &)
{
    // Brace matching
    int pos = GetCurrentPos();
    int ch = GetCharAt(pos - 1);
    const wxString braces = "()[]{}<>";
    if (braces.Find((wxChar)ch) != wxNOT_FOUND)
    {
        int other = BraceMatch(pos - 1);
        if (other != wxSTC_INVALID_POSITION)
            BraceHighlight(pos - 1, other);
        else
            BraceBadLight(pos - 1);
    }
    else
    {
        BraceHighlight(wxSTC_INVALID_POSITION, wxSTC_INVALID_POSITION);
    }
}

void EditorPage::OnCharAdded(wxStyledTextEvent &evt)
{
    // Auto-indent: match previous line's indentation
    char ch = (char)evt.GetKey();
    if (ch == '\n')
    {
        int line = GetCurrentLine();
        if (line > 0)
        {
            wxString indent = GetLineText(line - 1);
            int spaces = 0;
            while (spaces < (int)indent.Len() &&
                   (indent[spaces] == ' ' || indent[spaces] == '\t'))
                ++spaces;
            if (spaces > 0)
                InsertText(GetCurrentPos(), indent.Left(spaces));
            GotoPos(GetCurrentPos() + spaces);
        }
    }
    // Auto-close brackets
    const char opens[] = "([{\"'";
    const char closes[] = ")]}\"'";
    for (int i = 0; opens[i]; ++i)
    {
        if (ch == opens[i])
        {
            InsertText(GetCurrentPos(), wxString(closes[i]));
            break;
        }
    }
}
