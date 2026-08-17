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

#include "JsonStyledTextCtrl.h"
#include <json/json.h>

// ─────────────────────────────────────────────────────────────────────────────
// EditorPage – one wxStyledTextCtrl per tab
// ─────────────────────────────────────────────────────────────────────────────

class EditorPage : public JsonStyledTextCtrl {
public:
    wxString filepath;   // full path; empty = new unsaved file
    bool     modified = false;

    EditorPage(wxWindow* parent, const wxString& path = wxEmptyString);

    // ── Tab title: filename or "Untitled" (with leading "●" if modified) ──
    wxString GetTitle() const;
    void SetDisplayNameOverride(const wxString& name) { m_displayNameOverride = name; }
    const wxString& GetDisplayNameOverride() const { return m_displayNameOverride; }

    // ── Load file from disk ──
    bool LoadFile(const wxString& path) ;

    // ── Save to disk ──
    bool SaveFile(const wxString& path = wxEmptyString);

    // ── Apply JSON-driven editor theme + syntax ──
    void ApplyTheme();

    void ApplySyntax();
    const wxString& GetLanguageName() const { return m_languageName; }
private:
    Json::Value m_theme;
    wxString m_languageName = "Plain Text";
    wxString m_displayNameOverride;
    void OnChange(wxStyledTextEvent&);

    void OnUpdateUI(wxStyledTextEvent&);

    void OnCharAdded(wxStyledTextEvent& evt);

    void OnContextMenu(wxContextMenuEvent& event);
};

