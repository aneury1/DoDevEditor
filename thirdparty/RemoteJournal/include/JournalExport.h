#pragma once
#include <filesystem>

#include "JournalTable.h"
namespace dodev::journal {
// Exports a snapshot to a new database. Existing files are never overwritten.
bool sqliteExportAvailable();
bool exportSqlite(const Table& table, bool filtered,
                  const std::filesystem::path& path, std::string& error);
}  // namespace dodev::journal
