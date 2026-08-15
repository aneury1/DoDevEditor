
#if 0 
#ifndef WX_LIBVTERM_PANEL_H
#define WX_LIBVTERM_PANEL_H
#include <wx/wx.h>
#include <wx/dcbuffer.h>

///#define LINUX_PLATFORM
#ifdef  LINUX_PLATFORM
#include <vterm.h>
#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#endif

#include <cstring>
#include <thread>
#include <mutex>
#include <atomic>

#ifndef LINUX_PLATFORM
struct VTermRect{};
#endif


class LibVTermPanel : public wxPanel
{
#ifdef  LINUX_PLATFORM
    // ── Static damage callback (function pointer – no capture) ────────────────
    static int s_Damage(VTermRect /*rect*/, void* user)
    {
        static_cast<LibVTermPanel*>(user)->m_dirty = true;
        return 1;
    }

    inline static VTermScreenCallbacks s_cbs{};
#endif
public:
    explicit LibVTermPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY,
                  wxDefaultPosition, wxDefaultSize,
                  wxWANTS_CHARS | wxNO_BORDER)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(*wxBLACK);

        // ── Monospace font ────────────────────────────────────────────────────
        m_font = wxFont(12, wxFONTFAMILY_TELETYPE,
                        wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL,
                        false, "Monospace");
#ifdef  LINUX_PLATFORM
        // Measure a single cell
        {
            wxClientDC dc(this);
            dc.SetFont(m_font);
            wxSize sz = dc.GetTextExtent("M");
            m_cw = std::max(1, sz.x);
            m_ch = std::max(1, sz.y + 2);  // +2 px leading
        }

        // Initial column / row count
        wxSize panel = GetClientSize();
        m_cols = std::max(1, panel.x / m_cw);
        m_rows = std::max(1, panel.y / m_ch);


        // ── libvterm ──────────────────────────────────────────────────────────
        m_vt = vterm_new(m_rows, m_cols);
        vterm_set_utf8(m_vt, 1);

        m_scr = vterm_obtain_screen(m_vt);

        s_cbs          = {};
        s_cbs.damage   = &LibVTermPanel::s_Damage;
        vterm_screen_set_callbacks(m_scr, &s_cbs, this);
        vterm_screen_reset(m_scr, 1);

        // ── PTY + bash ────────────────────────────────────────────────────────
        struct winsize ws{};
        ws.ws_col    = (unsigned short)m_cols;
        ws.ws_row    = (unsigned short)m_rows;
        ws.ws_xpixel = (unsigned short)(m_cols * m_cw);
        ws.ws_ypixel = (unsigned short)(m_rows * m_ch);

        m_pid = forkpty(&m_fd, nullptr, nullptr, &ws);

        if (m_pid == 0) {           // ── child ──────────────────────────────
            setenv("TERM",      "xterm-256color", 1);
            setenv("COLORTERM", "truecolor",       1);
            execl("/bin/bash", "bash", nullptr);
            _exit(1);
        }

        // Parent: set PTY non-blocking
        int fl = fcntl(m_fd, F_GETFL, 0);
        fcntl(m_fd, F_SETFL, fl | O_NONBLOCK);

        // ── Reader thread (epoll) ─────────────────────────────────────────────
        m_running = true;
        m_thread  = std::thread([this]
        {
            int ep = epoll_create1(0);

            epoll_event ev{};
            ev.events  = EPOLLIN | EPOLLHUP | EPOLLERR;
            ev.data.fd = m_fd;
            epoll_ctl(ep, EPOLL_CTL_ADD, m_fd, &ev);

            epoll_event events[4];

            while (m_running)
            {
                int n = epoll_wait(ep, events, 4, 50 /*ms timeout*/);

                for (int i = 0; i < n; ++i)
                {
                    if (events[i].events & EPOLLIN)
                    {
                        char buf[4096];
                        ssize_t r;
                        // Drain all available data before unlocking
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
        });

        // ── wx event bindings ────────────────────────────────────────────────
        Bind(wxEVT_PAINT,     &LibVTermPanel::OnPaint,   this);
        Bind(wxEVT_SIZE,      &LibVTermPanel::OnSize,    this);
        Bind(wxEVT_KEY_DOWN,  &LibVTermPanel::OnKeyDown, this);
        Bind(wxEVT_CHAR,      &LibVTermPanel::OnChar,    this);
        // Focus on click
        Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&){ SetFocus(); });

        m_timer.SetOwner(this);
        Bind(wxEVT_TIMER, [this](wxTimerEvent&)
        {
            // Repaint only when vterm signalled damage
            if (m_dirty.exchange(false))
                Refresh(false);
        });
        m_timer.Start(16); // ~60 fps

#endif
    }

    ~LibVTermPanel()
    {
        #ifdef  LINUX_PLATFORM
        m_timer.Stop();
        m_running = false;
        if (m_thread.joinable()) m_thread.join();
        if (m_fd  >= 0) close(m_fd);
        if (m_vt)       vterm_free(m_vt);
        #endif
    }

    // Write a command string to the shell (appends '\n')
    void Write(const std::string& cmd)
    {
        #ifdef  LINUX_PLATFORM
        std::string s = cmd + "\n";
        ::write(m_fd, s.c_str(), s.size());
        #endif
    }

