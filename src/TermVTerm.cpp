

// ─────────────────────────────────────────────────────────────────────────────
//  Platform detection + PTY includes
// ─────────────────────────────────────────────────────────────────────────────
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory> // std::unique_ptr (attr-list buffer)

// ConPTY handle – defined in consoleapi.h from SDK 10.0.17763+.
// We typedef our own name so we compile with any MinGW SDK.
using HPCON_t = PVOID;

// Function-pointer types for dynamic loading
using PFN_CreatePseudoConsole = HRESULT(WINAPI *)(COORD, HANDLE, HANDLE, DWORD, HPCON_t *);
using PFN_ResizePseudoConsole = HRESULT(WINAPI *)(HPCON_t, COORD);
using PFN_ClosePseudoConsole = VOID(WINAPI *)(HPCON_t);
#else
#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#endif

#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include <vterm.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include "TermVTerm.h"


// ── Damage callback (must be a plain function pointer – no captures) ───────
int LibVTermPanel::s_Damage(VTermRect /*rect*/, void *user)
{
    static_cast<LibVTermPanel *>(user)->m_dirty = true;
    return 1;
}

LibVTermPanel::LibVTermPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY,
              wxDefaultPosition, wxDefaultSize,
              wxWANTS_CHARS | wxNO_BORDER)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(*wxBLACK);

    // ── Monospace font ────────────────────────────────────────────────────
    m_font = wxFont(12, wxFONTFAMILY_TELETYPE,
                    wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL,
                    false,
#ifdef _WIN32
                    "Consolas"
#else
                    "Monospace"
#endif
    );

    {
        wxClientDC dc(this);
        dc.SetFont(m_font);
        wxSize sz = dc.GetTextExtent("M");
        m_cw = std::max(1, sz.x);
        m_ch = std::max(1, sz.y + 2);
    }

    wxSize panel = GetClientSize();
    m_cols = std::max(1, panel.x / m_cw);
    m_rows = std::max(1, panel.y / m_ch);

    // ── libvterm ──────────────────────────────────────────────────────────
    m_vt = vterm_new(m_rows, m_cols);
    vterm_set_utf8(m_vt, 1);
    m_scr = vterm_obtain_screen(m_vt);

    s_cbs = {};
    s_cbs.damage = &LibVTermPanel::s_Damage;
    vterm_screen_set_callbacks(m_scr, &s_cbs, this);
    vterm_screen_reset(m_scr, 1);

    // ── Spawn shell + PTY ─────────────────────────────────────────────────
    if (!SpawnShell())
    {
        wxLogError("LibVTermPanel: failed to start terminal process.");
        return;
    }

    // ── Reader thread ─────────────────────────────────────────────────────
    m_running = true;
    m_thread = std::thread(&LibVTermPanel::ReaderThread, this);

    // ── wx event bindings ────────────────────────────────────────────────
    Bind(wxEVT_PAINT, &LibVTermPanel::OnPaint, this);
    Bind(wxEVT_SIZE, &LibVTermPanel::OnSize, this);
    Bind(wxEVT_KEY_DOWN, &LibVTermPanel::OnKeyDown, this);
    Bind(wxEVT_CHAR, &LibVTermPanel::OnChar, this);
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &)
         { SetFocus(); });

    m_timer.SetOwner(this);
    Bind(wxEVT_TIMER, [this](wxTimerEvent &)
         {
            if (m_dirty.exchange(false)) Refresh(false); });
    m_timer.Start(16); // ~60 fps
}

LibVTermPanel::~LibVTermPanel()
{
    m_timer.Stop();
    m_running = false;
    CleanupPTY(); // signals the reader thread to exit
    if (m_thread.joinable())
        m_thread.join();
    if (m_vt)
        vterm_free(m_vt);
}

// Write a command string to the shell (appends '\n')
void LibVTermPanel::Write(const std::string &cmd)
{
    std::string s = cmd + "\n";
    WritePTY(s.data(), s.size());
}

