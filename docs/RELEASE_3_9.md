# DoDevEditor Raven 3.9 — Desktop Launcher, Workspace Search and Symbols Sidebar

## Overview

This release is based on **Raven 3.8** and keeps the bundled AI Assistant,
Source Symbols, QR Code, Serial Monitor and Remote Journal plugins.

### 1. VS Code-style launching

Install the editor (example for a user-local Linux installation):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build --parallel
cmake --install build
```

Commands (requires `~/.local/bin` in your PATH):

```sh
dodeveditor .                       # Open current folder; prompt comes back immediately
dodeveditor src/main.cpp            # Open a file in a new editor window
dodeveditor main.cpp "my file.h"    # Open multiple files
dodeveditor --foreground .          # Foreground session, logs on calling terminal
dodeveditor --wait src/main.cpp     # Wait until the GUI exits
~/.local/bin/DoDevEditor .          # Raw executable, foreground/debugging
```

On Linux, `dodeveditor` creates a separate session, disconnects stdin/stdout/stderr
from the terminal, and redirects output to:

```
${XDG_STATE_HOME:-$HOME/.local/state}/DoDevEditor/launcher.log
```

A `.desktop` file is installed at
`<prefix>/share/applications/dodeveditor.desktop` with `Exec=<absolute path>/dodeveditor %F`
and `Terminal=false`. For user-local installations, the file manager should
then offer **Open With → DoDevEditor** for matching MIME types.

If the option does not show up, ensure `~/.local/share/applications` is indexed
by your desktop. On desktop environments with `update-desktop-database`:

```sh
update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
```

You can set DoDevEditor as the preferred plain-text editor using
`xdg-mime default dodeveditor.desktop text/plain`. No root privilege is required
when installing under `$HOME/.local`.

**Limitations:** unlike VS Code, launching a second command currently starts
another window instead of sending IPC to an existing one. `--wait` waits for
the whole new editor process, not just one tab. This launcher does not alter
or replace the executable `DoDevEditor`.

### 2. Search panel — beside Explorer and Git

Use **Edit → Find in Workspace**, `Ctrl+Shift+F` with the existing default
keymap, **Ver → Buscar en archivos**, or the toolbar's **Buscar** button.

A dockable **BUSCAR** tab appears in the left panel group:

- Search text, optionally a regular expression (ECMAScript `std::regex`).
- Toggle case sensitivity and whole-word matching.
- Include patterns: `**/*.cpp,**/*.h`.
- Exclude patterns: `*.generated.cpp,tests/**`.
- Results table with match excerpt, match type and file:line:column.
- Double-click a result to open its document and select the matching text.
- **Replace selected** performs an undoable replacement in an editor tab (save
  with `Ctrl+S`).
- **Replace all** requires confirmation, updates open editor buffers using an
  undoable editor operation, and writes unopened files via an atomic rename.

Default exclusions: `.git`, `.svn`, `.hg`, `build`, `dist`, `.cache`, `node_modules`,
`vendor` and `.venv`. Binary and oversized files are skipped. Scan limits are
2,000 files and 10,000 hits by default. The status label displays a truncated
indicator if a limit is reached.

**Caution:** Replacement can modify many files. Commit or back up first. Open
buffers are changed in memory; **save them manually**. The replacement engine
refuses to write through symlinks and detects an already-existing staging file.
Search is currently synchronous, so large workspaces may briefly block the UI.

### 3. Source Symbols sidebar

The symbols tool now opens a separate **SYMBOLS** tab in the same dock group as
**ARCHIVOS**, **GIT** and **BUSCAR**, rather than under Tasks / Terminal.
The table has exactly these columns:

| Entity name | Type | Filepath : line |
|---|---|---|
| `Widget::paint` | method | `src/Widget.cpp:71` |

- Filter by entity name, type, or path.
- Double-click a symbol to navigate to its source and line.
- `Export CSV` includes name, type, filepath, line and signature.
- **Todo el proyecto** scans source files in the workspace (capped at 150 files).
- The active document uses **Clang-first**, falling back to Regex according to
  the Source Symbols plugin settings.
- Workspace-wide symbols use the **Regex parser** to avoid starting a compiler
  subprocess for every source file from the UI thread.

### Build and tests without Raven GUI

To validate the non-GUI libraries without network access:

```sh
cmake -S . -B build-headless -DDODEV_BUILD_GUI=OFF -DDODEV_BUILD_TESTS=ON
cmake --build build-headless --parallel
ctest --test-dir build-headless --output-on-failure --parallel 4
```

The GUI requires Raven and Vulkan. CMake will fetch Raven with visible Git
progress when `DODEV_BUILD_GUI=ON`. The complete graphical binary still needs
testing on a Raven/Vulkan-capable host.

### Apply the patch

From an **original, unchanged 3.8 project directory**:

```sh
git apply --check /path/to/DoDevEditor_Raven_3_8_to_3_9.patch
git apply /path/to/DoDevEditor_Raven_3_8_to_3_9.patch
cmake -S . -B build
cmake --build build --parallel
```

For a locally changed 3.8 source tree, copy or merge the changes manually
rather than forcing a patch that fails its `--check` step.