private:
#ifdef  LINUX_PLATFORM
    // ── VTermColor → wxColour ─────────────────────────────────────────────────
    static wxColour ToWx(const VTermColor& c, bool fg)
    {
        if (VTERM_COLOR_IS_DEFAULT_FG(&c)) return {204, 204, 204};
        if (VTERM_COLOR_IS_DEFAULT_BG(&c)) return {  0,   0,   0};

        if (VTERM_COLOR_IS_RGB(&c))
            return {c.rgb.red, c.rgb.green, c.rgb.blue};

        if (VTERM_COLOR_IS_INDEXED(&c))
        {
            uint8_t idx = c.indexed.idx;

            // Standard 16 ANSI colours
            static const wxColour k16[16] = {
                {  0,  0,  0}, {170,  0,  0}, {  0,170,  0}, {170, 85,  0},
                {  0,  0,170}, {170,  0,170}, {  0,170,170}, {170,170,170},
                { 85, 85, 85}, {255, 85, 85}, { 85,255, 85}, {255,255, 85},
                { 85, 85,255}, {255, 85,255}, { 85,255,255}, {255,255,255},
            };
            if (idx < 16) return k16[idx];

            // 216-colour cube  (indices 16–231)
            if (idx < 232)
            {
                idx -= 16;
                uint8_t b = idx % 6; idx /= 6;
                uint8_t g = idx % 6; idx /= 6;
                uint8_t r = idx % 6;
                auto cv = [](uint8_t x) -> uint8_t { return x ? 55 + x * 40 : 0; };
                return {cv(r), cv(g), cv(b)};
            }

            // Greyscale ramp  (indices 232–255)
            uint8_t v = (uint8_t)(8 + (idx - 232) * 10);
            return {v, v, v};
        }

        return fg ? wxColour{204, 204, 204} : wxColour{0, 0, 0};
    }
#endif
    // ── Resize panel + vterm + PTY ────────────────────────────────────────────
    void OnSize(wxSizeEvent& evt)
    {
#ifdef  LINUX_PLATFORM
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

            struct winsize ws{};
            ws.ws_col    = (unsigned short)m_cols;
            ws.ws_row    = (unsigned short)m_rows;
            ws.ws_xpixel = (unsigned short)(m_cols * m_cw);
            ws.ws_ypixel = (unsigned short)(m_rows * m_ch);
            ioctl(m_fd, TIOCSWINSZ, &ws);
        }

        Refresh(false);
