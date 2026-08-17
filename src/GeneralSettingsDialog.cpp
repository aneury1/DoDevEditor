#include "GeneralSettingsDialog.h"

#include "AIClient.h"
#include "AISecretStore.h"
#include "Config.h"
#include "EditorConfigManager.h"

#include <wx/notebook.h>
#include <wx/statline.h>
#include <wx/scrolwin.h>

#ifndef DODEV_ENABLE_MANUAL_SYMBOLS
#define DODEV_ENABLE_MANUAL_SYMBOLS 0
#endif

namespace
{
AIProviderKind ProviderFromSelection(int selection)
{
    if (selection < 0 || selection > static_cast<int>(AIProviderKind::Gemini))
        return AIProviderKind::OpenAI;
    return static_cast<AIProviderKind>(selection);
}

AISecretSource SecretSourceFromSelection(int selection)
{
    if (selection < 0 || selection > static_cast<int>(AISecretSource::None))
        return AISecretSource::Environment;
    return static_cast<AISecretSource>(selection);
}
}

GeneralSettingsDialog::GeneralSettingsDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "General Settings", wxDefaultPosition, wxSize(780, 760),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    SetBackgroundColour(wxColour(37, 37, 38));
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* notebook = new wxNotebook(this, wxID_ANY);

    // ---------------------------------------------------------------------
    // Editor appearance
    // ---------------------------------------------------------------------
    auto* editorPage = new wxPanel(notebook);
    editorPage->SetBackgroundColour(wxColour(37, 37, 38));
    auto* editorSizer = new wxBoxSizer(wxVERTICAL);
    auto* editorTitle = new wxStaticText(editorPage, wxID_ANY, "Editor appearance");
    editorTitle->SetForegroundColour(wxColour(230, 230, 230));
    wxFont editorTitleFont = editorTitle->GetFont();
    editorTitleFont.SetWeight(wxFONTWEIGHT_BOLD);
    editorTitle->SetFont(editorTitleFont);
    editorSizer->Add(editorTitle, 0, wxALL, 12);

    auto* themeRow = new wxBoxSizer(wxHORIZONTAL);
    auto* themeLabel = new wxStaticText(editorPage, wxID_ANY, "Theme");
    themeLabel->SetForegroundColour(wxColour(210, 210, 210));
    m_editorTheme = new wxChoice(editorPage, wxID_ANY);
    m_editorThemeIds = EditorConfigManager::ThemeIds();
    for (const wxString& id : m_editorThemeIds)
        m_editorTheme->Append(EditorConfigManager::ThemeDisplayName(id));
    themeRow->Add(themeLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    themeRow->Add(m_editorTheme, 1, wxEXPAND);
    editorSizer->Add(themeRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* themeHelp = new wxStaticText(editorPage, wxID_ANY,
        "Themes are JSON files in config/themes. Syntax definitions are JSON files in config/syntax.\n"
        "Edit those files to change colours, extensions or keyword sets without rebuilding DoDevEditor.");
    themeHelp->SetForegroundColour(wxColour(165, 165, 165));
    themeHelp->Wrap(650);
    editorSizer->Add(themeHelp, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    editorSizer->AddStretchSpacer(1);
    editorPage->SetSizer(editorSizer);

    // ---------------------------------------------------------------------
    // AI general
    // ---------------------------------------------------------------------
    auto* generalPage = new wxPanel(notebook);
    generalPage->SetBackgroundColour(wxColour(37, 37, 38));
    auto* generalSizer = new wxBoxSizer(wxVERTICAL);
    auto* generalTitle = new wxStaticText(generalPage, wxID_ANY, "AI integration");
    generalTitle->SetForegroundColour(wxColour(230, 230, 230));
    wxFont titleFont = generalTitle->GetFont();
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    titleFont.SetPointSize(titleFont.GetPointSize() + 1);
    generalTitle->SetFont(titleFont);
    generalSizer->Add(generalTitle, 0, wxALL, 12);

    m_enabled = new wxCheckBox(generalPage, wxID_ANY, "Enable AI Chat integration");
    m_enabled->SetForegroundColour(wxColour(220, 220, 220));
    generalSizer->Add(m_enabled, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* contextBox = new wxStaticBoxSizer(wxVERTICAL, generalPage, "Default editor context");
    m_includeCurrentFile = new wxCheckBox(generalPage, wxID_ANY, "Include current file when sending a prompt");
    m_includeSelection = new wxCheckBox(generalPage, wxID_ANY, "Include selected text when available");
    m_includeGitDiff = new wxCheckBox(generalPage, wxID_ANY, "Include the active Git diff when a diff tab is selected");
    m_includeLLVM = new wxCheckBox(generalPage, wxID_ANY,
        "Include code symbols/call hierarchy context (manual parser and LLVM when available)");
    for (wxCheckBox* checkbox : {m_includeCurrentFile, m_includeSelection, m_includeGitDiff, m_includeLLVM})
        checkbox->SetForegroundColour(wxColour(220, 220, 220));
    contextBox->Add(m_includeCurrentFile, 0, wxALL, 6);
    contextBox->Add(m_includeSelection, 0, wxLEFT | wxRIGHT | wxBOTTOM, 6);
    contextBox->Add(m_includeGitDiff, 0, wxLEFT | wxRIGHT | wxBOTTOM, 6);
    contextBox->Add(m_includeLLVM, 0, wxLEFT | wxRIGHT | wxBOTTOM, 6);
    generalSizer->Add(contextBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* note = new wxStaticText(generalPage, wxID_ANY,
        "Project source is only sent when the corresponding context option is enabled.\n"
        "Secrets are never written to config.json by DoDevEditor. Ollama and llama.cpp local modes require no secret by default.");
    note->SetForegroundColour(wxColour(165, 165, 165));
    generalSizer->Add(note, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    generalSizer->AddStretchSpacer(1);
    generalPage->SetSizer(generalSizer);

    // ---------------------------------------------------------------------
    // Manual source analysis (dependency-free C/C++/Kotlin parser)
    // ---------------------------------------------------------------------
    auto* analysisPage = new wxPanel(notebook);
    analysisPage->SetBackgroundColour(wxColour(37, 37, 38));
    auto* analysisSizer = new wxBoxSizer(wxVERTICAL);
    auto* analysisTitle = new wxStaticText(analysisPage, wxID_ANY, "Manual symbol && call parsing");
    analysisTitle->SetForegroundColour(wxColour(230, 230, 230));
    wxFont analysisTitleFont = analysisTitle->GetFont();
    analysisTitleFont.SetWeight(wxFONTWEIGHT_BOLD);
    analysisTitle->SetFont(analysisTitleFont);
    analysisSizer->Add(analysisTitle, 0, wxALL, 12);

    m_manualEnabled = new wxCheckBox(analysisPage, wxID_ANY, "Enable dependency-free source parsing");
    m_manualAutoParse = new wxCheckBox(analysisPage, wxID_ANY, "Parse automatically when the current file/tab changes");
    m_manualEnabled->SetForegroundColour(wxColour(220, 220, 220));
    m_manualAutoParse->SetForegroundColour(wxColour(220, 220, 220));
    analysisSizer->Add(m_manualEnabled, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    analysisSizer->Add(m_manualAutoParse, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* languagesBox = new wxStaticBoxSizer(wxVERTICAL, analysisPage, "Language parsers");
    m_manualC = new wxCheckBox(analysisPage, wxID_ANY, "C (.c)");
    m_manualCpp = new wxCheckBox(analysisPage, wxID_ANY, "C++ (.cpp/.cc/.cxx/.h/.hpp/...)");
    m_manualKotlin = new wxCheckBox(analysisPage, wxID_ANY, "Kotlin (.kt/.kts)");
    for (wxCheckBox* checkbox : {m_manualC, m_manualCpp, m_manualKotlin})
    {
        checkbox->SetForegroundColour(wxColour(220, 220, 220));
        languagesBox->Add(checkbox, 0, wxALL, 5);
    }
    analysisSizer->Add(languagesBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* featuresBox = new wxStaticBoxSizer(wxVERTICAL, analysisPage, "Parsed information");
    m_manualSymbols = new wxCheckBox(analysisPage, wxID_ANY, "Symbols");
    m_manualCalls = new wxCheckBox(analysisPage, wxID_ANY, "Call hierarchy (callers / callees)");
    m_manualTypes = new wxCheckBox(analysisPage, wxID_ANY, "Types / namespaces / packages / aliases");
    m_manualFunctions = new wxCheckBox(analysisPage, wxID_ANY, "Functions / methods / constructors");
    m_manualVariables = new wxCheckBox(analysisPage, wxID_ANY, "Variables / fields / properties");
    m_manualMacros = new wxCheckBox(analysisPage, wxID_ANY, "C/C++ macros");
    for (wxCheckBox* checkbox : {m_manualSymbols, m_manualCalls, m_manualTypes,
                                 m_manualFunctions, m_manualVariables, m_manualMacros})
    {
        checkbox->SetForegroundColour(wxColour(220, 220, 220));
        featuresBox->Add(checkbox, 0, wxALL, 5);
    }
    analysisSizer->Add(featuresBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* analysisNote = new wxStaticText(analysisPage, wxID_ANY,
#if DODEV_ENABLE_MANUAL_SYMBOLS
        "This parser is built into DoDevEditor and uses only C++17. It does not require LLVM, "
        "libclang, tree-sitter, or another parsing library. Runtime-disabled languages are not tokenized or parsed."
#else
        "Manual source parsing was disabled at compile time. Rebuild with -DDODEV_ENABLE_MANUAL_SYMBOLS=ON "
        "or use --manual-symbols with the build script."
#endif
    );
    analysisNote->SetForegroundColour(wxColour(165, 165, 165));
    analysisNote->Wrap(650);
    analysisSizer->Add(analysisNote, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
#if !DODEV_ENABLE_MANUAL_SYMBOLS
    for (wxCheckBox* checkbox : {m_manualEnabled, m_manualAutoParse, m_manualC, m_manualCpp, m_manualKotlin,
                                 m_manualSymbols, m_manualCalls, m_manualTypes, m_manualFunctions,
                                 m_manualVariables, m_manualMacros})
        checkbox->Enable(false);
#endif
    analysisSizer->AddStretchSpacer(1);
    analysisPage->SetSizer(analysisSizer);

    // ---------------------------------------------------------------------
    // Provider and secrets
    // ---------------------------------------------------------------------
    auto* aiPage = new wxScrolledWindow(notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                        wxVSCROLL | wxTAB_TRAVERSAL);
    aiPage->SetBackgroundColour(wxColour(37, 37, 38));
    aiPage->SetScrollRate(0, 10);
    auto* aiSizer = new wxBoxSizer(wxVERTICAL);
    auto* grid = new wxFlexGridSizer(2, 10, 12);
    grid->AddGrowableCol(1, 1);

    auto addLabel = [aiPage, grid](const wxString& text)
    {
        auto* label = new wxStaticText(aiPage, wxID_ANY, text);
        label->SetForegroundColour(wxColour(200, 200, 200));
        grid->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    };

    addLabel("Provider");
    m_provider = new wxChoice(aiPage, wxID_ANY);
    m_provider->Append("OpenAI");
    m_provider->Append("Anthropic / Claude");
    m_provider->Append("OpenAI-compatible");
    m_provider->Append("GitHub Copilot CLI");
    m_provider->Append("Ollama (local)");
    m_provider->Append("llama.cpp (local)");
    m_provider->Append("Google Gemini");
    grid->Add(m_provider, 1, wxEXPAND);

    addLabel("Model");
    m_model = new wxTextCtrl(aiPage, wxID_ANY);
    m_model->SetHint("Provider model ID; Ollama can discover local models below");
    grid->Add(m_model, 1, wxEXPAND);

    addLabel("Base URL");
    m_baseUrl = new wxTextCtrl(aiPage, wxID_ANY);
    grid->Add(m_baseUrl, 1, wxEXPAND);

    addLabel("Max output tokens");
    m_maxTokens = new wxSpinCtrl(aiPage, wxID_ANY);
    m_maxTokens->SetRange(64, 65536);
    grid->Add(m_maxTokens, 0, wxEXPAND);

    addLabel("Secret source");
    m_secretSource = new wxChoice(aiPage, wxID_ANY);
    m_secretSource->Append("Environment variable");
    m_secretSource->Append("Session secret (memory only)");
    m_secretSource->Append("OS keyring (secret-tool)");
    m_secretSource->Append("Existing provider login");
    m_secretSource->Append("No secret / local endpoint");
    grid->Add(m_secretSource, 1, wxEXPAND);

    addLabel("Environment variable");
    m_envVariable = new wxTextCtrl(aiPage, wxID_ANY);
    grid->Add(m_envVariable, 1, wxEXPAND);

    addLabel("Secret / API key");
    m_secretValue = new wxTextCtrl(aiPage, wxID_ANY, wxEmptyString,
                                   wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
    m_secretValue->SetHint("Enter to set/replace; never written to config.json");
    grid->Add(m_secretValue, 1, wxEXPAND);

    addLabel("Copilot executable");
    m_copilotExecutable = new wxTextCtrl(aiPage, wxID_ANY);
    grid->Add(m_copilotExecutable, 1, wxEXPAND);

    aiSizer->Add(grid, 0, wxEXPAND | wxALL, 14);

    // Provider discovery/testing shared by llama.cpp and Gemini.
    auto* providerBox = new wxStaticBoxSizer(wxVERTICAL, aiPage, "Provider discovery / runtime");
    auto* providerRow = new wxBoxSizer(wxHORIZONTAL);
    auto* providerModelsLabel = new wxStaticText(aiPage, wxID_ANY, "Detected models");
    providerModelsLabel->SetForegroundColour(wxColour(205, 205, 205));
    m_providerModels = new wxChoice(aiPage, wxID_ANY);
    m_providerRefreshModels = new wxButton(aiPage, wxID_ANY, "Refresh Models");
    m_providerTest = new wxButton(aiPage, wxID_ANY, "Test Provider");
    providerRow->Add(providerModelsLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    providerRow->Add(m_providerModels, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    providerRow->Add(m_providerRefreshModels, 0, wxRIGHT, 6);
    providerRow->Add(m_providerTest, 0);
    providerBox->Add(providerRow, 0, wxEXPAND | wxALL, 6);

    auto* providerRuntimeGrid = new wxFlexGridSizer(2, 8, 12);
    providerRuntimeGrid->AddGrowableCol(1, 1);
    auto addRuntimeLabel = [aiPage, providerRuntimeGrid](const wxString& text)
    {
        auto* label = new wxStaticText(aiPage, wxID_ANY, text);
        label->SetForegroundColour(wxColour(195, 195, 195));
        providerRuntimeGrid->Add(label, 0, wxALIGN_CENTER_VERTICAL);
    };

    addRuntimeLabel("llama.cpp");
    auto* llamaRuntime = new wxBoxSizer(wxHORIZONTAL);
    m_llamaCppStream = new wxCheckBox(aiPage, wxID_ANY, "Streaming");
    m_llamaCppStream->SetForegroundColour(wxColour(215, 215, 215));
    m_llamaCppTemperature = new wxSpinCtrlDouble(aiPage, wxID_ANY);
    m_llamaCppTemperature->SetRange(0.0, 2.0);
    m_llamaCppTemperature->SetIncrement(0.1);
    m_llamaCppTemperature->SetDigits(2);
    llamaRuntime->Add(m_llamaCppStream, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    llamaRuntime->Add(new wxStaticText(aiPage, wxID_ANY, "Temperature"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    llamaRuntime->Add(m_llamaCppTemperature, 0, wxALIGN_CENTER_VERTICAL);
    providerRuntimeGrid->Add(llamaRuntime, 1, wxEXPAND);

    addRuntimeLabel("Gemini");
    auto* geminiRuntime = new wxBoxSizer(wxHORIZONTAL);
    geminiRuntime->Add(new wxStaticText(aiPage, wxID_ANY, "Temperature"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    m_geminiTemperature = new wxSpinCtrlDouble(aiPage, wxID_ANY);
    m_geminiTemperature->SetRange(0.0, 2.0);
    m_geminiTemperature->SetIncrement(0.1);
    m_geminiTemperature->SetDigits(2);
    geminiRuntime->Add(m_geminiTemperature, 0, wxALIGN_CENTER_VERTICAL);
    providerRuntimeGrid->Add(geminiRuntime, 1, wxEXPAND);
    providerBox->Add(providerRuntimeGrid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    m_providerStatus = new wxStaticText(aiPage, wxID_ANY, "Select llama.cpp or Gemini to discover models.");
    m_providerStatus->SetForegroundColour(wxColour(155, 155, 155));
    providerBox->Add(m_providerStatus, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
    aiSizer->Add(providerBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);

    // Ollama-specific controls.
    auto* ollamaBox = new wxStaticBoxSizer(wxVERTICAL, aiPage, "Ollama local runtime");
    auto* modelRow = new wxBoxSizer(wxHORIZONTAL);
    auto* detectedLabel = new wxStaticText(aiPage, wxID_ANY, "Detected models");
    detectedLabel->SetForegroundColour(wxColour(205, 205, 205));
    m_ollamaModels = new wxChoice(aiPage, wxID_ANY);
    m_ollamaRefreshModels = new wxButton(aiPage, wxID_ANY, "Refresh Models");
    m_ollamaTest = new wxButton(aiPage, wxID_ANY, "Test Connection");
    modelRow->Add(detectedLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    modelRow->Add(m_ollamaModels, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    modelRow->Add(m_ollamaRefreshModels, 0, wxRIGHT, 6);
    modelRow->Add(m_ollamaTest, 0);
    ollamaBox->Add(modelRow, 0, wxEXPAND | wxALL, 6);

    auto* ollamaGrid = new wxFlexGridSizer(2, 8, 12);
    ollamaGrid->AddGrowableCol(1, 1);
    auto addOllamaLabel = [aiPage, ollamaGrid](const wxString& text)
    {
        auto* label = new wxStaticText(aiPage, wxID_ANY, text);
        label->SetForegroundColour(wxColour(195, 195, 195));
        ollamaGrid->Add(label, 0, wxALIGN_CENTER_VERTICAL);
    };

    addOllamaLabel("Temperature");
    m_ollamaTemperature = new wxSpinCtrlDouble(aiPage, wxID_ANY);
    m_ollamaTemperature->SetRange(0.0, 2.0);
    m_ollamaTemperature->SetIncrement(0.1);
    m_ollamaTemperature->SetDigits(2);
    ollamaGrid->Add(m_ollamaTemperature, 0, wxEXPAND);

    addOllamaLabel("Context window");
    m_ollamaContext = new wxSpinCtrl(aiPage, wxID_ANY);
    m_ollamaContext->SetRange(512, 1048576);
    ollamaGrid->Add(m_ollamaContext, 0, wxEXPAND);

    addOllamaLabel("Keep alive");
    m_ollamaKeepAlive = new wxTextCtrl(aiPage, wxID_ANY);
    m_ollamaKeepAlive->SetHint("5m, 30m, -1, 0");
    ollamaGrid->Add(m_ollamaKeepAlive, 1, wxEXPAND);

    addOllamaLabel("Generation");
    auto* generationRow = new wxBoxSizer(wxHORIZONTAL);
    m_ollamaStream = new wxCheckBox(aiPage, wxID_ANY, "Streaming");
    m_ollamaThink = new wxCheckBox(aiPage, wxID_ANY, "Thinking (when model supports it)");
    m_ollamaStream->SetForegroundColour(wxColour(215, 215, 215));
    m_ollamaThink->SetForegroundColour(wxColour(215, 215, 215));
    generationRow->Add(m_ollamaStream, 0, wxRIGHT, 14);
    generationRow->Add(m_ollamaThink, 0);
    ollamaGrid->Add(generationRow, 1, wxEXPAND);

    ollamaBox->Add(ollamaGrid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
    m_ollamaStatus = new wxStaticText(aiPage, wxID_ANY, "Ollama: not tested");
    m_ollamaStatus->SetForegroundColour(wxColour(155, 155, 155));
    ollamaBox->Add(m_ollamaStatus, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
    aiSizer->Add(ollamaBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);

    m_secretHelp = new wxStaticText(aiPage, wxID_ANY, wxEmptyString);
    m_secretHelp->SetForegroundColour(wxColour(165, 165, 165));
    m_secretHelp->Wrap(680);
    aiSizer->Add(m_secretHelp, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);

    auto* providerNote = new wxStaticText(aiPage, wxID_ANY,
        "OpenAI uses the Responses API. Claude uses Anthropic Messages.\n"
        "OpenAI-compatible supports local/self-hosted Chat Completions endpoints.\n"
        "Ollama uses the native /api/chat endpoint and discovers models from /api/tags.\n"
        "llama.cpp uses /v1/chat/completions, /v1/models and /health.\n"
        "Gemini uses generateContent with model discovery from the Gemini Models API.\n"
        "Copilot uses 'copilot -p' with your existing Copilot CLI authentication and is run in read-only chat mode.");
    providerNote->SetForegroundColour(wxColour(165, 165, 165));
    aiSizer->Add(providerNote, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);
    aiSizer->AddStretchSpacer(1);
    aiPage->SetSizer(aiSizer);
    aiPage->FitInside();

    notebook->AddPage(editorPage, "Editor", true);
    notebook->AddPage(analysisPage, "Code Analysis", false);
    notebook->AddPage(generalPage, "AI General", false);
    notebook->AddPage(aiPage, "Provider && Secrets", false);
    root->Add(notebook, 1, wxEXPAND | wxALL, 8);

    auto* buttons = CreateSeparatedButtonSizer(wxOK | wxCANCEL);
    if (buttons)
        root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    SetSizer(root);

    LoadValues();

    m_provider->Bind(wxEVT_CHOICE, [this](wxCommandEvent&)
    {
        UpdateProviderDefaults(true);
        UpdateSecretControls();
        UpdateOllamaControls();
        UpdateProviderDiscoveryControls();
        if (ProviderFromSelection(m_provider->GetSelection()) == AIProviderKind::Ollama)
            RefreshOllamaModels(false);
        else if (ProviderFromSelection(m_provider->GetSelection()) == AIProviderKind::LlamaCpp ||
                 ProviderFromSelection(m_provider->GetSelection()) == AIProviderKind::Gemini)
            RefreshProviderModels(false);
    });
    m_secretSource->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { UpdateSecretControls(); });
    m_ollamaModels->Bind(wxEVT_CHOICE, [this](wxCommandEvent&)
    {
        const wxString selected = m_ollamaModels->GetStringSelection();
        if (!selected.IsEmpty())
            m_model->SetValue(selected);
    });
    m_ollamaRefreshModels->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RefreshOllamaModels(true); });
    m_ollamaTest->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { TestOllamaConnection(); });
    m_providerModels->Bind(wxEVT_CHOICE, [this](wxCommandEvent&)
    {
        const wxString selected = m_providerModels->GetStringSelection();
        if (!selected.IsEmpty())
            m_model->SetValue(selected);
    });
    m_providerRefreshModels->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RefreshProviderModels(true); });
    m_providerTest->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { TestProviderConnection(); });

    Bind(wxEVT_BUTTON, [this](wxCommandEvent& event)
    {
        if (event.GetId() != wxID_OK)
        {
            event.Skip();
            return;
        }
        SaveValues();
    }, wxID_OK);
}

AIProviderSettings GeneralSettingsDialog::SettingsFromControls() const
{
    AIProviderSettings settings;
    settings.enabled = m_enabled->GetValue();
    settings.provider = ProviderFromSelection(m_provider->GetSelection());
    settings.model = m_model->GetValue();
    settings.baseUrl = m_baseUrl->GetValue();
    settings.secretSource = SecretSourceFromSelection(m_secretSource->GetSelection());
    settings.environmentVariable = m_envVariable->GetValue();
    settings.keyringId = AISettings::DefaultKeyringId(settings.provider);
    settings.copilotExecutable = m_copilotExecutable->GetValue();
    settings.maxOutputTokens = m_maxTokens->GetValue();
    settings.ollamaStream = m_ollamaStream->GetValue();
    settings.ollamaThink = m_ollamaThink->GetValue();
    settings.ollamaTemperature = m_ollamaTemperature->GetValue();
    settings.ollamaContext = m_ollamaContext->GetValue();
    settings.ollamaKeepAlive = m_ollamaKeepAlive->GetValue();
    settings.llamaCppStream = m_llamaCppStream->GetValue();
    settings.llamaCppTemperature = m_llamaCppTemperature->GetValue();
    settings.geminiTemperature = m_geminiTemperature->GetValue();
    settings.includeCurrentFile = m_includeCurrentFile->GetValue();
    settings.includeSelection = m_includeSelection->GetValue();
    settings.includeGitDiff = m_includeGitDiff->GetValue();
    settings.includeLLVM = m_includeLLVM->GetValue();

    if (settings.provider == AIProviderKind::GitHubCopilotCLI)
        settings.secretSource = AISecretSource::ExistingLogin;
    else if (settings.provider == AIProviderKind::Ollama)
        settings.secretSource = AISecretSource::None;
    return settings;
}

void GeneralSettingsDialog::LoadValues()
{
    const wxString activeTheme = wxString::FromUTF8(AppEditorConfig::GetEditorTheme().c_str());
    int themeSelection = wxNOT_FOUND;
    for (size_t i = 0; i < m_editorThemeIds.size(); ++i)
    {
        if (m_editorThemeIds[i] == activeTheme)
        {
            themeSelection = static_cast<int>(i);
            break;
        }
    }
    if (themeSelection == wxNOT_FOUND && !m_editorThemeIds.IsEmpty())
        themeSelection = 0;
    if (m_editorTheme && themeSelection != wxNOT_FOUND)
        m_editorTheme->SetSelection(themeSelection);

    const ManualSymbolRuntimeConfig manual = AppEditorConfig::GetManualSymbolRuntimeConfig();
    m_manualEnabled->SetValue(manual.enabled);
    m_manualAutoParse->SetValue(manual.autoParse);
    m_manualC->SetValue(manual.cEnabled);
    m_manualCpp->SetValue(manual.cppEnabled);
    m_manualKotlin->SetValue(manual.kotlinEnabled);
    m_manualSymbols->SetValue(manual.symbolsEnabled);
    m_manualCalls->SetValue(manual.callsEnabled);
    m_manualTypes->SetValue(manual.typesEnabled);
    m_manualFunctions->SetValue(manual.functionsEnabled);
    m_manualVariables->SetValue(manual.variablesEnabled);
    m_manualMacros->SetValue(manual.macrosEnabled);

    const AIProviderSettings settings = AISettings::Load();
    m_enabled->SetValue(settings.enabled);
    m_provider->SetSelection(static_cast<int>(settings.provider));
    m_model->SetValue(settings.model);
    m_baseUrl->SetValue(settings.baseUrl);
    m_maxTokens->SetValue(settings.maxOutputTokens);
    m_secretSource->SetSelection(static_cast<int>(settings.secretSource));
    m_envVariable->SetValue(settings.environmentVariable);
    m_copilotExecutable->SetValue(settings.copilotExecutable);
    m_ollamaStream->SetValue(settings.ollamaStream);
    m_ollamaThink->SetValue(settings.ollamaThink);
    m_ollamaTemperature->SetValue(settings.ollamaTemperature);
    m_ollamaContext->SetValue(settings.ollamaContext);
    m_ollamaKeepAlive->SetValue(settings.ollamaKeepAlive);
    m_llamaCppStream->SetValue(settings.llamaCppStream);
    m_llamaCppTemperature->SetValue(settings.llamaCppTemperature);
    m_geminiTemperature->SetValue(settings.geminiTemperature);
    m_includeCurrentFile->SetValue(settings.includeCurrentFile);
    m_includeSelection->SetValue(settings.includeSelection);
    m_includeGitDiff->SetValue(settings.includeGitDiff);
    m_includeLLVM->SetValue(settings.includeLLVM);
    UpdateSecretControls();
    UpdateOllamaControls();
    UpdateProviderDiscoveryControls();

    if (settings.provider == AIProviderKind::Ollama)
        RefreshOllamaModels(false);
    else if (settings.provider == AIProviderKind::LlamaCpp ||
             settings.provider == AIProviderKind::Gemini)
        RefreshProviderModels(false);
}

void GeneralSettingsDialog::UpdateProviderDefaults(bool forceDefaults)
{
    const AIProviderKind provider = ProviderFromSelection(m_provider->GetSelection());
    if (forceDefaults || m_baseUrl->GetValue().IsEmpty())
        m_baseUrl->SetValue(AISettings::DefaultBaseUrl(provider));
    if (forceDefaults || m_envVariable->GetValue().IsEmpty())
        m_envVariable->SetValue(AISettings::DefaultEnvironmentVariable(provider));

    if (provider == AIProviderKind::GitHubCopilotCLI)
    {
        m_secretSource->SetSelection(static_cast<int>(AISecretSource::ExistingLogin));
        if (m_model->GetValue().IsEmpty())
            m_model->SetValue("auto");
    }
    else if (provider == AIProviderKind::Ollama)
    {
        m_secretSource->SetSelection(static_cast<int>(AISecretSource::None));
        if (m_baseUrl->GetValue().IsEmpty())
            m_baseUrl->SetValue("http://localhost:11434");
    }
    else if (provider == AIProviderKind::LlamaCpp)
    {
        m_secretSource->SetSelection(static_cast<int>(AISecretSource::None));
        if (m_baseUrl->GetValue().IsEmpty())
            m_baseUrl->SetValue("http://127.0.0.1:8080");
    }
    else if (provider == AIProviderKind::Gemini)
    {
        m_secretSource->SetSelection(static_cast<int>(AISecretSource::Environment));
        if (m_envVariable->GetValue().IsEmpty())
            m_envVariable->SetValue("GEMINI_API_KEY");
    }
}

wxString GeneralSettingsDialog::CurrentSecretKey() const
{
    return AISettings::DefaultKeyringId(ProviderFromSelection(m_provider->GetSelection()));
}

void GeneralSettingsDialog::UpdateSecretControls()
{
    const AIProviderKind provider = ProviderFromSelection(m_provider->GetSelection());
    const bool copilot = provider == AIProviderKind::GitHubCopilotCLI;
    const bool ollama = provider == AIProviderKind::Ollama;
    const bool llamaCpp = provider == AIProviderKind::LlamaCpp;
    const bool optionalNoSecret = provider == AIProviderKind::OpenAICompatible || llamaCpp;
    if (copilot)
        m_secretSource->SetSelection(static_cast<int>(AISecretSource::ExistingLogin));
    else if (ollama)
        m_secretSource->SetSelection(static_cast<int>(AISecretSource::None));

    AISecretSource source = SecretSourceFromSelection(m_secretSource->GetSelection());
    if (!copilot && !ollama && source == AISecretSource::ExistingLogin)
    {
        m_secretSource->SetSelection(static_cast<int>(AISecretSource::Environment));
        source = AISecretSource::Environment;
    }
    if (!copilot && !ollama && !optionalNoSecret && source == AISecretSource::None)
    {
        m_secretSource->SetSelection(static_cast<int>(AISecretSource::Environment));
        source = AISecretSource::Environment;
    }

    m_baseUrl->Enable(!copilot);
    m_copilotExecutable->Enable(copilot);
    m_secretSource->Enable(!copilot && !ollama);
    m_envVariable->Enable(!copilot && !ollama && source == AISecretSource::Environment);
    m_secretValue->Enable(!copilot && !ollama &&
                          (source == AISecretSource::Session || source == AISecretSource::OSKeyring));

    if (ollama)
    {
        m_secretHelp->SetLabel(
            "Local Ollama uses no API key by default. The server URL is configurable; the default is http://localhost:11434. "
            "Source context stays on your machine when this local provider is selected.");
    }
    else if (llamaCpp && source == AISecretSource::None)
    {
        m_secretHelp->SetLabel(
            "Local llama.cpp uses no API key unless llama-server was started with --api-key. "
            "If authentication is enabled, select Environment, Session Secret, or OS Keyring instead.");
    }
    else if (copilot)
    {
        m_secretHelp->SetLabel(
            "Copilot authentication is managed by the official Copilot CLI. Run 'copilot login' once, or configure one of its supported token environment variables outside DoDevEditor.");
    }
    else if (source == AISecretSource::Environment)
    {
        m_secretHelp->SetLabel(
            "DoDevEditor reads the secret from this environment variable when a request is sent. The key is not persisted by the editor. For an OpenAI-compatible local endpoint, leave the variable name empty if no authentication is required.");
    }
    else if (source == AISecretSource::Session)
    {
        const bool hasSecret = AISecretStore::HasSessionSecret(CurrentSecretKey());
        m_secretHelp->SetLabel(hasSecret
            ? wxString("A session secret is already loaded in memory. Enter a new value only to replace it; it is lost when DoDevEditor exits.")
            : wxString("The secret is held in process memory only and is lost when DoDevEditor exits."));
    }
    else if (source == AISecretSource::OSKeyring)
    {
        m_secretHelp->SetLabel(AISecretStore::IsKeyringAvailable()
            ? wxString("The secret is stored/retrieved through Linux secret-tool (libsecret). Enter a value to store or replace it. The value is never copied into config.json.")
            : wxString("secret-tool was not detected. Install libsecret tools or select Environment/Session Secret."));
    }
    else
    {
        m_secretHelp->SetLabel("Authentication is handled by the selected provider outside DoDevEditor.");
    }
    Layout();
}

void GeneralSettingsDialog::UpdateOllamaControls()
{
    const bool ollama = ProviderFromSelection(m_provider->GetSelection()) == AIProviderKind::Ollama;
    for (wxWindow* window : {static_cast<wxWindow*>(m_ollamaModels),
                             static_cast<wxWindow*>(m_ollamaRefreshModels),
                             static_cast<wxWindow*>(m_ollamaTest),
                             static_cast<wxWindow*>(m_ollamaStream),
                             static_cast<wxWindow*>(m_ollamaThink),
                             static_cast<wxWindow*>(m_ollamaTemperature),
                             static_cast<wxWindow*>(m_ollamaContext),
                             static_cast<wxWindow*>(m_ollamaKeepAlive)})
    {
        window->Enable(ollama);
    }
    if (!ollama)
    {
        m_ollamaStatus->SetLabel("Ollama controls become active when the Ollama provider is selected.");
        m_ollamaStatus->SetForegroundColour(wxColour(145, 145, 145));
    }
}

wxString GeneralSettingsDialog::CurrentTestingSecret() const
{
    const AISecretSource source = SecretSourceFromSelection(m_secretSource->GetSelection());
    if ((source == AISecretSource::Session || source == AISecretSource::OSKeyring) &&
        !m_secretValue->GetValue().IsEmpty())
    {
        return m_secretValue->GetValue();
    }
    if (source == AISecretSource::None || source == AISecretSource::ExistingLogin)
        return wxString();

    AIProviderSettings settings = SettingsFromControls();
    wxString error;
    return AISecretStore::Resolve(settings, &error);
}

void GeneralSettingsDialog::UpdateProviderDiscoveryControls()
{
    const AIProviderKind provider = ProviderFromSelection(m_provider->GetSelection());
    const bool llamaCpp = provider == AIProviderKind::LlamaCpp;
    const bool gemini = provider == AIProviderKind::Gemini;
    const bool active = llamaCpp || gemini;

    m_providerModels->Enable(active);
    m_providerRefreshModels->Enable(active);
    m_providerTest->Enable(active);
    m_llamaCppStream->Enable(llamaCpp);
    m_llamaCppTemperature->Enable(llamaCpp);
    m_geminiTemperature->Enable(gemini);

    if (!active)
    {
        m_providerModels->Clear();
        m_providerStatus->SetLabel("Select llama.cpp or Gemini to discover models.");
        m_providerStatus->SetForegroundColour(wxColour(145, 145, 145));
    }
    else if (llamaCpp)
    {
        m_providerStatus->SetLabel("llama.cpp: not tested");
        m_providerStatus->SetForegroundColour(wxColour(155, 155, 155));
    }
    else
    {
        m_providerStatus->SetLabel("Gemini: not tested");
        m_providerStatus->SetForegroundColour(wxColour(155, 155, 155));
    }
}

void GeneralSettingsDialog::RefreshProviderModels(bool showMessage)
{
    const AIProviderKind provider = ProviderFromSelection(m_provider->GetSelection());
    if (provider != AIProviderKind::LlamaCpp && provider != AIProviderKind::Gemini)
        return;

    AIProviderSettings settings = SettingsFromControls();
    const wxString secret = CurrentTestingSecret();
    wxString error;
    std::vector<wxString> models;
    if (provider == AIProviderKind::LlamaCpp)
        models = AIClient::ListLlamaCppModels(settings, secret, &error);
    else
        models = AIClient::ListGeminiModels(settings, secret, &error);

    m_providerModels->Clear();
    for (const wxString& model : models)
        m_providerModels->Append(model);

    if (!error.IsEmpty())
    {
        m_providerStatus->SetLabel(AISettings::ProviderName(provider) + " unavailable: " + error);
        m_providerStatus->SetForegroundColour(wxColour(225, 125, 125));
        if (showMessage)
            wxMessageBox(error, AISettings::ProviderName(provider) + " model discovery",
                         wxOK | wxICON_ERROR, this);
        return;
    }

    int selected = wxNOT_FOUND;
    for (size_t i = 0; i < models.size(); ++i)
    {
        if (models[i] == m_model->GetValue())
        {
            selected = static_cast<int>(i);
            break;
        }
    }
    if (selected == wxNOT_FOUND && !models.empty())
    {
        selected = 0;
        m_model->SetValue(models.front());
    }
    if (selected != wxNOT_FOUND)
        m_providerModels->SetSelection(selected);

    m_providerStatus->SetLabel(AISettings::ProviderName(provider) +
                               wxString::Format(" — %zu model(s) available", models.size()));
    m_providerStatus->SetForegroundColour(wxColour(145, 205, 155));
}

void GeneralSettingsDialog::TestProviderConnection()
{
    const AIProviderKind provider = ProviderFromSelection(m_provider->GetSelection());
    if (provider != AIProviderKind::LlamaCpp && provider != AIProviderKind::Gemini)
        return;

    AIProviderSettings settings = SettingsFromControls();
    const wxString secret = CurrentTestingSecret();
    ProviderStatus status;
    if (provider == AIProviderKind::LlamaCpp)
        status = AIClient::QueryLlamaCppStatus(settings, secret);
    else
        status = AIClient::QueryGeminiStatus(settings, secret);

    if (!status.ok)
    {
        m_providerStatus->SetLabel(AISettings::ProviderName(provider) + " unavailable: " + status.error);
        m_providerStatus->SetForegroundColour(wxColour(225, 125, 125));
        return;
    }

    m_providerStatus->SetLabel(status.detail);
    m_providerStatus->SetForegroundColour(wxColour(145, 205, 155));
    RefreshProviderModels(false);
}

void GeneralSettingsDialog::RefreshOllamaModels(bool showMessage)
{
    if (ProviderFromSelection(m_provider->GetSelection()) != AIProviderKind::Ollama)
        return;

    AIProviderSettings settings = SettingsFromControls();
    wxString error;
    const std::vector<wxString> models = AIClient::ListOllamaModels(settings, &error);
    m_ollamaModels->Clear();
    for (const wxString& model : models)
        m_ollamaModels->Append(model);

    if (!error.IsEmpty())
    {
        m_ollamaStatus->SetLabel("Ollama unavailable: " + error);
        m_ollamaStatus->SetForegroundColour(wxColour(225, 125, 125));
        if (showMessage)
            wxMessageBox(error, "Ollama model discovery", wxOK | wxICON_ERROR, this);
        return;
    }

    int selected = wxNOT_FOUND;
    for (size_t i = 0; i < models.size(); ++i)
    {
        if (models[i] == m_model->GetValue())
        {
            selected = static_cast<int>(i);
            break;
        }
    }
    if (selected == wxNOT_FOUND && !models.empty())
    {
        // Model IDs are provider-specific. If the previously configured model
        // does not exist in this Ollama instance, select a real installed one
        // instead of silently keeping an OpenAI/Claude model ID.
        selected = 0;
        m_model->SetValue(models.front());
    }
    if (selected != wxNOT_FOUND)
        m_ollamaModels->SetSelection(selected);

    m_ollamaStatus->SetLabel(wxString::Format("Ollama connected — %zu model(s) installed", models.size()));
    m_ollamaStatus->SetForegroundColour(wxColour(145, 205, 155));
}

void GeneralSettingsDialog::TestOllamaConnection()
{
    AIProviderSettings settings = SettingsFromControls();
    const OllamaStatus status = AIClient::QueryOllamaStatus(settings);
    if (!status.ok)
    {
        m_ollamaStatus->SetLabel("Ollama unavailable: " + status.error);
        m_ollamaStatus->SetForegroundColour(wxColour(225, 125, 125));
        return;
    }

    wxString text = "Connected";
    if (!status.version.IsEmpty())
        text += " — Ollama " + status.version;
    text += wxString::Format(" — %zu installed", status.models.size());
    if (!status.runningModels.empty())
    {
        text += wxString::Format(", %zu loaded: ", status.runningModels.size());
        for (size_t i = 0; i < status.runningModels.size(); ++i)
        {
            if (i) text += ", ";
            text += status.runningModels[i];
        }
    }
    else
    {
        text += ", no model currently loaded";
    }
    m_ollamaStatus->SetLabel(text);
    m_ollamaStatus->SetForegroundColour(wxColour(145, 205, 155));
    RefreshOllamaModels(false);
}

void GeneralSettingsDialog::SaveValues()
{
    wxString selectedEditorTheme = "vscode";
    if (m_editorTheme && m_editorTheme->GetSelection() != wxNOT_FOUND)
    {
        const int selection = m_editorTheme->GetSelection();
        if (selection >= 0 && static_cast<size_t>(selection) < m_editorThemeIds.size())
            selectedEditorTheme = m_editorThemeIds[static_cast<size_t>(selection)];
    }

    AIProviderSettings settings = SettingsFromControls();

    const wxString enteredSecret = m_secretValue->GetValue();
    if (settings.secretSource == AISecretSource::Session && !enteredSecret.IsEmpty())
    {
        AISecretStore::SetSessionSecret(settings.keyringId, enteredSecret);
    }
    else if (settings.secretSource == AISecretSource::OSKeyring && !enteredSecret.IsEmpty())
    {
        wxString error;
        if (!AISecretStore::StoreInKeyring(settings.keyringId, enteredSecret, &error))
        {
            wxMessageBox(error, "Unable to store secret", wxOK | wxICON_ERROR, this);
            return;
        }
    }

    ManualSymbolRuntimeConfig manual;
    manual.enabled = m_manualEnabled->GetValue();
    manual.autoParse = m_manualAutoParse->GetValue();
    manual.cEnabled = m_manualC->GetValue();
    manual.cppEnabled = m_manualCpp->GetValue();
    manual.kotlinEnabled = m_manualKotlin->GetValue();
    manual.symbolsEnabled = m_manualSymbols->GetValue();
    manual.callsEnabled = m_manualCalls->GetValue();
    manual.typesEnabled = m_manualTypes->GetValue();
    manual.functionsEnabled = m_manualFunctions->GetValue();
    manual.variablesEnabled = m_manualVariables->GetValue();
    manual.macrosEnabled = m_manualMacros->GetValue();

    const wxScopedCharBuffer themeUtf8 = selectedEditorTheme.utf8_str();
    AppEditorConfig::SetEditorTheme(themeUtf8.data() ? std::string(themeUtf8.data()) : std::string("vscode"));
    AppEditorConfig::SetManualSymbolRuntimeConfig(manual);
    AISettings::Save(settings);
    m_saved = true;
    EndModal(wxID_OK);
}
