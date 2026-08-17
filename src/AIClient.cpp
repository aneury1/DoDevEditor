#include "AIClient.h"

#include "AISecretStore.h"

#include <wx/ffile.h>
#include <wx/filename.h>
#include <wx/filefn.h>
#include <wx/utils.h>

#include <json/json.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <utility>

#ifndef __WXMSW__
#include <sys/stat.h>
#include <sys/wait.h>
#endif

namespace
{
std::string ToUtf8(const wxString& value)
{
    const wxCharBuffer buffer = value.ToUTF8();
    return buffer.data() ? std::string(buffer.data()) : std::string();
}

wxString FromUtf8(const std::string& value)
{
    return wxString::FromUTF8(value.c_str(), value.size());
}


wxString QuoteShell(const wxString& value)
{
#ifdef __WXMSW__
    wxString escaped = value;
    escaped.Replace("\"", "\\\"");
    return wxString("\"") + escaped + "\"";
#else
    wxString escaped = value;
    escaped.Replace("'", "'\"'\"'");
    return wxString("'") + escaped + "'";
#endif
}

std::string CurlConfigQuote(const wxString& value)
{
    std::string text = ToUtf8(value);
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (char c : text)
    {
        if (c == '\\' || c == '"')
            escaped.push_back('\\');
        if (c == '\n')
        {
            escaped += "\\n";
            continue;
        }
        if (c == '\r')
            continue;
        escaped.push_back(c);
    }
    return escaped;
}

bool WritePrivateTextFile(const wxString& path, const std::string& content)
{
    std::ofstream file(ToUtf8(path), std::ios::binary | std::ios::trunc);
    if (!file)
        return false;
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    file.close();
#ifndef __WXMSW__
    chmod(ToUtf8(path).c_str(), S_IRUSR | S_IWUSR);
#endif
    return true;
}

long NormalizeSystemCode(int code)
{
    if (code == -1)
        return -1;
#ifndef __WXMSW__
    if (WIFEXITED(code))
        return WEXITSTATUS(code);
    if (WIFSIGNALED(code))
        return 128 + WTERMSIG(code);
#endif
    return code;
}

wxString ReadTextFile(const wxString& path)
{
    std::ifstream file(ToUtf8(path), std::ios::binary);
    if (!file)
        return wxString();
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return FromUtf8(buffer.str());
}

long RunCommandCapture(const wxString& command, wxString& output, wxString& error)
{
    const wxString outputPath = wxFileName::CreateTempFileName("dodev-ai-out-");
    const wxString errorPath = wxFileName::CreateTempFileName("dodev-ai-err-");
    if (outputPath.IsEmpty() || errorPath.IsEmpty())
    {
        error = "Unable to create command output files.";
        return -1;
    }

    const wxString redirected = command + " >" + QuoteShell(outputPath) + " 2>" + QuoteShell(errorPath);
    const int rawCode = std::system(ToUtf8(redirected).c_str());
    output = ReadTextFile(outputPath);
    error = ReadTextFile(errorPath);
    wxRemoveFile(outputPath);
    wxRemoveFile(errorPath);
    return NormalizeSystemCode(rawCode);
}

struct CurlResult
{
    long code = -1;
    wxString output;
    wxString error;
};

CurlResult PostJson(const wxString& url,
                    const std::vector<std::pair<wxString, wxString>>& headers,
                    const std::string& body)
{
    CurlResult result;
    const wxString bodyPath = wxFileName::CreateTempFileName("dodev-ai-body-");
    const wxString configPath = wxFileName::CreateTempFileName("dodev-ai-curl-");

    if (bodyPath.IsEmpty() || configPath.IsEmpty())
    {
        result.error = "Unable to create temporary request files.";
        return result;
    }

    if (!WritePrivateTextFile(bodyPath, body))
    {
        result.error = "Unable to write the temporary AI request body.";
        wxRemoveFile(bodyPath);
        wxRemoveFile(configPath);
        return result;
    }

    std::ostringstream cfg;
    cfg << "silent\n";
    cfg << "show-error\n";
    cfg << "fail-with-body\n";
    cfg << "request = \"POST\"\n";
    cfg << "connect-timeout = 20\n";
    cfg << "max-time = 180\n";
    cfg << "url = \"" << CurlConfigQuote(url) << "\"\n";
    for (const auto& header : headers)
    {
        cfg << "header = \"" << CurlConfigQuote(header.first + ": " + header.second) << "\"\n";
    }
    cfg << "data-binary = \"@" << CurlConfigQuote(bodyPath) << "\"\n";

    if (!WritePrivateTextFile(configPath, cfg.str()))
    {
        result.error = "Unable to write the temporary curl configuration.";
        wxRemoveFile(bodyPath);
        wxRemoveFile(configPath);
        return result;
    }

    result.code = RunCommandCapture("curl --config " + QuoteShell(configPath), result.output, result.error);

    wxRemoveFile(bodyPath);
    wxRemoveFile(configPath);
    return result;
}


CurlResult GetUrl(const wxString& url,
                  const std::vector<std::pair<wxString, wxString>>& headers = {})
{
    CurlResult result;
    const wxString configPath = wxFileName::CreateTempFileName("dodev-ai-curl-get-");
    if (configPath.IsEmpty())
    {
        result.error = "Unable to create temporary curl configuration.";
        return result;
    }

    std::ostringstream cfg;
    cfg << "silent\n";
    cfg << "show-error\n";
    cfg << "fail-with-body\n";
    cfg << "connect-timeout = 5\n";
    cfg << "max-time = 20\n";
    cfg << "url = \"" << CurlConfigQuote(url) << "\"\n";
    for (const auto& header : headers)
        cfg << "header = \"" << CurlConfigQuote(header.first + ": " + header.second) << "\"\n";
    if (!WritePrivateTextFile(configPath, cfg.str()))
    {
        result.error = "Unable to write temporary curl configuration.";
        wxRemoveFile(configPath);
        return result;
    }

    result.code = RunCommandCapture("curl --config " + QuoteShell(configPath), result.output, result.error);
    wxRemoveFile(configPath);
    return result;
}

wxString TrimTrailingSlashes(wxString value)
{
    while (value.EndsWith("/"))
        value.RemoveLast();
    return value;
}

wxString OllamaEndpoint(const AIProviderSettings& settings, const wxString& endpoint)
{
    wxString base = settings.baseUrl.IsEmpty()
        ? AISettings::DefaultBaseUrl(AIProviderKind::Ollama)
        : settings.baseUrl;
    base = TrimTrailingSlashes(base);
    if (base.EndsWith("/api"))
        return base + "/" + endpoint;
    return base + "/api/" + endpoint;
}

wxString LlamaCppRoot(const AIProviderSettings& settings)
{
    wxString base = settings.baseUrl.IsEmpty()
        ? AISettings::DefaultBaseUrl(AIProviderKind::LlamaCpp)
        : settings.baseUrl;
    base = TrimTrailingSlashes(base);
    const wxString chatSuffix = "/v1/chat/completions";
    if (base.EndsWith(chatSuffix))
        base = base.Left(base.length() - chatSuffix.length());
    else if (base.EndsWith("/v1"))
        base = base.Left(base.length() - 3);
    return TrimTrailingSlashes(base);
}

wxString LlamaCppEndpoint(const AIProviderSettings& settings, const wxString& endpoint)
{
    return LlamaCppRoot(settings) + endpoint;
}

wxString GeminiRoot(const AIProviderSettings& settings)
{
    wxString base = settings.baseUrl.IsEmpty()
        ? AISettings::DefaultBaseUrl(AIProviderKind::Gemini)
        : settings.baseUrl;
    return TrimTrailingSlashes(base);
}

wxString NormalizeGeminiModel(wxString model)
{
    if (model.StartsWith("models/"))
        model = model.Mid(7);
    return model;
}

wxString GeminiGenerateEndpoint(const AIProviderSettings& settings)
{
    return GeminiRoot(settings) + "/models/" + NormalizeGeminiModel(settings.model) + ":generateContent";
}

wxString ResolveProviderSecret(const AIProviderSettings& settings,
                               const wxString& secretOverride,
                               wxString* error)
{
    if (error)
        *error = wxString();
    if (!secretOverride.IsEmpty())
        return secretOverride;
    if (settings.secretSource == AISecretSource::None ||
        settings.secretSource == AISecretSource::ExistingLogin)
        return wxString();
    return AISecretStore::Resolve(settings, error);
}

std::vector<std::pair<wxString, wxString>> BearerHeaders(const wxString& secret)
{
    std::vector<std::pair<wxString, wxString>> headers;
    if (!secret.IsEmpty())
        headers.push_back({"Authorization", "Bearer " + secret});
    return headers;
}

void AppendMessages(Json::Value& array, const std::vector<AIMessage>& messages);

Json::Value BuildOllamaBody(const AIRequest& request, bool stream)
{
    Json::Value body(Json::objectValue);
    body["model"] = ToUtf8(request.settings.model);
    body["stream"] = stream;
    if (!request.settings.ollamaKeepAlive.IsEmpty())
        body["keep_alive"] = ToUtf8(request.settings.ollamaKeepAlive);
    if (request.settings.ollamaThink)
        body["think"] = true;

    Json::Value options(Json::objectValue);
    options["temperature"] = request.settings.ollamaTemperature;
    options["num_ctx"] = request.settings.ollamaContext;
    options["num_predict"] = request.settings.maxOutputTokens;
    body["options"] = options;

    Json::Value messages(Json::arrayValue);
    if (!request.systemPrompt.IsEmpty())
    {
        Json::Value system(Json::objectValue);
        system["role"] = "system";
        system["content"] = ToUtf8(request.systemPrompt);
        messages.append(system);
    }
    AppendMessages(messages, request.messages);
    body["messages"] = messages;
    return body;
}

Json::Value BuildLlamaCppBody(const AIRequest& request, bool stream)
{
    Json::Value body(Json::objectValue);
    body["model"] = ToUtf8(request.settings.model);
    body["stream"] = stream;
    body["temperature"] = request.settings.llamaCppTemperature;
    body["max_tokens"] = request.settings.maxOutputTokens;

    Json::Value messages(Json::arrayValue);
    if (!request.systemPrompt.IsEmpty())
    {
        Json::Value system(Json::objectValue);
        system["role"] = "system";
        system["content"] = ToUtf8(request.systemPrompt);
        messages.append(system);
    }
    AppendMessages(messages, request.messages);
    body["messages"] = messages;
    return body;
}

Json::Value BuildGeminiBody(const AIRequest& request)
{
    Json::Value body(Json::objectValue);
    if (!request.systemPrompt.IsEmpty())
    {
        Json::Value instruction(Json::objectValue);
        Json::Value parts(Json::arrayValue);
        Json::Value part(Json::objectValue);
        part["text"] = ToUtf8(request.systemPrompt);
        parts.append(part);
        instruction["parts"] = parts;
        body["system_instruction"] = instruction;
    }

    Json::Value contents(Json::arrayValue);
    for (const AIMessage& message : request.messages)
    {
        Json::Value content(Json::objectValue);
        content["role"] = message.role == "assistant" ? "model" : "user";
        Json::Value parts(Json::arrayValue);
        Json::Value part(Json::objectValue);
        part["text"] = ToUtf8(message.content);
        parts.append(part);
        content["parts"] = parts;
        contents.append(content);
    }
    body["contents"] = contents;

    Json::Value generation(Json::objectValue);
    generation["temperature"] = request.settings.geminiTemperature;
    generation["maxOutputTokens"] = request.settings.maxOutputTokens;
    body["generationConfig"] = generation;
    return body;
}

bool ParseJson(const wxString& text, Json::Value& root, wxString& error)
{
    const std::string utf8 = ToUtf8(text);
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream stream(utf8);
    if (!Json::parseFromStream(builder, stream, &root, &errors))
    {
        error = "Unable to parse provider response as JSON: " + FromUtf8(errors);
        return false;
    }
    return true;
}

wxString ExtractApiError(const Json::Value& root)
{
    if (root["error"].isString())
        return FromUtf8(root["error"].asString());
    if (root["error"].isObject())
    {
        const Json::Value& error = root["error"];
        if (error["message"].isString())
            return FromUtf8(error["message"].asString());
        if (error["type"].isString())
            return FromUtf8(error["type"].asString());
    }
    if (root["message"].isString())
        return FromUtf8(root["message"].asString());
    return wxString();
}

void AppendMessages(Json::Value& array, const std::vector<AIMessage>& messages)
{
    for (const AIMessage& message : messages)
    {
        Json::Value item(Json::objectValue);
        item["role"] = ToUtf8(message.role);
        item["content"] = ToUtf8(message.content);
        array.append(item);
    }
}

AIResult CurlFailure(const CurlResult& curl, const Json::Value* parsed = nullptr)
{
    AIResult result;
    result.exitCode = curl.code;
    if (parsed)
        result.error = ExtractApiError(*parsed);
    if (result.error.IsEmpty())
        result.error = curl.error;
    if (result.error.IsEmpty())
        result.error = curl.output;
    if (result.error.IsEmpty())
        result.error = "AI provider request failed.";
    return result;
}
}

