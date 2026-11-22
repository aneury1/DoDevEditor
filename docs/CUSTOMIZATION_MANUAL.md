# DoDevEditor Raven 3.3 — Complete Customization and Plugin Manual

**Audience:** developers, IDE administrators, designers and contributors.
**Project:** C++17 / Raven GUI / CMake / integrated PTY/ConPTY terminal.
**Scope:** preferences, file paths, fonts, themes, templates, shortcuts, terminal tasks, plugin manifests, the native plugin API, clang-format, Docker, troubleshooting and upgrades.

> This manual describes the code and configuration shipped in this package, not every VS Code feature. Plugin binaries run **inside** the editor with the current user's privileges. Use only trusted binaries. All external configuration files are optional; you can launch the editor with an empty configuration directory.

## 1. Contents and quick start

1. [Build and run](#2-build-and-run)
2. [Configuration discovery and safe defaults](#3-configuration-discovery-and-safe-defaults)
3. [All general settings](#4-settingsjson-reference)
4. [Fonts, editor and terminal](#5-fonts-and-editor-behavior)
5. [Themes and theme designer](#6-themes-and-raven-xml)
6. [Theme JSON and XML examples](#7-author-a-theme-in-json-or-xml)
7. [Templates](#8-extension-specific-file-templates)
8. [Keybindings](#9-keybindingsjson)
9. [Workspace tasks and terminal commands](#10-terminal-and-workspace-tasks)
10. [Plugin installation](#11-native-plugin-packaging-and-activation)
11. [Plugin SDK and extension points](#12-plugin-sdk-v1-and-the-additive-host-functions)
12. [Build a plugin](#13-build-your-own-plugin)
13. [clang-format plugin](#14-clang-format-plugin)
14. [Docker plugin](#15-docker-tools-plugin)
15. [Plugin settings JSON](#16-per-plugin-json-settings)
16. [Examples and recipes](#17-customization-recipes)
17. [Testing](#18-build-options-and-tests)
18. [Upgrade and patch conflicts](#19-upgrading-without-losing-local-changes)
19. [Security](#20-security-boundaries)
20. [Known limitations](#21-known-limitations)

## 2. Build and run

### Linux

You need a C++17 compiler, CMake 3.21+, Git, a Vulkan-capable driver and the platform dependencies required by your chosen Raven GUI commit.

```bash
unzip DoDevEditor_Raven_3_3_Extensions.zip
cd DoDevEditor_Raven_3_3_Extensions
cmake -S . -B build -DDODEV_BUILD_GUI=ON \
  -DDODEV_BUILD_PLUGIN_EXAMPLES=ON -DDODEV_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The build uses `FetchContent_Declare(raven GIT_REPOSITORY https://github.com/aneury1/Raven.git GIT_PROGRESS TRUE)` and explicitly disables the quiet download mode. Git progress is shown during clone/checkout. You need network access during the **first** GUI configure (unless Raven is already cached), not when the editor starts.

For a **headless** verification without fetching Raven:

```bash
cmake -S . -B build-headless -DDODEV_BUILD_GUI=OFF \
  -DDODEV_BUILD_PLUGIN_EXAMPLES=ON -DDODEV_BUILD_TESTS=ON
cmake --build build-headless --parallel
ctest --test-dir build-headless --output-on-failure
```

The GUI executable is `build/DoDevEditor` for most single-config generators; Visual Studio and multi-config generators may place it under `build/Debug` or `build/Release`.

### Windows

Use MSVC (Visual Studio C++ build tools), CMake, Git and the Vulkan/Raven prerequisites. From a Developer PowerShell:

```powershell
cmake -S . -B build -DDODEV_BUILD_GUI=ON -DDODEV_BUILD_PLUGIN_EXAMPLES=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The terminal uses ConPTY and requires a sufficiently recent Windows build. Native plugins have `.dll` extensions. A project built with different C++ runtimes must still respect the plugin's **C ABI**.

### Run with an isolated configuration

```bash
DODEV_CONFIG_DIR="$HOME/dodev-sandbox" ./build/DoDevEditor
```

The editor starts with defaults even when `$HOME/dodev-sandbox` does not exist.

## 3. Configuration discovery and safe defaults

`SettingsManager::defaultDirectory()` resolves the directory as follows:

| OS | Default directory | Override |
|---|---|---|
| Linux | `${XDG_CONFIG_HOME:-$HOME/.config}/DoDevEditor` | `DODEV_CONFIG_DIR=/path` |
| Windows | `%APPDATA%\\DoDevEditor` | `DODEV_CONFIG_DIR=C:\\path` |
| macOS | `~/Library/Application Support/DoDevEditor` | `DODEV_CONFIG_DIR=/path` |

Configuration hierarchy on disk:

```text
<config>/
├── settings.json                   # optional application preferences
├── keybindings.json                # optional shortcut overrides
├── settings.ini                     # legacy file, read only for migration
├── themes/                          # optional external JSON and XML themes
│   └── my-custom.json
├── templates/                       # optional extension templates
│   ├── templates.json
│   └── cmake.tpl
└── plugins/                         # optional plugin installation root
    ├── clangformat/                 # installed native module and manifest
    │   ├── plugin.json
    │   ├── libclangformat_plugin.so
    │   └── settings.json            # plugin preference storage
    └── docker/
        ├── plugin.json
        ├── libdocker_plugin.so
        └── settings.json
```

The plugin *installation root* may be customized. Plugin preferences always use `<config>/plugins/<plugin-id>/settings.json`; the installation folder and the per-plugin settings folder can therefore be different.

### Missing or damaged files

- No `settings.json`: built-in defaults are used; the legacy `settings.ini` can seed `theme=` and `folder=`.
- Invalid whole `settings.json`: defaults are loaded **in memory**; the bad file is preserved and subsequent automatic save is refused until you fix or rename it.
- Missing `keybindings.json`: default shortcuts apply. Invalid entries and conflicts are ignored.
- Missing theme or template directory: embedded defaults continue working.
- Missing plugin folder: editor starts normally, with no external plugins.
- Missing font file: Raven's available default font is used.
- Missing `tasks.json`: optional build-task defaults are available through the terminal task subsystem.

**Important:** native plugin settings use their own JSON loader. Corrupt plugin settings may be read as defaults; back up the file before editing it, since updating a plugin setting may replace the file.

## 4. `settings.json` reference

The schema is `schemaVersion: 1` for version 3.3. Unknown keys are generally preserved. Values are validated by `SettingsManager::validate()`.

### Full example

```json
{
  "schemaVersion": 1,
  "general": {
    "language": "en",
    "restoreSession": true,
    "autoSave": false,
    "lastFolder": "/home/developer/projects/MyApp"
  },
  "editor": {
    "fontFamily": "monospace",
    "fontFile": "/home/developer/.local/share/fonts/JetBrainsMono-Regular.ttf",
    "fontSize": 14,
    "tabSize": 4,
    "insertSpaces": true,
    "wordWrap": false,
    "lineNumbers": true
  },
  "appearance": {
    "theme": "Music Nocturne",
    "uiFontFile": "",
    "uiFontSize": 13,
    "customThemesDirectory": "",
    "preferredThemeFormat": "json"
  },
  "terminal": {
    "fontFamily": "monospace",
    "fontFile": "",
    "fontSize": 14,
    "defaultShell": "",
    "scrollbackLines": 5000
  },
  "templates": {
    "directory": "",
    "associations": {
      ".cpp": "cpp17",
      ".h": "cpp-header",
      ".py": "python",
      ".json": "json",
      ".sql": "sql-query"
    }
  },
  "plugins": {
    "enabled": true,
    "directory": "",
    "safeMode": false,
    "enabledIds": ["com.dodev.clangformat", "com.dodev.docker"],
    "disabled": []
  }
}
```

### Settings key table

| Key | Type | Default | Allowed or implemented behavior |
|---|---|---|---|
| `schemaVersion` | integer | `1` | Only schema v1 is currently understood |
| `general.language` | string | `en` | Preference; complete UI localization is not implemented |
| `general.restoreSession` | boolean | `true` | Session preference |
| `general.autoSave` | boolean | `false` | Editor auto-save preference; check integration for individual document types |
| `general.lastFolder` | string | `""` | Last opened project folder |
| `editor.fontFamily` | string | `monospace` | Reserved; custom files are the working font selection mechanism |
| `editor.fontFile` | string | `""` | Optional TTF/OTF path |
| `editor.fontSize` | integer | `14` | Clamped to 8–64 |
| `editor.tabSize` | integer | `4` | Clamped to 1–16 |
| `editor.insertSpaces` | boolean | `true` | Replace tab input with spaces |
| `editor.wordWrap` | boolean | `false` | Wrap long lines |
| `editor.lineNumbers` | boolean | `true` | Show line number gutter |
| `appearance.theme` | string | `Music Nocturne` | Embedded or imported theme name |
| `appearance.uiFontFile` | string | `""` | Optional UI TTF/OTF path |
| `appearance.uiFontSize` | integer | `13` | Clamped to 9–36 |
| `appearance.customThemesDirectory` | string | `""` | Empty means `<config>/themes` |
| `appearance.preferredThemeFormat` | string | `json` | `json` or `xml` |
| `terminal.fontFamily` | string | `monospace` | Reserved family string |
| `terminal.fontFile` | string | `""` | Optional terminal font |
| `terminal.fontSize` | integer | `14` | Clamped to 8–48 |
| `terminal.defaultShell` | string | `""` | Empty means native shell choice |
| `terminal.scrollbackLines` | integer | `3000` | Clamped to 100–200000 |
| `templates.directory` | string | `""` | Empty means `<config>/templates` |
| `templates.associations` | object | defaults | Maps extension → template ID |
| `plugins.enabled` | boolean | `true` | Enable external native plugin subsystem |
| `plugins.safeMode` | boolean | `false` | Block loading all external plugins |
| `plugins.directory` | string | `""` | Empty means `<config>/plugins` |
| `plugins.enabledIds` | array | `[]` | **Explicit allowlist** of plugin IDs |
| `plugins.disabled` | array | `[]` | Additional denylist |

### Editing settings through the GUI

Open the Settings dialog from the application menu. The embedded Raven window contains tabs **General**, **Editor & Fonts**, **Appearance**, **Terminal**, **Templates**, **Keybindings**, and **Plugins**. Make changes, then use **Apply / Save**; select **Cancel** to discard unapplied values, or **Restore defaults** for defaults. Plugin enable/disable changes require restarting the editor.

## 5. Fonts and editor behavior

Use a locally installed `.ttf` or `.otf` file. This may be different for the application widgets, editor and terminal. Example:

```json
{
  "editor": {"fontFile": "/home/me/fonts/FiraCode-Regular.ttf", "fontSize": 16},
  "appearance": {"uiFontFile": "/home/me/fonts/Inter-Regular.ttf", "uiFontSize": 13},
  "terminal": {"fontFile": "/home/me/fonts/JetBrainsMono-Regular.ttf", "fontSize": 14}
}
```

Do not replace the entire `settings.json` with only those sections unless you deliberately want defaults for the omitted keys. The Settings window can browse local font files. Font ligatures, per-language font changes and a full installed-font dropdown are not part of the current public contract.

## 6. Themes and Raven XML

The base application ships **13 embedded themes** (`resources/EmbeddedUi.h`). They include `Music Nocturne`, `Music Aurora Glass`, `Music Daylight`, `VS Code Dark Modern`, `VS Code Light Modern`, plus the previous presets. They are always available even with no files in `<config>/themes`.

Custom themes can be authored with **Appearance → Create / edit theme**, given a unique name, previewed, and exported as JSON or XML to a chosen path. Theme names must not collide with a built-in preset when using the editor's theme designer.

The model in `include/dodev/themes/ThemeManager.h` includes:

- `name`: displayed theme name;
- `base`: Raven `Dark`, `Light`, `GlassDark` or `GlassLight`;
- `accent`: primary accent color;
- `window`, `panel`, `control`, `input`, `text`, `separator`;
- syntax colors: `keyword`, `string`, `comment`;
- `cornerRadius`: corner radius of common widgets.

Custom files in `<config>/themes` are imported at startup; you can instead specify `appearance.customThemesDirectory` or import an individual file via the dialog. Embedded themes are read-only source assets.

**Round-trip note:** imported Raven XML may carry advanced `<Style>` classes and material configuration that the simplified JSON model does not represent. Keep the XML source if you depend on advanced material effects.

## 7. Author a theme in JSON or XML

### JSON — example `themes/ocean-glass.json`

```json
{
  "formatVersion": 1,
  "name": "Ocean Glass",
  "base": "GlassDark",
  "accent": "#5AC8FA",
  "cornerRadius": 12,
  "colors": {
    "window": "#0C1420",
    "panel": "#162438",
    "control": "#26394C",
    "input": "#101C2D",
    "text": "#EAF7FF",
    "separator": "#324B62"
  },
  "syntax": {
    "keyword": "#7DC8FF",
    "string": "#B0E7AF",
    "comment": "#8199AE"
  }
}
```

Select the imported theme under Appearance. The default JSON file example is also available at `examples/themes/my-dark.json`.

### XML — generated from `ThemeDefinition`

The precise XML vocabulary depends on Raven's theme implementation. **Do not assume an arbitrary QSS, GTK CSS, or HTML stylesheet is accepted.** To generate a valid native file, use **Theme Designer → Save as XML**. For a machine-generated file, construct a `ThemeDefinition` and call `toXml()`. The resulting XML can be loaded by `Rtheme_load_from_memory` and is what the GUI exports.

### Change VS Code-like colors only

Duplicate `VS Code Dark Modern` from the Designer, choose a new name and change the accent, panel, input, separator and syntax colors. Export as JSON for straightforward editing or XML when you need additional Raven-specific controls.

## 8. Extension-specific file templates

Templates can be built in, user-defined or registered dynamically by a native plugin. The default `TemplateRegistry` supports C++17, headers, Python, JSON, XML and CMake templates.

### Create a new `.sql` template

1. Create `<config>/templates/sql.tpl`:

```sql
-- ${FILE_NAME} | ${PROJECT_NAME} | ${DATE}
SELECT id, name
FROM example_table
WHERE id = ?;
```

2. Create `<config>/templates/templates.json`:

```json
{
  "templates": [
    {
      "id": "sql-query",
      "name": "SQL Query",
      "extension": ".sql",
      "file": "sql.tpl"
    }
  ]
}
```

3. Set the mapping in the main `settings.json`:

```json
{
  "templates": {
    "directory": "",
    "associations": {
      ".sql": "sql-query"
    }
  }
}
```

4. Restart if you created the templates after the initial directory scan. Use **File → New from Template**, select the template and enter the filename.

### Supported placeholders

| Placeholder | Meaning |
|---|---|
| `${FILE_NAME}` | Filename entered by the user |
| `${CLASS_NAME}` | Class-style name derived from filename stem |
| `${PROJECT_NAME}` | Current workspace/project label |
| `${DATE}` | Local date generated by the editor |
| `${AUTHOR}` | Provided template author variable |

The generated file opens as an **unsaved editor tab**; saving is a separate action. A plugin may register new templates using `register_template`.

## 9. `keybindings.json`

This is an optional **JSON array**, not a map. Custom overrides coexist with built-in defaults.

```json
[
  { "key": "Ctrl+G", "command": "navigation.go_to_line" },
  { "key": "Ctrl+P", "command": "navigation.quick_open" },
  { "key": "Ctrl+Shift+P", "command": "navigation.command_palette" },
  { "key": "Ctrl+Alt+F", "command": "clangformat.apply" },
  { "key": "Ctrl+Alt+D", "command": "docker.prompt" },
  { "key": "", "command": "edit.redo" }
]
```

The last entry explicitly **unbinds** `edit.redo`. Plugin commands can receive bindings when their plugins are enabled. A disabled plugin's binding can remain on disk, ready for the next activation.

### Built-in navigation and terminal shortcuts

| Key | Command |
|---|---|
| `Ctrl+G` | `navigation.go_to_line` |
| `Ctrl+P` | `navigation.quick_open` |
| `Ctrl+Shift+P` | `navigation.command_palette` |
| `Ctrl+K` | `keybindings.show` |
| `Ctrl+S` | `file.save` |
| `Ctrl+Shift+S` | `file.save_as` |
| `Ctrl+F` | `search.show` |
| `Ctrl+H` | `search.replace_show` |
| `Ctrl+Shift+F` | `edit.find_workspace` |
| `Ctrl+Shift+B` | `terminal.run_build` |
| `Ctrl+Grave` | `terminal.focus` |
| `Ctrl+Shift+Grave` | `terminal.new` |

Use **Go → Keyboard Shortcuts** or `Ctrl+K` to record a shortcut physically, filter all registered commands, edit a shortcut, unbind it or restore defaults. The registry detects duplicate shortcuts. Only single-stroke combinations are supported: VS Code's two-stage chords (`Ctrl+K Ctrl+S`) are **not** implemented.

### Shortcut names and parsing

Use names such as `Ctrl`, `Shift`, `Alt`, `Super`, `Grave`, `Enter`, `Tab`, `F1`–`F12`, and arrow keys. Avoid unmodified printable keys because they would interfere with text entry. Check `docs/KEYBINDINGS.md` for the complete parser contract and limitations.

## 10. Terminal and workspace tasks

The integrated terminal offers long-lived PTY shells (Linux/macOS) and ConPTY on compatible Windows systems. It is **not** a process-output-only panel. You can create multiple terminal sessions and split terminal views using the Terminal menu.

### Shell setting

```json
{
  "terminal": {
    "defaultShell": "bash",
    "scrollbackLines": 7500,
    "fontSize": 13
  }
}
```

On Windows use `pwsh`, `cmd`, or the default PowerShell configuration supported by `TerminalSession.cpp`.

### Workspace `.vscode/tasks.json`

DoDevEditor implements a **subset** of VS Code task syntax:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Configure",
      "type": "shell",
      "command": "cmake -S . -B build"
    },
    {
      "label": "Build",
      "type": "shell",
      "command": "cmake --build build -j 4",
      "group": {"kind": "build", "isDefault": true}
    },
    {
      "label": "Test",
      "type": "shell",
      "command": "ctest --test-dir build --output-on-failure",
      "group": "test"
    }
  ]
}
```

Supported fields are `label`, `type` (`shell` or `process`), `command`, `args` (array of strings), `group` and `options.cwd`. The loader does not implement the entire VS Code variable-substitution, debugger or problem-matcher format. Run tasks from **Terminal → Run Task / Run Build Task**. `Terminal → Configure Tasks` can generate a default task file.

### Safe command injection via plugins

The plugin host's `execute_terminal_command` sends a **single line** to the currently active terminal, starting a terminal if necessary. The host refuses embedded newlines and NULs. A plugin should quote each argument and should not concatenate untrusted values into shell code.

## 11. Native plugin packaging and activation

Plugins are optional shared libraries plus a mandatory `plugin.json` manifest:

```text
<config>/plugins/my-plugin/
├── plugin.json
└── libmy_plugin.so
```

On Windows the binary is `my_plugin.dll`; on macOS it is `libmy_plugin.dylib`.

### Manifest example

```json
{
  "id": "com.example.myplugin",
  "name": "My Plugin",
  "version": "1.0.0",
  "description": "Adds a command, menu and settings",
  "apiVersion": 1,
  "entry": {
    "linux": "libmy_plugin.so",
    "windows": "my_plugin.dll",
    "macos": "libmy_plugin.dylib"
  }
}
```

The loader requires a valid plugin ID, API version 1 and a **relative** binary filename without parent directory traversal. Manifests are discovered on startup; **native code does not load automatically**.

### Enable a plugin

1. Build the `.so`/`.dll` and place it next to its manifest.
2. Open **Settings → Plugins**; choose the directory containing installed plugin folders.
3. Select the plugin and click **Enable selected**.
4. Apply and save preferences, then restart.
5. Check the Plugins menu and Tools/Edit menu for registered actions.
6. If the plugin is not visible, inspect the manifest ID and filename, confirm that `plugins.safeMode` is false, and check the load diagnostics.

You can also opt in through the main `settings.json`:

```json
{
  "plugins": {
    "enabled": true,
    "safeMode": false,
    "enabledIds": ["com.dodev.clangformat", "com.dodev.docker"],
    "disabled": []
  }
}
```

**Do not enable an unknown binary merely because it came from a ZIP.** Native plugins are in-process and are not sandboxed. Disabling a plugin affects the next restart; the loader does not currently hot-unload it.

## 12. Plugin SDK v1 and the additive host functions

Public header: `include/dodev/sdk/dodev_plugin_api.h`.

```cpp
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(
    const DodevHostAPI* host, DodevPluginAPI* plugin);
```

The binary boundary is a fixed-layout **C API**, even when the implementation uses C++17. Do not export C++ STL containers, exceptions or native Raven types across the plugin ABI.

### Existing contributions

| Host function | What the plugin can add |
|---|---|
| `register_command` | Named executable command |
| `register_menu` | Item in File, Edit, View, Tools or Plugins |
| `register_tab` | On-demand tab content |
| `register_panel` | Docked panel in Left, Right, Bottom or Tab |
| `register_template` | Extension file template |
| `register_theme_xml` | In-memory Raven XML theme |
| `register_setting` | String setting with default and description |
| `get_setting` / `set_setting` | Plugin preference JSON |
| `subscribe_event` | Events such as `document.opened` |
| `log` | Severity-coded plugin diagnostics |

### Additive API v1 capabilities in 3.3

The host struct **appends** these function pointers after `subscribe_event`; existing binaries compiled against earlier v1 layout remain compatible because the preceding fields do not move.

| New host callback | Behavior |
|---|---|
| `get_active_document(ctx, &path, &contents)` | Read the active text document buffer; returned pointers are temporary and must be copied |
| `replace_active_document(ctx, contents)` | Replace active writable text buffer; the Raven editor insertion path preserves undo operations |
| `execute_terminal_command(ctx, line)` | Send a single command to the active persistent PTY/ConPTY session |
| `prompt_input(ctx, title, label, initial, callback, data)` | Show embedded-Raven dialog; callback runs when user confirms |

Plugins compiled with the **new** header should check `DODEV_HOST_HAS(host, get_active_document)` or the other optional callbacks before calling them. Plugins compiled with an older v1 header continue to see the same original fields.

```cpp
if (DODEV_HOST_HAS(host, execute_terminal_command)) {
    host->execute_terminal_command(host->context, "'docker' 'ps' '-a'");
}
```

The prompt callback is asynchronous with respect to the original command invocation. The plugin must not retain the dialog's temporary UI builder, and a callback must not assume the active document remained unchanged while the dialog was open. UI hooks execute on the Raven UI thread; long blocking operations may delay rendering.

### Add a menu item

```cpp
void hello(const DodevHostAPI* h, void*) {
    h->log(h->context, 1, "Hello from MyPlugin");
}
// Inside dodev_plugin_init:
host->register_command(host->context, "myplugin.hello", "Say Hello", &hello, nullptr);
host->register_menu(host->context, "Tools", "Say Hello", "myplugin.hello");
```

### Add a tab

```cpp
void createTab(const DodevHostAPI*, const DodevUiBuilder* ui, void*) {
    ui->label(ui->context, "Plugin settings and tools");
    ui->button(ui->context, "Execute", "myplugin.hello");
}
// Inside init:
host->register_tab(host->context, "myplugin.main", "My Plugin", &createTab, nullptr);
```

The tab and panel `DodevUiBuilder` supports **label**, **button** and **read_only_text**. Plugins do not receive direct access to arbitrary Raven widget pointers in API v1.

### Plugin events

Known host events include `app.ready`, `app.shutting_down`, `document.opened`, `document.saved` and `theme.changed`. Payloads are JSON strings, with event-specific keys. A plugin must not assume every payload contains the same structure.

## 13. Build your own plugin

Clone `plugins/examples/HelloPlugin` and adjust its module name, manifest ID and registered commands. Add a CMake MODULE target:

```cmake
add_library(my_plugin MODULE plugins/examples/MyPlugin/MyPlugin.cpp)
target_include_directories(my_plugin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_target_properties(my_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugin-examples/my-plugin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugin-examples/my-plugin")
configure_file(plugins/examples/MyPlugin/plugin.json
    "${CMAKE_BINARY_DIR}/plugin-examples/my-plugin/plugin.json" COPYONLY)
```

The exported symbol must be named `dodev_plugin_init`, return nonzero after successful registration, and populate `DodevPluginAPI::api_version`. Check `struct_size` before using fields. Use `shutdown` to release plugin-managed resources; keep Raven widget ownership with the host.

### Plugin settings

```cpp
host->register_setting(host->context,
    "message", "Hello", "Greeting displayed by the plugin");

const char* current = host->get_setting(host->context,"message");
host->set_setting(host->context,"message","Updated greeting");
```

Settings appear in **Settings → Plugins**, once the plugin has loaded and declared them. The current UI displays **string** values; numeric or boolean settings can be interpreted by the plugin after reading them as strings.

## 14. clang-format plugin

**ID:** `com.dodev.clangformat`
**Source:** `plugins/examples/ClangFormat/ClangFormatPlugin.cpp`
**Binary:** `libclangformat_plugin.so` (Linux), `clangformat_plugin.dll` (Windows)

The plugin registers two commands:

| Command ID | Behavior |
|---|---|
| `clangformat.apply` | Run clang-format against a copy of the active buffer and replace the in-memory text after success |
| `clangformat.style_prompt` | Prompt for a clang-format style and save it in the plugin's JSON settings |

The first command is available from **Edit → Format with clang-format** and the second from **Tools → clang-format Style...**. A **clang-format Tools** tab also provides buttons for both.

### Install

Build with `DODEV_BUILD_PLUGIN_EXAMPLES=ON`. The module and manifest are under `build/plugin-examples/clangformat/`.

```bash
CONFIG="${DODEV_CONFIG_DIR:-${XDG_CONFIG_HOME:-$HOME/.config}/DoDevEditor}"
mkdir -p "$CONFIG/plugins/clangformat"
cp build/plugin-examples/clangformat/{plugin.json,libclangformat_plugin.so} \
   "$CONFIG/plugins/clangformat/"
```

Enable `com.dodev.clangformat` in **Settings → Plugins**, restart and open a text file. You must install the **clang-format executable separately**; it is not downloaded or bundled by this project.

### Plugin JSON preferences

File: `<config>/plugins/com.dodev.clangformat/settings.json`:

```json
{
  "executable": "clang-format",
  "style": "LLVM"
}
```

`style` can be `LLVM`, `Google`, `Chromium`, `Mozilla`, `WebKit`, `GNU`, `Microsoft`, or an explicit `file:/absolute/path/to/.clang-format` where supported by the installed clang-format version. The plugin copies content to a temporary file before formatting; plain `file` styling may resolve relative to the temporary directory, **not** your workspace. Prefer an explicit path when depending on a project-specific `.clang-format` file.

The plugin does **not** save the source file automatically and does **not** format binary/CSV/DLT documents. It uses a temporary file and invokes the external process synchronously; very large files may temporarily block the UI. Format failures do not replace the editor buffer.

### Prompt example

1. Open **Tools → clang-format Style...**.
2. Enter `Google` and click **Execute / Apply**.
3. Open `example.cpp`, choose **Edit → Format with clang-format**.
4. Review changes and save with `Ctrl+S`. Use `Ctrl+Z` if you want to undo formatting.

Assign a shortcut by adding `{ "key": "Ctrl+Alt+F", "command": "clangformat.apply" }` to `keybindings.json` or through Keyboard Shortcuts Settings.

## 15. Docker Tools plugin

**ID:** `com.dodev.docker`
**Source:** `plugins/examples/Docker/DockerPlugin.cpp`
**Binary:** `libdocker_plugin.so` (Linux), `docker_plugin.dll` (Windows)

The Docker plugin is a **Docker CLI integration**; it does not embed Docker Engine's HTTP API or ship a daemon. Docker must already be installed and usable by the current user.

### Commands

| Command ID | Command or action |
|---|---|
| `docker.ps` | `docker ps -a` |
| `docker.images` | `docker images` |
| `docker.version` | `docker version` |
| `docker.compose_up` | `docker compose -f <composeFile> up -d` |
| `docker.compose_down` | `docker compose -f <composeFile> down` |
| `docker.build` | `docker build -t <buildTag> <buildContext>` |
| `docker.logs` | Prompt for a container and follow with `docker logs --tail 200` |
| `docker.run` | Prompt for an image and run it with `docker run --rm -it` |
| `docker.prompt` | Prompt for Docker CLI arguments and execute them in the current terminal |

All commands are registered under **Tools** and a **Docker Tools** tab. `docker.compose_down` can stop applications; `docker.run` may pull images and start processes. These actions are initiated by the user, never automatically at startup.

### JSON preferences

File: `<config>/plugins/com.dodev.docker/settings.json`:

```json
{
  "executable": "docker",
  "composeFile": "compose.yaml",
  "buildTag": "dodev-app:latest",
  "buildContext": ".",
  "container": "",
  "image": "alpine:latest"
}
```

You can change these from **Settings → Plugins → Docker Tools → Plugin setting** and save the value. A non-default Docker binary can be configured by absolute path, for example `/usr/local/bin/docker`.

### Execute prompt: examples

Open **Tools → Docker: Execute Command Prompt...**. Enter one of:

```text
ps -a
images
compose ps
container ls --all
inspect my-container
logs --tail 100 my-container
```

The user input is parsed as literal argument tokens, quoted and sent to the integrated terminal. The plugin does **not** evaluate `;`, `&&`, pipes or `$()` as shell operators from the prompt; it treats them as argument contents. You may also enter the leading `docker` word; it will be removed to avoid duplicate prefixes.

### Compose files and relative paths

Docker commands run in the **terminal session's current working directory**, normally the folder in which the terminal was started. If your `compose.yaml` lives in a different directory, set `composeFile` to a suitable path or change the terminal directory. The plugin does not automatically create Dockerfiles, build contexts or Compose YAML files.

### Linux Docker permissions

```bash
docker version
docker ps -a
```

If you receive `permission denied`, check daemon status and account permissions. Adding a user to the `docker` group grants broad privileges; understand the security implications before doing so. You do not need to run the editor as root.

## 16. Per-plugin JSON settings

For an enabled plugin, the host writes settings under `<config>/plugins/<plugin-id>/settings.json`. Example:

```text
<config>/plugins/com.dodev.clangformat/settings.json
<config>/plugins/com.dodev.docker/settings.json
<config>/plugins/com.dodev.hello/settings.json
```

This is separate from a plugin's installation folder. The plugin registers settings with `register_setting`, providing a key, string default and description. Settings are loaded at startup; `set_setting` saves immediately. **Do not put passwords, payment keys or access tokens in plain-text plugin JSON.** Current SDK does not provide a secrets vault.

### How to use General Settings for plugin options

1. Enable plugin and restart.
2. Open **Settings → Plugins**.
3. Select the installed plugin in the list.
4. Select a registered **Plugin setting** in the dropdown.
5. Enter the new string value and click **Save value**.
6. Execute its command or, if the plugin caches settings during initialization, restart for that plugin's setting to take effect.

The clang-format and Docker examples query settings at command time, so their settings normally apply to subsequent commands immediately.

## 17. Customization recipes

### Recipe A — VS Code-like appearance and shortcuts

Set `appearance.theme` to `VS Code Dark Modern`, `editor.fontSize` to `14`, `editor.tabSize` to `4` and `terminal.scrollbackLines` to `5000`. Leave `keybindings.json` absent to use default `Ctrl+P`, `Ctrl+G`, `Ctrl+Shift+P` and related commands.

### Recipe B — Raven_music glass style

Select `Music Aurora Glass`, then duplicate it in the Theme Designer and choose a new name. Adjust accent and surface colors, preview and export to `<config>/themes/my-music-glass.xml`.

### Recipe C — Format C++ with one keystroke

Enable clang-format; set its executable and style. Bind `Ctrl+Alt+F` to `clangformat.apply`. Open a text file and use the shortcut. Review and save the result.

### Recipe D — Build and test a project

Create `.vscode/tasks.json` from the example in Section 10. Open the project folder, run the build task with `Ctrl+Shift+B`, inspect the PTY output, then run CTest through the Terminal menu.

### Recipe E — Run Docker Compose from the editor

Enable Docker Tools, set `composeFile` to `compose.yaml`, open a workspace containing it and select **Tools → Docker: Compose Up**. The command executes in the active terminal. Use **Compose Down** only when you intend to stop the services.

### Recipe F — Temporary no-plugins mode

To investigate a crash or a suspicious plugin, edit the main JSON:

```json
{
  "plugins": {"enabled": true, "safeMode": true}
}
```

Restart. Native plugin code is not loaded while safe mode is true. Built-in editing and terminal components are unaffected.

## 18. Build options and tests

| CMake option | Default | Purpose |
|---|---|---|
| `DODEV_BUILD_GUI` | `ON` | Fetch Raven and compile the GUI executable |
| `DODEV_BUILD_TESTS` | `ON` | Compile all headless tests |
| `DODEV_BUILD_PLUGIN_EXAMPLES` | `ON` | Build Hello, clang-format and Docker native modules |
| `DODEV_RAVEN_REPOSITORY` | `https://github.com/aneury1/Raven.git` | Override remote source |
| `DODEV_RAVEN_GIT_TAG` | `main` | Branch, tag or commit to fetch |

Run the headless tests:

```bash
cmake -S . -B build-headless -DDODEV_BUILD_GUI=OFF
cmake --build build-headless --parallel
ctest --test-dir build-headless --output-on-failure
```

The suite covers settings defaults, JSON import/export, themes, templates, keybindings, editor operations, PTY, task parsing, Hello plugin and format/Docker plugin integration. **Passing headless tests does not prove the Raven/Vulkan GUI has been rendered successfully.** The native GUI must be compiled and visually tested against your chosen Raven revision.

## 19. Upgrading without losing local changes

The reported error:

```text
git apply --check DoDevEditor_Raven_3_2_Keybindings.patch
error: patch failed: include/EditorApp.h
error: patch failed: src/EditorApp.cpp
error: patch failed: src/EditorSettings.cpp
```

means the file contents in your local directory **do not match the exact 3.1 source snapshot** used to generate the previous patch. Causes include local edits, different base version, already applied changes, or line-ending transformations.

### Safest: full-source package

Extract `DoDevEditor_Raven_3_3_Extensions.zip` into a **new directory**. Point it at the same user configuration directory or copy your settings and workspace when ready. This does not touch the existing project.

### Clean base: use the correct patch

- If your project matches the original **3.1** source, use `DoDevEditor_Raven_3_1_to_3_3.patch`.
- If it matches the original **3.2** source, use `DoDevEditor_Raven_3_2_to_3_3.patch`.

```bash
cd /path/to/your/DoDevEditor
# Recommended: commit or back up modifications first.
git status --short
git apply --check /path/to/DoDevEditor_Raven_3_2_to_3_3.patch
git apply /path/to/DoDevEditor_Raven_3_2_to_3_3.patch
```

Do not run the 3.2 → 3.3 patch on version 3.0 or 3.1. If `--check` fails, **do not use `git apply --reject` blindly**; it can leave a half-updated project.

### Locally modified base: conflict-aware updater

The complete package ships `scripts/apply_upgrade.py` and original source snapshots under `upgrade/bases/{3.1,3.2}`. For a working tree with local changes, the script attempts three-way text merges with `git merge-file` and refuses to overwrite a conflicted file. It checks all changes **before writing any destination file** and creates backups of changed files.

```bash
python3 /path/to/DoDevEditor_Raven_3_3_Extensions/scripts/apply_upgrade.py \
  --target /path/to/your/existing/DoDevEditor --from 3.1 --dry-run

python3 /path/to/DoDevEditor_Raven_3_3_Extensions/scripts/apply_upgrade.py \
  --target /path/to/your/existing/DoDevEditor --from 3.1
```

On conflict, it creates `*.dodev-merge-conflict` files for inspection and does not apply changes. Backups of files it updates are written under `.dodev-upgrade-backup-<timestamp>`.

The updater requires Python 3 and Git. It handles UTF-8 text, and skips internal upgrade metadata. No tool can guarantee conflict-free application to an unknown modified source tree.

### Diagnostic commands

```bash
git status --short
git diff -- include/EditorApp.h src/EditorApp.cpp src/EditorSettings.cpp
grep -n 'project(DoDevEditor VERSION' CMakeLists.txt
git apply --check --verbose /path/to/upgrade.patch
```

If your local version is unknown, extract the full package in a new folder and manually reconcile your custom UI or code changes with a file comparison tool.

## 20. Security boundaries

- **Plugins are native code.** A plugin with malicious behavior has access to your files and operating system account; API-based permission declarations are not sandboxing.
- **Docker is privileged.** A functioning Docker daemon can launch containers with broad host access; inspect third-party images and Compose YAML before executing them.
- **Formatting is an external process.** `clang-format` is executed as a program and consumes the active document's contents; only install trusted executables and configurations.
- **Settings are plain-text JSON.** They are not encrypted and are not suitable for passwords, API keys, payment cryptographic keys or other secrets.
- **Terminal is a real shell.** Commands may modify or delete data. The Docker prompt's argument tokenizer avoids shell operator evaluation for user input, but the Docker CLI itself can perform destructive operations.
- **Config files are user-controlled.** Keep file permissions appropriate. Never execute untrusted plugin binaries just to inspect their settings.

## 21. Known limitations

1. The GUI depends on a compatible Raven commit and Vulkan stack; it has not been built in a GitHub-disconnected test environment.
2. Editor selections are contiguous; VS Code-compatible multi-cursor and rectangular selection are not implemented.
3. Keyboard chords such as `Ctrl+K Ctrl+S` are not implemented; shortcuts are single-step.
4. JSON themes express a simplified theme model. Advanced Raven XML styling may not round-trip through JSON.
5. Plugin UI widgets are limited to labels, buttons and read-only text views in v1.
6. Native plugins cannot be unloaded safely mid-session; activation changes require restart.
7. clang-format invocation is synchronous; the plugin does not automatically save formatted buffers.
8. The Docker integration is a terminal CLI wrapper, not an embedded Docker Desktop GUI or remote Docker REST client.
9. Full VS Code task/problem-matcher/debugger syntax is not implemented.
10. Plugin settings currently expose string values; a schema-based typed settings UI is a future enhancement.

## 22. Relevant source files

| Feature | Implementation |
|---|---|
| Settings schema/loading/saving | `src/settings/SettingsManager.cpp` |
| JSON parser/serializer | `src/settings/Json.cpp` |
| Settings Raven window | `resources/EmbeddedSettingsUi.h`, `src/EditorSettings.cpp` |
| User theme model | `src/themes/ThemeManager.cpp` |
| Embedded themes | `resources/EmbeddedUi.h` |
| Editor commands | `src/editor/EditorCommands.cpp` |
| Shortcut registry | `src/keybindings/KeybindingManager.cpp` |
| Keybinding integration | `src/EditorKeybindings.cpp` |
| Template expansion | `src/templates/TemplateRegistry.cpp` |
| PTY/ConPTY | `src/TerminalSession.cpp` |
| Task parsing | `src/terminal/TerminalTasks.cpp` |
| Plugin ABI | `include/dodev/sdk/dodev_plugin_api.h` |
| Plugin discovery/runtime | `src/plugins/PluginManager.cpp` |
| Plugin GUI bridge and prompt | `src/EditorSettings.cpp`, `resources/EmbeddedPluginPromptUi.h` |
| clang-format module | `plugins/examples/ClangFormat/ClangFormatPlugin.cpp` |
| Docker module | `plugins/examples/Docker/DockerPlugin.cpp` |
| Example plugin | `plugins/examples/HelloPlugin/HelloPlugin.cpp` |
| Upgrade utility | `scripts/apply_upgrade.py` |

---

For shorter focused references, read `docs/SETTINGS_V3.md`, `docs/KEYBINDINGS.md`, `docs/THEMES_AND_TEMPLATES.md`, `docs/PLUGIN_SDK_V1.md`, `docs/TERMINAL.md` and `docs/VSCODE_MENUS.md`.
