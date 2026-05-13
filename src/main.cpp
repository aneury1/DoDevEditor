/**
 * wxEditor - A VSCode-like multi-tab text editor
 * Single-file implementation using wxWidgets + wxStyledTextCtrl
 *
 * Build (Linux):
 *   g++ wxEditor.cpp -o wxEditor $(wx-config --cxxflags --libs stc,aui,core,base) -std=c++17
 *
 * Build (Windows MSYS2/MinGW):
 *   g++ wxEditor.cpp -o wxEditor.exe $(wx-config --cxxflags --libs stc,aui,core,base) -std=c++17
 *
 * Build (macOS):
 *   g++ wxEditor.cpp -o wxEditor $(wx-config --cxxflags --libs stc,aui,core,base) -std=c++17
 *
 * CMake: See CMakeLists.txt
 */

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

// ─────────────────────────────────────────────────────────────────────────────
// Constants & IDs
// ─────────────────────────────────────────────────────────────────────────────

enum {
    ID_NEW_FILE        = wxID_HIGHEST + 1,
    ID_OPEN_FILE,
    ID_OPEN_FOLDER,
    ID_SAVE_FILE,
    ID_SAVE_AS,
    ID_SAVE_ALL,
    ID_CLOSE_TAB,
    ID_CLOSE_ALL_TABS,
    ID_TOGGLE_SIDEBAR,
    ID_TOGGLE_WORDWRAP,
    ID_TOGGLE_WHITESPACE,
    ID_FIND,
    ID_FIND_NEXT,
    ID_FIND_PREV,
    ID_GOTO_LINE,
    ID_ZOOM_IN,
    ID_ZOOM_OUT,
    ID_ZOOM_RESET,
    ID_ABOUT,
    ID_TREE_CTRL,
    ID_NOTEBOOK,
    ID_FIND_TEXT,
    ID_FIND_BTN,
    ID_FIND_CLOSE,
};

// ─────────────────────────────────────────────────────────────────────────────
// VSCode-inspired dark colour palette
// ─────────────────────────────────────────────────────────────────────────────

namespace Colors {
    const wxColour BG          (30,  30,  30);   // editor background
    const wxColour BG_PANEL    (37,  37,  38);   // sidebar / panel bg
    const wxColour BG_ACTIVE   (45,  45,  48);   // active tab
    const wxColour BG_INACTIVE (37,  37,  38);   // inactive tab
    const wxColour FG          (212, 212, 212);  // default text
    const wxColour COMMENT     (106, 153,  85);  // green comments
    const wxColour KEYWORD     ( 86, 156, 214);  // blue keywords
    const wxColour STRING      (206, 145, 120);  // orange strings
    const wxColour NUMBER      (181, 206, 168);  // light-green numbers
    const wxColour PREPROC     (155, 155, 155);  // grey preprocessor
    const wxColour OPERATOR    (212, 212, 212);  // white operators
    const wxColour IDENTIFIER  (156, 220, 254);  // light-blue identifiers
    const wxColour LINENUMBER_BG(37, 37,  38);
    const wxColour LINENUMBER_FG(133,133, 133);
    const wxColour CARET       (0,  120, 212);   // VS blue caret line
    const wxColour CARET_FG    (255,255, 255);
    const wxColour SELECTION   (38,  79, 120);
    const wxColour BRACE_OK    (215, 215,  0);
    const wxColour BRACE_BAD   (255,  0,   0);
    const wxColour SIDEBAR_TEXT(187,187, 187);
    const wxColour SIDEBAR_SEL (37,  37,  38);
    const wxColour STATUSBAR_BG( 0,  122, 204);
    const wxColour STATUSBAR_FG(255,255, 255);
    const wxColour MODIFIED    (231, 151,  0);   // dot on modified tab
    const wxColour TYPE        (78,  201, 176);  // teal types/classes
    const wxColour FUNCTION    (220, 220, 170);  // yellow functions
}

// ─────────────────────────────────────────────────────────────────────────────
// Syntax language detection
// ─────────────────────────────────────────────────────────────────────────────

struct LangInfo {
    int         lexer;
    wxString    keywords0;
    wxString    keywords1;   // secondary (types, etc.)
    wxString    name;
};

