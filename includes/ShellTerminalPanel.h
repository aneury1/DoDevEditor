#ifndef WX_SHELL_TERMINAL_PANEL_H
#define WX_SHELL_TERMINAL_PANEL_H

#include <wx/wx.h>
#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>

class ShellTerminalPanel : public wxPanel
{
public:
    ShellTerminalPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        SetBackgroundColour(wxColour(20, 20, 20));

        auto* sizer = new wxBoxSizer(wxVERTICAL);

        // ── Output view ──
        m_output = new wxTextCtrl(
            this,
            wxID_ANY,
            "",
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE |
            wxTE_READONLY |
            wxTE_RICH2);

        m_output->SetBackgroundColour(wxColour(20, 20, 20));
        m_output->SetForegroundColour(wxColour(220, 220, 220));

        sizer->Add(m_output, 1, wxEXPAND);

        // ── Input ──
        m_input = new wxTextCtrl(
            this,
            wxID_ANY,
            "",
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_PROCESS_ENTER);

        m_input->SetBackgroundColour(wxColour(30, 30, 30));
        m_input->SetForegroundColour(wxColour(220, 220, 220));

        sizer->Add(m_input, 0, wxEXPAND);

        SetSizer(sizer);

        BindEvents();

        StartShell();
        StartEpollThread();
        StartUiTimer();
    }

    ~ShellTerminalPanel()
    {
        running = false;

        if (epollThread.joinable())
            epollThread.join();

        if (masterFd > 0)
            close(masterFd);

        if (pid > 0)
            kill(pid, SIGKILL);
    }

private:
    // ─────────────────────────────
    // SHELL START
    // ─────────────────────────────
    void StartShell()
    {
        pid = forkpty(&masterFd, nullptr, nullptr, nullptr);

        if (pid == 0)
        {
            execl("/bin/bash", "bash", nullptr);
            _exit(1);
        }

        fcntl(masterFd, F_SETFL, O_NONBLOCK);
    }

    // ─────────────────────────────
    // EPOLL THREAD
    // ─────────────────────────────
    void StartEpollThread()
    {
        running = true;

        epollThread = std::thread([this]()
        {
            int ep = epoll_create1(0);

            epoll_event ev{};
            ev.events = EPOLLIN;
            ev.data.fd = masterFd;

            epoll_ctl(ep, EPOLL_CTL_ADD, masterFd, &ev);

            epoll_event events[10];

            while (running)
            {
                int n = epoll_wait(ep, events, 10, 50);

                for (int i = 0; i < n; i++)
                {
                    if (events[i].data.fd == masterFd)
                    {
                        char buffer[4096];
                        ssize_t r = read(masterFd, buffer, sizeof(buffer));

                        if (r > 0)
                        {
                            std::lock_guard<std::mutex> lock(mtx);
                            queue.push(std::string(buffer, r));
                        }
                    }
                }
            }

            close(ep);
        });
    }

    // ─────────────────────────────
    // UI TIMER (wx thread safe)
    // ─────────────────────────────
    void StartUiTimer()
    {
        Bind(wxEVT_TIMER, [this](wxTimerEvent&)
        {
            std::lock_guard<std::mutex> lock(mtx);

            while (!queue.empty())
            {
                m_output->AppendText(queue.front());
                queue.pop();
            }

        });

        timer.SetOwner(this);
        timer.Start(30);
    }

    // ─────────────────────────────
    // INPUT HANDLING
    // ─────────────────────────────
    void BindEvents()
    {
        m_input->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&)
        {
            std::string cmd = m_input->GetValue().ToStdString();
            cmd += "\n";

            write(masterFd, cmd.c_str(), cmd.size());

            m_input->Clear();
        });
    }

private:
    wxTextCtrl* m_output = nullptr;
    wxTextCtrl* m_input = nullptr;

    int masterFd = -1;
    pid_t pid = -1;

    std::thread epollThread;
    std::atomic<bool> running{false};

    std::queue<std::string> queue;
    std::mutex mtx;

    wxTimer timer;
};

#endif