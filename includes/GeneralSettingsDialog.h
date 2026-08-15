#pragma once

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/arrstr.h>

#include "AISettings.h"

class GeneralSettingsDialog : public wxDialog
{
public:
    explicit GeneralSettingsDialog(wxWindow* parent);
    bool SettingsChanged() const { return m_saved; }

private:
    wxChoice* m_editorTheme = nullptr;
    wxArrayString m_editorThemeIds;
    wxCheckBox* m_enabled = nullptr;
    wxChoice* m_provider = nullptr;
    wxTextCtrl* m_model = nullptr;
    wxTextCtrl* m_baseUrl = nullptr;
    wxChoice* m_secretSource = nullptr;
    wxTextCtrl* m_envVariable = nullptr;
    wxTextCtrl* m_secretValue = nullptr;
    wxStaticText* m_secretHelp = nullptr;
    wxTextCtrl* m_copilotExecutable = nullptr;
    wxSpinCtrl* m_maxTokens = nullptr;

    wxChoice* m_ollamaModels = nullptr;
    wxButton* m_ollamaRefreshModels = nullptr;
    wxButton* m_ollamaTest = nullptr;
    wxStaticText* m_ollamaStatus = nullptr;
    wxCheckBox* m_ollamaStream = nullptr;
    wxCheckBox* m_ollamaThink = nullptr;
    wxSpinCtrlDouble* m_ollamaTemperature = nullptr;
    wxSpinCtrl* m_ollamaContext = nullptr;
    wxTextCtrl* m_ollamaKeepAlive = nullptr;
    wxCheckBox* m_includeCurrentFile = nullptr;
    wxCheckBox* m_includeSelection = nullptr;
    wxCheckBox* m_includeGitDiff = nullptr;
    wxCheckBox* m_includeLLVM = nullptr;
    bool m_saved = false;

    void LoadValues();
    void UpdateProviderDefaults(bool forceDefaults);
    void UpdateSecretControls();
    void UpdateOllamaControls();
    void RefreshOllamaModels(bool showMessage = true);
    void TestOllamaConnection();
    AIProviderSettings SettingsFromControls() const;
    void SaveValues();
    wxString CurrentSecretKey() const;
};
