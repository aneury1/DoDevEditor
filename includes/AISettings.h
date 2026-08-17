#pragma once

#include <wx/string.h>

#include "Config.h"

enum class AIProviderKind
{
    OpenAI = 0,
    Anthropic,
    OpenAICompatible,
    GitHubCopilotCLI,
    Ollama,
    LlamaCpp,
    Gemini
};

enum class AISecretSource
{
    Environment = 0,
    Session,
    OSKeyring,
    ExistingLogin,
    None
};

struct AIProviderSettings
{
    bool enabled = true;
    AIProviderKind provider = AIProviderKind::OpenAI;
    wxString model;
    wxString baseUrl;
    AISecretSource secretSource = AISecretSource::Environment;
    wxString environmentVariable = "OPENAI_API_KEY";
    wxString keyringId = "openai";
    wxString copilotExecutable = "copilot";
    int maxOutputTokens = 4096;

    // Native Ollama settings. baseUrl is the server root, e.g. http://localhost:11434.
    bool ollamaStream = true;
    bool ollamaThink = false;
    double ollamaTemperature = 0.2;
    int ollamaContext = 32768;
    wxString ollamaKeepAlive = "5m";

    // llama.cpp server settings. baseUrl is the server root, e.g. http://127.0.0.1:8080.
    bool llamaCppStream = true;
    double llamaCppTemperature = 0.2;

    // Gemini generation settings.
    double geminiTemperature = 0.2;

    bool includeCurrentFile = true;
    bool includeSelection = true;
    bool includeGitDiff = true;
    bool includeLLVM = true;
};

class AISettings
{
public:
    static AIProviderSettings Load();
    static void Save(const AIProviderSettings& settings);

    static wxString ProviderName(AIProviderKind provider);
    static wxString DefaultBaseUrl(AIProviderKind provider);
    static wxString DefaultEnvironmentVariable(AIProviderKind provider);
    static wxString DefaultKeyringId(AIProviderKind provider);
};
