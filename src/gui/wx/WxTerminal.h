#pragma once
#include <wx/clipbrd.h>
#include <wx/dcbuffer.h>
#include <wx/wx.h>

#include <algorithm>
#include <functional>

#include "TerminalScreen.h"
#include "TerminalSession.h"

// The PTY owns line editing and echo; this widget only renders VT cells and
// sends keys.
class WxTerminal : public wxPanel {
  dodev::TerminalSession& session;
  dodev::TerminalScreen& screen;
  wxFont font{wxFontInfo(11).Family(wxFONTFAMILY_TELETYPE)};
  wxColour bg{10, 15, 26}, fg{226, 232, 240}, selection{45, 79, 120};
  int cw = 9, ch = 18, anchor = -1, end = -1;
  bool selecting = false;
  std::function<bool()> ensure;
  static wxString text(const std::string& s) {
    return wxString::FromUTF8(s.data(), s.size());
  }
  wxColour colour(dodev::TerminalColor c, bool background) const {
    if (background && c.r == 10 && c.g == 15 && c.b == 26) return bg;
    if (!background && c.r == 226 && c.g == 232 && c.b == 240) return fg;
    return wxColour(c.r, c.g, c.b);
  }
  int position(wxPoint p) const {
    return std::clamp(p.y / ch, 0, screen.rows() - 1) * screen.columns() +
           std::clamp(p.x / cw, 0, screen.columns() - 1);
  }
  bool selected(int i) const {
    return anchor >= 0 && end >= 0 && anchor != end &&
           i >= std::min(anchor, end) && i <= std::max(anchor, end);
  }
  void paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(bg));
    dc.Clear();
    dc.SetFont(font);
    const auto size = GetClientSize();
    for (int r = 0; r < screen.rows() && r * ch < size.y; ++r) {
      for (int c = 0; c < screen.columns() && c * cw < size.x; ++c) {
        const auto& cell = screen.cell(r, c);
        bool sel = selected(r * screen.columns() + c);
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(sel ? selection : colour(cell.background, true)));
        dc.DrawRectangle(c * cw, r * ch, cw, ch);
        if (cell.glyph != " ") {
          auto f = font;
          f.SetWeight(cell.bold ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
          dc.SetFont(f);
          dc.SetTextForeground(colour(cell.foreground, false));
          dc.DrawText(text(cell.glyph), c * cw, r * ch);
        }
      }
    }
    if (session.running() && screen.cursorVisible() && HasFocus()) {
      dc.SetPen(wxPen(fg));
      dc.SetBrush(*wxTRANSPARENT_BRUSH);
      dc.DrawRectangle(screen.cursorX() * cw, screen.cursorY() * ch, cw, ch);
    }
  }
  void key(wxKeyEvent& e) {
    int k = e.GetKeyCode();
    std::string bytes;
    if (e.ControlDown() && e.ShiftDown() && k == 'C') {
      copy();
      return;
    }
    if (e.ControlDown() && e.ShiftDown() && k == 'V') {
      paste();
      return;
    }
    if (e.ShiftDown() && k == WXK_PAGEUP) {
      screen.scroll(screen.rows() - 1);
      clearSelection();
      return;
    }
    if (e.ShiftDown() && k == WXK_PAGEDOWN) {
      screen.scroll(1 - screen.rows());
      clearSelection();
      return;
    }
    if (e.ControlDown() && !e.AltDown() && k >= 'A' && k <= 'Z')
      bytes = std::string(1, char(k - 'A' + 1));
    else
      switch (k) {
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER:
          bytes = "\r";
          break;
        case WXK_BACK:
          bytes = "\x7f";
          break;
        case WXK_TAB:
          bytes = e.ShiftDown() ? "\x1b[Z" : "\t";
          break;
        case WXK_ESCAPE:
          bytes = "\x1b";
          break;
        case WXK_UP:
          bytes = screen.applicationCursor() ? "\x1bOA" : "\x1b[A";
          break;
        case WXK_DOWN:
          bytes = screen.applicationCursor() ? "\x1bOB" : "\x1b[B";
          break;
        case WXK_RIGHT:
          bytes = screen.applicationCursor() ? "\x1bOC" : "\x1b[C";
          break;
        case WXK_LEFT:
          bytes = screen.applicationCursor() ? "\x1bOD" : "\x1b[D";
          break;
        case WXK_HOME:
          bytes = screen.applicationCursor() ? "\x1bOH" : "\x1b[H";
          break;
        case WXK_END:
          bytes = screen.applicationCursor() ? "\x1bOF" : "\x1b[F";
          break;
        case WXK_DELETE:
          bytes = "\x1b[3~";
          break;
        case WXK_INSERT:
          bytes = "\x1b[2~";
          break;
        case WXK_PAGEUP:
          bytes = "\x1b[5~";
          break;
        case WXK_PAGEDOWN:
          bytes = "\x1b[6~";
          break;
        default:
          e.Skip();
          return;
      }
    send(bytes);
  }

 public:
  WxTerminal(wxWindow* parent, dodev::TerminalSession& s,
             dodev::TerminalScreen& v, std::function<bool()> start)
      : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                wxWANTS_CHARS),
        session(s),
        screen(v),
        ensure(std::move(start)) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetFont(font);
    Bind(wxEVT_PAINT, &WxTerminal::paint, this);
    Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
      resizeTerminal();
      e.Skip();
    });
    Bind(wxEVT_KEY_DOWN, &WxTerminal::key, this);
    Bind(wxEVT_CHAR, [this](wxKeyEvent& e) {
      int u = e.GetUnicodeKey();
      if (u >= 32 && u != WXK_NONE) {
        wxString str;
        str += wxUniChar(static_cast<unsigned int>(u));
        auto b = str.ToUTF8();
        if (b.data()) send(std::string(b.data(), b.length()));
      } else
        e.Skip();
    });
    Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& e) {
      Refresh();
      e.Skip();
    });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) {
      Refresh();
      e.Skip();
    });
    Bind(wxEVT_MOUSEWHEEL, [this](wxMouseEvent& e) {
      screen.scroll(e.GetWheelRotation() > 0 ? 3 : -3);
      clearSelection();
    });
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
      SetFocus();
      anchor = end = position(e.GetPosition());
      selecting = true;
      if (!HasCapture()) CaptureMouse();
      Refresh();
    });
    Bind(wxEVT_MOTION, [this](wxMouseEvent& e) {
      if (selecting && e.Dragging()) {
        end = position(e.GetPosition());
        Refresh();
      }
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) {
      if (selecting) end = position(e.GetPosition());
      selecting = false;
      if (HasCapture()) ReleaseMouse();
      Refresh();
    });
    Bind(wxEVT_MOUSE_CAPTURE_LOST,
         [this](wxMouseCaptureLostEvent&) { selecting = false; });
    Bind(wxEVT_CONTEXT_MENU, [this](wxContextMenuEvent&) {
      wxMenu m;
      m.Append(1, "Copy");
      m.Append(2, "Paste");
      m.Append(3, "Scroll to bottom");
      m.Bind(wxEVT_MENU, [this](wxCommandEvent& e) {
        if (e.GetId() == 1)
          copy();
        else if (e.GetId() == 2)
          paste();
        else {
          screen.toBottom();
          clearSelection();
        }
      });
      PopupMenu(&m);
    });
  }
  void setTheme(const wxColour& b, const wxColour& f, const wxColour& sel,
                int size, const wxString& family) {
    bg = b;
    fg = f;
    selection = sel;
    font = wxFont(
        wxFontInfo(std::clamp(size, 8, 48)).Family(wxFONTFAMILY_TELETYPE));
    if (!family.empty() && family != "monospace") font.SetFaceName(family);
    SetFont(font);
    resizeTerminal();
    Refresh();
  }
  bool AcceptsFocus() const override { return true; }
  void resizeTerminal() {
    wxClientDC dc(this);
    dc.SetFont(font);
    wxCoord w, h;
    dc.GetTextExtent("M", &w, &h);
    cw = std::max(1, int(w));
    ch = std::max(1, int(h) + 2);
    auto sz = GetClientSize();
    if (sz.x < 20 || sz.y < 20) return;
    screen.resize(sz.x / cw, sz.y / ch);
    if (session.running()) session.resize(screen.columns(), screen.rows());
    clearSelection();
  }
  void clearSelection() {
    anchor = end = -1;
    Refresh();
  }
  void send(const std::string& bytes) {
    if (!session.running() && !ensure()) return;
    screen.toBottom();
    clearSelection();
    if (!session.sendBytes(bytes)) wxLogWarning("Terminal write failed");
  }
  void copy() {
    std::string s;
    if (anchor >= 0 && end >= 0 && anchor != end) {
      int a = std::min(anchor, end), b = std::max(anchor, end);
      for (int i = a; i <= b; ++i) {
        if (i > a && i % screen.columns() == 0) s += '\n';
        s += screen.cell(i / screen.columns(), i % screen.columns()).glyph;
      }
    } else
      s = screen.fullText();
    if (wxTheClipboard->Open()) {
      wxTheClipboard->SetData(new wxTextDataObject(text(s)));
      wxTheClipboard->Close();
    }
  }
  void paste() {
    wxTextDataObject d;
    if (wxTheClipboard->Open()) {
      bool ok = wxTheClipboard->GetData(d);
      wxTheClipboard->Close();
      if (ok) {
        auto b = d.GetText().ToUTF8();
        if (b.data()) {
          std::string value(b.data(), b.length());
          if (value.find_first_of("\r\n") != std::string::npos &&
              !screen.bracketedPaste() &&
              wxMessageBox("Paste multiple lines into the terminal? They may "
                           "execute immediately.",
                           "Terminal paste", wxYES_NO | wxICON_WARNING,
                           this) != wxYES)
            return;
          if (screen.bracketedPaste())
            value = "\x1b[200~" + value + "\x1b[201~";
          send(value);
        }
      }
    }
  }
};
