#pragma once

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
    ID_ADD_FOLDER_WORKSPACE,
    ID_REMOVE_FOLDER_WORKSPACE,
    ID_OPEN_WORKSPACE,
    ID_SAVE_WORKSPACE_AS,
    ID_CLOSE_WORKSPACE,
    ID_SAVE_FILE,
    ID_SAVE_AS,
    ID_SAVE_ALL,
    ID_FILE_HISTORY,
    ID_CLOSE_TAB,
    ID_CLOSE_ALL_TABS,
    ID_TOGGLE_SIDEBAR,
    ID_TOGGLE_WORDWRAP,
    ID_TOGGLE_DOCKER,
    ID_OPEN_AI_CHAT,
    ID_GENERAL_SETTINGS,
    ID_TOGGLE_WHITESPACE,
    ID_FIND,
    ID_FIND_ALL,
    ID_FIND_NEXT,
    ID_FIND_PREV,
    ID_QUICK_OPEN,
    ID_GOTO_LINE,
    ID_GOTO_DEFINITION,
    ID_CPP_PARSE_SYMBOLS,
    ID_CPP_SYNTAX_CHECK,
    ID_CPP_COMPILE,
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

static inline LangInfo DetectLanguage(const wxString& filename) {
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