static LangInfo DetectLanguage(const wxString& filename) {
    wxString ext = wxFileName(filename).GetExt().Lower();

    // C / C++
    if (ext == "c" || ext == "h" || ext == "cpp" || ext == "cxx" ||
        ext == "cc" || ext == "hpp" || ext == "hxx" || ext == "inl") {
        return { wxSTC_LEX_CPP,
            "alignas alignof and and_eq asm auto bitand bitor break case catch class compl "
            "concept const consteval constexpr constinit const_cast continue co_await co_return "
            "co_yield decltype default delete do dynamic_cast else enum explicit export extern "
            "false final for friend goto if import inline module namespace new noexcept not "
            "not_eq nullptr operator or or_eq override private protected public reinterpret_cast "
            "requires return sizeof static static_assert static_cast struct switch template "
            "this thread_local throw true try typedef typeid typename union using virtual void "
            "volatile while xor xor_eq",
            "bool char char8_t char16_t char32_t double float int long short signed unsigned "
            "wchar_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t "
            "size_t ptrdiff_t nullptr_t string wstring vector map unordered_map set list "
            "unique_ptr shared_ptr weak_ptr pair tuple optional variant any",
            "C/C++" };
    }
    // Python
    if (ext == "py" || ext == "pyw") {
        return { wxSTC_LEX_PYTHON,
            "False None True and as assert async await break class continue def del elif else "
            "except finally for from global if import in is lambda nonlocal not or pass raise "
            "return try while with yield",
            "int float str bool list dict set tuple bytes bytearray type object super print "
            "len range enumerate zip map filter sorted reversed abs min max sum open "
            "isinstance issubclass hasattr getattr setattr delattr callable repr str format "
            "self cls __init__ __str__ __repr__ __len__ __getitem__ __setitem__ __delitem__",
            "Python" };
    }
    // JavaScript / TypeScript
    if (ext == "js" || ext == "mjs" || ext == "cjs" || ext == "jsx" ||
        ext == "ts" || ext == "tsx") {
        return { wxSTC_LEX_CPP,   // CPP lexer works well for JS/TS
            "async await break case catch class const continue debugger default delete do else "
            "export extends false finally for from function get if import in instanceof let new "
            "null of return set static super switch this throw true try typeof undefined var "
            "void while with yield",
            "string number boolean symbol bigint object any unknown never void Array Object "
            "Promise Map Set Record Partial Required Readonly Pick Omit Exclude Extract "
            "NonNullable ReturnType InstanceType Parameters console document window",
            ext.StartsWith("t") ? "TypeScript" : "JavaScript" };
    }
    // HTML
    if (ext == "html" || ext == "htm" || ext == "xhtml") {
        return { wxSTC_LEX_HTML,
            "a abbr address article aside audio b blockquote body br button canvas caption "
            "cite code col colgroup data datalist dd del details dfn dialog div dl dt em "
            "embed fieldset figcaption figure footer form h1 h2 h3 h4 h5 h6 head header "
            "hr html i iframe img input ins kbd label legend li link main map mark meta nav "
            "noscript object ol optgroup option output p picture pre progress q rp rt ruby "
            "s samp script section select small source span strong style sub summary sup "
            "table tbody td template textarea tfoot th thead time title tr track u ul var "
            "video wbr",
            "class id style href src alt title type name value placeholder action method "
            "rel charset content lang media width height async defer crossorigin integrity",
            "HTML" };
    }
    // CSS
    if (ext == "css" || ext == "scss" || ext == "sass" || ext == "less") {
        return { wxSTC_LEX_CSS,
            "align-content align-items align-self animation animation-delay animation-direction "
            "animation-duration animation-fill-mode animation-iteration-count animation-name "
            "animation-play-state animation-timing-function appearance background background-attachment "
            "background-clip background-color background-image background-origin background-position "
            "background-repeat background-size border border-bottom border-bottom-color border-bottom-left-radius "
            "border-bottom-right-radius border-bottom-style border-bottom-width border-box border-collapse "
            "border-color border-image border-left border-left-color border-left-style border-left-width "
            "border-radius border-right border-right-color border-right-style border-right-width border-spacing "
            "border-style border-top border-top-color border-top-left-radius border-top-right-radius "
            "border-top-style border-top-width border-width bottom box-shadow box-sizing color columns "
            "content cursor direction display filter flex flex-direction flex-flow flex-grow flex-shrink "
            "flex-wrap float font font-family font-size font-size-adjust font-stretch font-style "
            "font-variant font-weight gap grid grid-area grid-auto-columns grid-auto-flow grid-auto-rows "
            "grid-column grid-column-end grid-column-gap grid-column-start grid-gap grid-row grid-row-end "
            "grid-row-gap grid-row-start grid-template grid-template-areas grid-template-columns "
            "grid-template-rows height justify-content justify-items justify-self left letter-spacing "
            "line-height list-style list-style-image list-style-position list-style-type margin margin-bottom "
            "margin-left margin-right margin-top max-height max-width min-height min-width object-fit "
            "opacity order outline overflow overflow-x overflow-y padding padding-bottom padding-left "
            "padding-right padding-top pointer-events position resize right text-align text-decoration "
            "text-indent text-overflow text-shadow text-transform top transform transform-origin transition "
            "transition-delay transition-duration transition-property transition-timing-function "
            "user-select vertical-align visibility white-space width word-break word-spacing word-wrap "
            "z-index",
            "", "CSS" };
    }
    // XML / SVG
    if (ext == "xml" || ext == "svg" || ext == "xaml" || ext == "plist" || ext == "rss") {
        return { wxSTC_LEX_XML, "", "", "XML" };
    }
    // JSON
    if (ext == "json" || ext == "jsonc") {
        return { wxSTC_LEX_JSON,
            "true false null", "", "JSON" };
    }
    // Rust
    if (ext == "rs") {
        return { wxSTC_LEX_RUST,
            "as async await break const continue crate dyn else enum extern false fn for if "
            "impl in let loop match mod move mut pub ref return self Self static struct super "
            "trait true type union unsafe use where while",
            "i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize f32 f64 bool char str "
            "String Vec HashMap HashSet Option Result Box Rc Arc Cell RefCell",
            "Rust" };
    }
    // Go
    if (ext == "go") {
        return { wxSTC_LEX_CPP,
            "break case chan const continue default defer else fallthrough for func go goto "
            "if import interface map package range return select struct switch type var",
            "bool byte complex64 complex128 error float32 float64 int int8 int16 int32 int64 "
            "rune string uint uint8 uint16 uint32 uint64 uintptr true false nil iota "
            "append cap close copy delete len make new panic print println recover",
            "Go" };
    }
    // Java / Kotlin
    if (ext == "java") {
        return { wxSTC_LEX_CPP,
            "abstract assert break case catch class const continue default do else enum extends "
            "false final finally for goto if implements import instanceof interface native new "
            "null package private protected public return static strictfp super switch synchronized "
            "this throw throws transient true try var volatile while",
            "boolean byte char double float int long short void String Object Integer Long Double "
            "Float Boolean Character Byte Short List ArrayList Map HashMap Set HashSet",
            "Java" };
    }
    // Shell / Bash
    if (ext == "sh" || ext == "bash" || ext == "zsh" || ext == "fish") {
        return { wxSTC_LEX_BASH,
            "if then else elif fi case esac while for in do done function select until "
            "break continue return exit local readonly export declare unset set alias unalias "
            "source eval exec true false",
            "", "Shell" };
    }
    // CMake
    if (ext == "cmake" || filename.Lower().EndsWith("cmakelists.txt")) {
        return { wxSTC_LEX_CMAKE,
            "if else elseif endif while endwhile foreach endforeach function endfunction macro "
            "endmacro return set list string file math message include find_package add_executable "
            "add_library target_link_libraries target_include_directories install configure_file "
            "project cmake_minimum_required option add_definitions add_subdirectory",
            "", "CMake" };
    }
    // Markdown
    if (ext == "md" || ext == "markdown") {
        return { wxSTC_LEX_MARKDOWN, "", "", "Markdown" };
    }
    // YAML
    if (ext == "yml" || ext == "yaml") {
        return { wxSTC_LEX_YAML,
            "true false null yes no on off", "", "YAML" };
    }
    // Lua
    if (ext == "lua") {
        return { wxSTC_LEX_LUA,
            "and break do else elseif end false for function goto if in local nil not or "
            "repeat return then true until while",
            "assert collectgarbage coroutine dofile error getmetatable io ipairs load loadfile "
            "math next os pairs pcall print rawequal rawget rawlen rawset require select "
            "setmetatable string table tostring tonumber type unpack xpcall",
            "Lua" };
    }
    // SQL
    if (ext == "sql") {
        return { wxSTC_LEX_SQL,
            "ADD ALL ALTER AND AS ASC BACKUP BETWEEN BY CASCADE CASE CHECK COLUMN CONSTRAINT "
            "CREATE CROSS DATABASE DEFAULT DELETE DESC DISTINCT DROP ELSE END EXEC EXISTS "
            "FOREIGN FROM FULL GRANT GROUP HAVING IN INDEX INNER INSERT INTO IS JOIN KEY LEFT "
            "LIKE LIMIT NOT NULL ON OR ORDER OUTER PRIMARY PROCEDURE REFERENCES REPLACE RIGHT "
            "ROWNUM SELECT SET TABLE THEN TOP TRUNCATE UNION UNIQUE UPDATE VALUES VIEW WHERE WITH",
            "INT INTEGER BIGINT SMALLINT TINYINT FLOAT DOUBLE DECIMAL NUMERIC REAL CHAR VARCHAR "
            "NCHAR NVARCHAR TEXT BLOB BINARY VARBINARY BOOLEAN BOOL DATE TIME DATETIME TIMESTAMP",
            "SQL" };
    }

    // Plain text / unknown
    return { wxSTC_LEX_NULL, "", "", "Plain Text" };
}

// ─────────────────────────────────────────────────────────────────────────────
// EditorPage – one wxStyledTextCtrl per tab
// ─────────────────────────────────────────────────────────────────────────────

class EditorPage : public wxStyledTextCtrl {
public:
    wxString filepath;   // full path; empty = new unsaved file
    bool     modified = false;

    EditorPage(wxWindow* parent, const wxString& path = wxEmptyString)
        : wxStyledTextCtrl(parent, wxID_ANY)
        , filepath(path)
    {
        ApplyTheme();
        if (!path.IsEmpty()) LoadFile(path);

        Bind(wxEVT_STC_CHANGE, &EditorPage::OnChange, this);
        Bind(wxEVT_STC_UPDATEUI, &EditorPage::OnUpdateUI, this);
        Bind(wxEVT_STC_CHARADDED, &EditorPage::OnCharAdded, this);
    }

    // ── Tab title: filename or "Untitled" (with leading "●" if modified) ──
    wxString GetTitle() const {
        wxString base = filepath.IsEmpty() ? "Untitled" : wxFileName(filepath).GetFullName();
        return (modified ? wxString(L"\u25cf ") : wxString("")) + base;
    }

