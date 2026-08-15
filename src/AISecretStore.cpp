#include "AISecretStore.h"

#include <wx/utils.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

std::map<std::string, wxString> AISecretStore::s_sessionSecrets;
std::mutex AISecretStore::s_mutex;

std::string AISecretStore::MapKey(const wxString& value)
{
    const wxCharBuffer buffer = value.ToUTF8();
    return buffer.data() ? std::string(buffer.data()) : std::string();
}

wxString AISecretStore::QuoteShell(const wxString& value)
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

void AISecretStore::SetSessionSecret(const wxString& key, const wxString& secret)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (secret.IsEmpty())
        s_sessionSecrets.erase(MapKey(key));
    else
        s_sessionSecrets[MapKey(key)] = secret;
}

bool AISecretStore::HasSessionSecret(const wxString& key)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    const auto it = s_sessionSecrets.find(MapKey(key));
    return it != s_sessionSecrets.end() && !it->second.IsEmpty();
}

bool AISecretStore::IsKeyringAvailable()
{
#ifdef __WXMSW__
    return false;
#else
    FILE* pipe = popen("command -v secret-tool 2>/dev/null", "r");
    if (!pipe)
        return false;
    char buffer[256] = {};
    const bool found = std::fgets(buffer, sizeof(buffer), pipe) != nullptr;
    const int code = pclose(pipe);
    return found && code == 0;
#endif
}

bool AISecretStore::StoreInKeyring(const wxString& key, const wxString& secret, wxString* error)
{
    if (error)
        *error = wxString();
#ifdef __WXMSW__
    if (error)
        *error = "OS keyring storage is currently implemented for Linux via secret-tool.";
    return false;
#else
    if (!IsKeyringAvailable())
    {
        if (error)
            *error = "secret-tool is not installed. Install libsecret tools or choose Environment/Session Secret.";
        return false;
    }
    if (secret.IsEmpty())
    {
        if (error)
            *error = "Secret cannot be empty.";
        return false;
    }

    const wxString command = "secret-tool store --label=" + QuoteShell("DoDevEditor AI secret") +
                             " application DoDevEditor provider " + QuoteShell(key);
    const wxCharBuffer commandBuffer = command.ToUTF8();
    if (!commandBuffer.data())
    {
        if (error)
            *error = "Unable to encode keyring command.";
        return false;
    }

    FILE* pipe = popen(commandBuffer.data(), "w");
    if (!pipe)
    {
        if (error)
            *error = "Unable to start secret-tool.";
        return false;
    }

    const wxCharBuffer secretBuffer = secret.ToUTF8();
    if (!secretBuffer.data())
    {
        pclose(pipe);
        if (error)
            *error = "Unable to encode secret.";
        return false;
    }

    std::fwrite(secretBuffer.data(), 1, std::strlen(secretBuffer.data()), pipe);
    std::fwrite("\n", 1, 1, pipe);
    const int code = pclose(pipe);
    if (code != 0)
    {
        if (error)
            *error = "secret-tool failed to store the secret.";
        return false;
    }
    return true;
#endif
}

bool AISecretStore::ClearKeyring(const wxString& key, wxString* error)
{
    if (error)
        *error = wxString();
#ifdef __WXMSW__
    if (error)
        *error = "OS keyring storage is currently implemented for Linux via secret-tool.";
    return false;
#else
    if (!IsKeyringAvailable())
    {
        if (error)
            *error = "secret-tool is not installed.";
        return false;
    }
    const wxString command = "secret-tool clear application DoDevEditor provider " + QuoteShell(key) + " >/dev/null 2>&1";
    const wxCharBuffer commandBuffer = command.ToUTF8();
    const int code = commandBuffer.data() ? std::system(commandBuffer.data()) : -1;
    if (code != 0)
    {
        if (error)
            *error = "secret-tool failed to clear the secret.";
        return false;
    }
    return true;
#endif
}

wxString AISecretStore::Resolve(const AIProviderSettings& settings, wxString* error)
{
    if (error)
        *error = wxString();

    if (settings.provider == AIProviderKind::GitHubCopilotCLI &&
        settings.secretSource == AISecretSource::ExistingLogin)
        return wxString();

    switch (settings.secretSource)
    {
    case AISecretSource::Environment:
    {
        wxString value;
        if (settings.environmentVariable.IsEmpty() || !wxGetEnv(settings.environmentVariable, &value) || value.IsEmpty())
        {
            if (error)
                *error = "Environment variable '" + settings.environmentVariable + "' is not set.";
            return wxString();
        }
        return value;
    }
    case AISecretSource::Session:
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const auto it = s_sessionSecrets.find(MapKey(settings.keyringId));
        if (it == s_sessionSecrets.end() || it->second.IsEmpty())
        {
            if (error)
                *error = "No session secret is configured. Open General Settings and enter one.";
            return wxString();
        }
        return it->second;
    }
    case AISecretSource::OSKeyring:
    {
#ifdef __WXMSW__
        if (error)
            *error = "OS keyring lookup is currently implemented for Linux via secret-tool.";
        return wxString();
#else
        if (!IsKeyringAvailable())
        {
            if (error)
                *error = "secret-tool is not installed.";
            return wxString();
        }
        const wxString command = "secret-tool lookup application DoDevEditor provider " + QuoteShell(settings.keyringId) + " 2>/dev/null";
        const wxCharBuffer commandBuffer = command.ToUTF8();
        if (!commandBuffer.data())
        {
            if (error)
                *error = "Unable to encode keyring lookup command.";
            return wxString();
        }
        FILE* pipe = popen(commandBuffer.data(), "r");
        if (!pipe)
        {
            if (error)
                *error = "Unable to start secret-tool.";
            return wxString();
        }
        std::string raw;
        char buffer[512];
        while (std::fgets(buffer, sizeof(buffer), pipe))
            raw += buffer;
        const int code = pclose(pipe);
        while (!raw.empty() && (raw.back() == '\n' || raw.back() == '\r'))
            raw.pop_back();
        if (code != 0 || raw.empty())
        {
            if (error)
                *error = "No OS keyring secret was found for '" + settings.keyringId + "'.";
            return wxString();
        }
        return wxString::FromUTF8(raw.c_str(), raw.size());
#endif
    }
    case AISecretSource::ExistingLogin:
    case AISecretSource::None:
        return wxString();
    }

    if (error)
        *error = "Unknown secret source.";
    return wxString();
}
