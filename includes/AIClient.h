#pragma once

#include <wx/string.h>
#include <vector>
#include <functional>

#include "AISettings.h"

struct AIMessage
{
    wxString role;
    wxString content;
};

struct AIRequest
{
    AIProviderSettings settings;
    std::vector<AIMessage> messages;
    wxString systemPrompt;
    wxString workingDirectory;
};

struct AIResult
{
    bool ok = false;
    long exitCode = -1;
    wxString text;
    wxString error;
};

struct OllamaStatus
{
    bool ok = false;
    wxString error;
    wxString version;
    std::vector<wxString> models;
    std::vector<wxString> runningModels;
};

class AIClient
{
public:
    using StreamCallback = std::function<void(const wxString&)>;

    static AIResult Send(const AIRequest& request);
    static AIResult SendStreaming(const AIRequest& request, StreamCallback onChunk);

    static OllamaStatus QueryOllamaStatus(const AIProviderSettings& settings);
    static std::vector<wxString> ListOllamaModels(const AIProviderSettings& settings,
                                                  wxString* error = nullptr);

private:
    static AIResult SendOpenAI(const AIRequest& request, const wxString& secret);
    static AIResult SendAnthropic(const AIRequest& request, const wxString& secret);
    static AIResult SendOpenAICompatible(const AIRequest& request, const wxString& secret);
    static AIResult SendOllama(const AIRequest& request, bool stream, StreamCallback onChunk);
    static AIResult SendCopilotCLI(const AIRequest& request);
};