AIResult AIClient::Send(const AIRequest& request)
{
    AIResult result;
    if (!request.settings.enabled)
    {
        result.error = "AI integration is disabled in General Settings.";
        return result;
    }

    if (request.settings.provider == AIProviderKind::GitHubCopilotCLI)
        return SendCopilotCLI(request);
    if (request.settings.provider == AIProviderKind::Ollama)
        return SendOllama(request, false, StreamCallback());

    wxString secretError;
    wxString secret;
    const bool secretOptional =
        request.settings.provider == AIProviderKind::OpenAICompatible ||
        request.settings.provider == AIProviderKind::LlamaCpp;
    const bool noSecretSelected = request.settings.secretSource == AISecretSource::None;
    const bool emptyOptionalEnvironment =
        secretOptional &&
        request.settings.secretSource == AISecretSource::Environment &&
        request.settings.environmentVariable.IsEmpty();
    if (!noSecretSelected && !emptyOptionalEnvironment)
    {
        secret = AISecretStore::Resolve(request.settings, &secretError);
        if (secret.IsEmpty())
        {
            result.error = secretError.IsEmpty() ? wxString("No API secret is configured.") : secretError;
            return result;
        }
    }

    switch (request.settings.provider)
    {
    case AIProviderKind::OpenAI:
        return SendOpenAI(request, secret);
    case AIProviderKind::Anthropic:
        return SendAnthropic(request, secret);
    case AIProviderKind::OpenAICompatible:
        return SendOpenAICompatible(request, secret);
    case AIProviderKind::Ollama:
        return SendOllama(request, false, StreamCallback());
    case AIProviderKind::GitHubCopilotCLI:
        return SendCopilotCLI(request);
    case AIProviderKind::LlamaCpp:
        return SendLlamaCpp(request, secret, false, StreamCallback());
    case AIProviderKind::Gemini:
        return SendGemini(request, secret);
    }

    result.error = "Unknown AI provider.";
    return result;
}


