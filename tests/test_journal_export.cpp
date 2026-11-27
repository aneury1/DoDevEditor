#include <sqlite3.h>

#include <cassert>
#include <chrono>
#include <fstream>

#include "JournalExport.h"
#include "JournalFilterState.h"
using namespace dodev::journal;
int main() {
  const std::string first =
      R"({"__REALTIME_TIMESTAMP":"1700000000123456","PRIORITY":"3","APP_ID":"PAYMENT","_SYSTEMD_UNIT":"pay.service","MESSAGE":"timeout\nretry 'quoted'"})";
  Table t;
  RowBatch b;
  b.received = 3;
  b.lines = {first, "SSH diagnostic",
             R"({"MESSAGE":"healthcheck","APP_ID":"PAYMENT","PRIORITY":"6"})"};
  t.ingest(b);
  wxui::JournalFilterState state;
  state.payload = "timeout|retry";
  state.payloadRegex = true;
  state.application = "PAYMENT";
  state.from = "2023-11-14";
  state.to = "2023-11-14";
  TableFilter f;
  std::string error;
  assert(state.compile(f, error));
  t.filter(f);
  assert(t.visible().size() == 1);
  state.payload = "[";
  assert(!state.compile(f, error));
  assert(f.accepts(t.at(0)));
  state.payload = "timeout";
  state.from = "bad date";
  assert(!state.compile(f, error));
  auto dir = std::filesystem::temp_directory_path() /
             ("journal-export-test-" +
              std::to_string(
                  std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(dir);
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() { std::filesystem::remove_all(path); }
  } cleanup{dir};
  assert(sqliteExportAvailable());
  auto visible = dir / "visible.sqlite", all = dir / "all.db";
  assert(exportSqlite(t, true, visible, error));
  assert(exportSqlite(t, false, all, error));
  assert(!exportSqlite(t, false, visible, error));
  auto query = [&](const std::filesystem::path& path, const char* sql) {
    sqlite3* db = nullptr;
    assert(sqlite3_open_v2(path.u8string().c_str(), &db, SQLITE_OPEN_READONLY,
                           nullptr) == SQLITE_OK);
    sqlite3_stmt* stmt = nullptr;
    assert(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK);
    assert(sqlite3_step(stmt) == SQLITE_ROW);
    auto p = sqlite3_column_text(stmt, 0);
    std::string val = p ? std::string(reinterpret_cast<const char*>(p),
                                      sqlite3_column_bytes(stmt, 0))
                        : "";
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return val;
  };
  assert(query(visible, "SELECT count(*) FROM journal_logs") == "1");
  assert(query(all, "SELECT count(*) FROM journal_logs") == "3");
  assert(query(visible, "SELECT timestamp_us FROM journal_logs") ==
         "1700000000123456");
  assert(query(visible, "SELECT payload FROM journal_logs") ==
         "timeout\nretry 'quoted'");
  assert(query(visible, "SELECT CAST(raw_record AS TEXT) FROM journal_logs") ==
         first);
  assert(query(all,
               "SELECT count(*) FROM journal_logs WHERE diagnostic=1 AND "
               "timestamp_us IS NULL") == "1");
  assert(query(all, "PRAGMA integrity_check") == "ok");
  assert(
      query(visible, "SELECT value FROM export_metadata WHERE key='scope'") ==
      "visible");
  assert(query(all, "SELECT value FROM export_metadata WHERE key='scope'") ==
         "all_buffered");
  assert(t.exportText(true).find("healthcheck") == std::string::npos);
  assert(t.exportText(false).find("healthcheck") != std::string::npos);
  auto rulesPath = wxui::journalDefaultFilters(dir);
  std::filesystem::create_directories(rulesPath.parent_path());
  RuleSpec spec;
  spec.name = "hide heartbeat";
  spec.type = "negative";
  spec.payload = "healthcheck";
  Rule rule;
  assert(compileRule(spec, rule, error));
  RuleSet rules;
  rules.rules.push_back(rule);
  std::ofstream(rulesPath) << rules.encode();
  RuleSet loaded;
  assert(wxui::readJournalRules(rulesPath, loaded, error));
  assert(loaded.rules.size() == 1 && !loaded.accepts(t.rows()[2]));
}
