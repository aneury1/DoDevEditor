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
    ShellProcess()
    {
        pid = forkpty(&masterFd, nullptr, nullptr, nullptr);

        if (pid == 0)
        {
            // child → shell
            execl("/bin/bash", "bash", nullptr);
            _exit(1);
        }

        // parent → make non-blocking
        fcntl(masterFd, F_SETFL, O_NONBLOCK);
    }

    ~ShellProcess()
    {
        if (pid > 0)
            kill(pid, SIGKILL);
        close(masterFd);
    }

    // send command to shell
    void write(const std::string& cmd)
    {
        std::string data = cmd + "\n";
        ::write(masterFd, data.c_str(), data.size());
    }

    // read available output (non-blocking)
    std::string readOutput()
    {
        char buffer[4096];
        std::string result;

        while (true)
        {
            ssize_t n = ::read(masterFd, buffer, sizeof(buffer));

            if (n > 0)
            {
                result.append(buffer, n);
            }
            else
            {
                break;
            }
        }

        return result;
    }

private:
    int masterFd = -1;
    pid_t pid = -1;
};