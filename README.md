# DoDevEditor Raven 3.0

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
unzip DoDevEditor_Raven_3_0.zip
cd DoDevEditor_Raven_3_0
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
