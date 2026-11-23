# DoDevEditor Raven 3.5 — Serial Monitor Plugin

**Plugin ID:** `com.dodev.serialmonitor` · **Commands:** `serialmonitor.open` · **Menu:** `Herramientas → Monitor serie...`

This document describes the new **bundled native Serial Monitor extension**, not a mockup. The serial I/O, fixed-string and regex filtering, and native plugin registration are tested headlessly; the Raven/Vulkan GUI requires a full build and manual verification on a machine with Raven available.

## 1. What it does

- Opens a Raven GUI configuration dialog with port detection, manual path/COM entry, baud, data bits, parity and stop bits.
- Streams serial input into **Tasks · Serial**, a dedicated tab in the lower terminal dock area, alongside ordinary interactive terminal tabs.
- Filters text **during capture** using one of: All, Fixed text, Regex (ECMAScript), with optional case sensitivity.
- Permits filter edits and switching **matching-only / all lines** without stopping capture.
- Starts/stops sessions without blocking the Raven UI. Supports unplug/error status and a bounded capture buffer.
- Exports captured data to UTF-8-ish text (`.txt`) by a **Save File** dialog. Exports matching lines by default; toggle **Matches / All** in the Tasks tab to export the full retained capture.
- Discovers `/dev/ttyUSB*`, `/dev/ttyACM*`, `/dev/ttyS*`, `/dev/ttyAMA*`, `/dev/rfcomm*` on Linux; searches COM1–COM64 via `QueryDosDevice` on Windows. A device path can also be entered manually.
- Optional Linux **Polkit** privilege request *only if a normal open reports EACCES and the user explicitly checks the option*. The app never asks for or captures a sudo password.

## 2. Build and install

From the complete project root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$HOME/.local"
```

The existing CMake FetchContent retrieves Raven with `GIT_PROGRESS=TRUE`. Bundled plugin sources are **offline** in `thirdparty/SerialMonitor` and require no additional download.

Expected Linux install layout:

```text
~/.local/bin/
  DoDevEditor
  dodev-serial-reader-helper
  plugins/
    serial-monitor/
      plugin.json
      libdodev_serial_monitor_plugin.so
    qrcode/
      plugin.json
      libdodev_qrcode_plugin.so
