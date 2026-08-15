#ifndef JsonStyledTextCtrl_defined
#define JsonStyledTextCtrl_defined

#include <fstream>
#include <wx/wx.h>
#include <wx/stc/stc.h>
#include <json/json.h>

class JsonStyledTextCtrl : public wxStyledTextCtrl
{
public:
    JsonStyledTextCtrl(wxWindow *parent, wxWindowID id = wxID_ANY);

    void OpenFile(const wxString &path);

    void ApplyTheme(const Json::Value &theme);

    void ApplyDarkTheme();

private:
    wxColour ToColor(const Json::Value &arr);

    void ApplyBase(const Json::Value &theme);

    void ApplyLexer(const Json::Value &theme);

    void ApplyKeywords(const Json::Value &theme);

    void ApplyStyles(const Json::Value &theme);
    void ApplyLineNumbers(const Json::Value &theme);

    void ApplyCaret(const Json::Value &theme);

    void ApplySelection(const Json::Value &theme);

    void OnCharAdded(wxStyledTextEvent &event);

    wxString GetSuggestions(const wxString &prefix);
    void ShowAutocomplete();
    void OnKeyDown(wxKeyEvent &event);
};

#endif /// JsonStyledTextCtrl_defined