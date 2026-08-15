#include "AIChatPage.h"

#include "AISettings.h"

#include <wx/filename.h>
#include <wx/weakref.h>
#include <wx/app.h>

#include <thread>
#include <algorithm>

namespace
{
wxString LimitText(const wxString& text, size_t maxChars)
{
    if (text.length() <= maxChars)
        return text;
    return text.Left(maxChars) + "\n\n[Context truncated by DoDevEditor]";
}

AIProviderKind ProviderFromSelection(int selection)
{
    if (selection < 0 || selection > static_cast<int>(AIProviderKind::Ollama))
        return AIProviderKind::OpenAI;
    return static_cast<AIProviderKind>(selection);
}
}

AIChatPage::AIChatPage(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    SetBackgroundColour(wxColour(30, 30, 30));
    m_settings = AISettings::Load();

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* toolbar = new wxPanel(this);
    toolbar->SetBackgroundColour(wxColour(37, 37, 38));
    auto* toolbarSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* title = new wxStaticText(toolbar, wxID_ANY, "AI CHAT");
    title->SetForegroundColour(wxColour(225, 225, 225));
    wxFont titleFont = title->GetFont();
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    title->SetFont(titleFont);

    m_providerChoice = new wxChoice(toolbar, wxID_ANY);
    m_providerChoice->Append("OpenAI");
    m_providerChoice->Append("Anthropic / Claude");
    m_providerChoice->Append("OpenAI-compatible");
    m_providerChoice->Append("GitHub Copilot CLI");
    m_providerChoice->Append("Ollama (local)");

    auto* newChatButton = new wxButton(toolbar, wxID_ANY, "New Chat", wxDefaultPosition, wxSize(82, 28));
    auto* settingsButton = new wxButton(toolbar, wxID_ANY, "Settings", wxDefaultPosition, wxSize(76, 28));

    toolbarSizer->Add(title, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 8);
    toolbarSizer->Add(m_providerChoice, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    toolbarSizer->AddStretchSpacer(1);
    toolbarSizer->Add(newChatButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    toolbarSizer->Add(settingsButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    toolbar->SetSizer(toolbarSizer);
    root->Add(toolbar, 0, wxEXPAND);

    m_status = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_status->SetForegroundColour(wxColour(160, 160, 160));
    root->Add(m_status, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8);

    m_historyWindow = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                           wxVSCROLL | wxBORDER_NONE);
    m_historyWindow->SetBackgroundColour(wxColour(30, 30, 30));
    m_historyWindow->SetScrollRate(0, 12);
    m_historySizer = new wxBoxSizer(wxVERTICAL);
    m_historyWindow->SetSizer(m_historySizer);
    root->Add(m_historyWindow, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);

    auto* contextBar = new wxPanel(this);
    contextBar->SetBackgroundColour(wxColour(37, 37, 38));
    auto* contextSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* contextLabel = new wxStaticText(contextBar, wxID_ANY, "Context:");
    contextLabel->SetForegroundColour(wxColour(170, 170, 170));
    m_includeCurrentFile = new wxCheckBox(contextBar, wxID_ANY, "Current file");
    m_includeSelection = new wxCheckBox(contextBar, wxID_ANY, "Selection");
    m_includeGitDiff = new wxCheckBox(contextBar, wxID_ANY, "Git diff");
    m_includeLLVM = new wxCheckBox(contextBar, wxID_ANY, "LLVM");
    for (wxCheckBox* checkbox : {m_includeCurrentFile, m_includeSelection, m_includeGitDiff, m_includeLLVM})
        checkbox->SetForegroundColour(wxColour(205, 205, 205));
    contextSizer->Add(contextLabel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 8);
    contextSizer->Add(m_includeCurrentFile, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    contextSizer->Add(m_includeSelection, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    contextSizer->Add(m_includeGitDiff, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    contextSizer->Add(m_includeLLVM, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    contextBar->SetSizer(contextSizer);
    root->Add(contextBar, 0, wxEXPAND | wxTOP, 6);

    auto* composer = new wxPanel(this);
    composer->SetBackgroundColour(wxColour(37, 37, 38));
    auto* composerSizer = new wxBoxSizer(wxHORIZONTAL);
    m_prompt = new wxTextCtrl(composer, wxID_ANY, wxEmptyString,
                              wxDefaultPosition, wxSize(-1, 88), wxTE_MULTILINE);
    m_prompt->SetBackgroundColour(wxColour(45, 45, 48));
    m_prompt->SetForegroundColour(wxColour(230, 230, 230));
    m_prompt->SetHint("Ask about your code...  Ctrl+Enter to send");
    m_sendButton = new wxButton(composer, wxID_ANY, "Send", wxDefaultPosition, wxSize(76, 34));
    composerSizer->Add(m_prompt, 1, wxEXPAND | wxALL, 8);
    composerSizer->Add(m_sendButton, 0, wxALIGN_BOTTOM | wxRIGHT | wxBOTTOM, 8);
    composer->SetSizer(composerSizer);
    root->Add(composer, 0, wxEXPAND);

    SetSizer(root);

    m_sendButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SendPrompt(); });
    newChatButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { NewChat(); });
    settingsButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        if (m_settingsCallback)
            m_settingsCallback();
    });
    m_providerChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&)
    {
        const AIProviderKind oldProvider = m_settings.provider;
        m_settings.provider = ProviderFromSelection(m_providerChoice->GetSelection());
        if (m_settings.provider != oldProvider)
        {
            m_settings.baseUrl = AISettings::DefaultBaseUrl(m_settings.provider);
            m_settings.environmentVariable = AISettings::DefaultEnvironmentVariable(m_settings.provider);
            m_settings.keyringId = AISettings::DefaultKeyringId(m_settings.provider);
            // Models are provider-specific. Never accidentally send the previous
            // provider's model ID to a newly selected provider.
            m_settings.model = m_settings.provider == AIProviderKind::GitHubCopilotCLI
                ? wxString("auto")
                : wxString();
            if (m_settings.provider == AIProviderKind::GitHubCopilotCLI)
                m_settings.secretSource = AISecretSource::ExistingLogin;
            else if (m_settings.provider == AIProviderKind::Ollama)
                m_settings.secretSource = AISecretSource::None;
            else
                m_settings.secretSource = AISecretSource::Environment;
        }
        UpdateProviderDisplay();
    });
    m_prompt->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event)
    {
        if (event.ControlDown() && (event.GetKeyCode() == WXK_RETURN || event.GetKeyCode() == WXK_NUMPAD_ENTER))
        {
            SendPrompt();
            return;
        }
        event.Skip();
    });

    ReloadSettings();
    AddSystemNotice("AI Chat is ready. Configure a provider and secret in General Settings, then send a prompt.");
}

