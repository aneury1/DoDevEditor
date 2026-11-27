#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "JournalRules.h"
#include "JournalTable.h"
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#endif
using namespace dodev::journal;
int main() {
  const std::string line =
      R"({"__REALTIME_TIMESTAMP":"1700000000123456","PRIORITY":"3","_HOSTNAME":"node","_SYSTEMD_UNIT":"api.service","SYSLOG_IDENTIFIER":"api","_PID":"12","MESSAGE":"disk \"full\"\nretry"})";
  auto r = parseRecord(1, line);
  assert(r.timestamp == "2023-11-14 22:13:20.123456Z");
  assert(r.priority == 3 && r.level() == "Error" && r.unit == "api.service" &&
         r.pid == "12");
  assert(r.message == "disk \"full\"\nretry");
  assert(!r.diagnostic);
  auto diagnostic = parseRecord(2, "Permission denied (publickey,password).");
  assert(diagnostic.diagnostic && diagnostic.priority == -1);
  auto binary = parseRecord(3, R"({"MESSAGE":[65,66],"PRIORITY":6})");
  assert(binary.message == "[65,66]" && binary.priority == 6);
  assert(parseRecord(4, R"({"MESSAGE":null})").message.find("omitted") !=
         std::string::npos);
  Table t;
  RowBatch batch;
  batch.received = 3;
  batch.lines = {line, diagnostic.raw, binary.raw};
  t.ingest(batch);
  assert(t.rows().size() == 3 && t.cursor() == 3);
  TableFilter f;
  f.text = "DISK";
  t.filter(f);
  assert(t.visible().size() == 1 && t.at(0).id == 1);
  f.caseSensitive = true;
  t.filter(f);
  assert(t.visible().empty());
  f.caseSensitive = false;
  f.text = "";
  f.priority = 8;
  t.filter(f);
  assert(t.visible().size() == 1 && t.at(0).id == 2);
  // Independent payload/service filters combine with generic text and priority.
  f.priority = -1;
  f.payload = "DISK";
  f.service = "API.SERVICE";
  t.filter(f);
  assert(t.visible().size() == 1 && t.at(0).id == 1);
  f.payload = "api.service";
  t.filter(f);
  assert(t.visible().empty());
  f.payload = "disk";
  f.service = "node";
  t.filter(f);
  assert(t.visible().empty());
  f.service = "api";
  f.caseSensitive = true;
  f.payload = "DISK";
  t.filter(f);
  assert(t.visible().empty());
  f.caseSensitive = false;
  f.payload = "disk";
  f.text = "node";
  t.filter(f);
  assert(t.visible().size() == 1);
  f.text = "";
  f.payload = "";
  f.service = "";
  f.regex.emplace("DISK.*full", std::regex::ECMAScript | std::regex::icase);
  t.filter(f);
  assert(t.visible().size() == 1 && t.at(0).id == 1);
  f.regex.reset();
  std::string rangeError;
  assert(TimeRange::parse("2023-11-14 22:13:20.123456Z",
                          "2023-11-14T22:13:20.123456Z", f.time, rangeError));
  assert(f.time.from == 1700000000123456ULL &&
         f.time.to == 1700000000123456ULL);
  t.filter(f);
  assert(t.visible().size() == 1 && t.at(0).id == 1);
  assert(TimeRange::parse("2023-11-14", "2023-11-14", f.time, rangeError));
  t.filter(f);
  assert(t.visible().size() == 1);
  assert(TimeRange::parse("", "2023-11-14 22:13:20", f.time, rangeError));
  t.filter(f);
  assert(t.visible().size() == 1);
  assert(
      TimeRange::parse("2023-11-14 22:13:20.123457", "", f.time, rangeError));
  t.filter(f);
  assert(t.visible().empty());
  auto previous = f.time;
  assert(!TimeRange::parse("2023-02-29", "", f.time, rangeError) &&
         f.time.from == previous.from);
  assert(!TimeRange::parse("2023-11-15", "2023-11-14", f.time, rangeError));
  assert(!TimeRange::parse("2023-11-14 24:00:00", "", f.time, rangeError));
  assert(
      !TimeRange::parse("2023-11-14T22:13:20+01:00", "", f.time, rangeError));
  assert(!TimeRange::parse("2023-11-14T22:13:20.", "", f.time, rangeError));
  assert(TimeRange::parse("2024-02-29", "2024-02-29", f.time, rangeError));
  assert(*f.time.to - *f.time.from == 86400000000ULL - 1);
  assert(TimeRange::parse("1970-01-01", "1970-01-01 00:00:00", f.time,
                          rangeError));
  assert(f.time.from == 0ULL && f.time.to == 999999ULL);
  assert(TimeRange::parse("", "", f.time, rangeError));
  t.filter(f);
  assert(t.visible().size() == 3);
  auto userUnit = parseRecord(
      5, R"({"MESSAGE":"test","_SYSTEMD_USER_UNIT":"app.service"})");
  assert(userUnit.unit == "app.service");
  f.priority = 3;
  t.filter(f);
  assert(t.exportText(true).find("retry") != std::string::npos);
  assert(t.exportCsv(true).find("disk \"\"full\"\"\nretry") !=
         std::string::npos);
  assert(t.exportCsv(false).find("Permission denied") != std::string::npos);
  RowBatch delta;
  delta.received = 4;
  delta.dropped = 2;
  delta.firstId = 4;
  delta.lines = {line};
  t.ingest(delta);
  assert(t.rows().size() == 2 && t.rows()[0].id == 3 && t.at(0).id == 4);
  t.ingest(delta);
  assert(t.rows().size() == 2);
  t.clear();
  assert(t.cursor() == 0 && t.rows().empty());
  // Independent filters and marker ordering.
  RuleSet rules;
  std::string ruleError;
  auto addRule = [&](RuleSpec s) {
    Rule rule;
    assert(compileRule(s, rule, ruleError));
    rules.rules.push_back(std::move(rule));
  };
  RuleSpec positive;
  positive.name = "disk";
  positive.payload = "disk";
  positive.application = "api";
  addRule(positive);
  positive.name = "binary";
  positive.application = "";
  positive.payload = "65";
  addRule(positive);
  RuleSpec negative;
  negative.name = "exclude binary";
  negative.type = "negative";
  negative.payload = "65";
  addRule(negative);
  RuleSpec marker;
  marker.name = "mark disk";
  marker.type = "marker";
  marker.payload = "disk.*full";
  marker.regex = true;
  marker.background = "#123456";
  addRule(marker);
  marker.name = "second marker";
  marker.background = "#ABCDEF";
  addRule(marker);
  Table multi;
  multi.ingest(batch);
  multi.rules(std::make_shared<const RuleSet>(rules));
  assert(multi.visible().size() == 1 && multi.at(0).id == 1);
  assert(multi.marker(0) && multi.marker(0)->background == "#123456");
  rules.rules[2].spec.enabled = false;
  multi.rules(std::make_shared<const RuleSet>(rules));
  assert(multi.visible().size() == 2 && multi.marker(1) == nullptr);
  rules.enabled = false;
  multi.rules(std::make_shared<const RuleSet>(rules));
  assert(multi.visible().size() == 3 && !multi.marker(0));
  rules.enabled = true;
  rules.rules[0].spec.enabled = false;
  rules.rules[1].spec.enabled = false;
  multi.rules(std::make_shared<const RuleSet>(rules));
  assert(multi.visible().size() == 3 && multi.marker(0));
  RuleSet decoded;
  assert(RuleSet::decode(rules.encode(), decoded, ruleError));
  assert(decoded.rules.size() == 5 && !decoded.rules[0].spec.enabled &&
         decoded.rules[3].spec.regex);
  assert(!RuleSet::decode(R"({"version":2})", decoded, ruleError) &&
         decoded.rules.size() == 5);
  marker.payload = "[";
  Rule invalid;
  assert(!compileRule(marker, invalid, ruleError));
  marker.payload = "disk";
  marker.from = "2023-11-15";
  marker.to = "2023-11-14";
  assert(!compileRule(marker, invalid, ruleError));
  marker.from = "2023-11-14";
  marker.to = "2023-11-14";
  marker.minPriority = 3;
  marker.maxPriority = 3;
  assert(compileRule(marker, invalid, ruleError) && invalid.matches(r) &&
         !invalid.matches(binary));
  TableFilter quick;
  quick.payload = "disk";
  multi.filter(quick);
  assert(multi.visible().size() == 1);
  multi.clear();
  assert(!multi.marker(0));
  multi.ingest(batch);
  multi.ingest(delta);
  assert(multi.visible().size() == 1 && multi.at(0).id == 4 && multi.marker(0));
  multi.ingest(delta);
  assert(multi.visible().size() == 1);
  // No hard-coded filter count: a large rule list remains serializable and
  // usable.
  RuleSet many;
  for (int n = 0; n < 128; ++n) {
    RuleSpec spec;
    spec.name = "rule " + std::to_string(n);
    spec.payload = "no match " + std::to_string(n);
    Rule compiled;
    assert(compileRule(spec, compiled, ruleError));
    many.rules.push_back(std::move(compiled));
  }
  assert(!many.accepts(r));
  many.rules.back().spec.enabled = false;
  assert(RuleSet::decode(many.encode(), decoded, ruleError) &&
         decoded.rules.size() == 128);
  assert(!RuleSet::decode(R"({"version":1,"rules":[{"payload":123}]})", decoded,
                          ruleError) &&
         decoded.rules.size() == 128);
  Options o;
  o.host = "localhost";
  o.user = "tester";
  o.structured = true;
  assert(Session::sshArguments(o).back().find("--output=json") !=
         std::string::npos);
  assert(Session::sshArguments(o).back().find(
             "--output-fields=MESSAGE,PRIORITY") != std::string::npos);
