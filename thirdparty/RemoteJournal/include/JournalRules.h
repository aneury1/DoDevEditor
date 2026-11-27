#pragma once
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

#include "JournalTable.h"
#include "dodev/settings/Json.h"

namespace dodev::journal {
// Persisted rule description. Empty conditions mean no restriction.
struct RuleSpec {
  std::string name = "New filter", type = "positive", payload, application,
              service, host, process, pid, from, to;
  std::string foreground = "#FFFFFF", background = "#815000";
  bool enabled = true, regex = false, caseSensitive = false;
  int minPriority = -1, maxPriority = -1;
};
struct Rule {
  RuleSpec spec;
  TableFilter filter;
  bool matches(const Record& r) const {
    auto contains = [&](const std::string& value, const std::string& term) {
      if (term.empty()) return true;
      if (spec.caseSensitive) return value.find(term) != std::string::npos;
      auto fold = [](std::string s) {
        for (auto& c : s)
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
      };
      return fold(value).find(fold(term)) != std::string::npos;
    };
    if (spec.minPriority >= 0 && (r.priority < spec.minPriority)) return false;
    if (spec.maxPriority >= 0 &&
        (r.priority < 0 || r.priority > spec.maxPriority))
      return false;
    return filter.accepts(r) && contains(r.host, spec.host) &&
           contains(r.process, spec.process) && contains(r.pid, spec.pid);
  }
};
inline bool compileRule(const RuleSpec& s, Rule& out, std::string& error) {
  if (s.name.empty()) {
    error = "Filter name is required";
    return false;
  }
  if (s.type != "positive" && s.type != "negative" && s.type != "marker") {
    error = "Unknown filter type";
    return false;
  }
  auto color = [](const std::string& x) {
    return x.empty() || (x.size() == 7 && x[0] == '#' &&
                         x.find_first_not_of("0123456789abcdefABCDEF", 1) ==
                             std::string::npos);
  };
  if (!color(s.foreground) || !color(s.background)) {
    error = "Colors must use #RRGGBB or be blank";
    return false;
  }
  if (s.minPriority < -1 || s.minPriority > 7 || s.maxPriority < -1 ||
      s.maxPriority > 7 ||
      (s.minPriority >= 0 && s.maxPriority >= 0 &&
       s.minPriority > s.maxPriority)) {
    error = "Invalid priority range (0 Emergency .. 7 Debug)";
    return false;
  }
  Rule r;
  r.spec = s;
  r.filter.payload = s.payload;
  r.filter.applicationId = s.application;
  r.filter.service = s.service;
  r.filter.caseSensitive = s.caseSensitive;
  if (!TimeRange::parse(s.from, s.to, r.filter.time, error)) return false;
  if (s.regex && !s.payload.empty()) try {
      r.filter.payloadRegex.emplace(
          s.payload, s.caseSensitive
                         ? std::regex::ECMAScript
                         : std::regex::ECMAScript | std::regex::icase);
    } catch (const std::regex_error&) {
      error = "Invalid payload regular expression";
      return false;
    }
  out = std::move(r);
  error.clear();
  return true;
}
struct RuleSet {
  bool enabled = true;
  std::vector<Rule> rules;
  // Positive rules OR together; negative rules veto; marker rules never hide.
  bool accepts(const Record& r) const {
    if (!enabled) return true;
    bool positive = false, hit = false;
    for (const auto& rule : rules) {
      if (!rule.spec.enabled || rule.spec.type == "marker") continue;
      if (rule.spec.type == "negative") {
        if (rule.matches(r)) return false;
      } else {
        positive = true;
        if (!hit && rule.matches(r)) hit = true;
      }
    }
    return !positive || hit;
  }
  const RuleSpec* marker(const Record& r) const {
    if (enabled)
      for (const auto& rule : rules)
        if (rule.spec.enabled && rule.spec.type == "marker" && rule.matches(r))
          return &rule.spec;
    return nullptr;
  }
  std::string encode() const {
    Json j = Json::object();
    j["version"] = 1;
    j["enabled"] = enabled;
    Json::Array a;
    for (const auto& rule : rules) {
      const auto& s = rule.spec;
      Json v = Json::object();
#define JFIELD(x) v[#x] = s.x;
      JFIELD(name)
      JFIELD(type)
      JFIELD(payload) JFIELD(application) JFIELD(service) JFIELD(host)
          JFIELD(process) JFIELD(pid) JFIELD(from) JFIELD(to) JFIELD(foreground)
              JFIELD(background) JFIELD(enabled) JFIELD(regex)
                  JFIELD(caseSensitive) JFIELD(minPriority) JFIELD(maxPriority)
#undef JFIELD
                      a.push_back(std::move(v));
    }
    j["rules"] = Json(std::move(a));
    return j.dump(2);
  }
  static bool decode(const std::string& text, RuleSet& out,
                     std::string& error) {
    Json j;
    if (!Json::parse(text, j, &error) || !j.isObject()) {
      error = "Invalid filter JSON";
      return false;
    }
    const Json& root = j;
    if (root["version"].integer() != 1 || !root["rules"].isArray()) {
      error = "Unsupported filter file";
      return false;
    }
    RuleSet result;
    result.enabled = root["enabled"].boolean(true);
    for (const auto& v : root["rules"].asArray()) {
      if (!v.isObject()) {
        error = "Filter must be an object";
        return false;
      }
      // Reject malformed typed fields rather than silently making a match-all
      // rule.
      for (const auto* key :
           {"name", "type", "payload", "application", "service", "host",
            "process", "pid", "from", "to", "foreground", "background"})
        if (auto* f = v.find(key); f && !f->isString()) {
          error = std::string("Invalid text field: ") + key;
          return false;
        }
      for (const auto* key : {"enabled", "regex", "caseSensitive"})
        if (auto* f = v.find(key); f && !f->isBool()) {
          error = std::string("Invalid boolean field: ") + key;
          return false;
        }
      for (const auto* key : {"minPriority", "maxPriority"})
        if (auto* f = v.find(key);
            f && (!f->isNumber() || f->number() < -1 || f->number() > 7 ||
                  f->number() != f->integer())) {
          error = std::string("Invalid priority field: ") + key;
          return false;
        }
      RuleSpec s;
#define JSTR(x) s.x = v[#x].string(s.x);
      JSTR(name)
      JSTR(type)
      JSTR(payload) JSTR(application) JSTR(service) JSTR(host) JSTR(process)
          JSTR(pid) JSTR(from) JSTR(to) JSTR(foreground) JSTR(background)
#undef JSTR
              s.enabled = v["enabled"].boolean(true);
      s.regex = v["regex"].boolean(false);
      s.caseSensitive = v["caseSensitive"].boolean(false);
      s.minPriority = v["minPriority"].integer(-1);
      s.maxPriority = v["maxPriority"].integer(-1);
      Rule r;
      if (!compileRule(s, r, error)) {
        error = s.name + ": " + error;
        return false;
      }
      result.rules.push_back(std::move(r));
    }
    out = std::move(result);
    error.clear();
    return true;
  }
};
}  // namespace dodev::journal