void AIChatPage::SetContextProvider(std::function<AIEditorContext()> callback)
{
    m_contextProvider = std::move(callback);
}

void AIChatPage::SetSettingsCallback(std::function<void()> callback)
{
    m_settingsCallback = std::move(callback);
}

void AIChatPage::ReloadSettings()
{
    m_settings = AISettings::Load();
    m_providerChoice->SetSelection(static_cast<int>(m_settings.provider));
    m_includeCurrentFile->SetValue(m_settings.includeCurrentFile);
    m_includeSelection->SetValue(m_settings.includeSelection);
    m_includeGitDiff->SetValue(m_settings.includeGitDiff);
    m_includeLLVM->SetValue(m_settings.includeLLVM);
    UpdateProviderDisplay();
}

void AIChatPage::UpdateProviderDisplay()
{
    wxString text;
    if (!m_settings.enabled)
    {
        text = "AI integration disabled  |  General Settings → enable AI Chat";
        m_status->SetForegroundColour(wxColour(220, 130, 130));
    }
    else
    {
        text = AISettings::ProviderName(m_settings.provider);
        if (!m_settings.model.IsEmpty())
            text += "  |  model: " + m_settings.model;
        if (m_settings.provider == AIProviderKind::GitHubCopilotCLI)
            text += "  |  auth: Copilot CLI login";
        else if (m_settings.provider == AIProviderKind::Ollama)
            text += "  |  local: " + (m_settings.baseUrl.IsEmpty() ? wxString("http://localhost:11434") : m_settings.baseUrl);
        else if (m_settings.secretSource == AISecretSource::Environment)
            text += "  |  secret: env " + m_settings.environmentVariable;
        else if (m_settings.secretSource == AISecretSource::Session)
            text += "  |  secret: session memory";
        else if (m_settings.secretSource == AISecretSource::OSKeyring)
            text += "  |  secret: OS keyring";
        m_status->SetForegroundColour(wxColour(150, 190, 160));
    }
    m_status->SetLabel(text);
}

void AIChatPage::NewChat()
{
    if (m_busy)
        return;
    m_messages.clear();
    m_historyWindow->Freeze();
    m_historySizer->Clear(true);
    m_historyWindow->Thaw();
    m_historyWindow->Layout();
    m_historyWindow->FitInside();
    AddSystemNotice("New conversation started.");
    m_prompt->SetFocus();
}

