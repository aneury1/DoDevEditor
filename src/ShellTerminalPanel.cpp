

///////////////////////////////////////////////////////////////////////////////
//  ShellTerminalPanel.h
//  Cross-platform embedded shell panel for wxWidgets.
//
//  Linux  – forkpty() + epoll  → bash (or $SHELL)
//  Windows – CreateProcess()  → cmd.exe  (pipes, non-blocking via PeekNamedPipe)
//
//  CMakeLists.txt extra:
//    Linux:   target_link_libraries(... util)          # for forkpty
//    Windows: (no extra libs required)
///////////////////////////////////////////////////////////////////////////////

#include "ShellTerminalPanel.h"

// ── Constructor ──────────────────────────────────────────────────────────
ShellTerminalPanel::ShellTerminalPanel(wxWindow *_parent)
    : wxPanel(_parent, wxID_ANY)
{
#if STP_LINUX
    m_masterFd = -1;
    m_pid = -1;
#elif STP_WINDOWS
    m_hStdinWrite = INVALID_HANDLE_VALUE;
    m_hStdoutRead = INVALID_HANDLE_VALUE;
    ZeroMemory(&m_pi, sizeof(m_pi));
#endif
    _BuildUI();
    _BindEvents();
    _StartShell();
    _StartIOThread();
    _StartUITimer();
}

// ── Destructor ───────────────────────────────────────────────────────────
ShellTerminalPanel::~ShellTerminalPanel()
{
    m_running = false;

#if STP_WINDOWS
    // Kill child first so ReadFile unblocks in the IO thread
    if (m_pi.hProcess)
        TerminateProcess(m_pi.hProcess, 0);
    // Close our write end → child gets EOF on stdin
    if (m_hStdinWrite != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hStdinWrite);
        m_hStdinWrite = INVALID_HANDLE_VALUE;
    }
#elif STP_LINUX
    if (m_pid > 0)
        kill(m_pid, SIGKILL);
    if (m_masterFd >= 0)
        close(m_masterFd);
    m_masterFd = -1;
#endif

    if (m_ioThread.joinable())
        m_ioThread.join();

#if STP_WINDOWS
    if (m_hStdoutRead != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hStdoutRead);
        m_hStdoutRead = INVALID_HANDLE_VALUE;
    }
    if (m_pi.hProcess)
    {
        WaitForSingleObject(m_pi.hProcess, 500);
        CloseHandle(m_pi.hProcess);
        CloseHandle(m_pi.hThread);
        m_pi.hProcess = nullptr;
    }
#endif
}

 

// ─────────────────────────────────────────────────────────────────────────
// UI construction
// ─────────────────────────────────────────────────────────────────────────
void ShellTerminalPanel::_BuildUI()
{
    SetBackgroundColour(wxColour(20, 20, 20));

    wxFont mono(wxFontInfo(10)
                    .FaceName("Consolas")
                    .Family(wxFONTFAMILY_TELETYPE));

    auto *sizer = new wxBoxSizer(wxVERTICAL);

    m_output = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                              wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    m_output->SetBackgroundColour(wxColour(20, 20, 20));
    m_output->SetForegroundColour(wxColour(204, 204, 204));
    m_output->SetFont(mono);
    sizer->Add(m_output, 1, wxEXPAND);

    m_input = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                             wxDefaultPosition, wxDefaultSize,
                             wxTE_PROCESS_ENTER);
    m_input->SetBackgroundColour(wxColour(30, 30, 30));
    m_input->SetForegroundColour(wxColour(0, 220, 100));
    m_input->SetFont(mono);
    sizer->Add(m_input, 0, wxEXPAND);

    SetSizer(sizer);
}

// ─────────────────────────────────────────────────────────────────────────
// Shell launch  –  platform-specific
// ─────────────────────────────────────────────────────────────────────────
void ShellTerminalPanel::_StartShell()
{
#if STP_LINUX
    _Shell_Linux();
#elif STP_WINDOWS
    _Shell_Windows();
#endif
}

#if STP_LINUX
void ShellTerminalPanel::_Shell_Linux()
{
    m_pid = forkpty(&m_masterFd, nullptr, nullptr, nullptr);

    if (m_pid == 0)
    {
        // Child process: exec the user's preferred shell
        const char *shell = getenv("SHELL");
        if (!shell || shell[0] == '\0')
            shell = "/bin/bash";
        execl(shell, shell, nullptr);
        _exit(1); // exec failed
    }

    // Parent: make the master fd non-blocking
    int flags = fcntl(m_masterFd, F_GETFL, 0);
    fcntl(m_masterFd, F_SETFL, flags | O_NONBLOCK);
}
#endif

