# DoDevEditor Raven 3.8 — AI Provider Profiles and Model Discovery

Version 3.8 adds **local llama.cpp model discovery**, Ollama model discovery, and configurable per-server remote provider profiles to the AI Assistant. See [`docs/AI_PROVIDER_PROFILES.md`](docs/AI_PROVIDER_PROFILES.md) for installation, server commands, JSON settings and authentication.

The AI panel offers `Save Profile`, `+ Remote`, `+ Local`, `Delete`, `Refresh Models`, a detected-model dropdown and a manual model-ID input. Open from **Herramientas → AI Assistant...**, or **Settings → Plugins → AI Provider Profiles...**. The plugin must be enabled in Settings → Plugins and the editor restarted to activate it.

All existing bundled plugins and Clang/Regex symbol extraction from 3.7 are retained. Configuration files remain optional; missing AI profiles fall back to built-in local presets.

---

# DoDevEditor Raven 3.7 — AI Assistant + Clang-first Source Symbols

**New in 3.7:** a bundled, opt-in AI Assistant with local Ollama / OpenAI-compatible HTTPS providers, asynchronous chat and explicit active-file/selection context actions; Source Symbols now uses Clang's AST when available, with regex fallback and detailed parser diagnostics. The corrected embedded journal XML is retained.

Read [the complete 3.7 instructions](docs/RELEASE_3_7.md), [Clang parser notes](docs/SYMBOLS_CLANG.md), and [AI plugin guide](docs/AI_ASSISTANT.md).

# DoDevEditor Raven 3.6 — Serial Console, Symbols & Remote Journal

**New in 3.6:** interactive (bidirectional) serial console, bundled Source Symbols and Remote Journal plugins, and full retained terminal export. See [docs/RELEASE_3_6.md](docs/RELEASE_3_6.md) for installation, UI workflows, permissions and limitations.

# DoDevEditor Raven 3.5 — Bundled Serial Monitor

**New in 3.5:** `thirdparty/SerialMonitor` is built and installed automatically with the editor. Enable `com.dodev.serialmonitor` in Settings → Plugins and restart, then open **Herramientas → Monitor serie...**. Configure a serial port, capture logs in **Tasks · Serial** alongside Terminal, apply fixed/regex filters and export TXT via a Save dialog. Optional Polkit authorization on Linux does not ask for passwords inside the IDE.

**Complete guide:** [Serial Monitor Plugin](docs/SERIAL_MONITOR_PLUGIN.md).

---

> **Version 3.4 — QR Code plugin integrated in `thirdparty/QRCode`:**
> `DODEV_BUILD_QRCODE_PLUGIN=ON` by default compiles the plugin during a normal
> DoDevEditor build. `make install` / `cmake --install` installs its binary and
> `plugin.json` to `<prefix>/bin/plugins/qrcode/`. The editor discovers both
> installed and user plugins, prints discovery/status and paths to stdout/stderr,
> and shows the installed QR plugin in Settings → Plugins. Native plugins remain
> **opt-in** for security: enable `com.dodev.qrcode` in Settings → Plugins once
> and restart. [Installation & logs guide](docs/QR_BUNDLED_INSTALL.md).

> **Version 3.3 — Extended plugin bridge + tools + documentation:**
> Includes a complete customization manual (`docs/CUSTOMIZATION_MANUAL.md`),
> clang-format and Docker CLI plugins, an embedded Raven input prompt,
> two source patches and a conflict-aware migration utility.

**Start here:** [Complete 3.3 customization manual](docs/CUSTOMIZATION_MANUAL.md) · [Release notes](docs/RELEASE_3_3.md).

Plugin binaries are built with `DODEV_BUILD_PLUGIN_EXAMPLES=ON` in `build/plugin-examples/{hello,clangformat,docker}`. Install a module and manifest into `<config>/plugins/<folder>/`, enable its plugin ID under Settings → Plugins, and restart. The clang-format and Docker command-line tools themselves are external dependencies, not bundled.

For patch failures against modified 3.1/3.2 trees, use the conflict-aware `scripts/apply_upgrade.py` from the **complete** package. See Section 19 of the manual.

> **Version 3.2 — Configurable Keybindings:** Native Raven shortcuts are now
> bound dynamically from optional `keybindings.json`. A new Keybindings Settings tab
> supports search, key recording, conflict detection, unbind/reset, and immediate
> application. `Ctrl+G` opens Go to Line, `Ctrl+P` opens Quick Open, and
> `Ctrl+Shift+P` opens the Command Palette. See
> [`docs/KEYBINDINGS.md`](docs/KEYBINDINGS.md).

