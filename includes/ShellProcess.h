#pragma once

#include <fcntl.h>
#include <pty.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

class ShellProcess {
 public:
  ShellProcess();

  ~ShellProcess();

  // send command to shell
  void write(const std::string& cmd);

  // read available output (non-blocking)
  std::string readOutput();

 private:
  int masterFd = -1;
  pid_t pid = -1;
};