#pragma once
#include <filesystem>
#include <string>
namespace dodevqr {
std::string packPath(const std::filesystem::path& path);
std::string wrapPayload(const std::string& data, bool encrypt,
                        const std::string& password = {});
std::string unwrapPayload(const std::string& text,
                          const std::string& password = {});
bool encryptionAvailable();
}  // namespace dodevqr
