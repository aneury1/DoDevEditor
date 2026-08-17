#pragma once

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/arrstr.h>
#include <wx/filepicker.h>
#include <wx/dataview.h>

#include <functional>
#include <string>
#include <vector>

#include "AISettings.h"
#include "Config.h"

class GeneralSettingsDialog : public wxDialog
{
public:
    using LoadedPluginPathsProvider = std::function<std::vector<std::string>()>;
    using ApplyPluginsCallback = std::function<void()>;

    explicit GeneralSettingsDialog(wxWindow* parent,
                                   LoadedPluginPathsProvider loadedPluginPaths = {},
                                   ApplyPluginsCallback applyPlugins = {});
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

    wxChoice* m_providerModels = nullptr;
    wxButton* m_providerRefreshModels = nullptr;
    wxButton* m_providerTest = nullptr;
    wxStaticText* m_providerStatus = nullptr;

    wxCheckBox* m_llamaCppStream = nullptr;
    wxSpinCtrlDouble* m_llamaCppTemperature = nullptr;
    wxSpinCtrlDouble* m_geminiTemperature = nullptr;

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
    wxCheckBox* m_includeCodeAnalysis = nullptr;

    // Dependency-free manual symbol/call parser runtime controls.
    wxCheckBox* m_manualEnabled = nullptr;
    wxCheckBox* m_manualC = nullptr;
    wxCheckBox* m_manualCpp = nullptr;
    wxCheckBox* m_manualKotlin = nullptr;
    wxCheckBox* m_manualSymbols = nullptr;
    wxCheckBox* m_manualCalls = nullptr;
    wxCheckBox* m_manualTypes = nullptr;
    wxCheckBox* m_manualFunctions = nullptr;
    wxCheckBox* m_manualVariables = nullptr;
    wxCheckBox* m_manualMacros = nullptr;
    wxCheckBox* m_manualAutoParse = nullptr;

    // Plugin/runtime server settings.
    wxDirPickerCtrl* m_pluginsDirectory = nullptr;
    wxTextCtrl* m_httpBindAddress = nullptr;
    wxSpinCtrl* m_httpPort = nullptr;
    wxTextCtrl* m_httpAuthToken = nullptr;
    wxSpinCtrl* m_httpMaxTextMiB = nullptr;
    wxDataViewListCtrl* m_pluginList = nullptr;
    wxButton* m_pluginRefresh = nullptr;
    wxButton* m_pluginApply = nullptr;
    wxStaticText* m_pluginRuntimeStatus = nullptr;
    LoadedPluginPathsProvider m_loadedPluginPaths;
    ApplyPluginsCallback m_applyPlugins;
    bool m_saved = false;

    void LoadValues();
    void RefreshPluginList();
    PluginRuntimeConfig PluginSettingsFromControls() const;
    void SavePluginSettingsOnly();
    void UpdateProviderDefaults(bool forceDefaults);
    void UpdateSecretControls();
    void UpdateOllamaControls();
    void UpdateProviderDiscoveryControls();
    void RefreshProviderModels(bool showMessage = true);
    void TestProviderConnection();
    wxString CurrentTestingSecret() const;
    void RefreshOllamaModels(bool showMessage = true);
    void TestOllamaConnection();
    AIProviderSettings SettingsFromControls() const;
    void SaveValues();
    wxString CurrentSecretKey() const;
};
