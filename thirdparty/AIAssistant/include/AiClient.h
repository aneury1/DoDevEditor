#pragma once
#include <atomic>
#include <string>
#include <vector>

namespace dodev::ai {
struct Message {std::string role,content;};
struct Config {
    std::string endpoint="http://127.0.0.1:11434/v1/chat/completions";
    std::string model="llama3.2";
    std::string keyEnvironment="OPENAI_API_KEY";
    std::string authMode="auto"; // none, bearer, auto (legacy behavior)
    int timeoutSeconds=45;
    std::size_t maxContextBytes=16000;
};
struct Reply {bool success=false;std::string answer,error;long httpStatus=0;};
struct ModelList {bool success=false;std::vector<std::string> models;std::string error;long httpStatus=0;};
std::string deriveModelsEndpoint(const std::string& chatEndpoint);
ModelList parseModels(long httpStatus,const std::string& response);
ModelList listModels(const Config& config,const std::string& overrideEndpoint={},
                     const std::atomic<bool>* cancel=nullptr);
// Provider-neutral OpenAI-compatible Chat Completions request; Ollama /v1 works.
std::string requestJson(const Config& config,const std::vector<Message>& history,
                        const std::string& question,const std::string& document);
Reply parseReply(long httpStatus,const std::string& response);
bool validEndpoint(const std::string& endpoint,std::string* reason=nullptr);
Reply complete(const Config& config,const std::vector<Message>& history,
               const std::string& question,const std::string& document,
               const std::atomic<bool>* cancel=nullptr);
}
