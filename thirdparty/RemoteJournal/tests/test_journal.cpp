#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "RemoteJournal.h"
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#endif
int main() {
  using namespace dodev::journal;
  Options o;
  o.host = "example.com";
  o.user = "operator";
  o.unit = "nginx.service";
  assert(Session::validate(o));
  assert(Session::sshArguments(o).back().find("--unit='nginx.service'") !=
         std::string::npos);
  o.host = "-oProxyCommand=evil";
  assert(!Session::validate(o));
  o.host = "localhost";
  o.unit = "foo'; evil";
  assert(!Session::validate(o));
  o.unit = "nginx.service";
  Session s;
  std::string error;
  assert(!s.setFilter(Filter::Regex, "[", false, &error));
#ifndef _WIN32
  const auto root = std::filesystem::temp_directory_path() /
                    ("dodev-ssh-test-" + std::to_string(getpid()));
  std::filesystem::create_directories(root);
  const auto mock = root / "ssh";
  {
    std::ofstream out(mock);
    out << "#!/bin/sh\nprintf 'INFO start\\nERROR disk\\nINFO finish\\n'\n";
  }
  chmod(mock.c_str(), 0755);
  const char* existing = getenv("PATH");
  const std::string old = existing ? existing : "";
  setenv("PATH", (root.string() + ":" + old).c_str(), 1);
  assert(s.start(o, &error));
  for (int i = 0; i < 60 && s.running(); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  assert(s.snapshot(false).received == 3);
  assert(s.setFilter(Filter::Fixed, "error", false, &error));
  assert(s.snapshot(true).text.find("ERROR disk") != std::string::npos);
  assert(s.snapshot(true).text.find("INFO start") == std::string::npos);
  assert(s.exportText(false).find("INFO finish") != std::string::npos);
  s.stop();
  setenv("PATH", old.c_str(), 1);
  std::filesystem::remove_all(root);
#endif
  return 0;
}
