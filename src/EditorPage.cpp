 
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
    // Global editor settings
    StyleSetBackground(wxSTC_STYLE_DEFAULT, Colors::BG);
    StyleSetForeground(wxSTC_STYLE_DEFAULT, Colors::FG);
    StyleSetFont(wxSTC_STYLE_DEFAULT, wxFont(12, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL,
                                             wxFONTWEIGHT_NORMAL, false, "Consolas"));
    StyleClearAll();

    // Line numbers
    SetMarginType(0, wxSTC_MARGIN_NUMBER);
    SetMarginWidth(0, TextWidth(wxSTC_STYLE_LINENUMBER, "_99999"));
    StyleSetBackground(wxSTC_STYLE_LINENUMBER, Colors::LINENUMBER_BG);
    StyleSetForeground(wxSTC_STYLE_LINENUMBER, Colors::LINENUMBER_FG);

    // Fold margin
    SetMarginType(1, wxSTC_MARGIN_SYMBOL);
    SetMarginMask(1, wxSTC_MASK_FOLDERS);
    SetMarginWidth(1, 16);
    SetMarginSensitive(1, true);
    SetMarginBackground(1, Colors::BG_PANEL);
    SetProperty("fold", "1");
    SetProperty("fold.compact", "0");
    SetFoldFlags(wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED);
    MarkerDefine(wxSTC_MARKNUM_FOLDER, wxSTC_MARK_BOXPLUS, Colors::LINENUMBER_FG, Colors::BG_PANEL);
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_BOXMINUS, Colors::LINENUMBER_FG, Colors::BG_PANEL);
    MarkerDefine(wxSTC_MARKNUM_FOLDEREND, wxSTC_MARK_BOXPLUSCONNECTED, Colors::LINENUMBER_FG, Colors::BG_PANEL);
    MarkerDefine(wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_VLINE, Colors::LINENUMBER_FG, Colors::BG_PANEL);
    MarkerDefine(wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_LCORNER, Colors::LINENUMBER_FG, Colors::BG_PANEL);
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_BOXMINUSCONNECTED, Colors::LINENUMBER_FG, Colors::BG_PANEL);
    MarkerDefine(wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_TCORNER, Colors::LINENUMBER_FG, Colors::BG_PANEL);

    // Caret / selection
    SetCaretForeground(Colors::CARET_FG);
    SetCaretLineBackground(Colors::CARET);
    SetCaretLineVisible(true);
    SetCaretLineVisibleAlways(true);
    SetSelBackground(true, Colors::SELECTION);
    SetSelForeground(false, Colors::FG);

    // Brace matching
    StyleSetForeground(wxSTC_STYLE_BRACELIGHT, Colors::BRACE_OK);
    StyleSetForeground(wxSTC_STYLE_BRACEBAD, Colors::BRACE_BAD);
    StyleSetBold(wxSTC_STYLE_BRACELIGHT, true);

    // Indentation guides
    SetIndentationGuides(wxSTC_IV_LOOKBOTH);
    StyleSetBackground(wxSTC_STYLE_INDENTGUIDE, Colors::BG);
    StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, wxColour(60, 60, 60));

    // Editor behaviour
    SetUseTabs(false);
    SetTabWidth(4);
    SetIndent(4);
    SetTabIndents(true);
    SetBackSpaceUnIndents(true);
    SetEdgeColumn(120);
    SetEdgeColour(wxColour(60, 60, 60));
    SetEdgeMode(wxSTC_EDGE_LINE);
    SetScrollWidth(1);
    SetScrollWidthTracking(true);
    SetEOLMode(wxSTC_EOL_LF);

    // Auto-completion
    AutoCompSetAutoHide(false);
    AutoCompSetDropRestOfWord(true);

    ApplySyntax();
}

