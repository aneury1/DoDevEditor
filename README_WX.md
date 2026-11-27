# DoDevEditor wxWidgets 5.2.6 — Workspace plugin views

See [Plugin settings SDK](docs/PLUGIN_SETTINGS_SDK.md) for the dynamic settings pages, migration, compatibility and remaining native UI adapter boundary.

New: dedicated payload/service filters, inclusive UTC time ranges with microsecond precision, and Settings > Remote Journal > Output panel or Editor tabs. See [the update guide](docs/WX_5_2_1_JOURNAL_FILTERS.md).

# DoDevEditor wxWidgets 5.2.0 — Persistent Remote Journal output tabs

Remote Journal now opens a persistent tab in the Output panel with structured rows, filtering, copying and export. See [the journal guide](docs/WX_5_2_0_JOURNAL.md).

See [build hotfix notes](docs/WX_5_1_9_BUILD_FIX.md).


See [Build system and troubleshooting](docs/BUILD_SYSTEM.md) for module responsibilities, presets, dependency diagnostics and platform notes. The 5.1.7 application features are unchanged.

# DoDevEditor wxWidgets 5.1.3

New: preserve detected wxWidgets runtime paths at install and select/run workspace VSCode tasks beside the theme picker. See [release notes](docs/WX_5_1_3.md).

# DoDevEditor wxWidgets 5.1.2

New: VSCode JSONC tasks and Remote Journal SSH password mode. See [release notes](docs/WX_5_1_2.md).

# DoDevEditor wxWidgets 5.1.1 — Explorer and theme propagation

This full source package extends wxWidgets 5.0.2 using the retained Raven 4.3 frontend as the reference. The wx GUI target does not fetch or link Raven.

## Build

Arch/CachyOS: `sudo pacman -S --needed wxwidgets-gtk3 gtk3 gcc cmake ninja clang gdb git graphviz curl`
Ubuntu/Debian: `sudo apt install libwxgtk3.2-dev libgtk-3-dev pkg-config build-essential cmake ninja-build clang gdb git graphviz libcurl4-openssl-dev`

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cmake --install build --prefix "$HOME/.local"
~/.local/bin/dodeveditor --foreground /path/to/project
```
Use `dodeveditor .` for detached startup. Existing user settings are retained. Choose the theme from the toolbar or the Appearance settings. New configurations use VS Code Dark Modern.

## 5.1.1 changes

See [Explorer and theme fixes](docs/WX_5_1_1.md). The root is enumerated explicitly, expanded branches and selection survive refresh, and app-local GTK3 colors are applied to native controls. No Raven menu or palette was removed.

## Changes

- Native wx menu bar generated from the retained Raven XML: 100 entries and 11 separators with the original hierarchy, labels and action identifiers. Dynamic plugin commands are added separately.
- Settings dialog with General, Editor, Appearance, Terminal, Templates, Keyboard Shortcuts and Plugins. Reuses SettingsManager and KeybindingManager; plugin approval is retained. Plugin discovery/enablement changes require restart.
- Original 13 built-in theme palettes, custom XML/JSON import and export, theme designer and preview. Glass palettes are opaque. Native GTK3 CSS is enabled when GTK3 development files are available; OS-controlled decorations/dialogs can still differ.
- Single lazy directory tree, Scintilla editor, context menus, background symbol/call index, cached relationships and source navigation.
- Direct-input PTY terminal with additional sessions, split, rename, restart, copy/paste and export commands. GDB CLI sessions and VSCode launch configuration support are retained.
- Original editor command services, search and replace, navigation, templates, terminal tasks, plugin host and simplified native AI, serial and SSH journal dialogs.

## Validation and known gaps

In 5.1.0 the headless Release build completed and 35 inherited tests passed. The additional `python3 tests/test_wx_menu_parity.py` source test verifies menu entries, action references and settings categories. Neither result establishes native GUI compilation or runtime parity. wxWidgets development headers/libraries could not be installed in the build environment (package repository DNS failure). No GUI executable is included.

Settings are in a modal native dialog rather than a docked panel. File-based custom font loading is not ported; use installed font families. GTK/native menu rendering can differ from Raven. CSV/DLT viewers, AI, serial and journal interfaces are simplified compared with Raven; the experimental Data Server menu has no network controller. Full graphical GDB/MI2, full xterm compatibility and runtime parity of all plugin commands are not yet established. Original Raven sources are retained for comparison. Windows/macOS GUI builds are unverified.

Headless reproduction:
```sh
cmake -S . -B build-core -DDODEV_BUILD_GUI=OFF
cmake --build build-core -j$(nproc)
ctest --test-dir build-core --output-on-failure
```

## 5.1.4 task variables

See [VSCode task variables](docs/WX_5_1_4.md) and `examples/task-variables`.

## 5.1.5 launch profiles

See [Launch.json menu and direct execution](docs/WX_5_1_5.md). Open `examples/launch-profiles` as a workspace and use Run > Launch.json to select, configure or run a profile.

## 5.1.6: Docker Manager

Enable **Docker Manager** in Settings > Plugins, restart, then open Plugins > Docker Manager. See [Docker Manager documentation](docs/WX_5_1_6_DOCKER.md) for image/container operations, interactive terminal support, CLI settings and limitations.

## 5.1.7: Syntax Highlighting

Open **Settings > Syntax Highlighting** to configure theme-wide and language-specific token styles or individual keywords. Inheritance, preview, reset and persistent settings are described in [Syntax Highlighting](docs/WX_5_1_7_SYNTAX.md).

See `plugins/examples/SimpleModule/README.md` for the independently buildable plugin tutorial.

## 5.2.4 migration status

See [workspace profiles and plugin boundaries](docs/WX_5_2_4_PLUGIN_BOUNDARIES.md). The standalone plugin export is limited to Git History, QR Code and Simple Module; full GUI decoupling of legacy plugins remains pending.

## 5.2.5 Journal filter rules

See [Journal filter rules](docs/WX_5_2_5_JOURNAL_RULES.md).

## 5.2.6 Workspace plugin views

Enable Git History and Remote Journal in Settings > Plugins and restart. Workspace panels are created automatically when a project opens and loaded on selection. Git history is a table, and selecting a commit replaces the named Output view with full metadata and changed files (diff enabled by default for new settings). Journal Logs contains the inline workspace profile editor; SSH captures always open as editor tabs. See `docs/WX_5_2_6_WORKSPACE_VIEWS.md`.

### 5.2.7
Journal filters are managed in Workspace; captures export TXT, CSV or SQLite snapshots. See [Journal workspace and export](docs/WX_5_2_7_JOURNAL_WORKSPACE_EXPORT.md).

### 5.2.8
HTTP Client plugin: Workspace request form and history, one independent Editor tab per request (request and response), SQLite history and generic form/job SDK. See [HTTP Client](thirdparty/HttpClient/README.md).

### 5.2.9 — Windows dependency fallback

Windows builds automatically fetch pinned wxWidgets 3.2.6 when no installed package is detected. Download/verification/extraction logs are visible, and the PowerShell build script saves configure/build/test logs. See [Windows build guide](docs/WX_5_2_9_WINDOWS_BUILD.md).