AIResult AIClient::SendStreaming(const AIRequest& request, StreamCallback onChunk)
{
    if (!request.settings.enabled)
    {
        AIResult result;
        result.error = "AI integration is disabled in General Settings.";
        return result;
    }
    if (request.settings.provider == AIProviderKind::Ollama && request.settings.ollamaStream)
        return SendOllama(request, true, std::move(onChunk));
    if (request.settings.provider == AIProviderKind::LlamaCpp && request.settings.llamaCppStream)
    {
        wxString secretError;
        wxString secret;
        const bool emptyOptionalEnvironment =
            request.settings.secretSource == AISecretSource::Environment &&
            request.settings.environmentVariable.IsEmpty();
        if (request.settings.secretSource != AISecretSource::None && !emptyOptionalEnvironment)
        {
            secret = AISecretStore::Resolve(request.settings, &secretError);
            if (secret.IsEmpty())
            {
                AIResult result;
                result.error = secretError.IsEmpty() ? wxString("No llama.cpp API secret is configured.") : secretError;
                return result;
            }
        }
        return SendLlamaCpp(request, secret, true, std::move(onChunk));
    }

    AIResult result = Send(request);
    if (result.ok && onChunk)
        onChunk(result.text);
    return result;
}

std::vector<wxString> AIClient::ListOllamaModels(const AIProviderSettings& settings, wxString* error)
{
    if (error)
        *error = wxString();
    std::vector<wxString> models;
    const CurlResult curl = GetUrl(OllamaEndpoint(settings, "tags"));
    Json::Value root;
    wxString parseError;
    const bool parsed = ParseJson(curl.output, root, parseError);
    if (curl.code != 0 || !parsed)
    {
        if (error)
        {
            *error = curl.error;
            if (error->IsEmpty()) *error = parsed ? ExtractApiError(root) : parseError;
            if (error->IsEmpty()) *error = curl.output;
        }
        return models;
    }

    if (root["models"].isArray())
    {
        for (const Json::Value& item : root["models"])
        {
            if (item["name"].isString())
                models.push_back(FromUtf8(item["name"].asString()));
            else if (item["model"].isString())
                models.push_back(FromUtf8(item["model"].asString()));
        }
    }
    std::sort(models.begin(), models.end(), [](const wxString& a, const wxString& b)
    {
        return a.CmpNoCase(b) < 0;
    });
    return models;
}