#endif
        evt.Skip();
    }

    // ── Render ────────────────────────────────────────────────────────────────
    void OnPaint(wxPaintEvent&)
    {
#ifdef  LINUX_PLATFORM
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(*wxBLACK_BRUSH);
        dc.Clear();
        dc.SetFont(m_font);

        std::lock_guard<std::mutex> lk(m_mtx);

        // Cursor position
        VTermState* state = vterm_obtain_state(m_vt);
        VTermPos    cur{0, 0};
        vterm_state_get_cursorpos(state, &cur);

        VTermScreenCell cell;

        for (int row = 0; row < m_rows; ++row)
        {
            for (int col = 0; col < m_cols; )
            {
                // NOTE: VTermPos is {row, col} — original code had {x,y} which is wrong
                VTermPos pos{row, col};
                vterm_screen_get_cell(m_scr, pos, &cell);

                int w  = std::max(1, (int)cell.width);
                int px = col * m_cw;
                int py = row * m_ch;
                int pw = w   * m_cw;

                wxColour fg = ToWx(cell.fg, true);
                wxColour bg = ToWx(cell.bg, false);

                if (cell.attrs.reverse) std::swap(fg, bg);

                // ── Background fill ───────────────────────────────────────────
                if (bg.Red() || bg.Green() || bg.Blue())
                {
                    dc.SetBrush(wxBrush(bg));
                    dc.SetPen(*wxTRANSPARENT_PEN);
                    dc.DrawRectangle(px, py, pw, m_ch);
                }

                // ── Cursor block ──────────────────────────────────────────────
                if (row == cur.row && col == cur.col)
                {
                    dc.SetBrush(wxBrush(wxColour(200, 200, 200)));
                    dc.SetPen(*wxTRANSPARENT_PEN);
                    dc.DrawRectangle(px, py, pw, m_ch);
                    fg = *wxBLACK;
                }

                // ── Glyph ─────────────────────────────────────────────────────
                if (cell.chars[0] != 0)
                {
                    // cell.chars[] is uint32_t codepoints, NOT wchar_t*
                    wxString text;
                    for (int k = 0;
                         k < VTERM_MAX_CHARS_PER_CELL && cell.chars[k]; ++k)
                        text += wxUniChar(cell.chars[k]);

                    // Per-cell font style (bold / italic)
                    wxFont f = m_font;
                    if (cell.attrs.bold)
                        f.SetWeight(wxFONTWEIGHT_BOLD);
                    if (cell.attrs.italic)
                        f.SetStyle(wxFONTSTYLE_ITALIC);
                    if (f != m_font) dc.SetFont(f);

                    dc.SetTextForeground(fg);
                    dc.SetBackgroundMode(wxTRANSPARENT);
                    dc.DrawText(text, px, py);

                    if (f != m_font) dc.SetFont(m_font); // restore

                    // Underline
                    if (cell.attrs.underline)
                    {
                        dc.SetPen(wxPen(fg, 1));
                        dc.DrawLine(px, py + m_ch - 2, px + pw, py + m_ch - 2);
                    }
                    // Strike-through
                    if (cell.attrs.strike)
                    {
                        dc.SetPen(wxPen(fg, 1));
                        dc.DrawLine(px, py + m_ch / 2, px + pw, py + m_ch / 2);
                    }
                }

                col += w;
            }
        }
#endif
    }

    // ── Special / function keys → escape sequences ────────────────────────────
    void OnKeyDown(wxKeyEvent& evt)
    {
#ifdef  LINUX_PLATFORM
        struct Map { int key; const char* seq; } table[] = {
            { WXK_UP,       "\x1b[A"   }, { WXK_DOWN,     "\x1b[B"   },
            { WXK_RIGHT,    "\x1b[C"   }, { WXK_LEFT,     "\x1b[D"   },
            { WXK_HOME,     "\x1b[H"   }, { WXK_END,      "\x1b[F"   },
            { WXK_PAGEUP,   "\x1b[5~"  }, { WXK_PAGEDOWN, "\x1b[6~"  },
            { WXK_INSERT,   "\x1b[2~"  }, { WXK_DELETE,   "\x1b[3~"  },
            { WXK_F1,       "\x1bOP"   }, { WXK_F2,       "\x1bOQ"   },
            { WXK_F3,       "\x1bOR"   }, { WXK_F4,       "\x1bOS"   },
            { WXK_F5,       "\x1b[15~" }, { WXK_F6,       "\x1b[17~" },
            { WXK_F7,       "\x1b[18~" }, { WXK_F8,       "\x1b[19~" },
            { WXK_F9,       "\x1b[20~" }, { WXK_F10,      "\x1b[21~" },
            { WXK_F11,      "\x1b[23~" }, { WXK_F12,      "\x1b[24~" },
        };

        int k = evt.GetKeyCode();
        for (auto& m : table)
            if (k == m.key) { ::write(m_fd, m.seq, std::strlen(m.seq)); return; }
#endif
        evt.Skip(); // let OnChar handle everything else
    }

    // ── Printable + control characters ────────────────────────────────────────
    void OnChar(wxKeyEvent& evt)
    {
#ifdef  LINUX_PLATFORM
        wxChar uni = evt.GetUnicodeKey();
        if (uni == WXK_NONE) { evt.Skip(); return; }

        // Map special virtual keys
        switch (uni)
        {
            case WXK_BACK:        { char c = 0x7f; ::write(m_fd, &c, 1); return; }
            case WXK_RETURN:
            case WXK_NUMPAD_ENTER:{ char c = '\r'; ::write(m_fd, &c, 1); return; }
            case WXK_TAB:         { char c = '\t'; ::write(m_fd, &c, 1); return; }
            case WXK_ESCAPE:      { char c = 0x1b; ::write(m_fd, &c, 1); return; }
            default: break;
        }

        // Ctrl+letter → ASCII control code  (Ctrl+C = 0x03, etc.)
        if (evt.ControlDown())
        {
            if (uni >= 'A' && uni <= 'Z') { char c = (char)(uni - 'A' + 1); ::write(m_fd, &c, 1); return; }
            if (uni >= 'a' && uni <= 'z') { char c = (char)(uni - 'a' + 1); ::write(m_fd, &c, 1); return; }
        }

        // Regular Unicode → UTF-8 bytes
         wxString            s{wxUniChar(uni)};
        /// wxScopedCharBuffer  u8 = s.ToStdString();
        auto u8 = s.ToStdString();
        ::write(m_fd, u8.data(), u8.length());
#endif
 
    }