// ═════════════════════════════════════════════════════════════════════════
//  SpawnShell  –  platform implementations
// ═════════════════════════════════════════════════════════════════════════
bool LibVTermPanel::SpawnShell()
{
#ifdef _WIN32
    // ── Load ConPTY functions dynamically ─────────────────────────────────
    // This keeps the binary launchable on Windows < 1809, where we can at
    // least show a proper error message instead of crashing.
    HMODULE hK32 = GetModuleHandleW(L"kernel32.dll");
    m_pfnCreate = (PFN_CreatePseudoConsole)GetProcAddress(hK32, "CreatePseudoConsole");
    m_pfnResize = (PFN_ResizePseudoConsole)GetProcAddress(hK32, "ResizePseudoConsole");
    m_pfnClose = (PFN_ClosePseudoConsole)GetProcAddress(hK32, "ClosePseudoConsole");

    if (!m_pfnCreate)
    {
        wxMessageBox(
            "The embedded terminal requires Windows 10 version 1809 (October 2018) "
            "or later (ConPTY API not found in kernel32.dll).",
            "Terminal unavailable", wxICON_ERROR | wxOK);
        return false;
    }

    // ── Create two anonymous pipe pairs ───────────────────────────────────
    //
    //   PTY side:  hPTYin  ← app writes  |  hPTYout → app reads
    //   App side:  m_hWrite               |  m_hRead
    //
    HANDLE hPTYin = INVALID_HANDLE_VALUE;
    HANDLE hPTYout = INVALID_HANDLE_VALUE;

    if (!CreatePipe(&m_hRead, &hPTYout, nullptr, 0))
        return false;
    if (!CreatePipe(&hPTYin, &m_hWrite, nullptr, 0))
    {
        CloseHandle(m_hRead);
        CloseHandle(hPTYout);
        return false;
    }

    // ── Create the pseudo-console ─────────────────────────────────────────
    COORD size{(SHORT)m_cols, (SHORT)m_rows};
    HRESULT hr = m_pfnCreate(size, hPTYin, hPTYout, 0, &m_hPC);

    // ConPTY now owns both PTY-side handles; close our copies
    CloseHandle(hPTYin);
    CloseHandle(hPTYout);

    if (FAILED(hr))
    {
        CloseHandle(m_hRead);
        CloseHandle(m_hWrite);
        return false;
    }

    // ── Build STARTUPINFOEX with the ConPTY attribute ─────────────────────
    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);

    auto attrBuf = std::make_unique<char[]>(attrSize);
    LPPROC_THREAD_ATTRIBUTE_LIST attrList =
        reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrBuf.get());

    if (!InitializeProcThreadAttributeList(attrList, 1, 0, &attrSize))
        return false;

    if (!UpdateProcThreadAttribute(
            attrList, 0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
            m_hPC, sizeof(m_hPC), nullptr, nullptr))
    {
        DeleteProcThreadAttributeList(attrList);
        return false;
    }

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.lpAttributeList = attrList;

    // ── Choose shell: prefer PowerShell, fall back to cmd.exe ────────────
    wchar_t shell[MAX_PATH];
    {
        wchar_t tmp[MAX_PATH];
        bool found = false;

        // Try pwsh.exe (PowerShell 7+)
        if (!found && ExpandEnvironmentStringsW(L"%ProgramFiles%\\PowerShell\\7\\pwsh.exe", tmp, MAX_PATH) && GetFileAttributesW(tmp) != INVALID_FILE_ATTRIBUTES)
        {
            wcscpy_s(shell, tmp);
            found = true;
        }

        // Try classic Windows PowerShell
        if (!found && ExpandEnvironmentStringsW(L"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe", tmp, MAX_PATH) && GetFileAttributesW(tmp) != INVALID_FILE_ATTRIBUTES)
        {
            wcscpy_s(shell, tmp);
            found = true;
        }

        // Guaranteed fallback
        if (!found)
            wcscpy_s(shell, L"cmd.exe");
    }

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(
        nullptr, shell,
        nullptr, nullptr,
        FALSE, // do NOT inherit handles
        EXTENDED_STARTUPINFO_PRESENT,
        nullptr, nullptr,
        &si.StartupInfo, &pi);

    DeleteProcThreadAttributeList(attrList);

    if (!ok)
        return false;

    m_hProc = pi.hProcess;
    CloseHandle(pi.hThread); // we don't need the thread handle
    return true;

#else // ── Linux / macOS ────────────────────────────────────────────────────
    struct winsize ws{};
    ws.ws_col = (unsigned short)m_cols;
    ws.ws_row = (unsigned short)m_rows;
    ws.ws_xpixel = (unsigned short)(m_cols * m_cw);
    ws.ws_ypixel = (unsigned short)(m_rows * m_ch);

    m_pid = forkpty(&m_fd, nullptr, nullptr, &ws);
    if (m_pid < 0)
        return false;

    if (m_pid == 0)
    {
        // Child process
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        execl("/bin/bash", "bash", nullptr);
        _exit(1);
    }

    // Parent: make PTY fd non-blocking so epoll can drain it
    int fl = fcntl(m_fd, F_GETFL, 0);
    fcntl(m_fd, F_SETFL, fl | O_NONBLOCK);
    return true;
