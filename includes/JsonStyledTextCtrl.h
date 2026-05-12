#ifndef JsonStyledTextCtrl_defined
#define JsonStyledTextCtrl_defined

#include <wx/wx.h>
#include <wx/stc/stc.h>
#include <json/json.h>

class JsonStyledTextCtrl : public wxStyledTextCtrl
{
public:
    JsonStyledTextCtrl(wxWindow *parent, wxWindowID id = wxID_ANY)
        : wxStyledTextCtrl(parent, id)
    {
        AutoCompSetIgnoreCase(true);
        AutoCompSetChooseSingle(false);
        AutoCompSetSeparator(' ');
        AutoCompSetMaxHeight(10);
        Bind(wxEVT_STC_CHARADDED, &JsonStyledTextCtrl::OnCharAdded, this);
        Bind(wxEVT_KEY_DOWN, &JsonStyledTextCtrl::OnKeyDown, this);
    }

    void ApplyTheme(const Json::Value &theme)
    {
        ApplyBase(theme);
        ApplyLexer(theme);
        ApplyKeywords(theme);
        ApplyStyles(theme);
        ApplyLineNumbers(theme);
        ApplyCaret(theme);
        ApplySelection(theme);

        StyleClearAll();
        ApplyDarkTheme();
    }

    void ApplyDarkTheme()
    {
        // =========================
        // BASIC STYLE SETTINGS
        // =========================

        SetLexer(wxSTC_LEX_CPP);

        StyleSetBackground(wxSTC_STYLE_DEFAULT, wxColour(30, 30, 30));
        StyleSetForeground(wxSTC_STYLE_DEFAULT, wxColour(220, 220, 220));
        StyleSetFont(wxSTC_STYLE_DEFAULT, wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        StyleClearAll();

        // =========================
        // LINE NUMBERS
        // =========================

        SetMarginType(0, wxSTC_MARGIN_NUMBER);
        SetMarginWidth(0, 40);
        StyleSetForeground(wxSTC_STYLE_LINENUMBER, wxColour(100, 100, 100));
        StyleSetBackground(wxSTC_STYLE_LINENUMBER, wxColour(20, 20, 20));

        // =========================
        // KEYWORDS (C++)
        // =========================

        SetKeyWords(0,
                    "auto break case char const continue default do double else enum extern float for goto if int long register return short signed sizeof static struct switch typedef union unsigned void volatile while");

        StyleSetForeground(wxSTC_C_WORD, wxColour(86, 156, 214));   // blue keywords
        StyleSetForeground(wxSTC_C_COMMENT, wxColour(87, 166, 74)); // green comments
        StyleSetForeground(wxSTC_C_COMMENTLINE, wxColour(87, 166, 74));
        StyleSetForeground(wxSTC_C_STRING, wxColour(214, 157, 133)); // orange strings
        StyleSetForeground(wxSTC_C_NUMBER, wxColour(181, 206, 168));

        // =========================
        // CARET / CURSOR
        // =========================

        SetCaretForeground(wxColour(255, 255, 255));

        // =========================
        // SELECTION COLOR
        // =========================

        SetSelBackground(true, wxColour(51, 153, 255));

        // =========================
        // BACKGROUND
        // =========================

        StyleSetBackground(wxSTC_STYLE_DEFAULT, wxColour(30, 30, 30));
        SetBackgroundColour(wxColour(30, 30, 30));
    }

private:
    wxColour ToColor(const Json::Value &arr)
    {
        if (!arr.isArray() || arr.size() < 3)
            return wxColour(0, 0, 0);

        return wxColour(
            arr[0].asInt(),
            arr[1].asInt(),
            arr[2].asInt());
    }

    void ApplyBase(const Json::Value &theme)
    {
        if (theme.isMember("background"))
        {
            wxColour bg = ToColor(theme["background"]);
            SetBackgroundColour(bg);
            StyleSetBackground(wxSTC_STYLE_DEFAULT, bg);
        }

        if (theme.isMember("foreground"))
        {
            wxColour fg = ToColor(theme["foreground"]);
            StyleSetForeground(wxSTC_STYLE_DEFAULT, fg);
        }
    }

    void ApplyLexer(const Json::Value &theme)
    {
        if (!theme.isMember("lexer"))
            return;

        std::string lexer = theme["lexer"].asString();

        if (lexer == "cpp")
            SetLexer(wxSTC_LEX_CPP);
        else if (lexer == "python")
            SetLexer(wxSTC_LEX_PYTHON);
        else if (lexer == "javascript")
            SetLexer(wxSTC_LEX_CPP);
    }

    void ApplyKeywords(const Json::Value &theme)
    {
        if (!theme.isMember("keywords"))
            return;

        SetKeyWords(0, theme["keywords"].asString());
    }

    void ApplyStyles(const Json::Value &theme)
    {
        if (!theme.isMember("styles"))
            return;

        const Json::Value &s = theme["styles"];

        if (s.isMember("word"))
            StyleSetForeground(wxSTC_C_WORD, ToColor(s["word"]));

        if (s.isMember("comment"))
            StyleSetForeground(wxSTC_C_COMMENT, ToColor(s["comment"]));

        if (s.isMember("comment_line"))
            StyleSetForeground(wxSTC_C_COMMENTLINE, ToColor(s["comment_line"]));

        if (s.isMember("string"))
            StyleSetForeground(wxSTC_C_STRING, ToColor(s["string"]));

        if (s.isMember("number"))
            StyleSetForeground(wxSTC_C_NUMBER, ToColor(s["number"]));
    }

    void ApplyLineNumbers(const Json::Value &theme)
    {
        if (!theme.isMember("linenumber"))
            return;

        const Json::Value &ln = theme["linenumber"];

        SetMarginType(0, wxSTC_MARGIN_NUMBER);

        if (ln.isMember("width"))
            SetMarginWidth(0, ln["width"].asInt());

        if (ln.isMember("foreground"))
            StyleSetForeground(wxSTC_STYLE_LINENUMBER, ToColor(ln["foreground"]));

        if (ln.isMember("background"))
            StyleSetBackground(wxSTC_STYLE_LINENUMBER, ToColor(ln["background"]));
    }

    void ApplyCaret(const Json::Value &theme)
    {
        if (theme.isMember("caret"))
        {
            SetCaretForeground(ToColor(theme["caret"]));
        }
    }

    void ApplySelection(const Json::Value &theme)
    {
        if (theme.isMember("selection"))
        {
            SetSelBackground(true, ToColor(theme["selection"]));
        }
    }

    void OnCharAdded(wxStyledTextEvent &event)
    {
        char ch = event.GetKey();

        // Only trigger after letters (avoid spam on symbols)
        if (!isalpha(ch) && ch != '_')
            return;

        int pos = GetCurrentPos();
        int start = WordStartPosition(pos, true);

        wxString prefix = GetTextRange(start, pos);

        if (prefix.length() < 2)
            return;

        wxString suggestions =
            "int float double if else for while return class struct public private protected";

        wxString list = GetSuggestions(prefix);

        if (list.empty())
            return; // IMPORTANT

        AutoCompShow(prefix.length(), list);
        /// AutoCompShow(prefix.length(), suggestions);
    }

    wxString GetSuggestions(const wxString &prefix)
    {
        wxArrayString words = {
            "int", "float", "double", "if", "else", "for", "while",
            "return", "class", "struct", "public", "private", "protected"};

        wxString result;

        for (const auto &w : words)
        {
            if (w.StartsWith(prefix))
            {
                if (!result.empty())
                    result += " ";
                result += w;
            }
        }

        return result;
    }
    void ShowAutocomplete()
    {
        wxString words = "int float double if else for while return class struct";
        AutoCompShow(0, words);
    }
    void OnKeyDown(wxKeyEvent &event)
    {
        if (event.ControlDown() && event.GetKeyCode() == ' ')
        {
            ShowAutocomplete();
            return;
        }

        event.Skip();
    }
};

#endif /// JsonStyledTextCtrl_defined