private:
#ifdef  LINUX_PLATFORM
    VTerm*       m_vt  = nullptr;
    VTermScreen* m_scr = nullptr;
    int          m_fd  = -1;
    pid_t        m_pid = -1;

    int    m_cols = 80;
    int    m_rows = 24;
    int    m_cw   = 8;    // cell width  (px)
    int    m_ch   = 16;   // cell height (px)
    wxFont m_font;

    std::thread       m_thread;
    std::mutex        m_mtx;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_dirty{false};
    wxTimer           m_timer;
#endif
};

#endif // WX_LIBVTERM_PANEL_H
#endif 


#ifndef WX_LIBVTERM_PANEL_H
#define WX_LIBVTERM_PANEL_H

// ─────────────────────────────────────────────────────────────────────────────
//  Platform detection + PTY includes
// ─────────────────────────────────────────────────────────────────────────────
#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   define NOMINMAX
#   include <windows.h>
#   include <memory>   // std::unique_ptr (attr-list buffer)

    // ConPTY handle – defined in consoleapi.h from SDK 10.0.17763+.
    // We typedef our own name so we compile with any MinGW SDK.
    using HPCON_t = PVOID;

    // Function-pointer types for dynamic loading
    using PFN_CreatePseudoConsole = HRESULT (WINAPI*)(COORD, HANDLE, HANDLE, DWORD, HPCON_t*);
    using PFN_ResizePseudoConsole = HRESULT (WINAPI*)(HPCON_t, COORD);
    using PFN_ClosePseudoConsole  = VOID    (WINAPI*)(HPCON_t);
#else
#   include <pty.h>
#   include <unistd.h>
#   include <fcntl.h>
#   include <sys/epoll.h>
#   include <sys/ioctl.h>
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