#endif
}

// ═════════════════════════════════════════════════════════════════════════
//  CleanupPTY
// ═════════════════════════════════════════════════════════════════════════
void LibVTermPanel::CleanupPTY()
{
#ifdef _WIN32
    // Close the write pipe first so ReadFile in the reader thread unblocks
    if (m_hWrite != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hWrite);
        m_hWrite = INVALID_HANDLE_VALUE;
    }
    if (m_hRead != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hRead);
        m_hRead = INVALID_HANDLE_VALUE;
    }
    if (m_hProc != INVALID_HANDLE_VALUE)
    {
        TerminateProcess(m_hProc, 0);
        CloseHandle(m_hProc);
        m_hProc = INVALID_HANDLE_VALUE;
    }
    if (m_hPC && m_pfnClose)
    {
        m_pfnClose(m_hPC);
        m_hPC = nullptr;
    }
#else
    if (m_fd >= 0)
    {
        close(m_fd);
        m_fd = -1;
    }
    // bash will exit when its controlling terminal closes
#endif
}

// ═════════════════════════════════════════════════════════════════════════
//  WritePTY  –  send key data to the shell
// ═════════════════════════════════════════════════════════════════════════
void LibVTermPanel::WritePTY(const char *buf, size_t len)
{
    if (!len)
        return;
#ifdef _WIN32
    if (m_hWrite == INVALID_HANDLE_VALUE)
        return;
    DWORD written = 0;
    WriteFile(m_hWrite, buf, (DWORD)len, &written, nullptr);
#else
    if (m_fd < 0)
        return;
    ::write(m_fd, buf, len);
#endif
}

// ═════════════════════════════════════════════════════════════════════════
//  ReaderThread  –  pulls VT bytes from the PTY and feeds libvterm
// ═════════════════════════════════════════════════════════════════════════
void LibVTermPanel::ReaderThread()
{
#ifdef _WIN32
    // ReadFile blocks until data arrives or the pipe is closed – perfect.
    char buf[4096];
    DWORD n = 0;

    while (m_running)
    {
        BOOL ok = ReadFile(m_hRead, buf, sizeof buf, &n, nullptr);

        if (ok && n > 0)
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            vterm_input_write(m_vt, buf, n);
            vterm_screen_flush_damage(m_scr);
        }
        else
        {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
                break; // process exited
            // Any other transient error: small sleep, then retry
            Sleep(10);
        }
    }
    m_running = false;

#else // ── Linux: epoll for efficient blocking wait ─────────────────────────
    int ep = epoll_create1(0);

    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLHUP | EPOLLERR;
    ev.data.fd = m_fd;
    epoll_ctl(ep, EPOLL_CTL_ADD, m_fd, &ev);

    epoll_event events[4];

    while (m_running)
    {
        int n = epoll_wait(ep, events, 4, 50 /*ms*/);

        for (int i = 0; i < n; ++i)
        {
            if (events[i].events & EPOLLIN)
            {
                char buf[4096];
                ssize_t r;
                // Drain all available data before releasing the lock
                while ((r = ::read(m_fd, buf, sizeof buf)) > 0)
                {
                    std::lock_guard<std::mutex> lk(m_mtx);
                    vterm_input_write(m_vt, buf, (size_t)r);
                    vterm_screen_flush_damage(m_scr);
                }
            }
            if (events[i].events & (EPOLLHUP | EPOLLERR))
                m_running = false;
        }
    }

    close(ep);
#endif
}

// ═════════════════════════════════════════════════════════════════════════
//  ResizePTY
// ═════════════════════════════════════════════════════════════════════════
void LibVTermPanel::ResizePTY()
{
#ifdef _WIN32
    if (m_pfnResize && m_hPC)
    {
        COORD size{(SHORT)m_cols, (SHORT)m_rows};
        m_pfnResize(m_hPC, size);
    }
#else
    if (m_fd < 0)
        return;
    struct winsize ws{};
    ws.ws_col = (unsigned short)m_cols;
    ws.ws_row = (unsigned short)m_rows;
    ws.ws_xpixel = (unsigned short)(m_cols * m_cw);
    ws.ws_ypixel = (unsigned short)(m_rows * m_ch);
    ioctl(m_fd, TIOCSWINSZ, &ws);
#endif
}