> **Version 3.1 — VS Code-style Edit / Selection / Terminal menus:**
> Added 26 Edit menu items, 19 Selection items and 19 Terminal items. Commands
> include undo-safe text transforms, line operations, find-in-workspace, PTY/ConPTY
> terminal splitting, native task loading and build tasks. See
> [`docs/VSCODE_MENUS.md`](docs/VSCODE_MENUS.md) for details and compatibility limits.

# DoDevEditor Raven 3.4 — Bundled QR plugin

**Source project — C++17, Raven GUI, embedded XML, persistent IDE terminal, configurable Settings, custom themes, file templates and a native plugin SDK.**

DoDevEditor is an experimental Raven-based code editor derived from the prior MusicStyle edition. Its interactive PTY/ConPTY terminal, source editor, file explorer, Git, CSV/DLT viewers and existing music/glass themes are retained. `legacy/` contains the prior wxWidgets implementation for reference; it is not linked into the new binary. The older Data Server and image editor are still not complete.

## What is new in version 3.0

* **Settings window** — General, Editor & Fonts, Appearance, Terminal, Templates and Plugins. Choose `.ttf`/`.otf` font files and sizes for the text editor, Raven UI and terminal; control line numbers, word wrap, tab size and scrollback.
* **Optional versioned `settings.json`** — defaults are built into the program; missing configuration, missing font files, missing template directories, and missing plugin directories do not block startup. Existing `settings.ini` theme/last folder are imported into memory when JSON is missing. Malformed JSON is **not overwritten** automatically.
* **13 embedded themes** — including Music Nocturne, Music Aurora Glass, Music Daylight, Glass Studio, Glass Spotify, and **VS Code Dark Modern / VS Code Light Modern**. Every built-in theme and the UI for Settings, templates and Theme Designer are embedded XML string literals in `resources/*.h`.
* **Theme Designer** — change theme name, base (Dark/Light/Glass), accent, widget/surface colors, corner radius and syntax colors; live preview and import/export `.xml`/`.json` to a user-selected file. Custom theme directories are optional. A custom theme saved as XML retains the complete Raven-native XML; JSON represents the editable theme model. Some advanced properties of existing Raven XML styles may not round-trip through JSON.
* **File templates** — built-in C++17 source/header, Python, XML, JSON and CMake examples. Open **New from Template**, pick a template, enter a filename, preview and create an unsaved editor tab. Custom `templates.json` + `.tpl` and extension mappings are optional.
* **Native plugin SDK v1** — C ABI with C++17 host. Plugins may contribute commands, menus, document tabs, dock panels, templates, XML themes, settings and event listeners. Plugins must be explicitly enabled in Settings. The host owns the Raven controls; no Raven linkage is required for the sample plugin. The extension API does **not** provide arbitrary child widget access yet; it currently exposes label, button and read-only text builders.
* **Plugin Manager** — enable/disable plugins, configure per-plugin string settings in JSON, select plugin directory and use safe mode. Activation changes require an application restart. Example plugin source and CMake target included.

## Compile on Linux

Prerequisites: CMake 3.21+, C++17 compiler, Git, Vulkan-capable GPU and Raven's build dependencies.

```bash
unzip DoDevEditor_Raven_3_2_Keybindings.zip
cd DoDevEditor_Raven_3_2_Keybindings
chmod +x scripts/build.sh
./scripts/build.sh
```

Equivalent manual build (shows Git clone progress):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DDODEV_BUILD_GUI=ON -DDODEV_BUILD_TESTS=ON \
  -DFETCHCONTENT_QUIET=OFF
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/DoDevEditor
```

Raven is fetched from `https://github.com/aneury1/Raven.git` by CMake `FetchContent` with `GIT_PROGRESS=TRUE`. To use a local checkout instead of fetching:

```bash
cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_RAVEN=/path/to/Raven
```

