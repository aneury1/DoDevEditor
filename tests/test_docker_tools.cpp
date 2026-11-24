#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "dodev/docker/DockerQuery.h"
#include "dodev/docker/DockerTools.h"
using namespace dodev;
using namespace dodev::docker;
void require(bool b) {
  if (!b) throw std::runtime_error("Docker regression failed");
}
template <class F>
void rejects(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::exception&) {
    failed = true;
  }
  require(failed);
}
int main() {
  Connection c;
  c.executable = "/opt/docker cli";
  c.context = "remote";
  c.config = "/home/a b/.docker";
  c.flags = {"--debug"};
  RunOptions o;
  o.image = "alpine:latest";
  o.name = "test";
  o.ports = {"127.0.0.1:8080:80"};
  o.env = {"NAME=hello world; $(id)"};
  o.volumes = {"/some path:/data:ro"};
  o.extra = {"--health-cmd=echo ok", "--gpus=all"};
  o.command = {"/bin/sh", "-c", "echo hello world"};
  auto a = c.command(runArgs(o));
  require(a.front() == c.executable);
  require(a[1] == "--context" && a[2] == "remote");
  require(std::find(a.begin(), a.end(), o.env[0]) != a.end());
  require(std::find(a.begin(), a.end(), o.volumes[0]) != a.end());
  require(a.back() == "echo hello world");
  require(*(std::find(a.begin(), a.end(), o.image) + 1) == "/bin/sh");
  require(lines("a b\r\n--flag\n\nvalue\n") ==
          Args({"a b", "--flag", "value"}));
  rejects([&] {
    c.host = "tcp://localhost:2375";
    c.command({"ps"});
  });
  rejects([&] {
    o.image = "--privileged";
    runArgs(o);
  });
  o.image = "alpine";
  o.restart = "always";
  o.remove = true;
  rejects([&] { runArgs(o); });
  require(jsonLines("{\"ID\":\"abc\"}\r\n{\"ID\":\"def\"}\n").size() == 2);
  rejects([] { jsonLines("invalid JSON\n"); });
  Json j;
  require(Json::parse(
      "[{\"RepoTags\":[\"alpine:latest\"],\"RepoDigests\":[\"alpine@sha256:"
      "abcd\"],\"Config\":{\"Env\":[\"SECRET=abc\"]}}]",
      j));
  auto f = derivedDockerfile(j, "unused");
  require(f.find("FROM alpine@sha256:abcd") != std::string::npos);
  require(f.find("SECRET") == std::string::npos);
  rejects([] { derivedDockerfile(Json::object(), "a\nRUN id"); });
#ifndef _WIN32
  // Fake Docker CLI returning the same JSON-line format as image ls. No daemon
  // needed.
  auto output = capture({"/bin/sh", "-c", "printf '%s\\n' '{\"ID\":\"abc\"}'"},
                        std::filesystem::current_path());
  require(jsonLines(output).at(0)["ID"].string() == "abc");
  rejects([] {
    capture({"/bin/sh", "-c", "exit 7"}, std::filesystem::current_path());
  });
  std::atomic<bool> cancelled{true};
  rejects([&] {
    capture({"/bin/sh", "-c", "sleep 5"}, std::filesystem::current_path(),
            &cancelled);
  });
#endif
  std::cout << "Docker option construction, JSON inventory and derived "
               "Dockerfile tests passed\n";
}
