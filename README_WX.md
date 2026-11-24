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