#ifndef _WIN32
  auto dir = std::filesystem::temp_directory_path() /
             ("journal-table-" + std::to_string(getpid()));
  std::filesystem::create_directories(dir);
  auto mock = dir / "ssh";
  {
    std::ofstream s(mock);
    s << "#!/bin/sh\ncat <<'EOF'\n"
      << line << "\n"
      << binary.raw << "\nEOF\nprintf 'SSH final diagnostic without newline'\n";
  }
  chmod(mock.c_str(), 0755);
  auto old = std::string(getenv("PATH") ? getenv("PATH") : "");
  setenv("PATH", (dir.string() + ":" + old).c_str(), 1);
  Session session;
  std::string err;
  assert(session.start(o, &err));
  for (int i = 0; i < 100 && session.running(); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  assert(!session.running());
  auto all = session.rowsAfter(0);
  assert(all.received == 3 && all.lines.size() == 3 && all.firstId == 1);
  assert(all.lines[2] == "SSH final diagnostic without newline");
  auto tail = session.rowsAfter(2);
  assert(tail.firstId == 3 && tail.lines.size() == 1);
  assert(session.rowsAfter(3).lines.empty());
  session.stop();
  session.clear();
  assert(session.rowsAfter(0).received == 0);
  {
    std::ofstream s(mock);
    s << "#!/bin/sh\ni=0; while [ $i -lt 20005 ]; do printf 'row %s\\n' "
         "\"$i\"; i=$((i+1)); done\n";
  }
  assert(session.start(o, &err));
  for (int i = 0; i < 200 && session.running(); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  assert(!session.running());
  all = session.rowsAfter(0);
  assert(all.received == 20005 && all.dropped == 5 && all.firstId == 6 &&
         all.lines.size() == 20000);
  t.clear();
  t.ingest(all);
  assert(t.rows().front().id == 6 && t.rows().back().id == 20005);
  session.stop();
  setenv("PATH", old.c_str(), 1);
  std::filesystem::remove_all(dir);
#endif
}
