#pragma once

#include <wx/wx.h>
#include <wx/scrolwin.h>
#include <functional>
#include <vector>

#include "AIClient.h"

struct AIEditorContext
{
    wxString filePath;
    wxString currentFileText;
    wxString selection;
    wxString gitDiff;
    wxString llvmContext;
};

class AIChatPage : public wxPanel
{
public:
    explicit AIChatPage(wxWindow* parent);

    void SetContextProvider(std::function<AIEditorContext()> callback);
    void SetSettingsCallback(std::function<void()> callback);
    void ReloadSettings();

private:
    wxChoice* m_providerChoice = nullptr;
    wxStaticText* m_status = nullptr;
    wxScrolledWindow* m_historyWindow = nullptr;
    wxBoxSizer* m_historySizer = nullptr;
    wxTextCtrl* m_prompt = nullptr;
    wxButton* m_sendButton = nullptr;
    wxCheckBox* m_includeCurrentFile = nullptr;
    wxCheckBox* m_includeSelection = nullptr;
    wxCheckBox* m_includeGitDiff = nullptr;
    wxCheckBox* m_includeLLVM = nullptr;
    wxTextCtrl* m_streamingText = nullptr;
    wxString m_streamingBuffer;

    AIProviderSettings m_settings;
    std::vector<AIMessage> m_messages;
    std::function<AIEditorContext()> m_contextProvider;
    std::function<void()> m_settingsCallback;
    bool m_busy = false;

    void NewChat();
    void SendPrompt();
    void AddMessage(const wxString& role, const wxString& text);
    void AddSystemNotice(const wxString& text, bool error = false);
    void BeginStreamingMessage();
    void AppendStreamingChunk(const wxString& text);
    void EndStreamingMessage();
    wxString BuildSystemPrompt() const;
    void SetBusy(bool busy);
    void UpdateProviderDisplay();
    void HandleResult(const AIResult& result);
};