// ═════════════════════════════════════════════════════════════════════════
//  VTermColor → wxColour
// ═════════════════════════════════════════════════════════════════════════
  wxColour LibVTermPanel::ToWx(const VTermColor &c, bool fg)
{
    if (VTERM_COLOR_IS_DEFAULT_FG(&c))
        return {204, 204, 204};
    if (VTERM_COLOR_IS_DEFAULT_BG(&c))
        return {0, 0, 0};

    if (VTERM_COLOR_IS_RGB(&c))
        return {c.rgb.red, c.rgb.green, c.rgb.blue};

    if (VTERM_COLOR_IS_INDEXED(&c))
    {
        uint8_t idx = c.indexed.idx;

        // Standard 16 ANSI colours
        static const wxColour k16[16] = {
            {0, 0, 0},
            {170, 0, 0},
            {0, 170, 0},
            {170, 85, 0},
            {0, 0, 170},
            {170, 0, 170},
            {0, 170, 170},
            {170, 170, 170},
            {85, 85, 85},
            {255, 85, 85},
            {85, 255, 85},
            {255, 255, 85},
            {85, 85, 255},
            {255, 85, 255},
            {85, 255, 255},
            {255, 255, 255},
        };
        if (idx < 16)
            return k16[idx];

        // 216-colour cube (indices 16–231)
        if (idx < 232)
        {
            idx -= 16;
            uint8_t b = idx % 6;
            idx /= 6;
            uint8_t g = idx % 6;
            idx /= 6;
            uint8_t r = idx % 6;
            auto cv = [](uint8_t x) -> uint8_t
            { return x ? 55 + x * 40 : 0; };
            return {cv(r), cv(g), cv(b)};
        }

        // Greyscale ramp (indices 232–255)
        uint8_t v = (uint8_t)(8 + (idx - 232) * 10);
        return {v, v, v};
    }

    return fg ? wxColour{204, 204, 204} : wxColour{0, 0, 0};
}

// ═════════════════════════════════════════════════════════════════════════
//  OnSize  –  resize vterm + PTY when the panel resizes
// ═════════════════════════════════════════════════════════════════════════
void LibVTermPanel::OnSize(wxSizeEvent &evt)
{
    wxSize sz = GetClientSize();
    int nc = std::max(1, sz.x / m_cw);
    int nr = std::max(1, sz.y / m_ch);

    if (nc != m_cols || nr != m_rows)
    {
        m_cols = nc;
        m_rows = nr;

        {
            std::lock_guard<std::mutex> lk(m_mtx);
            vterm_set_size(m_vt, m_rows, m_cols);
            vterm_screen_flush_damage(m_scr);
        }

        ResizePTY();
    }

    Refresh(false);
    evt.Skip();
}