OllamaStatus AIClient::QueryOllamaStatus(const AIProviderSettings& settings)
{
    OllamaStatus status;
    status.models = ListOllamaModels(settings, &status.error);
    if (!status.error.IsEmpty())
        return status;

    const CurlResult versionCurl = GetUrl(OllamaEndpoint(settings, "version"));
    Json::Value versionRoot;
    wxString versionParseError;
    if (versionCurl.code == 0 && ParseJson(versionCurl.output, versionRoot, versionParseError) &&
        versionRoot["version"].isString())
    {
        status.version = FromUtf8(versionRoot["version"].asString());
    }

    const CurlResult curl = GetUrl(OllamaEndpoint(settings, "ps"));
    Json::Value root;
    wxString parseError;
    if (curl.code == 0 && ParseJson(curl.output, root, parseError) && root["models"].isArray())
    {
        for (const Json::Value& item : root["models"])
        {
            if (item["name"].isString())
                status.runningModels.push_back(FromUtf8(item["name"].asString()));
            else if (item["model"].isString())
                status.runningModels.push_back(FromUtf8(item["model"].asString()));
        }
    }
    status.ok = true;
    return status;
}

std::vector<wxString> AIClient::ListLlamaCppModels(const AIProviderSettings& settings,
                                                  const wxString& secretOverride,
                                                  wxString* error)
{
    if (error)
        *error = wxString();

    wxString secretError;
    const wxString secret = ResolveProviderSecret(settings, secretOverride, &secretError);
    if (settings.secretSource != AISecretSource::None && secret.IsEmpty() && !secretError.IsEmpty())
    {
        if (error)
            *error = secretError;
        return {};
    }

    std::vector<std::pair<wxString, wxString>> headers = BearerHeaders(secret);
    const CurlResult curl = GetUrl(LlamaCppEndpoint(settings, "/v1/models"), headers);
    Json::Value root;
    wxString parseError;
    const bool parsed = ParseJson(curl.output, root, parseError);
    if (curl.code != 0 || !parsed)
    {
        if (error)
        {
            *error = curl.error;
            if (error->IsEmpty()) *error = parsed ? ExtractApiError(root) : parseError;
            if (error->IsEmpty()) *error = curl.output;
        }
        return {};
    }

    std::vector<wxString> models;
    if (root["data"].isArray())
    {
        for (const Json::Value& item : root["data"])
        {
            if (item["id"].isString())
                models.push_back(FromUtf8(item["id"].asString()));
        }
    }
    std::sort(models.begin(), models.end(), [](const wxString& a, const wxString& b)
    {
        return a.CmpNoCase(b) < 0;
    });
    return models;
}

ProviderStatus AIClient::QueryLlamaCppStatus(const AIProviderSettings& settings,
                                             const wxString& secretOverride)
{
    ProviderStatus status;
    const CurlResult health = GetUrl(LlamaCppEndpoint(settings, "/health"));
    Json::Value healthRoot;
    wxString parseError;
    if (health.code != 0 || !ParseJson(health.output, healthRoot, parseError))
    {
        status.error = health.error;
        if (status.error.IsEmpty()) status.error = parseError;
        if (status.error.IsEmpty()) status.error = health.output;
        return status;
    }
    if (!healthRoot["status"].isString() || healthRoot["status"].asString() != "ok")
    {
        status.error = ExtractApiError(healthRoot);
        if (status.error.IsEmpty()) status.error = "llama.cpp server is not ready.";
        return status;
    }

    status.models = ListLlamaCppModels(settings, secretOverride, &status.error);
    if (!status.error.IsEmpty())
        return status;

    status.detail = wxString::Format("Ready — %zu model(s) exposed", status.models.size());
    status.ok = true;
    return status;
}

