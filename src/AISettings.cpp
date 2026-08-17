#include "AISettings.h"

#include <json/json.h>

namespace
{
std::string ToStd(const wxString& value)
{
    const wxCharBuffer buffer = value.ToUTF8();
    return buffer.data() ? std::string(buffer.data()) : std::string();
}

wxString FromJsonString(const Json::Value& value, const wxString& fallback = wxString())
{
    if (!value.isString())
        return fallback;
    return wxString::FromUTF8(value.asCString());
}

int ClampProvider(int value)
{
    if (value < static_cast<int>(AIProviderKind::OpenAI) ||
        value > static_cast<int>(AIProviderKind::Gemini))
        return static_cast<int>(AIProviderKind::OpenAI);
    return value;
}

int ClampSecretSource(int value)
{
    if (value < static_cast<int>(AISecretSource::Environment) ||
        value > static_cast<int>(AISecretSource::None))
        return static_cast<int>(AISecretSource::Environment);
    return value;
}
}

AIProviderSettings AISettings::Load()
{
    AIProviderSettings settings;
    const Json::Value& ai = AppEditorConfig::config["ai"];
    if (!ai.isObject())
    {
        settings.baseUrl = DefaultBaseUrl(settings.provider);
        return settings;
    }

    settings.enabled = ai.get("enabled", true).asBool();
    settings.provider = static_cast<AIProviderKind>(ClampProvider(ai.get("provider", 0).asInt()));
    settings.model = FromJsonString(ai["model"]);
    settings.baseUrl = FromJsonString(ai["base_url"], DefaultBaseUrl(settings.provider));
    settings.secretSource = static_cast<AISecretSource>(ClampSecretSource(ai.get("secret_source", 0).asInt()));
    settings.environmentVariable = FromJsonString(ai["environment_variable"], DefaultEnvironmentVariable(settings.provider));
    settings.keyringId = FromJsonString(ai["keyring_id"], DefaultKeyringId(settings.provider));
    settings.copilotExecutable = FromJsonString(ai["copilot_executable"], "copilot");
    settings.maxOutputTokens = ai.get("max_output_tokens", 4096).asInt();
    settings.ollamaStream = ai.get("ollama_stream", true).asBool();
    settings.ollamaThink = ai.get("ollama_think", false).asBool();
    settings.ollamaTemperature = ai.get("ollama_temperature", 0.2).asDouble();
    if (settings.ollamaTemperature < 0.0) settings.ollamaTemperature = 0.0;
    if (settings.ollamaTemperature > 2.0) settings.ollamaTemperature = 2.0;
    settings.ollamaContext = ai.get("ollama_context", 32768).asInt();
    if (settings.ollamaContext < 512) settings.ollamaContext = 512;
    if (settings.ollamaContext > 1048576) settings.ollamaContext = 1048576;
    settings.ollamaKeepAlive = FromJsonString(ai["ollama_keep_alive"], "5m");
    settings.llamaCppStream = ai.get("llamacpp_stream", true).asBool();
    settings.llamaCppTemperature = ai.get("llamacpp_temperature", 0.2).asDouble();
    if (settings.llamaCppTemperature < 0.0) settings.llamaCppTemperature = 0.0;
    if (settings.llamaCppTemperature > 2.0) settings.llamaCppTemperature = 2.0;
    settings.geminiTemperature = ai.get("gemini_temperature", 0.2).asDouble();
    if (settings.geminiTemperature < 0.0) settings.geminiTemperature = 0.0;
    if (settings.geminiTemperature > 2.0) settings.geminiTemperature = 2.0;
    if (settings.maxOutputTokens < 64)
        settings.maxOutputTokens = 64;
    if (settings.maxOutputTokens > 65536)
        settings.maxOutputTokens = 65536;
    settings.includeCurrentFile = ai.get("include_current_file", true).asBool();
    settings.includeSelection = ai.get("include_selection", true).asBool();
    settings.includeGitDiff = ai.get("include_git_diff", true).asBool();
    settings.includeCodeAnalysis = ai.get("include_code_analysis", true).asBool();
    return settings;
}