void AIChatPage::AddMessage(const wxString& role, const wxString& text)
{
    auto* row = new wxPanel(m_historyWindow);
    row->SetBackgroundColour(wxColour(30, 30, 30));
    auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);

    const bool user = role == "user";
    if (user)
        rowSizer->AddStretchSpacer(1);

    auto* bubble = new wxPanel(row);
    bubble->SetBackgroundColour(user ? wxColour(38, 79, 120) : wxColour(45, 45, 48));
    auto* bubbleSizer = new wxBoxSizer(wxVERTICAL);

    wxString labelText = user ? wxString("You") : AISettings::ProviderName(m_settings.provider);
    auto* label = new wxStaticText(bubble, wxID_ANY, labelText);
    label->SetForegroundColour(user ? wxColour(220, 235, 250) : wxColour(150, 200, 255));
    wxFont labelFont = label->GetFont();
    labelFont.SetWeight(wxFONTWEIGHT_BOLD);
    label->SetFont(labelFont);
    bubbleSizer->Add(label, 0, wxLEFT | wxRIGHT | wxTOP, 9);

    int visualLines = 1;
    for (size_t i = 0; i < text.length(); ++i)
        if (text[i] == '\n') ++visualLines;
    visualLines += static_cast<int>(text.length() / 90);
    visualLines = std::max(2, std::min(24, visualLines));

    auto* messageText = new wxTextCtrl(bubble, wxID_ANY, text,
                                       wxDefaultPosition, wxSize(620, 24 + visualLines * 18),
                                       wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE);
    messageText->SetBackgroundColour(bubble->GetBackgroundColour());
    messageText->SetForegroundColour(wxColour(235, 235, 235));
    bubbleSizer->Add(messageText, 1, wxEXPAND | wxALL, 8);
    bubble->SetSizer(bubbleSizer);
    bubble->SetMinSize(wxSize(360, -1));

    rowSizer->Add(bubble, 0, wxEXPAND | wxALL, 6);
    if (!user)
        rowSizer->AddStretchSpacer(1);
    row->SetSizer(rowSizer);

    m_historySizer->Add(row, 0, wxEXPAND);
    m_historyWindow->Layout();
    m_historyWindow->FitInside();
    m_historyWindow->Scroll(0, m_historyWindow->GetScrollRange(wxVERTICAL));
}

void AIChatPage::AddSystemNotice(const wxString& text, bool error)
{
    auto* label = new wxStaticText(m_historyWindow, wxID_ANY, text);
    label->SetForegroundColour(error ? wxColour(230, 120, 120) : wxColour(145, 145, 145));
    label->Wrap(760);
    m_historySizer->Add(label, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 10);
    m_historyWindow->Layout();
    m_historyWindow->FitInside();
    m_historyWindow->Scroll(0, m_historyWindow->GetScrollRange(wxVERTICAL));
}


void AIChatPage::BeginStreamingMessage()
{
    m_streamingBuffer.clear();
    auto* row = new wxPanel(m_historyWindow);
    row->SetBackgroundColour(wxColour(30, 30, 30));
    auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* bubble = new wxPanel(row);
    bubble->SetBackgroundColour(wxColour(45, 45, 48));
    auto* bubbleSizer = new wxBoxSizer(wxVERTICAL);
    auto* label = new wxStaticText(bubble, wxID_ANY, "Ollama (streaming)");
    label->SetForegroundColour(wxColour(150, 200, 255));
    wxFont labelFont = label->GetFont();
    labelFont.SetWeight(wxFONTWEIGHT_BOLD);
    label->SetFont(labelFont);
    bubbleSizer->Add(label, 0, wxLEFT | wxRIGHT | wxTOP, 9);

    m_streamingText = new wxTextCtrl(bubble, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxSize(620, 120),
                                     wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE);
    m_streamingText->SetBackgroundColour(bubble->GetBackgroundColour());
    m_streamingText->SetForegroundColour(wxColour(235, 235, 235));
    bubbleSizer->Add(m_streamingText, 1, wxEXPAND | wxALL, 8);
    bubble->SetSizer(bubbleSizer);
    bubble->SetMinSize(wxSize(360, -1));
    rowSizer->Add(bubble, 0, wxEXPAND | wxALL, 6);
    rowSizer->AddStretchSpacer(1);
    row->SetSizer(rowSizer);
    m_historySizer->Add(row, 0, wxEXPAND);
    m_historyWindow->Layout();
    m_historyWindow->FitInside();
}

void AIChatPage::AppendStreamingChunk(const wxString& text)
{
    if (!m_streamingText || text.IsEmpty())
        return;
    m_streamingBuffer += text;
    m_streamingText->AppendText(text);

    int visualLines = 2;
    for (size_t i = 0; i < m_streamingBuffer.length(); ++i)
        if (m_streamingBuffer[i] == '\n') ++visualLines;
    visualLines += static_cast<int>(m_streamingBuffer.length() / 90);
    visualLines = std::max(4, std::min(26, visualLines));
    m_streamingText->SetMinSize(wxSize(620, 24 + visualLines * 18));
    m_historyWindow->Layout();
    m_historyWindow->FitInside();
    m_historyWindow->Scroll(0, m_historyWindow->GetScrollRange(wxVERTICAL));
}