std::vector<wxString> AIClient::ListGeminiModels(const AIProviderSettings& settings,
                                                const wxString& secretOverride,
                                                wxString* error)
{
    if (error)
        *error = wxString();

    wxString secretError;
    const wxString secret = ResolveProviderSecret(settings, secretOverride, &secretError);
    if (secret.IsEmpty())
    {
        if (error)
            *error = secretError.IsEmpty() ? wxString("A Gemini API key is required.") : secretError;
        return {};
    }

    const CurlResult curl = GetUrl(GeminiRoot(settings) + "/models?pageSize=1000",
                                   {{"x-goog-api-key", secret}});
    Json::Value root;
    wxString parseError;
    const bool parsed = ParseJson(curl.output, root, parseError);
    if (curl.code != 0 || !parsed)
    {
        if (error)
        {
            *error = curl.error;
            if (error->IsEmpty()) *error = parsed ? ExtractApiError(root) : parseError;
            if (error->IsEmpty()) *error = curl.output;
        }
        return {};
    }

    std::vector<wxString> models;
    if (root["models"].isArray())
    {
        for (const Json::Value& item : root["models"])
        {
            if (!item["name"].isString())
                continue;
            bool supportsGenerate = true;
            if (item["supportedGenerationMethods"].isArray())
            {
                supportsGenerate = false;
                for (const Json::Value& method : item["supportedGenerationMethods"])
                {
                    if (method.isString() && method.asString() == "generateContent")
                    {
                        supportsGenerate = true;
                        break;
                    }
                }
            }
            if (supportsGenerate)
                models.push_back(NormalizeGeminiModel(FromUtf8(item["name"].asString())));
        }
    }
    std::sort(models.begin(), models.end(), [](const wxString& a, const wxString& b)
    {
        return a.CmpNoCase(b) < 0;
    });
    return models;
}

ProviderStatus AIClient::QueryGeminiStatus(const AIProviderSettings& settings,
                                           const wxString& secretOverride)
{
    ProviderStatus status;
    status.models = ListGeminiModels(settings, secretOverride, &status.error);
    if (!status.error.IsEmpty())
        return status;
    status.detail = wxString::Format("Connected — %zu generateContent model(s) available", status.models.size());
    status.ok = true;
    return status;
}

AIResult AIClient::SendOllama(const AIRequest& request, bool stream, StreamCallback onChunk)
{
    AIResult result;
    if (request.settings.model.IsEmpty())
    {
        result.error = "Select an Ollama model in General Settings.";
        return result;
    }

    const Json::Value body = BuildOllamaBody(request, stream);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    const std::string bodyText = Json::writeString(writer, body);
    const wxString endpoint = OllamaEndpoint(request.settings, "chat");

    if (!stream)
    {
        const CurlResult curl = PostJson(endpoint, {{"Content-Type", "application/json"}}, bodyText);
        Json::Value root;
        wxString parseError;
        const bool parsed = ParseJson(curl.output, root, parseError);
        if (curl.code != 0)
            return CurlFailure(curl, parsed ? &root : nullptr);
        if (!parsed)
        {
            result.error = parseError;
            return result;
        }
        if (root["message"]["content"].isString())
            result.text = FromUtf8(root["message"]["content"].asString());
        if (result.text.IsEmpty())
        {
            result.error = ExtractApiError(root);
            if (result.error.IsEmpty()) result.error = "Ollama returned no chat content.";
            return result;
        }
        result.ok = true;
        result.exitCode = 0;
        return result;
    }

    const wxString bodyPath = wxFileName::CreateTempFileName("dodev-ollama-body-");
    if (bodyPath.IsEmpty() || !WritePrivateTextFile(bodyPath, bodyText))
    {
        result.error = "Unable to create temporary Ollama request body.";
        return result;
    }

    wxString command = "curl --no-buffer --silent --show-error --fail-with-body --max-time 600 ";
    command += "-H " + QuoteShell("Content-Type: application/json") + " ";
    command += "--data-binary " + QuoteShell("@" + bodyPath) + " ";
    command += QuoteShell(endpoint) + " 2>&1";
    const wxCharBuffer commandBuffer = command.ToUTF8();
#ifdef __WXMSW__
    FILE* pipe = commandBuffer.data() ? _popen(commandBuffer.data(), "r") : nullptr;
#else
    FILE* pipe = commandBuffer.data() ? popen(commandBuffer.data(), "r") : nullptr;
#endif
    if (!pipe)
    {
        wxRemoveFile(bodyPath);
        result.error = "Unable to start curl for Ollama streaming.";
        return result;
    }

    std::string pending;
    char buffer[8192];
    bool providerError = false;
    while (std::fgets(buffer, sizeof(buffer), pipe))
    {
        pending += buffer;
        size_t newline = std::string::npos;
        while ((newline = pending.find('\n')) != std::string::npos)
        {
            std::string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (line.empty())
                continue;
            Json::Value chunk;
            wxString parseError;
            if (!ParseJson(FromUtf8(line), chunk, parseError))
                continue;
            const wxString apiError = ExtractApiError(chunk);
            if (!apiError.IsEmpty())
            {
                result.error = apiError;
                providerError = true;
                continue;
            }
            if (chunk["message"]["content"].isString())
            {
                const wxString text = FromUtf8(chunk["message"]["content"].asString());
                if (!text.IsEmpty())
                {
                    result.text += text;
                    if (onChunk) onChunk(text);
                }
            }
        }
    }
    if (!pending.empty())
    {
        Json::Value chunk;
        wxString parseError;
        if (ParseJson(FromUtf8(pending), chunk, parseError))
        {
            const wxString apiError = ExtractApiError(chunk);
            if (!apiError.IsEmpty())
            {
                result.error = apiError;
                providerError = true;
            }
            else if (chunk["message"]["content"].isString())
            {
                const wxString text = FromUtf8(chunk["message"]["content"].asString());
                if (!text.IsEmpty())
                {
                    result.text += text;
                    if (onChunk) onChunk(text);
                }
            }
        }
    }
#ifdef __WXMSW__
    const int rawCode = _pclose(pipe);
#else
    const int rawCode = pclose(pipe);
#endif
    wxRemoveFile(bodyPath);
    result.exitCode = NormalizeSystemCode(rawCode);
    if (providerError || result.exitCode != 0)
    {
        if (result.error.IsEmpty())
            result.error = "Ollama streaming request failed.";
        return result;
    }
    if (result.text.IsEmpty())
    {
        result.error = "Ollama returned no streaming chat content.";
        return result;
    }
    result.ok = true;
    return result;
}