## Windows build

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
.\build\Release\DoDevEditor.exe
```

The executable location can vary with the CMake generator. Terminal support targets ConPTY on Windows 10 1809+ and Windows 11; this platform has **not** been verified in this environment.

## Settings and paths

User configuration is stored at:

| Platform | Default directory |
| --- | --- |
| Linux | `${XDG_CONFIG_HOME:-$HOME/.config}/DoDevEditor` |
| Windows | `%APPDATA%\DoDevEditor` |
| macOS | `~/Library/Application Support/DoDevEditor` |

Override the root with `DODEV_CONFIG_DIR`. All files are optional; on first run the application can initialize without any files and save preferences later.

```text
DoDevEditor/                 (user configuration directory)
├── settings.json            (general/editor/appearance/terminal/plugins)
├── themes/                  (*.xml and *.json custom themes)
├── templates/               (templates.json, *.tpl)
└── plugins/
    ├── com.dodev.hello/
    │   └── settings.json    (plugin-specific preferences)
    └── hello/
        ├── plugin.json
        └── libhello_plugin.so (or hello_plugin.dll on Windows)
```

See `examples/settings.json`, `examples/themes/`, `examples/templates/` for optional starter files.

### Installing the sample plugin

1. Build with `-DDODEV_BUILD_PLUGIN_EXAMPLES=ON` (default).
2. Copy the entire `build/plugin-examples/hello/` folder (including `plugin.json` and the platform library) into `<config>/plugins/hello/`.
3. Launch DoDevEditor; open **Settings → Plugins**, select **Hello Plugin**, click **Enable selected** and **Apply / Save**, then restart.
4. Use **Tools → HelloPlugin: Say Hello** or **Plugins → Open Hello Plugin**. Its `greeting` preference is stored in `<config>/plugins/com.dodev.hello/settings.json`.

**Security:** native `.so` and `.dll` plugins execute with the same privileges as DoDevEditor. Only enable trusted modules. Plugin manifests and settings do not create a sandbox. Plugins are disabled until individually approved.

## Automated tests without Raven/Vulkan

```bash
cmake -S . -B build-core \
  -DDODEV_BUILD_GUI=OFF \
  -DDODEV_BUILD_TESTS=ON \
  -DDODEV_BUILD_PLUGIN_EXAMPLES=ON
cmake --build build-core --parallel 4
ctest --test-dir build-core --output-on-failure
```

The five tests cover JSON and missing-file defaults, settings migration/malformed file handling, themes (including parsing all 13 embedded presets), template placeholders, native plugin lifecycle/API, core CSV/DLT/files, PTY terminal and XML validity.

**Verification notice (2026-11-22):** all **5/5 headless tests pass on Linux**. The full Raven/Vulkan GUI was **not compiled or run** in the authoring environment because outbound GitHub access is unavailable; visual styling, Raven API compatibility and Windows/ConPTY integration must be verified by building on a machine with Raven's dependencies. This archive contains **source code, not a verified executable or APK**.

## Documentation

* [`docs/SETTINGS_V3.md`](docs/SETTINGS_V3.md) — settings contract, fonts, optional files, paths and precedence.
* [`docs/THEMES_AND_TEMPLATES.md`](docs/THEMES_AND_TEMPLATES.md) — theme formats, designer and extension templates.
* [`docs/PLUGIN_SDK_V1.md`](docs/PLUGIN_SDK_V1.md) — ABI and plugin implementation, lifecycle, UI contributions and example.
* [`docs/TERMINAL.md`](docs/TERMINAL.md) — interactive terminal behavior and limitations.
* [`docs/MIGRATION.md`](docs/MIGRATION.md) — migration notes for Raven GUI.
* [`docs/MUSIC_STYLE.md`](docs/MUSIC_STYLE.md) — inherited MusicStyle design.


### QR Capture in Herramientas

The **Herramientas → Capturar QR del archivo actual...** menu entry is always visible.
Enable `com.dodev.qrcode` in **Settings → Plugins**, restart, and select an active text
document to export as PNG/SVG. Details: `docs/QR_CAPTURE_MENU.md`.

## Raven 3.9 — VS Code-style desktop launcher and workspace explorer tabs

See [`docs/RELEASE_3_9.md`](docs/RELEASE_3_9.md) for the complete guide.

- `dodeveditor .` / `dodeveditor main.cpp` launch the GUI without leaving the
  shell attached. `--foreground` and `--wait` are available when needed.
- `cmake --install build` installs a Linux desktop entry for file-manager
  **Open With → DoDevEditor** and accepts selected files through `%F`.
- **BUSCAR** is a searchable, dockable panel next to **ARCHIVOS** and **GIT**:
  literal/regex search, include/exclude globs, case/whole-word options, result
  navigation, and confirmed replace-selected/replace-all operations.
- **SYMBOLS** is another tab beside the file manager, showing **Entity name**,
  **Type**, and **Filepath : line**. Double-click navigates to source.
- Built-in XML remains embedded in C++ and all bundled third-party plugins
  are preserved.