```

On Windows, the plugin is a `.dll` inside `bin/plugins/serial-monitor/`, and the root-only helper is not built.

## 3. Activate

1. Start DoDevEditor.
2. Open **Settings → Plugins**.
3. Enable `com.dodev.serialmonitor`.
4. Restart DoDevEditor. Native plugins require opt-in.
5. Choose **Herramientas → Monitor serie...**.
6. Pick a detected port or type a port name, select baud/data/parity/stop bits, and press **Start capture**.
7. The **Tasks · Serial** tab appears in the lower dock beside the other Terminal tabs.
8. Type a fixed string such as `ERROR` or a regex such as `^(ERROR|WARN).*`, choose the filter type and press **Apply filter**. Use **Matches / All** to toggle visibility and export scope.
9. Press **Export TXT...**, choose a path using the native Save dialog, then save.
10. Press **Stop** to release the serial device. Closing only the configuration dialog does not stop a running capture.

**Default serial line format:** 115200 baud, 8N1, raw/decoded as terminal-safe text. Control bytes are shown as escaped `\\xHH` sequences to avoid terminal control injection; carriage returns are ignored. NUL and binary payloads are not transparently preserved—this is a text monitor.

## 4. Filter behavior

- **All** shows every retained line.
- **Fixed text** searches for a literal substring, case-insensitive by default.
- **Regex** uses `std::regex` ECMAScript syntax, and reports invalid patterns without stopping the reader.
- Filters run against already-captured lines, so applying a new filter re-evaluates the retained capture.
- **Display / export matching lines only** defaults to enabled. The Tasks tab has a matching/all toggle. Raw records are retained within the buffer limit.
- The Tasks editor displays the most recent **2,500** matching lines. The capture buffer retains up to **20,000** lines. When full, the oldest records are discarded, and the status reports the drop count. Export reflects the retained data, not lines already discarded.

Common regex patterns:

| Pattern | Meaning |
|---|---|
| `ERROR` | Lines containing ERROR (case-insensitive by default) |
| `^(WARN|ERROR)` | Lines beginning with WARN or ERROR |
| `\\b0x[0-9a-fA-F]+\\b` | Hex values prefixed with 0x |
| `timeout|disconnect` | Either word |

Regex performance depends on the pattern and input. Avoid expressions that catastrophically backtrack on long or untrusted serial data.

## 5. Optional JSON settings

This plugin registers options via the existing plugin SDK. The plugin settings file is under the user's DoDevEditor configuration directory and is created only when values are changed.

Example file `plugins/com.dodev.serialmonitor/settings.json`:

```json
{
  "port": "/dev/ttyUSB0",
  "baud": "115200",
  "dataBits": "8",
  "parity": "N",
  "stopBits": "1",
  "filterMode": "regex",
  "filterText": "^(ERROR|WARN)",
  "caseSensitive": "false",
  "showOnlyMatches": "true",
  "allowPolkit": "false"
}
```

All plugin config files are **optional**. Missing or invalid configuration falls back to defaults; the application remains available with no serial devices connected.

## 6. Serial access and authorization

The plugin first tries to open the device as the current user. This is preferred. For a Linux `Permission denied` error, either:

- Use system-recommended group / udev rules (often `dialout`, sometimes `uucp`) and sign out/in again; or
- Check **If access is denied, request elevated access via Polkit**. The reader first attempts normal access, then runs the dedicated installed `dodev-serial-reader-helper` with `pkexec`. A desktop authentication agent may show the OS authorization dialog. No password is entered into DoDevEditor.

The elevated helper accepts *only* known serial TTY device paths resolving to `/dev/ttyUSB*`, `/dev/ttyACM*`, `/dev/ttyS*`, `/dev/ttyAMA*`, or `/dev/rfcomm*`, validates its serial settings, and writes received bytes to stdout. It does not accept shell commands, arbitrary paths or arbitrary file reads. Polkit may be unavailable on some systems; if so, prefer correct serial group membership.

For troubleshooting device permissions:

```bash
ls -l /dev/ttyUSB0
id -nG
command -v pkexec
```

For a system where `dialout` is the correct group, an administrator can use `sudo usermod -aG dialout "$USER"` and then sign out/in. Verify the device's actual group first; never run the entire IDE with sudo.

## 7. Logs and diagnostics

To see diagnostic messages and plugin discovery while developing, launch the editor from a system terminal:

```bash
~/.local/bin/DoDevEditor 2>&1 | tee dodev-serial-debug.log
```

- The **Tasks · Serial** tab displays device data and filter results.
- The **process terminal** shows plugin activation, capture status, errors and exported TXT path.
- The integrated interactive terminal remains independent and usable.
- No serial payload is automatically copied to the process terminal.

## 8. Plugin and host sources

```text
thirdparty/SerialMonitor/
  CMakeLists.txt
  plugin.json
  include/SerialMonitor.h
  src/SerialMonitor.cpp
  src/SerialMonitorPlugin.cpp
  src/SerialReaderHelper.cpp
  tests/test_serial.cpp
  tests/test_plugin.cpp
src/EditorSerial.cpp          # Raven dialog + Tasks tab host UI
resources/EmbeddedSerialUi.h # Embedded XML
include/dodev/sdk/dodev_plugin_api.h
```

The additive `DodevHostAPI::open_serial_monitor` callback is checked with `DODEV_HOST_HAS`. Existing v1 plugins remain compatible. The plugin itself only adds the menu, command, settings and help tab. The host integration supplies the Raven-native dialog and Tasks surface, while `dodev_serial_core` performs the actual serial I/O and filtering.

## 9. Tests and limitations

- `serial_monitor_filter_and_pty`: tests PTY capture, fixed-text and regex filters, text export, stop, and invalid arguments.
- `serial_plugin_registration`: tests enablement, menu registration and actual host invocation.
- `embedded_serial_ui_contract`: validates XML parsing and essential UI actions.
- QR and prior editor tests remain in the regression suite.

**Not yet verified:** a complete Raven/Vulkan GUI build, real USB serial hardware, Windows COM hardware, and Polkit authorization with a running desktop agent. These require an appropriate development workstation and device. Linux tests used a pseudo-terminal, not a physical serial adapter.