// ─────────────────────────────────────────────────────────────────────────────
class LibVTermPanel : public wxPanel
{
    // ── Damage callback (must be a plain function pointer – no captures) ───────
    static int s_Damage(VTermRect /*rect*/, void* user);
    inline static VTermScreenCallbacks s_cbs{};

public:
    explicit LibVTermPanel(wxWindow* parent);

    ~LibVTermPanel();
    // Write a command string to the shell (appends '\n')
    void Write(const std::string& cmd);


private:
    // ═════════════════════════════════════════════════════════════════════════
    //  Platform-specific state
    // ═════════════════════════════════════════════════════════════════════════
#ifdef _WIN32
    HPCON_t  m_hPC    = nullptr;
    HANDLE   m_hRead  = INVALID_HANDLE_VALUE;  // app reads PTY output from here
    HANDLE   m_hWrite = INVALID_HANDLE_VALUE;  // app writes keystrokes here
    HANDLE   m_hProc  = INVALID_HANDLE_VALUE;  // child process handle

    PFN_CreatePseudoConsole m_pfnCreate = nullptr;
    PFN_ResizePseudoConsole m_pfnResize = nullptr;
    PFN_ClosePseudoConsole  m_pfnClose  = nullptr;
#else
    int   m_fd  = -1;
    pid_t m_pid = -1;
#endif

    // ═════════════════════════════════════════════════════════════════════════
    //  SpawnShell  –  platform implementations
    // ═════════════════════════════════════════════════════════════════════════
    bool SpawnShell();


    // ═════════════════════════════════════════════════════════════════════════
    //  CleanupPTY
    // ═════════════════════════════════════════════════════════════════════════
    void CleanupPTY();



  
    // ═════════════════════════════════════════════════════════════════════════
    //  WritePTY  –  send key data to the shell
    // ═════════════════════════════════════════════════════════════════════════
    void WritePTY(const char* buf, size_t len);

    // ═════════════════════════════════════════════════════════════════════════
    //  ReaderThread  –  pulls VT bytes from the PTY and feeds libvterm
    // ═════════════════════════════════════════════════════════════════════════
    void ReaderThread();



    // ═════════════════════════════════════════════════════════════════════════
    //  ResizePTY
    // ═════════════════════════════════════════════════════════════════════════
    void ResizePTY();



    // ═════════════════════════════════════════════════════════════════════════
    //  VTermColor → wxColour
    // ═════════════════════════════════════════════════════════════════════════
    static wxColour ToWx(const VTermColor& c, bool fg);

    // ═════════════════════════════════════════════════════════════════════════
    //  OnSize  –  resize vterm + PTY when the panel resizes
    // ═════════════════════════════════════════════════════════════════════════
    void OnSize(wxSizeEvent& evt);




    // ═════════════════════════════════════════════════════════════════════════
    //  OnPaint  –  render the vterm screen cell-by-cell
    // ═════════════════════════════════════════════════════════════════════════
    void OnPaint(wxPaintEvent&);



 
    // ═════════════════════════════════════════════════════════════════════════
    //  OnKeyDown  –  function / navigation keys → escape sequences
    // ═════════════════════════════════════════════════════════════════════════
    void OnKeyDown(wxKeyEvent& evt);

  // ═════════════════════════════════════════════════════════════════════════
    //  OnChar  –  printable characters, control sequences, UTF-8
    // ═════════════════════════════════════════════════════════════════════════
    void OnChar(wxKeyEvent& evt);
  // ═════════════════════════════════════════════════════════════════════════
    //  Members
    // ═════════════════════════════════════════════════════════════════════════
    VTerm*       m_vt  = nullptr;
    VTermScreen* m_scr = nullptr;

    int m_cols = 80;
    int m_rows = 24;
    int m_cw   = 8;
    int m_ch   = 16;

    wxFont  m_font;
    wxTimer m_timer;

    std::thread       m_thread;
    std::mutex        m_mtx;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_dirty{false};
};

#endif // WX_LIBVTERM_PANEL_H