void AISettings::Save(const AIProviderSettings& settings)
{
    Json::Value ai(Json::objectValue);
    ai["enabled"] = settings.enabled;
    ai["provider"] = static_cast<int>(settings.provider);
    ai["model"] = ToStd(settings.model);
    ai["base_url"] = ToStd(settings.baseUrl);
    ai["secret_source"] = static_cast<int>(settings.secretSource);
    ai["environment_variable"] = ToStd(settings.environmentVariable);
    ai["keyring_id"] = ToStd(settings.keyringId);
    ai["copilot_executable"] = ToStd(settings.copilotExecutable);
    ai["max_output_tokens"] = settings.maxOutputTokens;
    ai["ollama_stream"] = settings.ollamaStream;
    ai["ollama_think"] = settings.ollamaThink;
    ai["ollama_temperature"] = settings.ollamaTemperature;
    ai["ollama_context"] = settings.ollamaContext;
    ai["ollama_keep_alive"] = ToStd(settings.ollamaKeepAlive);
    ai["llamacpp_stream"] = settings.llamaCppStream;
    ai["llamacpp_temperature"] = settings.llamaCppTemperature;
    ai["gemini_temperature"] = settings.geminiTemperature;
    ai["include_current_file"] = settings.includeCurrentFile;
    ai["include_selection"] = settings.includeSelection;
    ai["include_git_diff"] = settings.includeGitDiff;
    ai["include_code_analysis"] = settings.includeCodeAnalysis;
    AppEditorConfig::config["ai"] = ai;
    AppEditorConfig::Save();
}

wxString AISettings::ProviderName(AIProviderKind provider)
{
    switch (provider)
    {
    case AIProviderKind::OpenAI: return "OpenAI";
    case AIProviderKind::Anthropic: return "Anthropic / Claude";
    case AIProviderKind::OpenAICompatible: return "OpenAI-compatible";
    case AIProviderKind::Ollama: return "Ollama (local)";
    case AIProviderKind::GitHubCopilotCLI: return "GitHub Copilot CLI";
    case AIProviderKind::LlamaCpp: return "llama.cpp (local)";
    case AIProviderKind::Gemini: return "Google Gemini";
    }
    return "OpenAI";
}

wxString AISettings::DefaultBaseUrl(AIProviderKind provider)
{
    switch (provider)
    {
    case AIProviderKind::OpenAI: return "https://api.openai.com/v1/responses";
    case AIProviderKind::Anthropic: return "https://api.anthropic.com/v1/messages";
    case AIProviderKind::OpenAICompatible: return "http://localhost:11434/v1/chat/completions";
    case AIProviderKind::Ollama: return "http://localhost:11434";
    case AIProviderKind::GitHubCopilotCLI: return wxString();
    case AIProviderKind::LlamaCpp: return "http://127.0.0.1:8080";
    case AIProviderKind::Gemini: return "https://generativelanguage.googleapis.com/v1beta";
    }
    return wxString();
}

wxString AISettings::DefaultEnvironmentVariable(AIProviderKind provider)
{
    switch (provider)
    {
    case AIProviderKind::OpenAI: return "OPENAI_API_KEY";
    case AIProviderKind::Anthropic: return "ANTHROPIC_API_KEY";
    case AIProviderKind::OpenAICompatible: return "DODEV_AI_API_KEY";
    case AIProviderKind::Ollama: return wxString();
    case AIProviderKind::GitHubCopilotCLI: return "COPILOT_GITHUB_TOKEN";
    case AIProviderKind::LlamaCpp: return "LLAMA_API_KEY";
    case AIProviderKind::Gemini: return "GEMINI_API_KEY";
    }
    return wxString();
}

wxString AISettings::DefaultKeyringId(AIProviderKind provider)
{
    switch (provider)
    {
    case AIProviderKind::OpenAI: return "openai";
    case AIProviderKind::Anthropic: return "anthropic";
    case AIProviderKind::OpenAICompatible: return "openai-compatible";
    case AIProviderKind::Ollama: return "ollama";
    case AIProviderKind::GitHubCopilotCLI: return "github-copilot";
    case AIProviderKind::LlamaCpp: return "llamacpp";
    case AIProviderKind::Gemini: return "gemini";
    }
    return "ai";
}