    // ── Load file from disk ──
    bool LoadFile(const wxString& path) {
        filepath = path;
        wxFFile f(path, "rb");
        if (!f.IsOpened()) return false;
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
    bool SaveFile(const wxString& path = wxEmptyString) {
        wxString target = path.IsEmpty() ? filepath : path;
        if (target.IsEmpty()) return false;
        wxFFile f(target, "wb");
        if (!f.IsOpened()) return false;
        wxString content = GetText();
        f.Write(content);
        f.Close();
        if (!path.IsEmpty()) filepath = path;
        SetSavePoint();
        modified = false;
        ApplySyntax();
        return true;
    }

    // ── Apply VSCode dark theme + syntax ──
    void ApplyTheme() {
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
        MarkerDefine(wxSTC_MARKNUM_FOLDER,        wxSTC_MARK_BOXPLUS,  Colors::LINENUMBER_FG, Colors::BG_PANEL);
        MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN,    wxSTC_MARK_BOXMINUS, Colors::LINENUMBER_FG, Colors::BG_PANEL);
        MarkerDefine(wxSTC_MARKNUM_FOLDEREND,     wxSTC_MARK_BOXPLUSCONNECTED,  Colors::LINENUMBER_FG, Colors::BG_PANEL);
        MarkerDefine(wxSTC_MARKNUM_FOLDERSUB,     wxSTC_MARK_VLINE,    Colors::LINENUMBER_FG, Colors::BG_PANEL);
        MarkerDefine(wxSTC_MARKNUM_FOLDERTAIL,    wxSTC_MARK_LCORNER,  Colors::LINENUMBER_FG, Colors::BG_PANEL);
        MarkerDefine(wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_BOXMINUSCONNECTED, Colors::LINENUMBER_FG, Colors::BG_PANEL);
        MarkerDefine(wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_TCORNER,  Colors::LINENUMBER_FG, Colors::BG_PANEL);

        // Caret / selection
        SetCaretForeground(Colors::CARET_FG);
        SetCaretLineBackground(Colors::CARET);
        SetCaretLineVisible(true);
        SetCaretLineVisibleAlways(true);
        SetSelBackground(true, Colors::SELECTION);
        SetSelForeground(false, Colors::FG);

        // Brace matching
        StyleSetForeground(wxSTC_STYLE_BRACELIGHT, Colors::BRACE_OK);
        StyleSetForeground(wxSTC_STYLE_BRACEBAD,   Colors::BRACE_BAD);
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

    void ApplySyntax() {
        LangInfo lang = DetectLanguage(filepath);
        SetLexer(lang.lexer);

        // Clear all styles first to theme defaults
        for (int i = 0; i < wxSTC_STYLE_LASTPREDEFINED; ++i) {
            StyleSetBackground(i, Colors::BG);
            StyleSetForeground(i, Colors::FG);
        }

        // Apply language-specific styles
        switch (lang.lexer) {

        case wxSTC_LEX_CPP:
            SetKeyWords(0, lang.keywords0);
            SetKeyWords(1, lang.keywords1);
            StyleSetForeground(wxSTC_C_COMMENT,          Colors::COMMENT);
            StyleSetForeground(wxSTC_C_COMMENTLINE,      Colors::COMMENT);
            StyleSetForeground(wxSTC_C_COMMENTDOC,       Colors::COMMENT);
            StyleSetForeground(wxSTC_C_COMMENTLINEDOC,   Colors::COMMENT);
            StyleSetForeground(wxSTC_C_NUMBER,            Colors::NUMBER);
            StyleSetForeground(wxSTC_C_WORD,              Colors::KEYWORD);
            StyleSetBold(wxSTC_C_WORD, true);
            StyleSetForeground(wxSTC_C_WORD2,             Colors::TYPE);
            StyleSetForeground(wxSTC_C_STRING,            Colors::STRING);
            StyleSetForeground(wxSTC_C_CHARACTER,         Colors::STRING);
            StyleSetForeground(wxSTC_C_STRINGEOL,         Colors::STRING);
            StyleSetForeground(wxSTC_C_PREPROCESSOR,      Colors::PREPROC);
            StyleSetForeground(wxSTC_C_OPERATOR,          Colors::OPERATOR);
            StyleSetForeground(wxSTC_C_IDENTIFIER,        Colors::FG);
            StyleSetForeground(wxSTC_C_GLOBALCLASS,       Colors::TYPE);
            StyleSetForeground(wxSTC_C_REGEX,             Colors::STRING);
            StyleSetForeground(wxSTC_C_VERBATIM,          Colors::STRING);
            StyleSetForeground(wxSTC_C_TRIPLEVERBATIM,    Colors::STRING);
            StyleSetItalic(wxSTC_C_COMMENT, true);
            StyleSetItalic(wxSTC_C_COMMENTLINE, true);
            break;

        case wxSTC_LEX_PYTHON:
            SetKeyWords(0, lang.keywords0);
            SetKeyWords(1, lang.keywords1);
            StyleSetForeground(wxSTC_P_COMMENTLINE,  Colors::COMMENT);
            StyleSetForeground(wxSTC_P_COMMENTBLOCK, Colors::COMMENT);
            StyleSetForeground(wxSTC_P_NUMBER,       Colors::NUMBER);
            StyleSetForeground(wxSTC_P_WORD,         Colors::KEYWORD);
            StyleSetBold(wxSTC_P_WORD, true);
            StyleSetForeground(wxSTC_P_WORD2,        Colors::TYPE);
            StyleSetForeground(wxSTC_P_STRING,       Colors::STRING);
            StyleSetForeground(wxSTC_P_CHARACTER,    Colors::STRING);
            StyleSetForeground(wxSTC_P_TRIPLE,       Colors::STRING);
            StyleSetForeground(wxSTC_P_TRIPLEDOUBLE, Colors::STRING);
            StyleSetForeground(wxSTC_P_STRINGEOL,    Colors::STRING);
            StyleSetForeground(wxSTC_P_OPERATOR,     Colors::OPERATOR);
            StyleSetForeground(wxSTC_P_IDENTIFIER,   Colors::FG);
            StyleSetForeground(wxSTC_P_CLASSNAME,    Colors::TYPE);
            StyleSetForeground(wxSTC_P_DEFNAME,      Colors::FUNCTION);
            StyleSetForeground(wxSTC_P_DECORATOR,    Colors::PREPROC);
            StyleSetItalic(wxSTC_P_COMMENTLINE, true);
            break;

        case wxSTC_LEX_HTML:
        case wxSTC_LEX_XML:
            StyleSetForeground(wxSTC_H_TAG,           Colors::KEYWORD);
            StyleSetForeground(wxSTC_H_TAGUNKNOWN,    Colors::KEYWORD);
            StyleSetForeground(wxSTC_H_ATTRIBUTE,     Colors::IDENTIFIER);
            StyleSetForeground(wxSTC_H_ATTRIBUTEUNKNOWN, Colors::IDENTIFIER);
            StyleSetForeground(wxSTC_H_NUMBER,        Colors::NUMBER);
            StyleSetForeground(wxSTC_H_DOUBLESTRING,  Colors::STRING);
            StyleSetForeground(wxSTC_H_SINGLESTRING,  Colors::STRING);
            StyleSetForeground(wxSTC_H_COMMENT,       Colors::COMMENT);
            StyleSetForeground(wxSTC_H_ENTITY,        Colors::TYPE);
            StyleSetForeground(wxSTC_H_XMLSTART,      Colors::PREPROC);
            StyleSetForeground(wxSTC_H_XMLEND,        Colors::PREPROC);
            SetKeyWords(0, lang.keywords0);
            SetKeyWords(1, lang.keywords1);
            break;

        case wxSTC_LEX_JSON:
            StyleSetForeground(wxSTC_JSON_STRING,   Colors::STRING);
            StyleSetForeground(wxSTC_JSON_NUMBER,   Colors::NUMBER);
            StyleSetForeground(wxSTC_JSON_KEYWORD,  Colors::KEYWORD);
            StyleSetForeground(wxSTC_JSON_OPERATOR, Colors::OPERATOR);
            StyleSetForeground(wxSTC_JSON_PROPERTYNAME, Colors::IDENTIFIER);
            StyleSetForeground(wxSTC_JSON_LINECOMMENT,  Colors::COMMENT);
            StyleSetForeground(wxSTC_JSON_BLOCKCOMMENT, Colors::COMMENT);
            StyleSetForeground(wxSTC_JSON_ERROR,    Colors::BRACE_BAD);
            SetKeyWords(0, lang.keywords0);
            break;

        case wxSTC_LEX_RUST:
            StyleSetForeground(wxSTC_RUST_COMMENTLINE,   Colors::COMMENT);
            StyleSetForeground(wxSTC_RUST_COMMENTBLOCK,  Colors::COMMENT);
            StyleSetForeground(wxSTC_RUST_WORD,          Colors::KEYWORD);
            StyleSetForeground(wxSTC_RUST_WORD2,         Colors::TYPE);
            StyleSetForeground(wxSTC_RUST_NUMBER,        Colors::NUMBER);
            StyleSetForeground(wxSTC_RUST_STRING,        Colors::STRING);
            StyleSetForeground(wxSTC_RUST_CHARACTER,     Colors::STRING);
            StyleSetForeground(wxSTC_RUST_OPERATOR,      Colors::OPERATOR);
            StyleSetForeground(wxSTC_RUST_IDENTIFIER,    Colors::FG);
            StyleSetForeground(wxSTC_RUST_LIFETIME,      Colors::TYPE);
            StyleSetForeground(wxSTC_RUST_MACRO,         Colors::PREPROC);
            SetKeyWords(0, lang.keywords0);
            SetKeyWords(1, lang.keywords1);
            break;

        case wxSTC_LEX_BASH:
            StyleSetForeground(wxSTC_SH_COMMENTLINE, Colors::COMMENT);
            StyleSetForeground(wxSTC_SH_NUMBER,      Colors::NUMBER);
            StyleSetForeground(wxSTC_SH_WORD,        Colors::KEYWORD);
            StyleSetForeground(wxSTC_SH_STRING,      Colors::STRING);
            StyleSetForeground(wxSTC_SH_CHARACTER,   Colors::STRING);
            StyleSetForeground(wxSTC_SH_OPERATOR,    Colors::OPERATOR);
            StyleSetForeground(wxSTC_SH_IDENTIFIER,  Colors::FG);
            StyleSetForeground(wxSTC_SH_PARAM,       Colors::IDENTIFIER);
            SetKeyWords(0, lang.keywords0);
            break;

        case wxSTC_LEX_SQL:
            StyleSetForeground(wxSTC_SQL_COMMENT,      Colors::COMMENT);
            StyleSetForeground(wxSTC_SQL_COMMENTLINE,  Colors::COMMENT);
            StyleSetForeground(wxSTC_SQL_COMMENTDOC,   Colors::COMMENT);
            StyleSetForeground(wxSTC_SQL_NUMBER,       Colors::NUMBER);
            StyleSetForeground(wxSTC_SQL_WORD,         Colors::KEYWORD);
            StyleSetForeground(wxSTC_SQL_WORD2,        Colors::TYPE);
            StyleSetForeground(wxSTC_SQL_STRING,       Colors::STRING);
            StyleSetForeground(wxSTC_SQL_CHARACTER,    Colors::STRING);
            StyleSetForeground(wxSTC_SQL_OPERATOR,     Colors::OPERATOR);
            StyleSetForeground(wxSTC_SQL_IDENTIFIER,   Colors::FG);
            SetKeyWords(0, lang.keywords0);
            SetKeyWords(1, lang.keywords1);
            break;

        case wxSTC_LEX_CSS:
            StyleSetForeground(wxSTC_CSS_COMMENT,      Colors::COMMENT);
            StyleSetForeground(wxSTC_CSS_TAG,          Colors::KEYWORD);
            StyleSetForeground(wxSTC_CSS_CLASS,        Colors::FUNCTION);
            StyleSetForeground(wxSTC_CSS_PSEUDOCLASS,  Colors::TYPE);
            StyleSetForeground(wxSTC_CSS_UNKNOWN_PSEUDOCLASS, Colors::TYPE);
            StyleSetForeground(wxSTC_CSS_OPERATOR,     Colors::OPERATOR);
            StyleSetForeground(wxSTC_CSS_IDENTIFIER,   Colors::IDENTIFIER);
            StyleSetForeground(wxSTC_CSS_UNKNOWN_IDENTIFIER, Colors::FG);
            StyleSetForeground(wxSTC_CSS_VALUE,        Colors::STRING);
            StyleSetForeground(wxSTC_CSS_IMPORTANT,    Colors::KEYWORD);
            StyleSetForeground(wxSTC_CSS_DIRECTIVE,    Colors::PREPROC);
            StyleSetForeground(wxSTC_CSS_DOUBLESTRING, Colors::STRING);
            StyleSetForeground(wxSTC_CSS_SINGLESTRING, Colors::STRING);
            StyleSetForeground(wxSTC_CSS_ID,           Colors::FUNCTION);
            StyleSetForeground(wxSTC_CSS_ATTRIBUTE,    Colors::IDENTIFIER);
            SetKeyWords(0, lang.keywords0);
            break;

        case wxSTC_LEX_LUA:
            StyleSetForeground(wxSTC_LUA_COMMENT,      Colors::COMMENT);
            StyleSetForeground(wxSTC_LUA_COMMENTLINE,  Colors::COMMENT);
            StyleSetForeground(wxSTC_LUA_COMMENTDOC,   Colors::COMMENT);
            StyleSetForeground(wxSTC_LUA_NUMBER,       Colors::NUMBER);
            StyleSetForeground(wxSTC_LUA_WORD,         Colors::KEYWORD);
            StyleSetForeground(wxSTC_LUA_WORD2,        Colors::TYPE);
            StyleSetForeground(wxSTC_LUA_STRING,       Colors::STRING);
            StyleSetForeground(wxSTC_LUA_CHARACTER,    Colors::STRING);
            StyleSetForeground(wxSTC_LUA_OPERATOR,     Colors::OPERATOR);
            StyleSetForeground(wxSTC_LUA_IDENTIFIER,   Colors::FG);
            SetKeyWords(0, lang.keywords0);
            SetKeyWords(1, lang.keywords1);
            break;

        case wxSTC_LEX_YAML:
            StyleSetForeground(wxSTC_YAML_COMMENT,   Colors::COMMENT);
            StyleSetForeground(wxSTC_YAML_IDENTIFIER, Colors::IDENTIFIER);
            StyleSetForeground(wxSTC_YAML_KEYWORD,   Colors::KEYWORD);
            StyleSetForeground(wxSTC_YAML_NUMBER,    Colors::NUMBER);
            StyleSetForeground(wxSTC_YAML_REFERENCE, Colors::TYPE);
            StyleSetForeground(wxSTC_YAML_DOCUMENT,  Colors::PREPROC);
            StyleSetForeground(wxSTC_YAML_TEXT,      Colors::STRING);
            StyleSetForeground(wxSTC_YAML_ERROR,     Colors::BRACE_BAD);
            SetKeyWords(0, lang.keywords0);
            break;

        case wxSTC_LEX_MARKDOWN:
            StyleSetForeground(wxSTC_MARKDOWN_STRONG1,   Colors::KEYWORD);
            StyleSetForeground(wxSTC_MARKDOWN_STRONG2,   Colors::KEYWORD);
            StyleSetForeground(wxSTC_MARKDOWN_EM1,       Colors::TYPE);
            StyleSetForeground(wxSTC_MARKDOWN_EM2,       Colors::TYPE);
            StyleSetForeground(wxSTC_MARKDOWN_HEADER1,   Colors::FUNCTION);
            StyleSetForeground(wxSTC_MARKDOWN_HEADER2,   Colors::FUNCTION);
            StyleSetForeground(wxSTC_MARKDOWN_HEADER3,   Colors::FUNCTION);
            StyleSetForeground(wxSTC_MARKDOWN_HEADER4,   Colors::FUNCTION);
            StyleSetForeground(wxSTC_MARKDOWN_HEADER5,   Colors::FUNCTION);
            StyleSetForeground(wxSTC_MARKDOWN_HEADER6,   Colors::FUNCTION);
            StyleSetForeground(wxSTC_MARKDOWN_ULIST_ITEM, Colors::IDENTIFIER);
            StyleSetForeground(wxSTC_MARKDOWN_OLIST_ITEM, Colors::IDENTIFIER);
            StyleSetForeground(wxSTC_MARKDOWN_BLOCKQUOTE, Colors::COMMENT);
            StyleSetForeground(wxSTC_MARKDOWN_STRIKEOUT,  Colors::PREPROC);
            StyleSetForeground(wxSTC_MARKDOWN_HRULE,      Colors::OPERATOR);
            StyleSetForeground(wxSTC_MARKDOWN_LINK,       Colors::STRING);
            StyleSetForeground(wxSTC_MARKDOWN_CODE,       Colors::TYPE);
            StyleSetForeground(wxSTC_MARKDOWN_CODE2,      Colors::TYPE);
            StyleSetForeground(wxSTC_MARKDOWN_CODEBK,     Colors::TYPE);
            StyleSetBold(wxSTC_MARKDOWN_STRONG1, true);
            StyleSetBold(wxSTC_MARKDOWN_STRONG2, true);
            StyleSetBold(wxSTC_MARKDOWN_HEADER1, true);
            StyleSetBold(wxSTC_MARKDOWN_HEADER2, true);
            StyleSetItalic(wxSTC_MARKDOWN_EM1, true);
            StyleSetItalic(wxSTC_MARKDOWN_EM2, true);
            break;

        case wxSTC_LEX_CMAKE:
            StyleSetForeground(wxSTC_CMAKE_COMMENT,   Colors::COMMENT);
            StyleSetForeground(wxSTC_CMAKE_STRINGDQ,  Colors::STRING);
            StyleSetForeground(wxSTC_CMAKE_COMMANDS,  Colors::KEYWORD);
            StyleSetForeground(wxSTC_CMAKE_PARAMETERS, Colors::IDENTIFIER);
            StyleSetForeground(wxSTC_CMAKE_VARIABLE,  Colors::TYPE);
            StyleSetForeground(wxSTC_CMAKE_USERDEFINED, Colors::FUNCTION);
            SetKeyWords(0, lang.keywords0);
            break;

        default:
            break;
        }
    }

private:
    void OnChange(wxStyledTextEvent&) {
        modified = true;
    }

    void OnUpdateUI(wxStyledTextEvent&) {
        // Brace matching
        int pos = GetCurrentPos();
        int ch  = GetCharAt(pos - 1);
        const wxString braces = "()[]{}<>";
        if (braces.Find((wxChar)ch) != wxNOT_FOUND) {
            int other = BraceMatch(pos - 1);
            if (other != wxSTC_INVALID_POSITION)
                BraceHighlight(pos - 1, other);
            else
                BraceBadLight(pos - 1);
        } else {
            BraceHighlight(wxSTC_INVALID_POSITION, wxSTC_INVALID_POSITION);
        }
    }

    void OnCharAdded(wxStyledTextEvent& evt) {
        // Auto-indent: match previous line's indentation
        char ch = (char)evt.GetKey();
        if (ch == '\n') {
            int line = GetCurrentLine();
            if (line > 0) {
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
        const char opens[]  = "([{\"'";
        const char closes[] = ")]}\"'";
        for (int i = 0; opens[i]; ++i) {
            if (ch == opens[i]) {
                InsertText(GetCurrentPos(), wxString(closes[i]));
                break;
            }
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PathData – wxTreeItemData carrying a filesystem path
// (wxStringClientData is NOT a wxTreeItemData subclass, so we roll our own)
// ─────────────────────────────────────────────────────────────────────────────

class PathData : public wxTreeItemData {
public:
    explicit PathData(const wxString& path) : m_path(path) {}
    const wxString& GetPath() const { return m_path; }
private:
    wxString m_path;
};

// ─────────────────────────────────────────────────────────────────────────────
// FileTreeCtrl – folder explorer pane
// ─────────────────────────────────────────────────────────────────────────────

class FileTreeCtrl : public wxTreeCtrl {
public:
    wxString rootPath;

    FileTreeCtrl(wxWindow* parent)
        : wxTreeCtrl(parent, ID_TREE_CTRL, wxDefaultPosition, wxDefaultSize,
                     wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_HIDE_ROOT |
                     wxTR_SINGLE | wxNO_BORDER)
    {
        SetBackgroundColour(Colors::BG_PANEL);
        SetForegroundColour(Colors::SIDEBAR_TEXT);

        // Build image list
        wxImageList* il = new wxImageList(16, 16, true);
        il->Add(wxArtProvider::GetIcon(wxART_FOLDER,       wxART_OTHER, wxSize(16,16)));
        il->Add(wxArtProvider::GetIcon(wxART_FOLDER_OPEN,  wxART_OTHER, wxSize(16,16)));
        il->Add(wxArtProvider::GetIcon(wxART_NORMAL_FILE,  wxART_OTHER, wxSize(16,16)));
        AssignImageList(il);
    }

    void LoadFolder(const wxString& path) {
        DeleteAllItems();
        rootPath = path;
        wxTreeItemId root = AddRoot(path, 0, 1);
        PopulateDir(root, path);
       /// Expand(root);
    }

    // Return full path of selected file item (empty string if it's a directory)
    wxString GetSelectedFilePath() {
        wxTreeItemId sel = GetSelection();
        if (!sel.IsOk()) return wxEmptyString;
        if (ItemHasChildren(sel)) return wxEmptyString; // directory
        PathData* data = dynamic_cast<PathData*>(GetItemData(sel));
        return data ? data->GetPath() : wxString();
    }

private:
    void PopulateDir(wxTreeItemId parent, const wxString& path) {
        wxDir dir(path);
        if (!dir.IsOpened()) return;

        // First: subdirectories
        wxString name;
        if (dir.GetFirst(&name, wxEmptyString, wxDIR_DIRS)) {
            do {
                if (name.StartsWith(".")) continue; // skip hidden
                wxString full = path + wxFileName::GetPathSeparator() + name;
                wxTreeItemId child = AppendItem(parent, name, 0, 1,
                                                new PathData(full));
                SetItemTextColour(child, Colors::SIDEBAR_TEXT);
                AppendItem(child, "<loading>"); // placeholder so expand arrow shows
            } while (dir.GetNext(&name));
        }

        // Then: files
        if (dir.GetFirst(&name, wxEmptyString, wxDIR_FILES)) {
            do {
                if (name.StartsWith(".")) continue;
                wxString full = path + wxFileName::GetPathSeparator() + name;
                wxTreeItemId child = AppendItem(parent, name, 2, 2,
                                                new PathData(full));
                SetItemTextColour(child, Colors::SIDEBAR_TEXT);
            } while (dir.GetNext(&name));
        }
    }

public:
    // Expand on demand (lazy loading)
    void OnItemExpanding(wxTreeEvent& evt) {
        wxTreeItemId item = evt.GetItem();
        wxTreeItemIdValue cookie;
        wxTreeItemId first = GetFirstChild(item, cookie);
        if (first.IsOk() && GetItemText(first) == "<loading>") {
            Delete(first);
            PathData* data = dynamic_cast<PathData*>(GetItemData(item));
            if (data) PopulateDir(item, data->GetPath());
        }
    }

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(FileTreeCtrl, wxTreeCtrl)
    EVT_TREE_ITEM_EXPANDING(ID_TREE_CTRL, FileTreeCtrl::OnItemExpanding)
wxEND_EVENT_TABLE()

// ─────────────────────────────────────────────────────────────────────────────
// FindBar – inline find toolbar at the bottom
// ─────────────────────────────────────────────────────────────────────────────

class FindBar : public wxPanel {
public:
    wxTextCtrl*  textCtrl;
    wxCheckBox*  caseChk;
    wxCheckBox*  wholeChk;

    FindBar(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        SetBackgroundColour(Colors::BG_PANEL);
        auto* sizer = new wxBoxSizer(wxHORIZONTAL);

        auto label = new wxStaticText(this, wxID_ANY, "Find:");
        label->SetForegroundColour(Colors::FG);
        sizer->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 6);

        textCtrl = new wxTextCtrl(this, ID_FIND_TEXT, wxEmptyString,
                                  wxDefaultPosition, wxSize(220, -1),
                                  wxTE_PROCESS_ENTER);
        textCtrl->SetBackgroundColour(wxColour(60, 60, 60));
        textCtrl->SetForegroundColour(Colors::FG);
        sizer->Add(textCtrl, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

        auto btnPrev = new wxButton(this, ID_FIND_PREV,  L"\u25b2", wxDefaultPosition, wxSize(28,24));
        auto btnNext = new wxButton(this, ID_FIND_BTN,   L"\u25bc", wxDefaultPosition, wxSize(28,24));
        auto btnClose= new wxButton(this, ID_FIND_CLOSE, L"\u2715", wxDefaultPosition, wxSize(24,24));
        for (auto* b : {btnPrev, btnNext, btnClose}) {
            b->SetBackgroundColour(Colors::BG_PANEL);
            b->SetForegroundColour(Colors::FG);
        }
        sizer->Add(btnPrev,  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
        sizer->Add(btnNext,  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

        caseChk  = new wxCheckBox(this, wxID_ANY, "Match case");
        wholeChk = new wxCheckBox(this, wxID_ANY, "Whole word");
        caseChk ->SetForegroundColour(Colors::FG);
        wholeChk->SetForegroundColour(Colors::FG);
        caseChk ->SetBackgroundColour(Colors::BG_PANEL);
        wholeChk->SetBackgroundColour(Colors::BG_PANEL);
        sizer->Add(caseChk,  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        sizer->Add(wholeChk, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

        sizer->AddStretchSpacer();
        sizer->Add(btnClose, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

        SetSizer(sizer);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// MainFrame
// ─────────────────────────────────────────────────────────────────────────────

class MainFrame : public wxFrame {
    wxSplitterWindow* m_splitter   = nullptr;
    FileTreeCtrl*     m_tree       = nullptr;
    wxPanel*          m_editorPane = nullptr;
    wxAuiNotebook*    m_notebook   = nullptr;
    FindBar*          m_findBar    = nullptr;
    wxStatusBar*      m_statusBar  = nullptr;

    int               m_untitledCount = 0;

public:
    MainFrame()
        : wxFrame(nullptr, wxID_ANY, "wxEditor",
                  wxDefaultPosition, wxSize(1280, 780))
    {
        SetBackgroundColour(Colors::BG);
        BuildUI();
        BuildMenuBar();
        BuildStatusBar();
        SetupAccelerators();

        Centre();
        Show();

        // Open a blank tab on start
        NewTab();
    }

private:

    // ── UI construction ──────────────────────────────────────────────────────

    void BuildUI() {
        m_splitter = new wxSplitterWindow(this, wxID_ANY,
                                          wxDefaultPosition, wxDefaultSize,
                                          wxSP_3DSASH | wxSP_LIVE_UPDATE);
        m_splitter->SetBackgroundColour(Colors::BG_PANEL);

        // ── Sidebar (tree) ──
        auto* sidePanel = new wxPanel(m_splitter, wxID_ANY);
        sidePanel->SetBackgroundColour(Colors::BG_PANEL);
        auto* sideSizer = new wxBoxSizer(wxVERTICAL);

        auto* sideHeader = new wxPanel(sidePanel, wxID_ANY, wxDefaultPosition, wxSize(-1,28));
        sideHeader->SetBackgroundColour(wxColour(37, 37, 38));
        auto* hs = new wxBoxSizer(wxHORIZONTAL);
        auto* explorerLabel = new wxStaticText(sideHeader, wxID_ANY, "EXPLORER",
                                               wxDefaultPosition, wxDefaultSize);
        explorerLabel->SetForegroundColour(wxColour(187, 187, 187));
        explorerLabel->SetFont(explorerLabel->GetFont().Scale(0.8));
        hs->Add(explorerLabel, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 12);
        sideHeader->SetSizer(hs);
        sideSizer->Add(sideHeader, 0, wxEXPAND);

        m_tree = new FileTreeCtrl(sidePanel);
        sideSizer->Add(m_tree, 1, wxEXPAND);
        sidePanel->SetSizer(sideSizer);

        // ── Editor area ──
        m_editorPane = new wxPanel(m_splitter, wxID_ANY);
        m_editorPane->SetBackgroundColour(Colors::BG);
        auto* edSizer = new wxBoxSizer(wxVERTICAL);

        // Notebook (tabs)
        long nbStyle =
            wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ALL_TABS |
            wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS;
        m_notebook = new wxAuiNotebook(m_editorPane, ID_NOTEBOOK,
                                       wxDefaultPosition, wxDefaultSize, nbStyle);
        m_notebook->SetBackgroundColour(Colors::BG_ACTIVE);
        edSizer->Add(m_notebook, 1, wxEXPAND);

        // Find bar (hidden initially)
        m_findBar = new FindBar(m_editorPane);
        m_findBar->Hide();
        edSizer->Add(m_findBar, 0, wxEXPAND);

        m_editorPane->SetSizer(edSizer);

        m_splitter->SplitVertically(sidePanel, m_editorPane, 240);
        m_splitter->SetMinimumPaneSize(100);

        auto* frameSizer = new wxBoxSizer(wxVERTICAL);
        frameSizer->Add(m_splitter, 1, wxEXPAND);
        SetSizer(frameSizer);

        // Events
        m_notebook->Bind(wxEVT_AUINOTEBOOK_PAGE_CHANGED, &MainFrame::OnTabChanged, this);
        m_notebook->Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE,   &MainFrame::OnTabClose,   this);

        m_tree->Bind(wxEVT_TREE_ITEM_ACTIVATED, &MainFrame::OnTreeItemActivated, this);

        m_findBar->textCtrl->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&){ FindNext(true); });
        Bind(wxEVT_BUTTON, [this](wxCommandEvent&){ FindNext(true);  }, ID_FIND_BTN);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent&){ FindNext(false); }, ID_FIND_PREV);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent&){ CloseFindBar();  }, ID_FIND_CLOSE);
    }

    void BuildMenuBar() {
        auto* mb = new wxMenuBar();
        mb->SetBackgroundColour(Colors::BG_PANEL);

        // File
        auto* file = new wxMenu();
        file->Append(ID_NEW_FILE,     "&New\tCtrl+N");
        file->Append(ID_OPEN_FILE,    "&Open File...\tCtrl+O");
        file->Append(ID_OPEN_FOLDER,  "Open &Folder...\tCtrl+Shift+O");
        file->AppendSeparator();
        file->Append(ID_SAVE_FILE,    "&Save\tCtrl+S");
        file->Append(ID_SAVE_AS,      "Save &As...\tCtrl+Shift+S");
        file->Append(ID_SAVE_ALL,     "Save A&ll\tCtrl+Alt+S");
        file->AppendSeparator();
        file->Append(ID_CLOSE_TAB,    "&Close Tab\tCtrl+W");
        file->Append(ID_CLOSE_ALL_TABS,"Close All Tabs");
        file->AppendSeparator();
        file->Append(wxID_EXIT,       "E&xit\tAlt+F4");
        mb->Append(file, "&File");

        // Edit
        auto* edit = new wxMenu();
        edit->Append(wxID_UNDO,   "&Undo\tCtrl+Z");
        edit->Append(wxID_REDO,   "&Redo\tCtrl+Y");
        edit->AppendSeparator();
        edit->Append(wxID_CUT,    "Cu&t\tCtrl+X");
        edit->Append(wxID_COPY,   "&Copy\tCtrl+C");
        edit->Append(wxID_PASTE,  "&Paste\tCtrl+V");
        edit->Append(wxID_SELECTALL, "Select &All\tCtrl+A");
        edit->AppendSeparator();
        edit->Append(ID_FIND,     "&Find...\tCtrl+F");
        edit->Append(ID_FIND_NEXT,"Find &Next\tF3");
        edit->Append(ID_FIND_PREV,"Find &Previous\tShift+F3");
        edit->Append(ID_GOTO_LINE,"Go to &Line...\tCtrl+G");
        mb->Append(edit, "&Edit");

        // View
        auto* view = new wxMenu();
        view->Append(ID_TOGGLE_SIDEBAR,    "Toggle &Sidebar\tCtrl+B");
        view->AppendSeparator();
        view->AppendCheckItem(ID_TOGGLE_WORDWRAP,   "&Word Wrap\tAlt+Z");
        view->AppendCheckItem(ID_TOGGLE_WHITESPACE, "Show &Whitespace");
        view->AppendSeparator();
        view->Append(ID_ZOOM_IN,   "Zoom &In\tCtrl+=");
        view->Append(ID_ZOOM_OUT,  "Zoom &Out\tCtrl+-");
        view->Append(ID_ZOOM_RESET,"Reset &Zoom\tCtrl+0");
        mb->Append(view, "&View");

        // Help
        auto* help = new wxMenu();
        help->Append(ID_ABOUT, "&About wxEditor");
        mb->Append(help, "&Help");

        SetMenuBar(mb);

        // Bind
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ NewTab(); },                  ID_NEW_FILE);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ OpenFile(); },                ID_OPEN_FILE);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ OpenFolder(); },              ID_OPEN_FOLDER);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ SaveCurrentTab(); },          ID_SAVE_FILE);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ SaveAs(); },                  ID_SAVE_AS);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ SaveAll(); },                 ID_SAVE_ALL);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ CloseCurrentTab(); },         ID_CLOSE_TAB);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ CloseAllTabs(); },            ID_CLOSE_ALL_TABS);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ Close(); },                   wxID_EXIT);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ DispatchEdit(wxID_UNDO); },   wxID_UNDO);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ DispatchEdit(wxID_REDO); },   wxID_REDO);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ DispatchEdit(wxID_CUT); },    wxID_CUT);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ DispatchEdit(wxID_COPY); },   wxID_COPY);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ DispatchEdit(wxID_PASTE); },  wxID_PASTE);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ DispatchEdit(wxID_SELECTALL); }, wxID_SELECTALL);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ ShowFindBar(); },             ID_FIND);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ FindNext(true); },            ID_FIND_NEXT);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ FindNext(false); },           ID_FIND_PREV);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ GoToLine(); },                ID_GOTO_LINE);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ ToggleSidebar(); },           ID_TOGGLE_SIDEBAR);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ ToggleWordWrap(); },          ID_TOGGLE_WORDWRAP);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ ToggleWhitespace(); },        ID_TOGGLE_WHITESPACE);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ ZoomEditor(+1); },            ID_ZOOM_IN);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ ZoomEditor(-1); },            ID_ZOOM_OUT);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ ZoomEditor(0); },             ID_ZOOM_RESET);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ ShowAbout(); },               ID_ABOUT);

        Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnCloseWindow, this);
    }

    void BuildStatusBar() {
        m_statusBar = CreateStatusBar(4);
        m_statusBar->SetBackgroundColour(Colors::STATUSBAR_BG);
        m_statusBar->SetForegroundColour(Colors::STATUSBAR_FG);
        const int widths[] = { -1, 140, 120, 100 };
        m_statusBar->SetStatusWidths(4, widths);
        m_statusBar->SetStatusText("Ready", 0);
        m_statusBar->SetStatusText("Ln 1, Col 1", 1);
        m_statusBar->SetStatusText("UTF-8", 2);
        m_statusBar->SetStatusText("Plain Text", 3);
    }

    void SetupAccelerators() {
        // Additional accelerators not covered by menu shortcuts
        wxAcceleratorEntry entries[] = {
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'+', ID_ZOOM_IN),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD_ADD, ID_ZOOM_IN),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'-', ID_ZOOM_OUT),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD_SUBTRACT, ID_ZOOM_OUT),
        };
        SetAcceleratorTable(wxAcceleratorTable(4, entries));
    }

    // ── Tab helpers ──────────────────────────────────────────────────────────

    EditorPage* CurrentPage() {
        int sel = m_notebook->GetSelection();
        if (sel == wxNOT_FOUND) return nullptr;
        return static_cast<EditorPage*>(m_notebook->GetPage(sel));
    }

    void NewTab(const wxString& filepath = wxEmptyString) {
        auto* page = new EditorPage(m_notebook, filepath);
        wxString title = filepath.IsEmpty()
            ? wxString::Format("Untitled-%d", ++m_untitledCount)
            : wxFileName(filepath).GetFullName();
        m_notebook->AddPage(page, title, true);
        page->SetFocus();
        UpdateStatusBar(page);
        page->Bind(wxEVT_STC_UPDATEUI, [this, page](wxStyledTextEvent& e) {
            UpdateStatusBar(page);
            e.Skip();
        });
        page->Bind(wxEVT_STC_CHANGE, [this, page](wxStyledTextEvent& e) {
            int idx = m_notebook->GetPageIndex(page);
            if (idx != wxNOT_FOUND)
                m_notebook->SetPageText(idx, page->GetTitle());
            e.Skip();
        });
    }

    void OpenFileInTab(const wxString& path) {
        // Check if already open
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i) {
            auto* p = static_cast<EditorPage*>(m_notebook->GetPage(i));
            if (p->filepath == path) {
                m_notebook->SetSelection(i);
                return;
            }
        }
        NewTab(path);
    }

    void UpdateStatusBar(EditorPage* page) {
        if (!page) return;
        int line = page->GetCurrentLine() + 1;
        int col  = page->GetColumn(page->GetCurrentPos()) + 1;
        m_statusBar->SetStatusText(wxString::Format("Ln %d, Col %d", line, col), 1);
        m_statusBar->SetStatusText("UTF-8", 2);
        LangInfo lang = DetectLanguage(page->filepath);
        m_statusBar->SetStatusText(lang.name, 3);
        wxString title = page->filepath.IsEmpty() ? "Untitled" : page->filepath;
        m_statusBar->SetStatusText(title, 0);
    }

    void UpdateTabTitle(EditorPage* page) {
        int idx = m_notebook->GetPageIndex(page);
        if (idx != wxNOT_FOUND)
            m_notebook->SetPageText(idx, page->GetTitle());
    }

    // ── File operations ──────────────────────────────────────────────────────

    void NewFile() { NewTab(); }

    void OpenFile() {
        wxFileDialog dlg(this, "Open File", wxEmptyString, wxEmptyString,
            "All files (*.*)|*.*|"
            "C/C++ (*.c;*.cpp;*.h;*.hpp)|*.c;*.cpp;*.h;*.hpp|"
            "Python (*.py)|*.py|"
            "JavaScript/TypeScript (*.js;*.ts;*.jsx;*.tsx)|*.js;*.ts;*.jsx;*.tsx|"
            "Rust (*.rs)|*.rs|"
            "HTML (*.html;*.htm)|*.html;*.htm|"
            "CSS (*.css;*.scss)|*.css;*.scss|"
            "JSON (*.json)|*.json|"
            "Markdown (*.md)|*.md|"
            "Shell (*.sh;*.bash)|*.sh;*.bash|"
            "YAML (*.yml;*.yaml)|*.yml;*.yaml",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);

        if (dlg.ShowModal() == wxID_OK) {
            wxArrayString paths;
            dlg.GetPaths(paths);
            for (const auto& p : paths)
                OpenFileInTab(p);
        }
    }

    void OpenFolder() {
        wxDirDialog dlg(this, "Open Folder", wxEmptyString,
                        wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
        if (dlg.ShowModal() == wxID_OK)
            m_tree->LoadFolder(dlg.GetPath());
    }

    bool SaveCurrentTab() {
        auto* page = CurrentPage();
        if (!page) return false;
        if (page->filepath.IsEmpty()) return SaveAs();
        bool ok = page->SaveFile();
        if (ok) UpdateTabTitle(page);
        return ok;
    }

    bool SaveAs() {
        auto* page = CurrentPage();
        if (!page) return false;
        wxFileDialog dlg(this, "Save As", wxEmptyString,
                         page->filepath.IsEmpty() ? "Untitled.txt"
                                                   : wxFileName(page->filepath).GetFullName(),
                         "All files (*.*)|*.*",
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK) return false;
        bool ok = page->SaveFile(dlg.GetPath());
        if (ok) UpdateTabTitle(page);
        return ok;
    }

    void SaveAll() {
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i) {
            auto* p = static_cast<EditorPage*>(m_notebook->GetPage(i));
            if (p->modified) {
                if (p->filepath.IsEmpty()) {
                    m_notebook->SetSelection(i);
                    SaveAs();
                } else {
                    p->SaveFile();
                    m_notebook->SetPageText(i, p->GetTitle());
                }
            }
        }
    }

    bool ConfirmClose(EditorPage* page) {
        if (!page->modified) return true;
        wxString name = page->filepath.IsEmpty() ? "Untitled" : wxFileName(page->filepath).GetFullName();
        int answer = wxMessageBox(
            wxString::Format("'%s' has unsaved changes.\nSave before closing?", name),
            "Unsaved Changes",
            wxYES_NO | wxCANCEL | wxICON_WARNING, this);
        if (answer == wxCANCEL) return false;
        if (answer == wxYES) {
            if (page->filepath.IsEmpty()) return SaveAs();
            return page->SaveFile();
        }
        return true;
    }

    void CloseCurrentTab() {
        int sel = m_notebook->GetSelection();
        if (sel == wxNOT_FOUND) return;
        auto* page = static_cast<EditorPage*>(m_notebook->GetPage(sel));
        if (ConfirmClose(page)) m_notebook->DeletePage(sel);
    }

    void CloseAllTabs() {
        for (int i = (int)m_notebook->GetPageCount() - 1; i >= 0; --i) {
            auto* page = static_cast<EditorPage*>(m_notebook->GetPage(i));
            if (!ConfirmClose(page)) return;
            m_notebook->DeletePage(i);
        }
    }

    // ── Edit dispatch ────────────────────────────────────────────────────────

    void DispatchEdit(int id) {
        auto* page = CurrentPage();
        if (!page) return;
        switch (id) {
            case wxID_UNDO:      page->Undo(); break;
            case wxID_REDO:      page->Redo(); break;
            case wxID_CUT:       page->Cut(); break;
            case wxID_COPY:      page->Copy(); break;
            case wxID_PASTE:     page->Paste(); break;
            case wxID_SELECTALL: page->SelectAll(); break;
        }
    }

    // ── Find ────────────────────────────────────────────────────────────────

    void ShowFindBar() {
        m_findBar->Show();
        m_editorPane->Layout();
        m_findBar->textCtrl->SetFocus();
        auto* page = CurrentPage();
        if (page && page->GetSelectedText().length() > 0)
            m_findBar->textCtrl->SetValue(page->GetSelectedText());
    }

    void CloseFindBar() {
        m_findBar->Hide();
        m_editorPane->Layout();
        if (auto* p = CurrentPage()) p->SetFocus();
    }

    void FindNext(bool forward) {
        auto* page = CurrentPage();
        if (!page) return;
        wxString text = m_findBar->textCtrl->GetValue();
        if (text.IsEmpty()) return;

        int flags = 0;
        if (m_findBar->caseChk->IsChecked())  flags |= wxSTC_FIND_MATCHCASE;
        if (m_findBar->wholeChk->IsChecked()) flags |= wxSTC_FIND_WHOLEWORD;

        page->SearchAnchor();
        int res = forward ? page->SearchNext(flags, text)
                          : page->SearchPrev(flags, text);
        if (res == wxSTC_INVALID_POSITION) {
            // Wrap
            page->SetCurrentPos(forward ? 0 : page->GetLength());
            page->SearchAnchor();
            res = forward ? page->SearchNext(flags, text)
                          : page->SearchPrev(flags, text);
        }
        if (res != wxSTC_INVALID_POSITION) {
            page->EnsureCaretVisible();
            m_findBar->textCtrl->SetBackgroundColour(wxColour(60, 60, 60));
        } else {
            m_findBar->textCtrl->SetBackgroundColour(wxColour(100, 40, 40));
        }
        m_findBar->textCtrl->Refresh();
    }

    // ── View ────────────────────────────────────────────────────────────────

    void ToggleSidebar() {
        wxWindow* left = m_splitter->GetWindow1();
        if (left && left->IsShown()) {
            m_splitter->Unsplit(left);
            left->Hide();
        } else if (left) {
            left->Show();
            m_splitter->SplitVertically(left, m_splitter->GetWindow2(), 240);
        }
    }

    void ToggleWordWrap() {
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i) {
            auto* p = static_cast<EditorPage*>(m_notebook->GetPage(i));
            p->SetWrapMode(p->GetWrapMode() == wxSTC_WRAP_NONE
                           ? wxSTC_WRAP_WORD : wxSTC_WRAP_NONE);
        }
    }

    void ToggleWhitespace() {
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i) {
            auto* p = static_cast<EditorPage*>(m_notebook->GetPage(i));
            p->SetViewWhiteSpace(p->GetViewWhiteSpace() == wxSTC_WS_INVISIBLE
                                 ? wxSTC_WS_VISIBLEALWAYS : wxSTC_WS_INVISIBLE);
        }
    }

    void ZoomEditor(int delta) {
        auto* page = CurrentPage();
        if (!page) return;
        if (delta == 0) page->SetZoom(0);
        else            page->SetZoom(page->GetZoom() + delta);
    }

    void GoToLine() {
        auto* page = CurrentPage();
        if (!page) return;
        int maxLine = page->GetLineCount();
        wxString val = wxGetTextFromUser(
            wxString::Format("Go to line (1 – %d):", maxLine),
            "Go to Line", wxEmptyString, this);
        if (val.IsEmpty()) return;
        long line;
        if (val.ToLong(&line) && line >= 1 && line <= maxLine) {
            page->GotoLine(line - 1);
            page->SetFocus();
        }
    }

    void ShowAbout() {
        wxMessageBox(
            "wxEditor  v1.0\n\n"
            "A VSCode-inspired multi-tab code editor\n"
            "built with wxWidgets + wxStyledTextCtrl.\n\n"
            "Features:\n"
            "  • Multi-tab editing (drag to reorder)\n"
            "  • Syntax highlighting for 15+ languages\n"
            "  • Folder explorer with lazy loading\n"
            "  • Code folding & brace matching\n"
            "  • Inline find bar\n"
            "  • Auto-indent & auto-close brackets\n"
            "  • Word-wrap, whitespace display\n"
            "  • Zoom in/out\n",
            "About wxEditor", wxOK | wxICON_INFORMATION, this);
    }

    // ── Events ───────────────────────────────────────────────────────────────

    void OnTabChanged(wxAuiNotebookEvent& evt) {
        UpdateStatusBar(CurrentPage());
        evt.Skip();
    }

    void OnTabClose(wxAuiNotebookEvent& evt) {
        auto* page = static_cast<EditorPage*>(m_notebook->GetPage(evt.GetSelection()));
        if (!ConfirmClose(page)) evt.Veto();
    }

    void OnTreeItemActivated(wxTreeEvent& evt) {
        wxTreeItemId item = evt.GetItem();
        if (!item.IsOk()) return;
        if (m_tree->ItemHasChildren(item)) { evt.Skip(); return; }
        PathData* data = dynamic_cast<PathData*>(m_tree->GetItemData(item));
        if (!data) return;
        wxString path = data->GetPath();
        if (wxFileName::FileExists(path))
            OpenFileInTab(path);
    }

    void OnCloseWindow(wxCloseEvent& evt) {
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i) {
            auto* p = static_cast<EditorPage*>(m_notebook->GetPage(i));
            if (p->modified) {
                if (!ConfirmClose(p)) { evt.Veto(); return; }
            }
        }
        evt.Skip();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// App
// ─────────────────────────────────────────────────────────────────────────────

class EditorApp : public wxApp {
public:
    bool OnInit() override {
        SetAppName("wxEditor");
        wxInitAllImageHandlers();
        auto* frame = new MainFrame();

        // Open file(s) passed on command line
        for (int i = 1; i < argc; ++i) {
            wxString arg = argv[i];
            if (wxFileName::FileExists(arg))
                frame->GetChildren(); // tab already created in ctor; reopen
        }

        return true;
    }
};

wxIMPLEMENT_APP(EditorApp);