AIResult AIClient::SendLlamaCpp(const AIRequest& request, const wxString& secret,
                                bool stream, StreamCallback onChunk)
{
    AIResult result;
    if (request.settings.model.IsEmpty())
    {
        result.error = "Select a llama.cpp model in General Settings.";
        return result;
    }

    const Json::Value body = BuildLlamaCppBody(request, stream);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    const std::string bodyText = Json::writeString(writer, body);
    const wxString endpoint = LlamaCppEndpoint(request.settings, "/v1/chat/completions");

    std::vector<std::pair<wxString, wxString>> headers;
    headers.push_back({"Content-Type", "application/json"});
    if (!secret.IsEmpty())
        headers.push_back({"Authorization", "Bearer " + secret});

    if (!stream)
    {
        const CurlResult curl = PostJson(endpoint, headers, bodyText);
        Json::Value root;
        wxString parseError;
        const bool parsed = ParseJson(curl.output, root, parseError);
        if (curl.code != 0)
            return CurlFailure(curl, parsed ? &root : nullptr);
        if (!parsed)
        {
            result.error = parseError;
            return result;
        }
        if (root["choices"].isArray() && !root["choices"].empty() &&
            root["choices"][0]["message"]["content"].isString())
        {
            result.text = FromUtf8(root["choices"][0]["message"]["content"].asString());
        }
        if (result.text.IsEmpty())
        {
            result.error = ExtractApiError(root);
            if (result.error.IsEmpty()) result.error = "llama.cpp returned no chat content.";
            return result;
        }
        result.ok = true;
        result.exitCode = curl.code;
        return result;
    }

    const wxString bodyPath = wxFileName::CreateTempFileName("dodev-llamacpp-body-");
    const wxString configPath = wxFileName::CreateTempFileName("dodev-llamacpp-curl-");
    if (bodyPath.IsEmpty() || configPath.IsEmpty() || !WritePrivateTextFile(bodyPath, bodyText))
    {
        if (!bodyPath.IsEmpty()) wxRemoveFile(bodyPath);
        if (!configPath.IsEmpty()) wxRemoveFile(configPath);
        result.error = "Unable to create temporary llama.cpp request files.";
        return result;
    }

    std::ostringstream cfg;
    cfg << "no-buffer\n";
    cfg << "silent\n";
    cfg << "show-error\n";
    cfg << "fail-with-body\n";
    cfg << "request = \"POST\"\n";
    cfg << "connect-timeout = 20\n";
    cfg << "max-time = 600\n";
    cfg << "url = \"" << CurlConfigQuote(endpoint) << "\"\n";
    for (const auto& header : headers)
        cfg << "header = \"" << CurlConfigQuote(header.first + ": " + header.second) << "\"\n";
    cfg << "data-binary = \"@" << CurlConfigQuote(bodyPath) << "\"\n";
    if (!WritePrivateTextFile(configPath, cfg.str()))
    {
        wxRemoveFile(bodyPath);
        wxRemoveFile(configPath);
        result.error = "Unable to create temporary llama.cpp curl configuration.";
        return result;
    }

    const wxString command = "curl --config " + QuoteShell(configPath) + " 2>&1";
    const wxCharBuffer commandBuffer = command.ToUTF8();
#ifdef __WXMSW__
    FILE* pipe = commandBuffer.data() ? _popen(commandBuffer.data(), "r") : nullptr;
#else
    FILE* pipe = commandBuffer.data() ? popen(commandBuffer.data(), "r") : nullptr;
#endif
    if (!pipe)
    {
        wxRemoveFile(bodyPath);
        wxRemoveFile(configPath);
        result.error = "Unable to start curl for llama.cpp streaming.";
        return result;
    }

    std::string pending;
    char buffer[8192];
    bool providerError = false;
    while (std::fgets(buffer, sizeof(buffer), pipe))
    {
        pending += buffer;
        size_t newline = std::string::npos;
        while ((newline = pending.find('\n')) != std::string::npos)
        {
            std::string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("data:", 0) == 0)
                line.erase(0, 5);
            while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
                line.erase(line.begin());
            if (line.empty() || line == "[DONE]")
                continue;

            Json::Value chunk;
            wxString parseError;
            if (!ParseJson(FromUtf8(line), chunk, parseError))
                continue;
            const wxString apiError = ExtractApiError(chunk);
            if (!apiError.IsEmpty())
            {
                result.error = apiError;
                providerError = true;
                continue;
            }
            if (chunk["choices"].isArray() && !chunk["choices"].empty() &&
                chunk["choices"][0]["delta"]["content"].isString())
            {
                const wxString text = FromUtf8(chunk["choices"][0]["delta"]["content"].asString());
                if (!text.IsEmpty())
                {
                    result.text += text;
                    if (onChunk) onChunk(text);
                }
            }
        }
    }
