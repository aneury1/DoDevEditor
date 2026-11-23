#pragma once
#include "AiClient.h"
#include "dodev/settings/Json.h"
#include <string>
#include <vector>

namespace dodev::ai {
struct ProviderProfile {
    std::string id;
    std::string name;
    std::string kind; // llama.cpp, ollama, remote
    Config config;
    std::string modelsEndpoint; // optional override for nonstandard providers
};

std::vector<ProviderProfile> defaultProfiles();
Json profilesToJson(const std::vector<ProviderProfile>& profiles);
bool profilesFromJson(const Json& source,std::vector<ProviderProfile>& profiles,std::string* reason=nullptr);
bool validateProfile(const ProviderProfile& profile,std::string* reason=nullptr);
std::string nextProfileId(const std::vector<ProviderProfile>& profiles,const std::string& prefix);
} // namespace dodev::ai