#if STP_WINDOWS
void ShellTerminalPanel::_Shell_Windows()
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    // ── Pipe: parent writes → child reads (child's stdin) ────────────────
    HANDLE hStdinRead = INVALID_HANDLE_VALUE;
    CreatePipe(&hStdinRead, &m_hStdinWrite, &sa, 0);
    // Parent's write-end must NOT be inherited by child
    SetHandleInformation(m_hStdinWrite, HANDLE_FLAG_INHERIT, 0);

    // ── Pipe: child writes → parent reads (child's stdout + stderr) ──────
    HANDLE hStdoutWrite = INVALID_HANDLE_VALUE;
    CreatePipe(&m_hStdoutRead, &hStdoutWrite, &sa, 0);
    // Parent's read-end must NOT be inherited by child
    SetHandleInformation(m_hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    // ── Launch cmd.exe ────────────────────────────────────────────────────
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite; // merge stderr into stdout pipe
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    wchar_t cmdLine[] = L"cmd.exe";

    CreateProcessW(
        nullptr, // application name (use cmdLine)
        cmdLine, // command line
        nullptr, // process security
        nullptr, // thread security
        TRUE,    // inherit handles
        CREATE_NO_WINDOW,
        nullptr, // environment (inherit)
        nullptr, // working directory (inherit)
        &si, &m_pi);

    // ── Close the child-side handle copies held by the parent ─────────────
    // If we keep them open, ReadFile will never return ERROR_BROKEN_PIPE
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);
}
#endif

// ─────────────────────────────────────────────────────────────────────────
// IO thread  –  reads output from the child into m_queue
// ─────────────────────────────────────────────────────────────────────────
void ShellTerminalPanel::_StartIOThread()
{
    m_running = true;

#if STP_LINUX
    m_ioThread = std::thread([this]()
                             { _IOLoop_Linux(); });
#elif STP_WINDOWS
    m_ioThread = std::thread([this]()
                             { _IOLoop_Windows(); });
#endif
}

#if STP_LINUX
void ShellTerminalPanel::_IOLoop_Linux()
{
    int ep = epoll_create1(0);

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = m_masterFd;
    epoll_ctl(ep, EPOLL_CTL_ADD, m_masterFd, &ev);

    epoll_event events[8];
    char buf[4096];

    while (m_running)
    {
        int n = epoll_wait(ep, events, 8, 50 /*ms timeout*/);

        for (int i = 0; i < n; ++i)
        {
            if (events[i].data.fd != m_masterFd)
                continue;

            ssize_t r = read(m_masterFd, buf, sizeof(buf));

            if (r > 0)
            {
                std::lock_guard<std::mutex> lk(m_mtx);
                m_queue.push(std::string(buf, (size_t)r));
            }
            else if (r == 0)
            {
                m_running = false; // PTY closed (shell exited)
            }
            else if (errno != EAGAIN && errno != EINTR)
            {
                m_running = false; // real error
            }
        }
    }

    close(ep);
}
#endif