#ifdef __WXMSW__
    const int rawCode = _pclose(pipe);
#else
    const int rawCode = pclose(pipe);
#endif
    wxRemoveFile(bodyPath);
    wxRemoveFile(configPath);
    result.exitCode = NormalizeSystemCode(rawCode);
    if (providerError || result.exitCode != 0)
    {
        if (result.error.IsEmpty()) result.error = "llama.cpp streaming request failed.";
        return result;
    }
    if (result.text.IsEmpty())
    {
        result.error = "llama.cpp returned no streaming chat content.";
        return result;
    }
    result.ok = true;
    return result;
}

AIResult AIClient::SendGemini(const AIRequest& request, const wxString& secret)
{
    AIResult result;
    if (request.settings.model.IsEmpty())
    {
        result.error = "Select a Gemini model in General Settings.";
        return result;
    }
    if (secret.IsEmpty())
    {
        result.error = "A Gemini API key is required.";
        return result;
    }

    const Json::Value body = BuildGeminiBody(request);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    const CurlResult curl = PostJson(
        GeminiGenerateEndpoint(request.settings),
        {{"Content-Type", "application/json"}, {"x-goog-api-key", secret}},
        Json::writeString(writer, body));

    Json::Value root;
    wxString parseError;
    const bool parsed = ParseJson(curl.output, root, parseError);
    if (curl.code != 0)
        return CurlFailure(curl, parsed ? &root : nullptr);
    if (!parsed)
    {
        result.error = parseError;
        return result;
    }

    wxString text;
    if (root["candidates"].isArray())
    {
        for (const Json::Value& candidate : root["candidates"])
        {
            if (!candidate["content"]["parts"].isArray())
                continue;
            for (const Json::Value& part : candidate["content"]["parts"])
            {
                if (!part["text"].isString())
                    continue;
                if (!text.IsEmpty()) text += "\n";
                text += FromUtf8(part["text"].asString());
            }
            if (!text.IsEmpty())
                break;
        }
    }
    if (text.IsEmpty())
    {
        result.error = ExtractApiError(root);
        if (result.error.IsEmpty())
            result.error = "Gemini returned no text content.";
        return result;
    }

    result.ok = true;
    result.exitCode = curl.code;
    result.text = text;
    return result;
}

AIResult AIClient::SendOpenAI(const AIRequest& request, const wxString& secret)
{
    AIResult result;
    if (request.settings.model.IsEmpty())
    {
        result.error = "Set an OpenAI model in General Settings.";
        return result;
    }

    Json::Value body(Json::objectValue);
    body["model"] = ToUtf8(request.settings.model);
    body["max_output_tokens"] = request.settings.maxOutputTokens;
    if (!request.systemPrompt.IsEmpty())
        body["instructions"] = ToUtf8(request.systemPrompt);
    Json::Value input(Json::arrayValue);
    AppendMessages(input, request.messages);
    body["input"] = input;

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    const CurlResult curl = PostJson(
        request.settings.baseUrl.IsEmpty() ? AISettings::DefaultBaseUrl(AIProviderKind::OpenAI) : request.settings.baseUrl,
        {{"Authorization", "Bearer " + secret}, {"Content-Type", "application/json"}},
        Json::writeString(writer, body));

    Json::Value root;
    wxString parseError;
    const bool parsed = ParseJson(curl.output, root, parseError);
    if (curl.code != 0)
        return CurlFailure(curl, parsed ? &root : nullptr);
    if (!parsed)
    {
        result.error = parseError;
        return result;
    }

    wxString text;
    if (root["output_text"].isString())
        text = FromUtf8(root["output_text"].asString());
    if (text.IsEmpty() && root["output"].isArray())
    {
        for (const Json::Value& item : root["output"])
        {
            if (!item["content"].isArray())
                continue;
            for (const Json::Value& content : item["content"])
            {
                if (content["text"].isString())
                {
                    if (!text.IsEmpty()) text += "\n";
                    text += FromUtf8(content["text"].asString());
                }
            }
        }
    }

    if (text.IsEmpty())
    {
        result.error = ExtractApiError(root);
        if (result.error.IsEmpty())
            result.error = "OpenAI returned no text output.";
        return result;
    }

    result.ok = true;
    result.exitCode = curl.code;
    result.text = text;
    return result;
}

