# DoDevEditor Raven 3.2 — Keyboard shortcuts and navigation

DoDevEditor uses a configurable shortcut registry (C++17) and Raven's native `Rwindow_register_shortcut`/`Rwindow_unregister_shortcut` API. The menu XML no longer owns hard-coded shortcut assignments: `MenuItem` labels are updated from the runtime registry, preventing old shortcuts from remaining active after a rebind.

## Default shortcuts

| Shortcut | Command | Action |
|---|---|---|
| Ctrl+G | `navigation.go_to_line` | Navigate to a 1-based line and optional column (`42:8`) |
| Ctrl+P | `navigation.quick_open` | Search and open project files |
| Ctrl+Shift+P | `navigation.command_palette` | Search commands and execute them |
| Ctrl+K | `keybindings.show` | Open Settings → Keybindings directly |
| Ctrl+F / Ctrl+H | `search.show` / `search.replace_show` | Find / replace |
| Ctrl+Shift+F | `edit.find_workspace` | Search in workspace |
| Ctrl+S / Ctrl+Shift+S | `file.save` / `file.save_as` | Save / Save As |
| Ctrl+N / Ctrl+O | `file.new` / `file.open` | New / Open |
| Ctrl+Shift+B | `terminal.run_build` | Run build task |
| Ctrl+Grave | `terminal.focus` | Focus integrated terminal |
| Ctrl+Shift+Grave | `terminal.new` | New terminal |
| Ctrl+Shift+E | `panel.explorer` | Explorer |
| F3 / Shift+F3 | `search.next` / `search.prev` | Find next / previous |

Other standard menu shortcuts remain defined by `KeybindingManager::defaults()`. Every registered built-in menu command—including commands without shortcuts—and loaded plugin commands is available in the Keybindings settings UI.

## Editing shortcuts in the GUI

1. Open **Go → Keyboard Shortcuts** or press **Ctrl+K**.
2. Filter commands by name and select the desired command.
3. Type `Ctrl+Alt+G` in the shortcut field or select **Record keys** and physically press the combination.
4. Click **Apply**. The active window and menu label update immediately.
5. Use **Unbind**, **Reset selected**, or **Reset all** as required.

The editor detects collisions with bindings from other commands and refuses ambiguous assignments. `Esc` cancels shortcut capture. Printable keys without a modifier are rejected to avoid breaking normal editor typing.

## Persistent settings and file formats

The file is `$DODEV_CONFIG_DIR/keybindings.json` if the override environment variable is set, otherwise `SettingsManager::defaultDirectory()/keybindings.json` (normally `$XDG_CONFIG_HOME/DoDevEditor/` on Linux or `%APPDATA%/DoDevEditor/` on Windows).

Example:

```json
[
  { "key": "Ctrl+Alt+G", "command": "navigation.go_to_line" },
  { "key": "", "command": "edit.redo" }
]
```

Only overrides are written. If the file is missing, built-in defaults apply. Invalid JSON is not overwritten and does not prevent startup. All custom shortcuts are validated and conflicts fall back to safe defaults. Overrides for disabled plugins are retained for later activation.

## Quick Open details

Quick Open scans up to 20,000 project entries and indexes up to 3,000 paths, skipping `.git`, `build`, `node_modules`, `.venv`, `.cache` and `.idea`. Filtering displays up to 250 matches. It also indexes open files, and works without a folder when documents with on-disk paths are open.

Command Palette contains all registered commands, including active plugin commands. Shortcuts are **single stroke**, such as `Ctrl+G` or `Ctrl+Shift+P`; two-stage VS Code chords such as `Ctrl+K Ctrl+S` are not yet supported. `Ctrl+K` opens keybindings directly instead.

## Scope of tests and compatibility

The keybinding parser, collision detection, persistence, optional/malformed files, plugin override preservation, XML widget contracts, and navigation command wiring are covered in headless tests. Full Raven/Vulkan GUI compilation and rendering still need validation against a live Raven checkout. On layouts using AltGr or locale-specific modifier handling, native shortcut dispatch depends on Raven's platform backend.
