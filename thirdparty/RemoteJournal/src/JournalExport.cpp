#include "JournalExport.h"

#include <chrono>
#include <limits>
#include <stdexcept>
#include <system_error>
#ifdef DODEV_JOURNAL_SQLITE
#include <sqlite3.h>
#endif
namespace dodev::journal {
bool sqliteExportAvailable() {
#ifdef DODEV_JOURNAL_SQLITE
  return true;
#else
  return false;
#endif
}
bool exportSqlite(const Table& table, bool filtered,
                  const std::filesystem::path& path, std::string& error) {
#ifndef DODEV_JOURNAL_SQLITE
  (void)table;
  (void)filtered;
  (void)path;
  error =
      "SQLite export unavailable: rebuild with SQLite3 development libraries";
  return false;
#else
  // Stage the entire database beside the destination, then publish without
  // overwriting.
  std::filesystem::path staging;
  struct Cleanup {
    std::filesystem::path& p;
    ~Cleanup() {
      if (!p.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(p, ec);
      }
    }
  } cleanup{staging};
  struct Db {
    sqlite3* p = nullptr;
    ~Db() {
      if (p) sqlite3_close(p);
    }
  } db;
  struct Stmt {
    sqlite3_stmt* p = nullptr;
    ~Stmt() {
      if (p) sqlite3_finalize(p);
    }
  } stmt;
  try {
    if (path.empty() || std::filesystem::exists(path))
      throw std::runtime_error(
          "Choose a new SQLite database filename (existing files are not "
          "overwritten)");
    auto base = path.parent_path();
    if (base.empty()) base = std::filesystem::current_path();
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    for (int i = 0; i < 100; ++i) {
      auto candidate = base / (".journal-export-" + std::to_string(stamp) +
                               "-" + std::to_string(i));
      if (std::filesystem::create_directory(candidate)) {
        staging = candidate;
        break;
      }
    }
    if (staging.empty())
      throw std::runtime_error("Cannot create SQLite staging directory");
#ifndef _WIN32
    std::filesystem::permissions(staging, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
#endif
    auto temp = staging / "logs.sqlite";
    auto check = [&](int rc) {
      if (rc != SQLITE_OK)
        throw std::runtime_error(db.p ? sqlite3_errmsg(db.p)
                                      : "Cannot open SQLite database");
    };
    check(sqlite3_open_v2(temp.u8string().c_str(), &db.p,
                          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr));
    auto sql = [&](const char* command) {
      check(sqlite3_exec(db.p, command, nullptr, nullptr, nullptr));
    };
    sql("BEGIN IMMEDIATE; CREATE TABLE journal_logs (id INTEGER PRIMARY KEY, "
        "timestamp_utc TEXT, timestamp_us INTEGER, priority INTEGER NOT NULL, "
        "level TEXT, host TEXT, service TEXT, process TEXT, pid TEXT, "
        "application_id TEXT, payload TEXT, raw_record BLOB, diagnostic "
        "INTEGER NOT NULL); CREATE TABLE export_metadata (key TEXT PRIMARY "
        "KEY, value TEXT NOT NULL);");
    sql(filtered ? "INSERT INTO export_metadata VALUES "
                   "('scope','visible'),('schema_version','1');"
                 : "INSERT INTO export_metadata VALUES "
                   "('scope','all_buffered'),('schema_version','1');");
    check(sqlite3_prepare_v2(
        db.p, "INSERT INTO journal_logs VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)", -1,
        &stmt.p, nullptr));
    auto add = [&](const Record& r) {
      auto text = [&](int i, const std::string& s) {
        check(sqlite3_bind_text64(stmt.p, i, s.data(), s.size(),
                                  SQLITE_TRANSIENT, SQLITE_UTF8));
      };
      check(sqlite3_bind_int64(stmt.p, 1, static_cast<sqlite3_int64>(r.id)));
      text(2, r.timestamp);
      if (r.timestampMicros &&
          *r.timestampMicros <= static_cast<std::uint64_t>(
                                    std::numeric_limits<sqlite3_int64>::max()))
        check(sqlite3_bind_int64(
            stmt.p, 3, static_cast<sqlite3_int64>(*r.timestampMicros)));
      else
        check(sqlite3_bind_null(stmt.p, 3));
      check(sqlite3_bind_int(stmt.p, 4, r.priority));
      text(5, r.level());
      text(6, r.host);
      text(7, r.unit);
      text(8, r.process);
      text(9, r.pid);
      text(10, r.applicationId);
      text(11, r.message);
      check(sqlite3_bind_blob64(stmt.p, 12, r.raw.data(), r.raw.size(),
                                SQLITE_TRANSIENT));
      check(sqlite3_bind_int(stmt.p, 13, r.diagnostic ? 1 : 0));
      if (sqlite3_step(stmt.p) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(db.p));
      check(sqlite3_reset(stmt.p));
      check(sqlite3_clear_bindings(stmt.p));
    };
    if (filtered) {
      for (auto i : table.visible()) add(table.rows().at(i));
    } else
      for (const auto& r : table.rows()) add(r);
    const int finalStatus = sqlite3_finalize(stmt.p);
    stmt.p = nullptr;
    check(finalStatus);
    sql("CREATE INDEX journal_time ON journal_logs(timestamp_us); CREATE INDEX "
        "journal_app ON journal_logs(application_id); CREATE INDEX "
        "journal_service ON journal_logs(service); CREATE INDEX "
        "journal_priority ON journal_logs(priority); COMMIT;");
    check(sqlite3_close(db.p));
    db.p = nullptr;
#ifndef _WIN32
    std::filesystem::permissions(temp,
                                 std::filesystem::perms::owner_read |
                                     std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace);
#endif
    // copy_options::none refuses existing destinations, including concurrent
    // exports.
    std::filesystem::copy_file(temp, path, std::filesystem::copy_options::none);
    error.clear();
    return true;
  } catch (const std::exception& e) {
    error = e.what();
    return false;
  }
#endif
}
}  // namespace dodev::journal