void EditorPage::ApplySyntax()
{
    LangInfo lang = DetectLanguage(filepath);
    SetLexer(lang.lexer);

    // Clear all styles first to theme defaults
    for (int i = 0; i < wxSTC_STYLE_LASTPREDEFINED; ++i)
    {
        StyleSetBackground(i, Colors::BG);
        StyleSetForeground(i, Colors::FG);
    }

    // Apply language-specific styles
    switch (lang.lexer)
    {

    case wxSTC_LEX_CPP:
        SetKeyWords(0, lang.keywords0);
        SetKeyWords(1, lang.keywords1);
        StyleSetForeground(wxSTC_C_COMMENT, Colors::COMMENT);
        StyleSetForeground(wxSTC_C_COMMENTLINE, Colors::COMMENT);
        StyleSetForeground(wxSTC_C_COMMENTDOC, Colors::COMMENT);
        StyleSetForeground(wxSTC_C_COMMENTLINEDOC, Colors::COMMENT);
        StyleSetForeground(wxSTC_C_NUMBER, Colors::NUMBER);
        StyleSetForeground(wxSTC_C_WORD, Colors::KEYWORD);
        StyleSetBold(wxSTC_C_WORD, true);
        StyleSetForeground(wxSTC_C_WORD2, Colors::TYPE);
        StyleSetForeground(wxSTC_C_STRING, Colors::STRING);
        StyleSetForeground(wxSTC_C_CHARACTER, Colors::STRING);
        StyleSetForeground(wxSTC_C_STRINGEOL, Colors::STRING);
        StyleSetForeground(wxSTC_C_PREPROCESSOR, Colors::PREPROC);
        StyleSetForeground(wxSTC_C_OPERATOR, Colors::OPERATOR);
        StyleSetForeground(wxSTC_C_IDENTIFIER, Colors::FG);
        StyleSetForeground(wxSTC_C_GLOBALCLASS, Colors::TYPE);
        StyleSetForeground(wxSTC_C_REGEX, Colors::STRING);
        StyleSetForeground(wxSTC_C_VERBATIM, Colors::STRING);
        StyleSetForeground(wxSTC_C_TRIPLEVERBATIM, Colors::STRING);
        StyleSetItalic(wxSTC_C_COMMENT, true);
        StyleSetItalic(wxSTC_C_COMMENTLINE, true);
        break;

    case wxSTC_LEX_PYTHON:
        SetKeyWords(0, lang.keywords0);
        SetKeyWords(1, lang.keywords1);
        StyleSetForeground(wxSTC_P_COMMENTLINE, Colors::COMMENT);
        StyleSetForeground(wxSTC_P_COMMENTBLOCK, Colors::COMMENT);
        StyleSetForeground(wxSTC_P_NUMBER, Colors::NUMBER);
        StyleSetForeground(wxSTC_P_WORD, Colors::KEYWORD);
        StyleSetBold(wxSTC_P_WORD, true);
        StyleSetForeground(wxSTC_P_WORD2, Colors::TYPE);
        StyleSetForeground(wxSTC_P_STRING, Colors::STRING);
        StyleSetForeground(wxSTC_P_CHARACTER, Colors::STRING);
        StyleSetForeground(wxSTC_P_TRIPLE, Colors::STRING);
        StyleSetForeground(wxSTC_P_TRIPLEDOUBLE, Colors::STRING);
        StyleSetForeground(wxSTC_P_STRINGEOL, Colors::STRING);
        StyleSetForeground(wxSTC_P_OPERATOR, Colors::OPERATOR);
        StyleSetForeground(wxSTC_P_IDENTIFIER, Colors::FG);
        StyleSetForeground(wxSTC_P_CLASSNAME, Colors::TYPE);
        StyleSetForeground(wxSTC_P_DEFNAME, Colors::FUNCTION);
        StyleSetForeground(wxSTC_P_DECORATOR, Colors::PREPROC);
        StyleSetItalic(wxSTC_P_COMMENTLINE, true);
        break;

    case wxSTC_LEX_HTML:
    case wxSTC_LEX_XML:
        StyleSetForeground(wxSTC_H_TAG, Colors::KEYWORD);
        StyleSetForeground(wxSTC_H_TAGUNKNOWN, Colors::KEYWORD);
        StyleSetForeground(wxSTC_H_ATTRIBUTE, Colors::IDENTIFIER);
        StyleSetForeground(wxSTC_H_ATTRIBUTEUNKNOWN, Colors::IDENTIFIER);
        StyleSetForeground(wxSTC_H_NUMBER, Colors::NUMBER);
        StyleSetForeground(wxSTC_H_DOUBLESTRING, Colors::STRING);
        StyleSetForeground(wxSTC_H_SINGLESTRING, Colors::STRING);
        StyleSetForeground(wxSTC_H_COMMENT, Colors::COMMENT);
        StyleSetForeground(wxSTC_H_ENTITY, Colors::TYPE);
        StyleSetForeground(wxSTC_H_XMLSTART, Colors::PREPROC);
        StyleSetForeground(wxSTC_H_XMLEND, Colors::PREPROC);
        SetKeyWords(0, lang.keywords0);
        SetKeyWords(1, lang.keywords1);
        break;

    case wxSTC_LEX_JSON:
        StyleSetForeground(wxSTC_JSON_STRING, Colors::STRING);
        StyleSetForeground(wxSTC_JSON_NUMBER, Colors::NUMBER);
        StyleSetForeground(wxSTC_JSON_KEYWORD, Colors::KEYWORD);
        StyleSetForeground(wxSTC_JSON_OPERATOR, Colors::OPERATOR);
        StyleSetForeground(wxSTC_JSON_PROPERTYNAME, Colors::IDENTIFIER);
        StyleSetForeground(wxSTC_JSON_LINECOMMENT, Colors::COMMENT);
        StyleSetForeground(wxSTC_JSON_BLOCKCOMMENT, Colors::COMMENT);
        StyleSetForeground(wxSTC_JSON_ERROR, Colors::BRACE_BAD);
        SetKeyWords(0, lang.keywords0);
        break;

    case wxSTC_LEX_RUST:
        StyleSetForeground(wxSTC_RUST_COMMENTLINE, Colors::COMMENT);
        StyleSetForeground(wxSTC_RUST_COMMENTBLOCK, Colors::COMMENT);
        StyleSetForeground(wxSTC_RUST_WORD, Colors::KEYWORD);
        StyleSetForeground(wxSTC_RUST_WORD2, Colors::TYPE);
        StyleSetForeground(wxSTC_RUST_NUMBER, Colors::NUMBER);
        StyleSetForeground(wxSTC_RUST_STRING, Colors::STRING);
        StyleSetForeground(wxSTC_RUST_CHARACTER, Colors::STRING);
        StyleSetForeground(wxSTC_RUST_OPERATOR, Colors::OPERATOR);
        StyleSetForeground(wxSTC_RUST_IDENTIFIER, Colors::FG);
        StyleSetForeground(wxSTC_RUST_LIFETIME, Colors::TYPE);
        StyleSetForeground(wxSTC_RUST_MACRO, Colors::PREPROC);
        SetKeyWords(0, lang.keywords0);
        SetKeyWords(1, lang.keywords1);
        break;

    case wxSTC_LEX_BASH:
        StyleSetForeground(wxSTC_SH_COMMENTLINE, Colors::COMMENT);
        StyleSetForeground(wxSTC_SH_NUMBER, Colors::NUMBER);
        StyleSetForeground(wxSTC_SH_WORD, Colors::KEYWORD);
        StyleSetForeground(wxSTC_SH_STRING, Colors::STRING);
        StyleSetForeground(wxSTC_SH_CHARACTER, Colors::STRING);
        StyleSetForeground(wxSTC_SH_OPERATOR, Colors::OPERATOR);
        StyleSetForeground(wxSTC_SH_IDENTIFIER, Colors::FG);
        StyleSetForeground(wxSTC_SH_PARAM, Colors::IDENTIFIER);
        SetKeyWords(0, lang.keywords0);
        break;

    case wxSTC_LEX_SQL:
        StyleSetForeground(wxSTC_SQL_COMMENT, Colors::COMMENT);
        StyleSetForeground(wxSTC_SQL_COMMENTLINE, Colors::COMMENT);
        StyleSetForeground(wxSTC_SQL_COMMENTDOC, Colors::COMMENT);
        StyleSetForeground(wxSTC_SQL_NUMBER, Colors::NUMBER);
        StyleSetForeground(wxSTC_SQL_WORD, Colors::KEYWORD);
        StyleSetForeground(wxSTC_SQL_WORD2, Colors::TYPE);
        StyleSetForeground(wxSTC_SQL_STRING, Colors::STRING);
        StyleSetForeground(wxSTC_SQL_CHARACTER, Colors::STRING);
        StyleSetForeground(wxSTC_SQL_OPERATOR, Colors::OPERATOR);
        StyleSetForeground(wxSTC_SQL_IDENTIFIER, Colors::FG);
        SetKeyWords(0, lang.keywords0);
        SetKeyWords(1, lang.keywords1);
        break;

    case wxSTC_LEX_CSS:
        StyleSetForeground(wxSTC_CSS_COMMENT, Colors::COMMENT);
        StyleSetForeground(wxSTC_CSS_TAG, Colors::KEYWORD);
        StyleSetForeground(wxSTC_CSS_CLASS, Colors::FUNCTION);
        StyleSetForeground(wxSTC_CSS_PSEUDOCLASS, Colors::TYPE);
        StyleSetForeground(wxSTC_CSS_UNKNOWN_PSEUDOCLASS, Colors::TYPE);
        StyleSetForeground(wxSTC_CSS_OPERATOR, Colors::OPERATOR);
        StyleSetForeground(wxSTC_CSS_IDENTIFIER, Colors::IDENTIFIER);
        StyleSetForeground(wxSTC_CSS_UNKNOWN_IDENTIFIER, Colors::FG);
        StyleSetForeground(wxSTC_CSS_VALUE, Colors::STRING);
        StyleSetForeground(wxSTC_CSS_IMPORTANT, Colors::KEYWORD);
        StyleSetForeground(wxSTC_CSS_DIRECTIVE, Colors::PREPROC);
        StyleSetForeground(wxSTC_CSS_DOUBLESTRING, Colors::STRING);
        StyleSetForeground(wxSTC_CSS_SINGLESTRING, Colors::STRING);
        StyleSetForeground(wxSTC_CSS_ID, Colors::FUNCTION);
        StyleSetForeground(wxSTC_CSS_ATTRIBUTE, Colors::IDENTIFIER);
        SetKeyWords(0, lang.keywords0);
        break;

    case wxSTC_LEX_LUA:
        StyleSetForeground(wxSTC_LUA_COMMENT, Colors::COMMENT);
        StyleSetForeground(wxSTC_LUA_COMMENTLINE, Colors::COMMENT);
        StyleSetForeground(wxSTC_LUA_COMMENTDOC, Colors::COMMENT);
        StyleSetForeground(wxSTC_LUA_NUMBER, Colors::NUMBER);
        StyleSetForeground(wxSTC_LUA_WORD, Colors::KEYWORD);
        StyleSetForeground(wxSTC_LUA_WORD2, Colors::TYPE);
        StyleSetForeground(wxSTC_LUA_STRING, Colors::STRING);
        StyleSetForeground(wxSTC_LUA_CHARACTER, Colors::STRING);
        StyleSetForeground(wxSTC_LUA_OPERATOR, Colors::OPERATOR);
        StyleSetForeground(wxSTC_LUA_IDENTIFIER, Colors::FG);
        SetKeyWords(0, lang.keywords0);
        SetKeyWords(1, lang.keywords1);
        break;

    case wxSTC_LEX_YAML:
        StyleSetForeground(wxSTC_YAML_COMMENT, Colors::COMMENT);
        StyleSetForeground(wxSTC_YAML_IDENTIFIER, Colors::IDENTIFIER);
        StyleSetForeground(wxSTC_YAML_KEYWORD, Colors::KEYWORD);
        StyleSetForeground(wxSTC_YAML_NUMBER, Colors::NUMBER);
        StyleSetForeground(wxSTC_YAML_REFERENCE, Colors::TYPE);
        StyleSetForeground(wxSTC_YAML_DOCUMENT, Colors::PREPROC);
        StyleSetForeground(wxSTC_YAML_TEXT, Colors::STRING);
        StyleSetForeground(wxSTC_YAML_ERROR, Colors::BRACE_BAD);
        SetKeyWords(0, lang.keywords0);
        break;

    case wxSTC_LEX_MARKDOWN:
        StyleSetForeground(wxSTC_MARKDOWN_STRONG1, Colors::KEYWORD);
        StyleSetForeground(wxSTC_MARKDOWN_STRONG2, Colors::KEYWORD);
        StyleSetForeground(wxSTC_MARKDOWN_EM1, Colors::TYPE);
        StyleSetForeground(wxSTC_MARKDOWN_EM2, Colors::TYPE);
        StyleSetForeground(wxSTC_MARKDOWN_HEADER1, Colors::FUNCTION);
        StyleSetForeground(wxSTC_MARKDOWN_HEADER2, Colors::FUNCTION);
        StyleSetForeground(wxSTC_MARKDOWN_HEADER3, Colors::FUNCTION);
        StyleSetForeground(wxSTC_MARKDOWN_HEADER4, Colors::FUNCTION);
        StyleSetForeground(wxSTC_MARKDOWN_HEADER5, Colors::FUNCTION);
        StyleSetForeground(wxSTC_MARKDOWN_HEADER6, Colors::FUNCTION);
        StyleSetForeground(wxSTC_MARKDOWN_ULIST_ITEM, Colors::IDENTIFIER);
        StyleSetForeground(wxSTC_MARKDOWN_OLIST_ITEM, Colors::IDENTIFIER);
        StyleSetForeground(wxSTC_MARKDOWN_BLOCKQUOTE, Colors::COMMENT);
        StyleSetForeground(wxSTC_MARKDOWN_STRIKEOUT, Colors::PREPROC);
        StyleSetForeground(wxSTC_MARKDOWN_HRULE, Colors::OPERATOR);
        StyleSetForeground(wxSTC_MARKDOWN_LINK, Colors::STRING);
        StyleSetForeground(wxSTC_MARKDOWN_CODE, Colors::TYPE);
        StyleSetForeground(wxSTC_MARKDOWN_CODE2, Colors::TYPE);
        StyleSetForeground(wxSTC_MARKDOWN_CODEBK, Colors::TYPE);
        StyleSetBold(wxSTC_MARKDOWN_STRONG1, true);
        StyleSetBold(wxSTC_MARKDOWN_STRONG2, true);
        StyleSetBold(wxSTC_MARKDOWN_HEADER1, true);
        StyleSetBold(wxSTC_MARKDOWN_HEADER2, true);
        StyleSetItalic(wxSTC_MARKDOWN_EM1, true);
        StyleSetItalic(wxSTC_MARKDOWN_EM2, true);
        break;

    case wxSTC_LEX_CMAKE:
        StyleSetForeground(wxSTC_CMAKE_COMMENT, Colors::COMMENT);
        StyleSetForeground(wxSTC_CMAKE_STRINGDQ, Colors::STRING);
        StyleSetForeground(wxSTC_CMAKE_COMMANDS, Colors::KEYWORD);
        StyleSetForeground(wxSTC_CMAKE_PARAMETERS, Colors::IDENTIFIER);
        StyleSetForeground(wxSTC_CMAKE_VARIABLE, Colors::TYPE);
        StyleSetForeground(wxSTC_CMAKE_USERDEFINED, Colors::FUNCTION);
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