AIResult AIClient::SendAnthropic(const AIRequest& request, const wxString& secret)
{
    AIResult result;
    if (request.settings.model.IsEmpty())
    {
        result.error = "Set a Claude model in General Settings.";
        return result;
    }

    Json::Value body(Json::objectValue);
    body["model"] = ToUtf8(request.settings.model);
    body["max_tokens"] = request.settings.maxOutputTokens;
    if (!request.systemPrompt.IsEmpty())
        body["system"] = ToUtf8(request.systemPrompt);
    Json::Value messages(Json::arrayValue);
    AppendMessages(messages, request.messages);
    body["messages"] = messages;

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    const CurlResult curl = PostJson(
        request.settings.baseUrl.IsEmpty() ? AISettings::DefaultBaseUrl(AIProviderKind::Anthropic) : request.settings.baseUrl,
        {{"x-api-key", secret}, {"anthropic-version", "2023-06-01"}, {"Content-Type", "application/json"}},
        Json::writeString(writer, body));

    Json::Value root;
    wxString parseError;
    const bool parsed = ParseJson(curl.output, root, parseError);
    if (curl.code != 0)
        return CurlFailure(curl, parsed ? &root : nullptr);
    if (!parsed)
    {
        result.error = parseError;
        return result;
    }

    wxString text;
    if (root["content"].isArray())
    {
        for (const Json::Value& item : root["content"])
        {
            if (item["type"].isString() && item["type"].asString() == "text" && item["text"].isString())
            {
                if (!text.IsEmpty()) text += "\n";
                text += FromUtf8(item["text"].asString());
            }
        }
    }

    if (text.IsEmpty())
    {
        result.error = ExtractApiError(root);
        if (result.error.IsEmpty())
            result.error = "Anthropic returned no text output.";
        return result;
    }

    result.ok = true;
    result.exitCode = curl.code;
    result.text = text;
    return result;
}

AIResult AIClient::SendOpenAICompatible(const AIRequest& request, const wxString& secret)
{
    AIResult result;
    if (request.settings.model.IsEmpty())
    {
        result.error = "Set a model for the OpenAI-compatible provider in General Settings.";
        return result;
    }

    Json::Value body(Json::objectValue);
    body["model"] = ToUtf8(request.settings.model);
    Json::Value messages(Json::arrayValue);
    if (!request.systemPrompt.IsEmpty())
    {
        Json::Value system(Json::objectValue);
        system["role"] = "system";
        system["content"] = ToUtf8(request.systemPrompt);
        messages.append(system);
    }
    AppendMessages(messages, request.messages);
    body["messages"] = messages;

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    std::vector<std::pair<wxString, wxString>> headers;
    headers.push_back({"Content-Type", "application/json"});
    if (!secret.IsEmpty())
        headers.push_back({"Authorization", "Bearer " + secret});

    const CurlResult curl = PostJson(
        request.settings.baseUrl.IsEmpty() ? AISettings::DefaultBaseUrl(AIProviderKind::OpenAICompatible) : request.settings.baseUrl,
        headers,
        Json::writeString(writer, body));

    Json::Value root;
    wxString parseError;
    const bool parsed = ParseJson(curl.output, root, parseError);
    if (curl.code != 0)
        return CurlFailure(curl, parsed ? &root : nullptr);
    if (!parsed)
    {
        result.error = parseError;
        return result;
    }

    wxString text;
    if (root["choices"].isArray() && !root["choices"].empty())
    {
        const Json::Value& message = root["choices"][0]["message"];
        if (message["content"].isString())
            text = FromUtf8(message["content"].asString());
    }
    if (text.IsEmpty())
    {
        result.error = ExtractApiError(root);
        if (result.error.IsEmpty())
            result.error = "The OpenAI-compatible endpoint returned no chat content.";
        return result;
    }

    result.ok = true;
    result.exitCode = curl.code;
    result.text = text;
    return result;
}

AIResult AIClient::SendCopilotCLI(const AIRequest& request)
{
    AIResult result;
    wxString executable = request.settings.copilotExecutable;
    if (executable.IsEmpty())
        executable = "copilot";

    wxString prompt;
    if (!request.systemPrompt.IsEmpty())
        prompt += request.systemPrompt + "\n\n";
    for (const AIMessage& message : request.messages)
    {
        prompt += message.role == "assistant" ? "Assistant:\n" : "User:\n";
        prompt += message.content + "\n\n";
    }

    wxString command = QuoteShell(executable) + " -s --no-color --stream=off";
    // Programmatic chat may inspect repository context, but must not modify
    // files or execute arbitrary shell commands from this embedded chat.
    command += " --deny-tool=write --deny-tool=shell";
    if (!request.settings.model.IsEmpty())
        command += " --model=" + QuoteShell(request.settings.model);
    if (!request.workingDirectory.IsEmpty())
        command += " -C " + QuoteShell(request.workingDirectory);
    command += " -p " + QuoteShell(prompt);

    wxString output;
    wxString errors;
    result.exitCode = RunCommandCapture(command, output, errors);
    if (result.exitCode != 0)
    {
        result.error = errors;
        if (result.error.IsEmpty())
            result.error = output;
        if (result.error.IsEmpty())
            result.error = "GitHub Copilot CLI failed. Run 'copilot login' and verify the CLI is installed.";
        return result;
    }

    result.text = output;
    if (result.text.IsEmpty())
    {
        result.error = "GitHub Copilot CLI returned no response.";
        return result;
    }
    result.ok = true;
    return result;
}
