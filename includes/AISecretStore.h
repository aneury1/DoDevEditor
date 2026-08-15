#pragma once

#include <wx/string.h>
#include <map>
#include <mutex>

#include "AISettings.h"

class AISecretStore
{
public:
    static wxString Resolve(const AIProviderSettings& settings, wxString* error = nullptr);
    static void SetSessionSecret(const wxString& key, const wxString& secret);
    static bool HasSessionSecret(const wxString& key);
    static bool StoreInKeyring(const wxString& key, const wxString& secret, wxString* error = nullptr);
    static bool ClearKeyring(const wxString& key, wxString* error = nullptr);
    static bool IsKeyringAvailable();

private:
    static std::string MapKey(const wxString& value);
    static wxString QuoteShell(const wxString& value);
    static std::map<std::string, wxString> s_sessionSecrets;
    static std::mutex s_mutex;
};
