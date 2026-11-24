#pragma once
#include <string>
#include <vector>

#include "dodev/settings/Json.h"
namespace dodev::docker {
using Args = std::vector<std::string>;
struct Connection {
  std::string executable = "docker", context, host, config;
  Args flags;
  Args command(const Args&) const;
};
struct RunOptions {
  std::string image, name, hostname, workdir, user, network, restart, memory,
      cpus, entrypoint, platform;
  Args ports, env, volumes, mounts, labels, devices, capAdd, capDrop, extra,
      command;
  bool interactive = true, tty = true, detach = false, remove = false,
       readOnly = false, privileged = false;
};
Args runArgs(const RunOptions&);
Args lines(const std::string& text);
void validateOperand(const std::string& text, const char* name);
std::vector<Json> jsonLines(const std::string& text);
std::string derivedDockerfile(const Json& inspect, const std::string& fallback);
}  // namespace dodev::docker