void AIChatPage::EndStreamingMessage()
{
    m_streamingText = nullptr;
    m_streamingBuffer.clear();
    m_historyWindow->Layout();
    m_historyWindow->FitInside();
}

wxString AIChatPage::BuildSystemPrompt() const
{
    wxString system =
        "You are the coding assistant embedded in DoDevEditor. Be precise and practical. "
        "When context contains source code, treat it as the user's current editor context. "
        "Do not claim you changed files unless the user explicitly applies a suggested change.";

    if (!m_contextProvider)
        return system;

    const AIEditorContext context = m_contextProvider();
    if (m_includeSelection->GetValue() && !context.selection.IsEmpty())
    {
        system += "\n\nCURRENT SELECTION";
        if (!context.filePath.IsEmpty())
            system += " (" + context.filePath + ")";
        system += ":\n```\n" + LimitText(context.selection, 30000) + "\n```";
    }
    else if (m_includeCurrentFile->GetValue() && !context.currentFileText.IsEmpty())
    {
        system += "\n\nCURRENT FILE";
        if (!context.filePath.IsEmpty())
            system += " (" + context.filePath + ")";
        system += ":\n```\n" + LimitText(context.currentFileText, 70000) + "\n```";
    }

    if (m_includeGitDiff->GetValue() && !context.gitDiff.IsEmpty())
        system += "\n\nACTIVE GIT DIFF:\n```diff\n" + LimitText(context.gitDiff, 50000) + "\n```";

    if (m_includeLLVM->GetValue() && !context.llvmContext.IsEmpty())
        system += "\n\nLLVM SYMBOLS / CALL TREE:\n" + LimitText(context.llvmContext, 30000);

    return system;
}

void AIChatPage::SetBusy(bool busy)
{
    m_busy = busy;
    m_sendButton->Enable(!busy);
    m_prompt->Enable(!busy);
    m_providerChoice->Enable(!busy);
    m_sendButton->SetLabel(busy ? "Sending..." : "Send");
}

void AIChatPage::SendPrompt()
{
    if (m_busy)
        return;

    wxString prompt = m_prompt->GetValue();
    prompt.Trim(true).Trim(false);
    if (prompt.IsEmpty())
        return;
    if (!m_settings.enabled)
    {
        AddSystemNotice("AI integration is disabled. Open General Settings to enable it.", true);
        return;
    }

    AIMessage userMessage;
    userMessage.role = "user";
    userMessage.content = prompt;
    m_messages.push_back(userMessage);
    AddMessage("user", prompt);
    m_prompt->Clear();

    AIRequest request;
    request.settings = m_settings;
    request.settings.includeCurrentFile = m_includeCurrentFile->GetValue();
    request.settings.includeSelection = m_includeSelection->GetValue();
    request.settings.includeGitDiff = m_includeGitDiff->GetValue();
    request.settings.includeLLVM = m_includeLLVM->GetValue();
    request.messages = m_messages;
    request.systemPrompt = BuildSystemPrompt();
    if (m_contextProvider)
    {
        const AIEditorContext context = m_contextProvider();
        if (!context.filePath.IsEmpty())
            request.workingDirectory = wxFileName(context.filePath).GetPath();
    }

    SetBusy(true);
    m_status->SetLabel("Sending request to " + AISettings::ProviderName(request.settings.provider) + "...");

    const bool streaming = request.settings.provider == AIProviderKind::Ollama && request.settings.ollamaStream;
    if (streaming)
        BeginStreamingMessage();

    wxWeakRef<AIChatPage> weakThis(this);
    std::thread([weakThis, request, streaming]() mutable
    {
        AIResult result;
        if (streaming)
        {
            result = AIClient::SendStreaming(request, [weakThis](const wxString& chunk)
            {
                if (!wxTheApp)
                    return;
                wxTheApp->CallAfter([weakThis, chunk]()
                {
                    if (weakThis)
                        weakThis->AppendStreamingChunk(chunk);
                });
            });
        }
        else
        {
            result = AIClient::Send(request);
        }

        if (!wxTheApp)
            return;
        wxTheApp->CallAfter([weakThis, result]() mutable
        {
            if (weakThis)
                weakThis->HandleResult(result);
        });
    }).detach();
}

void AIChatPage::HandleResult(const AIResult& result)
{
    SetBusy(false);
    UpdateProviderDisplay();
    if (!result.ok)
    {
        if (m_streamingText)
            EndStreamingMessage();
        AddSystemNotice("AI request failed: " + result.error, true);
        m_prompt->SetFocus();
        return;
    }

    AIMessage assistantMessage;
    assistantMessage.role = "assistant";
    assistantMessage.content = result.text;
    m_messages.push_back(assistantMessage);
    if (m_streamingText)
        EndStreamingMessage();
    else
        AddMessage("assistant", result.text);
    m_prompt->SetFocus();
}