// ═════════════════════════════════════════════════════════════════════════
//  OnPaint  –  render the vterm screen cell-by-cell
// ═════════════════════════════════════════════════════════════════════════
void LibVTermPanel::OnPaint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(*wxBLACK_BRUSH);
    dc.Clear();
    dc.SetFont(m_font);

    std::lock_guard<std::mutex> lk(m_mtx);

    VTermState *state = vterm_obtain_state(m_vt);
    VTermPos cur{0, 0};
    vterm_state_get_cursorpos(state, &cur);

    VTermScreenCell cell;

    for (int row = 0; row < m_rows; ++row)
    {
        for (int col = 0; col < m_cols;)
        {
            VTermPos pos{row, col};
            vterm_screen_get_cell(m_scr, pos, &cell);

            int w = std::max(1, (int)cell.width);
            int px = col * m_cw;
            int py = row * m_ch;
            int pw = w * m_cw;

            wxColour fg = ToWx(cell.fg, true);
            wxColour bg = ToWx(cell.bg, false);

            if (cell.attrs.reverse)
                std::swap(fg, bg);

            // Background fill
            if (bg.Red() || bg.Green() || bg.Blue())
            {
                dc.SetBrush(wxBrush(bg));
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.DrawRectangle(px, py, pw, m_ch);
            }

            // Cursor block
            if (row == cur.row && col == cur.col)
            {
                dc.SetBrush(wxBrush(wxColour(200, 200, 200)));
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.DrawRectangle(px, py, pw, m_ch);
                fg = *wxBLACK;
            }

            // Glyph
            if (cell.chars[0] != 0)
            {
                wxString text;
                for (int k = 0;
                     k < VTERM_MAX_CHARS_PER_CELL && cell.chars[k]; ++k)
                    text += wxUniChar(cell.chars[k]);

                wxFont f = m_font;
                if (cell.attrs.bold)
                    f.SetWeight(wxFONTWEIGHT_BOLD);
                if (cell.attrs.italic)
                    f.SetStyle(wxFONTSTYLE_ITALIC);
                if (f != m_font)
                    dc.SetFont(f);

                dc.SetTextForeground(fg);
                dc.SetBackgroundMode(wxTRANSPARENT);
                dc.DrawText(text, px, py);

                if (f != m_font)
                    dc.SetFont(m_font);

                if (cell.attrs.underline)
                {
                    dc.SetPen(wxPen(fg, 1));
                    dc.DrawLine(px, py + m_ch - 2, px + pw, py + m_ch - 2);
                }
                if (cell.attrs.strike)
                {
                    dc.SetPen(wxPen(fg, 1));
                    dc.DrawLine(px, py + m_ch / 2, px + pw, py + m_ch / 2);
                }
            }

            col += w;
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════
//  OnKeyDown  –  function / navigation keys → escape sequences
// ═════════════════════════════════════════════════════════════════════════
void LibVTermPanel::OnKeyDown(wxKeyEvent &evt)
{
    struct Map
    {
        int key;
        const char *seq;
    };
    static constexpr Map table[] = {
        {WXK_UP, "\x1b[A"},
        {WXK_DOWN, "\x1b[B"},
        {WXK_RIGHT, "\x1b[C"},
        {WXK_LEFT, "\x1b[D"},
        {WXK_HOME, "\x1b[H"},
        {WXK_END, "\x1b[F"},
        {WXK_PAGEUP, "\x1b[5~"},
        {WXK_PAGEDOWN, "\x1b[6~"},
        {WXK_INSERT, "\x1b[2~"},
        {WXK_DELETE, "\x1b[3~"},
        {WXK_F1, "\x1bOP"},
        {WXK_F2, "\x1bOQ"},
        {WXK_F3, "\x1bOR"},
        {WXK_F4, "\x1bOS"},
        {WXK_F5, "\x1b[15~"},
        {WXK_F6, "\x1b[17~"},
        {WXK_F7, "\x1b[18~"},
        {WXK_F8, "\x1b[19~"},
        {WXK_F9, "\x1b[20~"},
        {WXK_F10, "\x1b[21~"},
        {WXK_F11, "\x1b[23~"},
        {WXK_F12, "\x1b[24~"},
    };

    int k = evt.GetKeyCode();
    for (const auto &m : table)
        if (k == m.key)
        {
            WritePTY(m.seq, std::strlen(m.seq));
            return;
        }

    evt.Skip(); // pass everything else to OnChar
}

// ═════════════════════════════════════════════════════════════════════════
//  OnChar  –  printable characters, control sequences, UTF-8
// ═════════════════════════════════════════════════════════════════════════
void LibVTermPanel::OnChar(wxKeyEvent &evt)
{
    wxChar uni = evt.GetUnicodeKey();
    if (uni == WXK_NONE)
    {
        evt.Skip();
        return;
    }

    // Special virtual keys
    switch (uni)
    {
    case WXK_BACK:
    case WXK_DELETE:
    {
        char c = 0x7f;
        WritePTY(&c, 1);
        return;
    }
    case WXK_RETURN:
    case WXK_NUMPAD_ENTER:
    {
        char c = '\r';
        WritePTY(&c, 1);
        return;
    }
    case WXK_TAB:
    {
        char c = '\t';
        WritePTY(&c, 1);
        return;
    }
    case WXK_ESCAPE:
    {
        char c = 0x1b;
        WritePTY(&c, 1);
        return;
    }
    default:
        break;
    }

    // Ctrl+letter → ASCII control code (e.g. Ctrl+C = 0x03)
    if (evt.ControlDown())
    {
        if (uni >= 'A' && uni <= 'Z')
        {
            char c = (char)(uni - 'A' + 1);
            WritePTY(&c, 1);
            return;
        }
        if (uni >= 'a' && uni <= 'z')
        {
            char c = (char)(uni - 'a' + 1);
            WritePTY(&c, 1);
            return;
        }
    }

    // Regular Unicode → UTF-8
    wxString s{wxUniChar(uni)};
    auto u8 = s.ToStdString();
    WritePTY(u8.data(), u8.size());
}
 
