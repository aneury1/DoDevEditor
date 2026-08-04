#ifndef WX_SHELL_TERMINAL_PANEL_H
#define WX_SHELL_TERMINAL_PANEL_H

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

// ── Platform detection ────────────────────────────────────────────────────────
#if defined(_WIN32) || defined(_WIN64)
#   define STP_WINDOWS 1
#elif defined(__linux__)
#   define STP_LINUX 1
#else
#   error "ShellTerminalPanel: unsupported platform (Linux and Windows only)"
#endif

// ── Platform-specific system headers ─────────────────────────────────────────
#if STP_WINDOWS
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#elif STP_LINUX
#   include <pty.h>          // forkpty
#   include <unistd.h>       // read, write, close, execl, _exit
#   include <fcntl.h>        // fcntl, O_NONBLOCK
#   include <signal.h>       // kill, SIGKILL
#   include <errno.h>        // errno, EAGAIN, EINTR
#   include <sys/wait.h>     // waitpid
#   include <sys/epoll.h>    // epoll_create1, epoll_ctl, epoll_wait
#endif

#include <wx/wx.h>
#include <wx/font.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class ShellTerminalPanel : public wxPanel
{
public:
    // ── Constructor ──────────────────────────────────────────────────────────
    explicit ShellTerminalPanel(wxWindow* _parent);

    // ── Destructor ───────────────────────────────────────────────────────────
    ~ShellTerminalPanel();

private:
    // ── UI widgets ────────────────────────────────────────────────────────────
    wxTextCtrl* m_output = nullptr;
    wxTextCtrl* m_input  = nullptr;
    wxTimer     m_timer;

    // ── Shared threading state ────────────────────────────────────────────────
    std::thread              m_ioThread;
    std::atomic<bool>        m_running{false};
    std::queue<std::string>  m_queue;
    std::mutex               m_mtx;

    // ── Platform-specific process / IO handles ────────────────────────────────
#if STP_LINUX
    int    m_masterFd;   // PTY master
    pid_t  m_pid;        // child PID
#elif STP_WINDOWS
    HANDLE              m_hStdinWrite;   // write end  → child stdin
    HANDLE              m_hStdoutRead;   // read end   ← child stdout+stderr
    PROCESS_INFORMATION m_pi;
#endif

    // ─────────────────────────────────────────────────────────────────────────
    // UI construction
    // ─────────────────────────────────────────────────────────────────────────
    void _BuildUI();

    // ─────────────────────────────────────────────────────────────────────────
    // Shell launch  –  platform-specific
    // ─────────────────────────────────────────────────────────────────────────
    void _StartShell();

#if STP_LINUX
    void _Shell_Linux();
#endif

#if STP_WINDOWS
    void _Shell_Windows();
#endif

    // ─────────────────────────────────────────────────────────────────────────
    // IO thread  –  reads output from the child into m_queue
    // ─────────────────────────────────────────────────────────────────────────
    void _StartIOThread();

#if STP_LINUX

void _IOLoop_Linux();
#endif

#if STP_WINDOWS
void _IOLoop_Windows();
#endif

    // ─────────────────────────────────────────────────────────────────────────
    // UI timer – drains m_queue on the wx main thread every 30 ms
    // ─────────────────────────────────────────────────────────────────────────
    void _StartUITimer();

    // ─────────────────────────────────────────────────────────────────────────
    // Input handling
    // ─────────────────────────────────────────────────────────────────────────
    void _BindEvents();
    void _SendLine(const wxString& _line);

    // ─────────────────────────────────────────────────────────────────────────
    // Output decoding
    //   Linux   → shell output is UTF-8
    //   Windows → cmd.exe uses OEM (console) code page → convert to wxString
    // ─────────────────────────────────────────────────────────────────────────
    static wxString _DecodeOutput(const std::string& _raw);

    // ─────────────────────────────────────────────────────────────────────────
    // ANSI / VT escape-sequence stripper
    //   Handles CSI sequences:  ESC [ <params> <final-byte>
    //   Handles OSC sequences:  ESC ] ... ST  (or BEL-terminated)
    //   Handles bare ESC + char sequences
    //   Strips \r (CR) that are not part of \r\n pairs
    // ─────────────────────────────────────────────────────────────────────────
    static std::string _StripAnsi(const std::string& _s);

};

#endif // WX_SHELL_TERMINAL_PANEL_H