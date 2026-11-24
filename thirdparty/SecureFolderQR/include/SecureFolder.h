#pragma once
#include <filesystem>
#include <string>
namespace secureqr {
void encrypt(const std::filesystem::path& source,
             const std::filesystem::path& output,
             const std::filesystem::path& secret);
void decrypt(const std::filesystem::path& input,
             const std::filesystem::path& output,
             const std::filesystem::path& secret);
}  // namespace secureqr
