#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace dodev {
namespace fs = std::filesystem;
struct CsvResult {
    std::vector<std::vector<std::string>> rows;
    bool valid = true;
    std::string error;
};
CsvResult parseCsv(const std::string& source, char separator=',');
std::string writeCsv(const std::vector<std::vector<std::string>>& rows, char separator=',');

struct DltEntry {
    std::string timestamp, ecu, apid, ctid, payload;
    std::uint8_t counter = 0;
};
struct DltResult {
    std::vector<DltEntry> entries;
    std::size_t skipped_bytes = 0;
    std::string error;
};
DltResult parseDlt(const std::vector<std::uint8_t>& data, std::size_t max_messages=100000);

std::string readText(const fs::path& path);
std::vector<std::uint8_t> readBytes(const fs::path& path);
void writeText(const fs::path& path, const std::string& data);
bool looksBinary(const std::vector<std::uint8_t>& bytes);
std::string hexDump(const std::vector<std::uint8_t>& data, std::size_t max_bytes=512*1024);
std::string fileExtension(const fs::path& p);
std::string pathUtf8(const fs::path& p);
std::string quoteShell(const std::string& s);
std::string captureCommand(const std::string& command, std::size_t max_bytes=1024*1024);
struct GitCommit { std::string sha, author, date, subject; };
std::vector<GitCommit> gitHistory(const fs::path& folder, std::string* err=nullptr);
} // namespace dodev
