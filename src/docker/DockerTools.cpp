#include "dodev/docker/DockerTools.h"

#include <cctype>
#include <sstream>
#include <stdexcept>
namespace dodev::docker {
namespace {
void text(const std::string& s) {
  if (s.find('\0') != std::string::npos ||
      s.find_first_of("\r\n") != std::string::npos)
    throw std::runtime_error("Invalid newline or NUL in Docker argument");
}
void option(Args& a, const char* flag, const std::string& v) {
  if (!v.empty()) {
    text(v);
    a.emplace_back(flag);
    a.push_back(v);
  }
}
void many(Args& a, const char* flag, const Args& values) {
  for (auto& v : values) option(a, flag, v);
}
void check(const Args& a) {
  for (auto& v : a) text(v);
}
}  // namespace
void validateOperand(const std::string& s, const char* name) {
  text(s);
  if (s.empty() || s.front() == '-' ||
      s.find_first_of(" \t") != std::string::npos)
    throw std::runtime_error(std::string("Invalid ") + name);
}
Args lines(const std::string& t) {
  Args r;
  std::istringstream in(t);
  std::string l;
  while (std::getline(in, l)) {
    if (!l.empty() && l.back() == '\r') l.pop_back();
    if (!l.empty()) r.push_back(l);
  }
  check(r);
  return r;
}
Args Connection::command(const Args& a) const {
  if (executable.empty())
    throw std::runtime_error("Docker executable is empty");
  if (!context.empty() && !host.empty())
    throw std::runtime_error("Choose a Docker context OR a host, not both");
  Args r{executable};
  option(r, "--context", context);
  option(r, "--host", host);
  option(r, "--config", config);
  check(flags);
  r.insert(r.end(), flags.begin(), flags.end());
  check(a);
  r.insert(r.end(), a.begin(), a.end());
  return r;
}
Args runArgs(const RunOptions& o) {
  validateOperand(o.image, "image reference");
  if (o.remove && !o.restart.empty() && o.restart != "no")
    throw std::runtime_error("--rm conflicts with restart policy");
  Args a{"run"};
  if (o.interactive) a.push_back("-i");
  if (o.tty) a.push_back("-t");
  if (o.detach) a.push_back("-d");
  if (o.remove) a.push_back("--rm");
  if (o.readOnly) a.push_back("--read-only");
  if (o.privileged) a.push_back("--privileged");
  option(a, "--name", o.name);
  option(a, "--hostname", o.hostname);
  option(a, "--workdir", o.workdir);
  option(a, "--user", o.user);
  option(a, "--network", o.network);
  option(a, "--restart", o.restart);
  option(a, "--memory", o.memory);
  option(a, "--cpus", o.cpus);
  option(a, "--entrypoint", o.entrypoint);
  option(a, "--platform", o.platform);
  many(a, "--publish", o.ports);
  many(a, "--env", o.env);
  many(a, "--volume", o.volumes);
  many(a, "--mount", o.mounts);
  many(a, "--label", o.labels);
  many(a, "--device", o.devices);
  many(a, "--cap-add", o.capAdd);
  many(a, "--cap-drop", o.capDrop);
  check(o.extra);
  a.insert(a.end(), o.extra.begin(), o.extra.end());
  a.push_back(o.image);
  check(o.command);
  a.insert(a.end(), o.command.begin(), o.command.end());
  return a;
}
std::vector<Json> jsonLines(const std::string& s) {
  std::vector<Json> out;
  std::istringstream stream(s);
  std::string l;
  while (std::getline(stream, l)) {
    if (!l.empty() && l.back() == '\r') l.pop_back();
    if (l.find_first_not_of(" \t") == std::string::npos) continue;
    Json j;
    std::string error;
    if (!Json::parse(l, j, &error) || !j.isObject())
      throw std::runtime_error("Invalid Docker JSON output: " + error + "\n" +
                               l.substr(0, 300));
    out.push_back(std::move(j));
  }
  return out;
}
std::string derivedDockerfile(const Json& data, const std::string& fallback) {
  const Json* obj = &data;
  if (data.isArray() && !data.asArray().empty()) obj = &data.asArray().front();
  if (!obj->isObject())
    throw std::runtime_error("Expected Docker image inspect object");
  std::string ref;
  for (auto& d : (*obj)["RepoDigests"].asArray())
    if (d.isString() && !d.string().empty()) {
      ref = d.string();
      break;
    }
  if (ref.empty())
    for (auto& d : (*obj)["RepoTags"].asArray())
      if (d.isString() && d.string() != "<none>:<none>") {
        ref = d.string();
        break;
      }
  if (ref.empty()) {
    if (fallback.rfind("sha256:", 0) == 0)
      throw std::runtime_error(
          "Tag this untagged image first, then export its Dockerfile");
    ref = fallback;
  }
  validateOperand(ref, "base image reference");
  for (unsigned char c : ref)
    if (!(std::isalnum(c) || c == '.' || c == '_' || c == '-' || c == '/' ||
          c == ':' || c == '@' || c == '[' || c == ']'))
      throw std::runtime_error("Base image is not a valid FROM reference");
  return "# Derived Dockerfile: original build instructions cannot be "
         "recovered.\n# Inherits the selected image filesystem and image "
         "configuration.\n# Does not reproduce container mounts, ports, "
         "secrets or runtime flags.\nFROM " +
         ref + "\n\n# Add your own RUN, COPY, ENV or CMD instructions here.\n";
}
}  // namespace dodev::docker
