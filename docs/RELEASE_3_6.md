# DoDevEditor Raven 3.6 — Interactive Serial / Symbols / Remote Journal

This package is the complete source tree based on **Raven 3.5 Serial Monitor**. The original 3.5 features, JSON settings, themes, QR plugin, VS Code menus, keybindings and shell terminal are retained.

## 1. Building and installing

Requirements: CMake >=3.27, C++17 compiler, Git; the regular Raven/Vulkan/SDL dependencies used by DoDevEditor. For the remote journal plugin, Linux/OpenSSH (`ssh`) on the desktop and systemd/journalctl on the remote Linux server are required.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build
```

CMake FetchContent downloads Raven from the configured GitHub repo with progress enabled during GUI builds. For headless tests without Raven, add `-DDODEV_BUILD_GUI=OFF`.

Installed paths (prefix `$HOME/.local`):

```text
~/.local/bin/DoDevEditor
~/.local/bin/dodev-serial-reader-helper
~/.local/bin/plugins/serial-monitor/plugin.json
~/.local/bin/plugins/serial-monitor/libdodev_serial_monitor_plugin.so
~/.local/bin/plugins/source-symbols/plugin.json
~/.local/bin/plugins/source-symbols/libdodev_source_symbols_plugin.so
~/.local/bin/plugins/remote-journal/plugin.json
~/.local/bin/plugins/remote-journal/libdodev_remote_journal_plugin.so
~/.local/bin/plugins/qrcode/...
```

Native plugins remain **opt-in**: enable each in **Settings → Plugins** and restart. A disabled plugin does not run native code and does not add its dynamic Tools menu. Logs on startup and during commands are written to the *process terminal*, not into the integrated shell by default. Capture them with `~/.local/bin/DoDevEditor 2>&1 | tee dodev.log`.

## 2. Serial Monitor — live keyboard console

1. Activate `com.dodev.serialmonitor` in Settings → Plugins and restart.
2. Open **Herramientas → Monitor serie...**.
3. Select a device (`/dev/ttyUSB0`, `/dev/ttyACM0`, or Windows `COM3`) and baud, bits, parity and stops. Press **Start capture**.
4. The **Tasks · Serial** tab appears alongside terminal tabs.
5. For *command mode*, enter a line in the `TX:` field and press Enter/Send. This sends the command followed by `CR`. There are separate Enter and Ctrl+C controls.
6. For *interactive character mode*, click **Focus keyboard** or the focusable **SERIAL KEYBOARD** strip and type directly. Supports normal UTF-8 key text, arrow keys, Home/End, Tab, Escape, Delete, Backspace, Enter, Ctrl+A..Ctrl+Z, and Alt+keys.
7. RX data continues appearing in the read-only log view. Configure fixed-text or ECMAScript regex filtering in the dialog or the Tasks tab. Export TXT through **Export TXT...**; this exports all retained or matching lines according to the active toggle.

The device decides whether it echoes typed characters and whether `CR`, `LF`, or terminal escape sequences are meaningful. This is a **bidirectional byte-oriented serial console**, not yet a complete VT100/PuTTY terminal emulator. The RX log is line-oriented and renders non-printable bytes in escaped form.

**Linux permission recovery:** use a normal user-owned serial device or correct group membership (often `dialout`/`uucp`). If access is denied, the optional Polkit helper may request authorization, but only after the user selects the checkbox. The helper is limited to whitelisted `/dev` TTY devices; it supports both TX and RX without a password field in the editor.

## 3. Source Symbols / Outline

Activate `com.dodev.sourcesymbols` and use **Herramientas → Symbols / Outline...** with an editable source document open. The **Tasks · Symbols** panel displays a real Raven table with **Line**, **Kind**, **Symbol**, and **Signature** columns. The extractor reads the *active editor buffer* (including unsaved text).

- Filter by symbol name or kind; click a row to inspect its signature; double-click / **Go To Symbol** to jump to its source line.
- Press **Refresh** to rescan edits; **Export CSV...** opens the system Save File dialog and exports the currently filtered rows.
- Built-in heuristics recognize common functions/types, C/C++/Java/Kotlin declarations, JS/TS functions and Python classes/functions, plus C/C++ preprocessor macros and includes.

**Important:** this is a bounded lexical outline, **not an AST or language-server implementation**. C++ templates, multiline declarations, overloaded operators, macros, nested languages and advanced syntax may produce incomplete/false-positive results. For exact C++ semantic symbols a future plugin should integrate clangd/libclang or Tree-sitter. Files over 4 MiB are truncated for this extractor, with a maximum of 15,000 records.

## 4. Remote Journal (SSH)

Activate `com.dodev.remotejournal`. Open **Herramientas → Remote Journal (SSH)...** and configure:

- Host/IP: `server.example.com`; SSH username: `operator`; port: `22`.
- Optional unit: `nginx.service` (empty means all journal entries readable to the remote user).
- Last lines: `200`; check **Follow live log** to use `journalctl --follow`.
- Optionally enable **sudo -n** only if the remote server already grants passwordless access for that command. By default, it uses unprivileged `journalctl` and follows the server's permissions.

Press **Connect**. Logs appear in **Tasks · Journal** alongside terminals, with fixed-text/regex filters, Disconnect, Clear and **Export TXT...**. The SSH subprocess runs in a background thread; journal output is buffered up to 20,000 lines (the oldest are discarded when full). Filtering can change while following.

The plugin launches `ssh` via direct argument-vector execution, **never through a local shell**. It accepts only validated host, user, unit, port, and line counts, and constructs a fixed remote `journalctl` command. SSH uses `BatchMode=yes`, `StrictHostKeyChecking=yes`, and a connection timeout. It does not ask for or store SSH passwords.

**Before connecting from the GUI**, verify keys and host trust in Terminal:

```bash
ssh operator@server.example.com 'journalctl --no-pager -n 20 --unit nginx.service'
```

If you lack journal permission, configure the remote user's journal access following your server policy, or explicitly allow tightly scoped `sudo -n` where appropriate. Do not disable SSH host key verification simply to suppress warnings.

**Platform boundary:** the remote journal process transport has been implemented for POSIX/Linux with OpenSSH. On Windows, the plugin and settings compile, but the journal session displays an explicit unsupported-platform message. Remote SSH transport on Windows needs a native pipe implementation. A real remote SSH connection has **not** been tested in this environment; integration tests use a controlled fake `ssh` executable.

## 5. Terminal output export

Use the **Terminal** menu:

- **Export Active Terminal Output...** saves all retained scrollback *and* the current visible VT screen of the active shell session.
- **Export All Terminal Sessions...** saves a combined TXT file with a title header for each currently open shell session.

Both commands open a standard Save File dialog; they do not print secrets intentionally, but shell output can contain tokens/passwords. Review TXT exports before sharing. The export is of the **retained screen buffer**, not a raw PTY byte transcript: previously erased/overwritten lines and output older than `terminal.scrollbackLines` are unavailable. Set the scrollback limit in Settings before capture if you need a longer log.

## 6. Testing

Headless verification:

```bash
cmake -S . -B build-headless -DDODEV_BUILD_GUI=OFF -DDODEV_BUILD_TESTS=ON
cmake --build build-headless --parallel
ctest --test-dir build-headless --output-on-failure
```

Tests cover serial bidirectional PTY TX/RX and filters, native module registration, source-symbol extraction, remote-journal stream through a mock SSH executable, optional settings, QR regression, terminal scrollback, and embedded XML contracts. Full Raven/Vulkan graphical integration and a real serial device/remote server require validation on a target machine.

## 7. Common problems

| Symptom | Cause and action |
|---|---|
| No Tools menu entry | Enable plugin in Settings → Plugins and restart. Examine process terminal logs for load errors. |
| No serial output | Confirm correct device, baud, RX/TX wiring, and permissions. |
| Serial console accepts keys but device does not respond | Verify device expects CR versus LF, and whether it echoes input. |
| SSH says host key verification failed | Run `ssh user@host` in a normal terminal first to verify/accept the correct host key. |
| SSH says permission denied | Configure key-based SSH; BatchMode deliberately prevents password prompts. |
| journalctl says insufficient permissions | Check user is permitted to read system journal on the remote system. |
| Symbols missing some methods | Heuristic parser does not implement full compiler syntax; use a language server for precise results. |
| Terminal TXT missing old output | Increase `terminal.scrollbackLines` before running long tasks. |
