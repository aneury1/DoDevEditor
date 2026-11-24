#include <filesystem>
#include <stdexcept>
#include <string>

#include "AiClient.h"
#include "AiProfiles.h"
#include "dodev/plugins/PluginManager.h"
#include "dodev/settings/Json.h"
#define REQUIRE(x)                                                            \
  do {                                                                        \
    if (!(x)) throw std::runtime_error(std::string("AI test failed: ") + #x); \
  } while (0)
int main() {
  using namespace dodev::ai;
  Config cfg;
  cfg.model = "local-code";
  const auto payload = requestJson(cfg, {}, "Explain this", "int main() {}\n");
  dodev::Json root;
  REQUIRE(dodev::Json::parse(payload, root));
  REQUIRE(root["model"].string() == "local-code");
  REQUIRE(root["messages"].isArray());
  REQUIRE(root["messages"].asArray().size() == 2);
  REQUIRE(root["messages"].asArray()[1]["content"].string().find("int main") !=
          std::string::npos);
  REQUIRE(validEndpoint("https://api.example.com/v1/chat/completions"));
  REQUIRE(validEndpoint("http://127.0.0.1:11434/v1/chat/completions"));
  REQUIRE(!validEndpoint("http://evil.example.com/v1/chat/completions"));
  REQUIRE(!validEndpoint("file:///etc/passwd"));
  const auto reply = parseReply(
      200,
      R"({"choices":[{"message":{"role":"assistant","content":"C++ answer"}}]})");
  REQUIRE(reply.success && reply.answer == "C++ answer");
  REQUIRE(!parseReply(429, "{}").success);
  REQUIRE(!parseReply(200, "{}").success);
  REQUIRE(deriveModelsEndpoint("http://127.0.0.1:8080/v1/chat/completions") ==
          "http://127.0.0.1:8080/v1/models");
  REQUIRE(deriveModelsEndpoint("https://example.com/v1/chat/completions") ==
          "https://example.com/v1/models");
  REQUIRE(deriveModelsEndpoint("https://example.com/custom-chat").empty());
  const auto models = parseModels(
      200,
      R"({"object":"list","data":[{"id":"gguf-local"},{"id":"code-model"},{"id":"code-model"}]})");
  REQUIRE(models.success && models.models.size() == 2);
  REQUIRE(models.models[0] == "code-model" && models.models[1] == "gguf-local");
  REQUIRE(!parseModels(200, "{}").success);
  REQUIRE(!parseModels(401, "{}").success);
  auto profiles = defaultProfiles();
  REQUIRE(profiles.size() == 2);
  REQUIRE(profiles[0].config.endpoint.find(":8080/") != std::string::npos);
  ProviderProfile remote;
  remote.id = "remote-1";
  remote.name = "Custom Remote";
  remote.kind = "remote";
  remote.config.endpoint = "https://api.example.com/v1/chat/completions";
  remote.modelsEndpoint = "https://api.example.com/v1/models";
  remote.config.model = "my-model";
  remote.config.authMode = "bearer";
  remote.config.keyEnvironment = "MY_REMOTE_KEY";
  profiles.push_back(remote);
  auto serialized = profilesToJson(profiles);
  std::vector<ProviderProfile> restored;
  REQUIRE(profilesFromJson(serialized, restored));
  REQUIRE(restored.size() == 3 &&
          restored.back().config.keyEnvironment == "MY_REMOTE_KEY");
  REQUIRE(serialized.dump().find("MY_REMOTE_KEY") != std::string::npos);
  REQUIRE(serialized.dump().find("sk-secret") == std::string::npos);
  REQUIRE(!profilesFromJson(dodev::Json::object(), restored));
  const auto settingsPath = std::filesystem::temp_directory_path() /
                            "dodev-ai-provider-settings-test";
  std::filesystem::remove_all(settingsPath);
  {
    dodev::SettingsManager settings(settingsPath);
    settings.load();
    dodev::TemplateRegistry templates;
    dodev::ThemeManager themes;
    dodev::PluginManager manager(settings, templates, themes);
    REQUIRE(manager.updatePluginSettingJson("com.dodev.aiassistant", "profiles",
                                            serialized));
    REQUIRE(manager.updatePluginSetting("com.dodev.aiassistant",
                                        "activeProfileId", "remote-1"));
    const auto saved = settings.loadPluginSettings("com.dodev.aiassistant");
    REQUIRE(saved["profiles"].isArray() &&
            saved["profiles"].asArray().size() == 3);
    REQUIRE(saved["activeProfileId"].string() == "remote-1");
    REQUIRE(manager.pluginSettingJson("com.dodev.aiassistant", "profiles")
                .isArray());
  }
  std::filesystem::remove_all(settingsPath);
  REQUIRE(nextProfileId(profiles, "remote") == "remote-2");
  Config bearer;
  bearer.endpoint = "https://api.example.com/v1/chat/completions";
  bearer.authMode = "bearer";
  bearer.keyEnvironment = "DODEV_AI_TEST_TOKEN";
  const auto unsafe = listModels(bearer, "https://other.example.com/v1/models");
  REQUIRE(!unsafe.success &&
          unsafe.error.find("same origin") != std::string::npos);
  return 0;
}
