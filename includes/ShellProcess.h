#pragma once

#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <iostream>
#include <cstring>

class ShellProcess
{
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