#if STP_WINDOWS
void ShellTerminalPanel::_IOLoop_Windows()
{
    char buf[4096];

    while (m_running)
    {
        // Non-blocking: peek how many bytes are available
        DWORD available = 0;
        BOOL ok = PeekNamedPipe(m_hStdoutRead,
                                nullptr, 0,
                                nullptr, &available, nullptr);
        if (!ok)
            break; // pipe broken → child has exited

        if (available > 0)
        {
            DWORD toRead = (available < (DWORD)sizeof(buf))
                               ? available
                               : (DWORD)sizeof(buf);
            DWORD rd = 0;

            if (!ReadFile(m_hStdoutRead, buf, toRead, &rd, nullptr) || rd == 0)
                break; // pipe broken

            std::lock_guard<std::mutex> lk(m_mtx);
            m_queue.push(std::string(buf, (size_t)rd));
        }
        else
        {
            // No data yet – check if the child is still alive
            DWORD exitCode = STILL_ACTIVE;
            GetExitCodeProcess(m_pi.hProcess, &exitCode);
            if (exitCode != STILL_ACTIVE)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
#endif

// ─────────────────────────────────────────────────────────────────────────
// UI timer – drains m_queue on the wx main thread every 30 ms
// ─────────────────────────────────────────────────────────────────────────
void ShellTerminalPanel::_StartUITimer()
{
    Bind(wxEVT_TIMER, [this](wxTimerEvent &)
         {
            // Swap under lock, then process outside the lock
            std::queue<std::string> local;
            {
                std::lock_guard<std::mutex> lk(m_mtx);
                std::swap(local, m_queue);
            }
            while (!local.empty())
            {
                m_output->AppendText(_DecodeOutput(local.front()));
                local.pop();
            } });

    m_timer.SetOwner(this);
    m_timer.Start(30);
}

// ─────────────────────────────────────────────────────────────────────────
// Input handling
// ─────────────────────────────────────────────────────────────────────────
void ShellTerminalPanel::_BindEvents()
{
    m_input->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &)
                  {
            wxString line = m_input->GetValue();
            m_input->Clear();
            _SendLine(line); });
}

void ShellTerminalPanel::_SendLine(const wxString &_line)
{
#if STP_LINUX
    std::string raw = std::string(_line.ToUTF8()) + "\n";
    if (m_masterFd >= 0)
        write(m_masterFd, raw.c_str(), raw.size());

#elif STP_WINDOWS
    std::string raw = _line.ToStdString() + "\r\n";
    if (m_hStdinWrite != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WriteFile(m_hStdinWrite,
                  raw.c_str(), (DWORD)raw.size(),
                  &written, nullptr);
    }
#endif
}

// ─────────────────────────────────────────────────────────────────────────
// Output decoding
//   Linux   → shell output is UTF-8
//   Windows → cmd.exe uses OEM (console) code page → convert to wxString
// ─────────────────────────────────────────────────────────────────────────
 wxString ShellTerminalPanel::_DecodeOutput(const std::string &_raw)
{
    std::string clean = _StripAnsi(_raw);

#if STP_LINUX
    return wxString::FromUTF8(clean.data(), clean.size());

#elif STP_WINDOWS
    if (clean.empty())
        return wxString();
    // OEM → UTF-16 → wxString
    int wlen = MultiByteToWideChar(CP_OEMCP, 0,
                                   clean.data(), (int)clean.size(),
                                   nullptr, 0);
    if (wlen <= 0)
        return wxString();
    std::wstring wide(wlen, L'\0');
    MultiByteToWideChar(CP_OEMCP, 0,
                        clean.data(), (int)clean.size(),
                        &wide[0], wlen);
    return wxString(wide.c_str(), wlen);
#endif
}

// ─────────────────────────────────────────────────────────────────────────
// ANSI / VT escape-sequence stripper
//   Handles CSI sequences:  ESC [ <params> <final-byte>
//   Handles OSC sequences:  ESC ] ... ST  (or BEL-terminated)
//   Handles bare ESC + char sequences
//   Strips \r (CR) that are not part of \r\n pairs
// ─────────────────────────────────────────────────────────────────────────
 std::string _StripAnsi(const std::string &_s)
{
    std::string out;
    out.reserve(_s.size());
    size_t i = 0;

    while (i < _s.size())
    {
        const unsigned char c = (unsigned char)_s[i];

        if (c == 0x1B) // ESC
        {
            ++i;
            if (i >= _s.size())
                break;

            const unsigned char next = (unsigned char)_s[i];

            if (next == '[') // CSI: ESC [ <param bytes> <intermediate bytes> <final byte>
            {
                ++i;
                // param bytes: 0x30–0x3F   intermediate: 0x20–0x2F
                while (i < _s.size() &&
                       (unsigned char)_s[i] < 0x40)
                    ++i;
                if (i < _s.size())
                    ++i; // skip final byte (0x40–0x7E)
            }
            else if (next == ']') // OSC: ESC ] ... ST or BEL
            {
                ++i;
                while (i < _s.size())
                {
                    unsigned char oc = (unsigned char)_s[i];
                    if (oc == 0x07)
                    {
                        ++i;
                        break;
                    } // BEL terminates
                    if (oc == 0x1B && i + 1 < _s.size() &&
                        (unsigned char)_s[i + 1] == '\\') // ST = ESC '\'
                    {
                        i += 2;
                        break;
                    }
                    ++i;
                }
            }
            else
            {
                ++i; // ESC + single character: skip both
            }
        }
        else if (c == '\r')
        {
            ++i;
            // CR + LF → keep the LF (it will be added on the next iteration)
            // bare CR (overwrite) → discard
        }
        else
        {
            out += (char)c;
            ++i;
        }
    }

    return out;
}
