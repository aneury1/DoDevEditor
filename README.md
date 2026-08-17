## DoDevEditor

this is a simple Text editor with some feature. I create as toy tool and for 
my own use, Im just sharing just if someone one to start from scratchs see 
what to do, for me work as expected. is not production ready text editor in this age
where there we could start any editor and plugins and so on. by now this does not contains
plugins, LSP Server and other nitty gritty details that would do "powerfull" 
it has the minimun I need in my days, probably in the far future I will improve it with 
these feature but by now just enjoy this. 

#this editor

# Build

## Linux - recommended

The project includes a build wrapper that checks the basic toolchain, handles
CMake 4.x policy compatibility for third-party dependencies, configures the
project, and builds in parallel.

```bash
./scripts/build-linux.sh
```

Useful variants:

```bash
./scripts/build-linux.sh debug
./scripts/build-linux.sh release --clean
./scripts/build-linux.sh relwithdebinfo --jobs 8 --ninja
```

If GTK3 development headers are missing, the script prints the appropriate
install command for Arch Linux, Debian/Ubuntu, or Fedora. The resulting Linux
executable is normally placed under `build/linux-release/bin/DoDevEditor`.


For a manual Linux configure/build, use:

```bash
cmake -S . -B build/linux-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux-release --parallel
```

## MinGW
```
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw32.cmake ..
cmake --build . -j2
```
### CMAKE Config

Remember on CMakeLists.txt set to TRUE the var CMAKE_WIN32_MINGW
and configure cmake with  cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw32.cmake .. 
otherwise linux would be the target.

Compile wxWidget for Mingw32
```sh
git clone --recurse-submodules https://github.com/wxWidgets/wxWidgets.git
cd wxWidgets
mkdir build-mingw32
cd build-mingw32
../configure --host=x86_64-w64-mingw32 --with-msw --prefix=/usr/x86_64-w64-mingw32 --disable-shared 
make -j$(nproc)
sudo make install #(make sure it get installed on prefix path. otherwise it would be a mess.)
### Check installation
ls /usr/x86_64-w64-mingw32/bin
ls /usr/x86_64-w64-mingw32/include/wx #(if this does not appear check ls | grep wx.* , probably you would see something  could be solve by ln -s <wx-version> <wx>)
ls /usr/x86_64-w64-mingw32/lib
```



#### LibGit2
```sh
git clone --recurse-submodules https://github.com/libgit2/libgit2.git
cd libgit2
mkdir build-mingw
cd build-mingw
cmake .. \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32 \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DBUILD_SHARED_LIBS=OFF \
    -DUSE_SSH=OFF \
    -DUSE_HTTPS=ON \
    -DUSE_OPENSSL=OFF \
    -DUSE_BUNDLED_ZLIB=ON \
    -G Ninja

run compilation
ninja 
or 
make 


then install 
sudo ninja install 
or 
sudo make install
```




### Calling Python-DLT From 

```
sudo apt update
sudo apt install python3-dev

```


####  Todo:

these working in progress feature must be implemented.
- context menu
- save history.
- auto completion
- GDB Integration
## Workspaces

DoDevEditor supports both a normal **single-folder** workflow and VS Code-style
**multi-root workspaces**. Saving a workspace file is optional.

File menu commands:

- `Open Folder...` — replace the current workspace with one folder. This keeps
  the simple folder-only workflow.
- `Add Folder to Workspace...` — add another independent root folder.
- `Remove Folder from Workspace...` — remove one root without deleting files.
- `Open Workspace...` — open a `.dodev-workspace` file.
- `Save Workspace As...` — persist the current folder set.
- `Close Workspace` — detach all workspace folders while leaving editor tabs open.

An unsaved multi-root workspace is restored from `config.json` on the next
launch, so creating a `.dodev-workspace` file is not required. A saved workspace
uses a small JSON format such as:

```json
{
  "version": 1,
  "folders": [
    { "path": "app" },
    { "path": "../shared" }
  ]
}
```

Relative paths are resolved from the directory containing the workspace file.
In a multi-root workspace the Explorer displays every root independently.
`Ctrl+P` and `Ctrl+Shift+F` search across all workspace roots. The Git and `SYMBOLS` analysis panels follow the workspace folder containing the active editor file.
Each root can therefore keep its own repository and project-local plugin/tool configuration.

## Search

- `Ctrl+F` — find in the current document.
- `Ctrl+Shift+F` — find in all text documents under every opened workspace folder.
- `F3` / `Shift+F3` — next / previous match.
- Workspace search supports Match case and Whole word, skips common generated/vendor directories, and opens a result at its matching line when activated.

## Git tab

The Explorer-side `GIT` tab uses the installed `git` command-line client and the active workspace folder. It shows working-tree changes and recent commits, and supports Refresh, Stage, Unstage, Discard and Commit. Double-click a working-tree change to open the side-by-side Git diff editor.

Selecting a row under **RECENT COMMITS** now populates **COMMIT CHANGES** with the files changed by that commit, including add/delete/modify/rename status. Double-click a commit file to open a read-only Parent ↔ Commit diff tab. This works independently from the working-tree Stage/Unstage diff.

## JSON syntax highlighting and themes

Editor syntax and theme resources are copied next to the executable under `config/` and are loaded at runtime. They can be edited without rebuilding DoDevEditor.

Default syntax definitions:

- `config/syntax/c.json`
- `config/syntax/cpp.json`
- `config/syntax/kotlin.json`
- `config/syntax/java.json`
- `config/syntax/javascript.json`

Each syntax JSON defines the language name, file extensions, lexer, keyword/type sets and lexer properties. The built-in language detector remains as a fallback for languages that do not yet have a JSON definition.

Default editor themes:

- `config/themes/dark.json` — DoDev Dark
- `config/themes/light.json` — DoDev Light
- `config/themes/vscode.json` — VS Code Dark Alike

Theme JSON controls editor/background, line numbers, caret, selection, indentation guides, edge colour, font and semantic syntax colours such as comments, keywords, types, strings, numbers, preprocessors, operators, identifiers and functions. Select the active theme from `Settings → General Settings → Editor`. Open editor tabs are re-themed immediately after saving settings.


## Arch Linux / modern GCC build note

On Linux the project now tells wxWidgets to use the **system zlib** instead of
its bundled zlib. This avoids modern GCC failures where the bundled `gz*.c`
sources reach POSIX calls such as `read()`, `close()` and `lseek()` without the
required declarations. The build script checks for zlib before CMake starts.

On Arch Linux, if needed:

```bash
sudo pacman -S --needed base-devel cmake ninja git pkgconf gtk3 zlib
```

The project still explicitly compiles C dependencies as GNU C17, while the
DoDevEditor sources remain C++17. This provides a stable baseline for older C
dependencies but is not used to hide missing function declarations.

Always use a clean build after changing wxWidgets/dependency settings:

```bash
./scripts/build-linux.sh release --clean
```

If compilation fails, the script stores the complete output in:

```text
build/linux-release/build.log
```

For detailed compiler commands and lower-memory compilation:

```bash
./scripts/build-linux.sh release --clean --jobs 2 --verbose
```


### wxString compatibility note

`SourceControlPanel::DecodeGitPath()` uses indexed access (`path[0]` and
`path[path.length() - 1]`) instead of STL-style `front()`/`back()`. This keeps
the Git panel compatible with wxString in the wxWidgets 3.2 build used by this
project.

## Built-in C / C++ / Kotlin symbols and call hierarchy (optional)

DoDevEditor includes a dependency-free source-analysis framework written in C++17. It does not require an external parser library, tree-sitter, or an LSP server. The first registered languages are:

- C: `.c`
- C++: `.cpp`, `.cc`, `.cxx`, `.c++`, `.h`, `.hpp`, `.hh`, `.hxx`, `.ipp`, `.inl`, `.tpp`
- Kotlin: `.kt`, `.kts`

The parser uses a reusable tokenizer, language-aware symbol passes, callable-body analysis, and a registry that maps file extensions to language backends. `ParserRegistry::RegisterExtension()` / `RegisterExtensions()` are intentionally public so later language support can reuse the same Symbols and Call Hierarchy UI.

The built-in parser extracts the common editor-navigation structures rather than attempting to be a full compiler. It recognizes C/C++ namespaces, classes/structs/unions/enums, typedefs/type aliases, functions/methods/constructors, fields/variables and macros. Kotlin support includes packages, classes/interfaces/objects/enums, functions/methods/extension functions, primary-constructor properties, properties/variables and type aliases.

Function bodies are analyzed in a second pass to build static call relationships. The **Call Hierarchy** view exposes both directions:

```text
PaymentProcessor::Process
├── Callees
│   ├── Validate
│   ├── BuildISO8583
│   └── SendTransaction
└── Callers
    ├── SaleTransaction::Execute
    └── RetryTransaction::Execute
```

Resolution is deliberately conservative. Calls that can be uniquely associated with a symbol receive a navigation target; ambiguous/dynamic calls are kept and marked unresolved instead of pretending compiler-grade certainty. Function pointers, virtual dispatch, reflection and macro-generated code may therefore remain unresolved. This is a **static call hierarchy**, not a runtime debugger call stack.

Use:

```text
Code -> Parse Symbols / Call Hierarchy
Code -> Show Call Hierarchy        Ctrl+Shift+H
Code -> Go to Definition           F12
```

F12 reparses the current unsaved buffer with the manual parser and uses only the built-in symbol/call index. Optional semantic compiler tooling is supplied by a separate runtime plugin.

### Runtime parser settings

`Settings -> General Settings -> Code Analysis` controls the built-in parser without rebuilding. Every major parser/analysis category can be disabled independently:

- master `Enable dependency-free source parsing` switch;
- C parser;
- C++ parser;
- Kotlin parser;
- automatic parsing on file/tab changes;
- Symbols;
- Call hierarchy (callers/callees);
- Types / namespaces / packages / aliases;
- Functions / methods / constructors;
- Variables / fields / properties;
- C/C++ macros.

Disabled languages are not tokenized or parsed. These choices are persisted under `manual_symbol_parser` in `config.json`.

Example:

```json
{
  "manual_symbol_parser": {
    "enabled": true,
    "auto_parse": true,
    "languages": {
      "c": true,
      "cpp": true,
      "kotlin": false
    },
    "features": {
      "symbols": true,
      "calls": true,
      "types": true,
      "functions": true,
      "variables": true,
      "macros": false
    }
  }
}
```

### Compile-time switch

The entire manual parser can also be removed from the executable:

```bash
cmake -S . -B build -DDODEV_ENABLE_MANUAL_SYMBOLS=OFF
```

Linux helper:

```bash
./scripts/build-linux.sh release --manual-symbols
./scripts/build-linux.sh release --no-manual-symbols
```

Windows MinGW helper:

```bash
./scripts/build-windows-mingw.sh release --manual-symbols
./scripts/build-windows-mingw.sh release --no-manual-symbols
```

When disabled, `src/symbols/ManualSymbolParser.cpp` is excluded from the target and `DODEV_ENABLE_MANUAL_SYMBOLS=0` removes the manual integration path from the Symbols panel.


## VS Code-style navigation

- `Ctrl+P` opens **Quick Open**, fuzzy-searching files under the current project folder.
- Quick Open skips generated/heavy folders such as `.git`, `.dodev`, `build`, `_deps`, `node_modules`, `dist`, `out`, and `.cache`.
- Use Up/Down to move through matches, Enter to open, and Esc to close.
- `Ctrl+G` opens **Go to Line/Column**. Enter either `42` or `42:7` to jump to a line and optional column.

## VS Code-style Git changes and diff editor

The Source Control tab now integrates staging and diff navigation directly with the editor:

- activate a changed file, or press **Diff**, to open an editor diff tab;
- unstaged changes compare **Index -> Working Tree**;
- staged changes compare **HEAD -> Index**;
- changed/deleted lines are highlighted on the original side and added/changed lines on the modified side;
- untracked files are shown as entirely new content;
- the diff tab is read-only and has **Refresh**, **Stage**, **Unstage**, **Discard**, and **Open File** actions;
- the Source Control list has the same actions in its right-click menu;
- **Stage** uses `git add -- <file>`;
- **Unstage** uses `git restore --staged` with `git reset HEAD` as a compatibility fallback;
- **Discard** requires confirmation;
- diff tabs are normal editor-area tabs but are safely excluded from Save All, source analysis, word-wrap changes, and other source-only operations.

Git status keeps the normal two-character porcelain status (`XY`): the first column is the index/staged state and the second is the working-tree state. When a file has both staged and unstaged changes, the diff opens the working-tree change first; staging the remaining change switches the view to the staged `HEAD -> Index` comparison.

## Runtime Docker inspector

Docker support is intentionally runtime-only and has no Docker SDK/library build dependency.
The editor starts with the Docker tab disabled. Toggle it at any time with:

- `View -> Docker Inspector`
- `Turn off` inside the Docker tab to remove it again without restarting DoDevEditor.

When enabled, the tab uses the local `docker` CLI and the active Docker context. It provides:

- all containers (`docker container ls -a`) with name, image, state, status, ports and ID;
- local images (`docker image ls`);
- Docker Engine/version information;
- `docker inspect` for a selected container or image;
- the last 300 timestamped log lines for a selected container;
- one-shot container statistics (`docker stats --no-stream`);
- optional 5-second auto-refresh.

If the Docker CLI is not installed, the daemon is stopped, or the current user cannot access
the daemon, the panel reports the Docker error and the rest of the editor continues to work.
No Docker headers, SDK, socket library, or compile-time Docker option is required.

## AI Chat and General Settings

DoDevEditor includes an AI Chat page in the main editor notebook.

Open it with:

```text
View -> AI Chat
Ctrl+Alt+I
```

The chat supports these provider modes:

- OpenAI via the Responses API.
- Anthropic / Claude via the Messages API.
- OpenAI-compatible Chat Completions endpoints (for example a local/self-hosted server).
- GitHub Copilot CLI via `copilot -p`; authentication remains managed by the official Copilot CLI.
- Native Ollama local inference through `/api/chat`, with model discovery, streaming and no-secret localhost mode.
- Native llama.cpp server integration through `/v1/chat/completions`, with `/health` checks, `/v1/models` discovery, optional API-key authentication and streaming.
- Google Gemini through `generateContent`, with API-key authentication and model discovery through the Gemini Models API.

Open configuration with:

```text
Settings -> General Settings...
```

The General Settings dialog contains editor-context defaults and provider/secret configuration. API secrets are intentionally not written to `config.json`.

Secret sources:

- **Environment variable** (default): e.g. `OPENAI_API_KEY`, `ANTHROPIC_API_KEY` or `GEMINI_API_KEY`.
- **Session secret**: stored only in DoDevEditor's process memory and discarded when the application exits.
- **OS keyring**: on Linux, uses `secret-tool`/libsecret. The secret is sent to `secret-tool` through stdin and is not placed in shell arguments.
- **Existing provider login**: used by GitHub Copilot CLI (`copilot login`).
- **No secret / local endpoint**: used by native local Ollama and by llama.cpp when the local server has no API key configured.

Example environment setup:

```bash
export OPENAI_API_KEY="..."
export ANTHROPIC_API_KEY="..."
export GEMINI_API_KEY="..."
```

For Linux OS-keyring support on Arch:

```bash
sudo pacman -S --needed libsecret
```

Direct HTTP providers use the installed `curl` executable at runtime. Request bodies and curl configuration are written to short-lived private temporary files and removed immediately after each request so API keys are not passed as curl command-line arguments.

The chat can optionally attach:

- the last active source file,
- the current source selection,
- the last active Git diff.
- dependency-free C/C++/Kotlin symbols and static call hierarchy in core; optional compiler-specific semantic tooling is supplied by a runtime plugin.

Context is capped before sending to avoid accidentally attaching very large files.

## Local History / Multi-save

> Build compatibility: the Local History dialog includes the concrete wxWidgets button and static-text headers explicitly, which is required by wxWidgets configurations where `wx/dialog.h` only forward-declares `wxButton`.


DoDevEditor keeps a recoverable local revision whenever a file is successfully
saved. This is independent of Git and works for files that are untracked or in
folders that are not Git repositories.

Use:

- `File -> Local History...`
- `Ctrl+Alt+H`

The first time an existing file is overwritten by DoDevEditor, its current disk
contents are captured as a **Baseline**. The newly saved contents are then stored
as the first **Save** revision. Every later explicit save adds another revision.
`Save As`, `Save All`, and the save-before-close path all use the same history
pipeline.

History is stored per workspace root under:

```text
.dodev/history/<file-path-hash>/
    index.json
    <revision>.snapshot
    <revision>.patch
```

`.snapshot` is the complete UTF-8 content used for reliable recovery. `.patch`
is a unified-diff-style view against the previous local revision. Keeping the
snapshot means restoring an old save does not depend on an unbroken chain of
patches. `.dodev/history/` is ignored by Git by default.

The Local History dialog lists the revisions newest first with timestamp, type,
change counts, size and line count. It provides **Patch** and **Snapshot**
previews. **Restore Selected** loads that historical content into the existing
editor tab as an unsaved change; it does not overwrite the working file until
the user explicitly saves again. This makes restoration safe and also allows
Undo before committing the restored content to disk.

In a multi-root workspace, each file's history is stored under the workspace
folder containing that file, so independent roots keep independent caches.

## Native Ollama AI Provider

DoDevEditor can use a local Ollama installation as a first-class AI provider.
Open:

```text
Settings -> General Settings... -> Provider & Secrets
```

and choose **Ollama (local)**. The default server is:

```text
http://localhost:11434
```

Ollama local mode uses no API secret by default. The editor talks to Ollama's
native API rather than routing it through the generic OpenAI-compatible mode.
The integration provides:

- native `/api/chat` conversations;
- automatic local model discovery through `/api/tags`;
- **Refresh Models** and **Test Connection** controls;
- server-version reporting from `/api/version` and loaded-model reporting from `/api/ps`;
- token-by-token streaming into the AI Chat page;
- configurable temperature, context window, keep-alive duration and thinking;
- configurable Ollama server URL;
- current file, current selection, Git diff and code symbol/call-hierarchy context.

A typical configuration is:

```text
Provider:       Ollama (local)
Base URL:       http://localhost:11434
Model:          qwen3-coder:30b
Secret source:  No secret / local endpoint
Streaming:      enabled
Temperature:    0.20
Context:        32768
Keep alive:     5m
```

The AI Chat page continues to run provider requests on a worker thread. Ollama
stream chunks are marshalled back onto the wxWidgets UI thread before updating
the conversation view.

## Native llama.cpp AI Provider

DoDevEditor can connect directly to `llama-server`. Choose **llama.cpp (local)**
from `Settings -> General Settings... -> Provider & Secrets`. The default server
root is:

```text
http://127.0.0.1:8080
```

The editor checks `/health`, discovers the exposed model IDs through `/v1/models`,
and sends chats through `/v1/chat/completions`. Streaming is enabled by default.
The local provider starts with **No secret / local endpoint**, but Environment,
Session Secret and OS Keyring can be selected when `llama-server` is started
with API-key authentication.

Example:

```bash
llama-server -m /path/to/model.gguf --host 127.0.0.1 --port 8080
```

## Google Gemini AI Provider

Choose **Google Gemini** from `Settings -> General Settings... -> Provider & Secrets`.
The default API root is:

```text
https://generativelanguage.googleapis.com/v1beta
```

The default secret source is the `GEMINI_API_KEY` environment variable. Session
and OS-keyring secrets are also supported. **Refresh Models** loads models that
advertise `generateContent` support and **Test Provider** validates the configured
key/API endpoint. The normal Current File, Selection, Git Diff and Code Analysis context
options are reused for Gemini chat requests.

## Explorer Tree + Folder List

The left Explorer now contains two simultaneous filesystem views:

```text
EXPLORER
+----------------------------------+
| Tree view                        |
|  workspace                       |
|   src                            |
|   include                        |
+----------------------------------+
| FILES IN FOLDER                  |
| Name       Type     Size Modified|
| main.cpp   CPP file 12 KB ...    |
| app.h      H file    3 KB ...    |
+----------------------------------+
```

The upper pane remains the existing expandable tree. The lower pane is a flat
list of the currently selected directory and shows **Name**, **Type**, **Size**
and **Modified** columns. Directories are sorted before files.

- Selecting a directory in the tree loads that directory in the list.
- Selecting a file in the tree loads its parent directory in the list.
- Double-clicking a file in the list opens it in an editor tab.
- Double-clicking a directory navigates the flat list into that directory.
- The **up** and **refresh** controls navigate to the parent and rescan the folder.
- Switching editor tabs updates the list to the active source file's directory.
- In multi-root workspaces the list follows whichever workspace root contains the
  current file or selected tree folder.


## Recent Files and Folders

The **File** menu now keeps two persistent MRU lists:

- **Recent Files** — the last 15 files opened or successfully saved.
- **Recent Folders** — the last 15 folders opened directly or added/opened through a workspace.

The newest item is shown first. Reopening an existing item moves it back to the top. If an entry no longer exists, selecting it removes it from the history. Both submenus include a **Clear Recent...** action.

The lists are persisted in `config.json` as `recent_files` and `recent_folders` and work in both single-folder and multi-root workspace modes.


## File menu: Recent Files and Recent Folders

The **File** menu contains two persistent IDE-style history submenus:

- **Recent Files** — up to 15 recently opened/saved files, newest first.
- **Recent Folders** — up to 15 recently opened workspace folders, newest first.

Entries are stored in `config.json` as `recent_files` and `recent_folders`. Reopening an entry moves it to the top. Missing paths are pruned automatically when the menu refreshes. Each submenu includes a **Clear Recent Files/Folders** action. The first nine entries also have numbered menu mnemonics for quicker keyboard access.


## Journal Log Inspector (optional)

DoDevEditor can be built with an optional cross-platform Journal Log Inspector. It adds **Tools → SSH Journal Logs...** and **Tools → Import Journal Logs...**.

The SSH tab launches the local OpenSSH client and runs `journalctl -o json --no-pager` on the remote Linux/systemd host. Controls include host, optional port, user, authentication mode, private-key path, optional systemd unit/application, **Follow (`-f`)**, **Current boot (`-b`)**, and a **Since** value such as `2026-08-16 08:00:00` or `-2 hours`. Leave the port blank to preserve the port from `~/.ssh/config` (or OpenSSH's default port 22).

Authentication modes are **SSH config / agent**, **Private key**, and (on Unix-like builds) **Password**. Config/agent and private-key mode use non-interactive `BatchMode=yes`, while password mode uses a temporary owner-only `SSH_ASKPASS` helper and keeps the password out of the command line. The helper is removed when the SSH process exits. Windows OpenSSH builds should use SSH config/agent or private-key authentication. The tab also has **Test SSH**, an **Accept new host key** option (`StrictHostKeyChecking=accept-new`), connection timeouts/keepalives, and a persistent diagnostics box that shows the actual OpenSSH stderr instead of replacing it with only an exit code.

Both live and imported tabs use the same table and filtering pipeline:

- App/unit filter using `std::regex`.
- Payload filter using `std::regex`.
- Optional case-sensitive matching.
- Export the currently visible rows to TXT or JSON.
- Multi-select rows and right-click to copy them, save selected rows as TXT, or save selected rows as JSON.
- Stop a live `ssh journalctl -f` process without closing the tab.

Import accepts `.txt`, `.log`, and `.json`. JSON accepts either the array format exported by DoDevEditor or journalctl JSON-lines. Text preserves every line and recognizes DoDevEditor's tab-separated TXT export.

The feature is enabled by default and can be removed at compile time:

```bash
cmake -S . -B build -DDODEV_ENABLE_JOURNAL_LOGS=OFF
cmake --build build
```

The Linux helper also supports:

```bash
./scripts/build-linux.sh release --journal-logs
./scripts/build-linux.sh release --no-journal-logs
```

For the project's existing MinGW cross-build, use for example:

```bash
cmake -S . -B build/windows -DBUILD_WINDOWS=ON -DDODEV_ENABLE_JOURNAL_LOGS=ON
cmake --build build/windows --parallel
```

Windows builds do not require systemd or journalctl locally. Live collection only requires an OpenSSH `ssh` client in `PATH` and a reachable remote Linux host with permission to read the journal. Import/filter/export remain available on Windows and Linux.

The repository also includes a MinGW helper for Linux-hosted Windows cross-builds:

```bash
./scripts/build-windows-mingw.sh release --journal-logs
./scripts/build-windows-mingw.sh release --no-journal-logs
```


## File Compare (text + binary)

DoDevEditor can optionally build a standalone side-by-side file comparison tab inspired by Beyond Compare. Open it with **Tools -> Compare Files...** and choose independent left and right files.

The tab provides:

- **Load Left...** and **Load Right...** file selectors.
- **Auto / Text / Binary** comparison mode.
- **Swap** and **Compare/Refresh** controls.
- Synchronized vertical scrolling between the left and right panes.
- **Previous Diff / Next Diff** navigation.
- Text comparison with aligned inserted/deleted/changed rows and optional **Ignore whitespace** / **Ignore case** matching.
- Binary comparison as aligned 16-byte hexadecimal + ASCII rows, including differing-byte count and first differing offset.
- Read-only compare panes: comparing files never modifies either source file.

Text mode uses a line LCS alignment for ordinary files. To prevent excessive memory use on very large inputs, it automatically falls back to a bounded linear alignment when the LCS matrix would be too large.

The module is enabled by default and can be removed from the build completely:

```bash
cmake -S . -B build -DDODEV_ENABLE_FILE_COMPARE=OFF
```

Linux helper:

```bash
./scripts/build-linux.sh release --file-compare
./scripts/build-linux.sh release --no-file-compare
```

Windows MinGW cross-build helper:

```bash
./scripts/build-windows-mingw.sh release --file-compare
./scripts/build-windows-mingw.sh release --no-file-compare
```

The feature uses only C++17 and wxWidgets, so the text and binary comparison implementation is the same on Linux and Windows.


## Runtime plugin SDK

DoDevEditor can load native plugins from a configurable plugin directory. The
default remains `plugins/` beside the executable, but **General Settings ->
Plugins -> Plugin folder** can point to any other directory. Relative paths are
resolved against the executable directory. The feature is enabled by default and the general build scripts also build all
plugins shipped under `plugins/` and deploy their `.so`/`.dll` files into the
runtime plugin directory:

```bash
./scripts/build-linux.sh release --plugins
./scripts/build-linux.sh release --no-plugins
```

Use a custom deployment directory that matches **General Settings -> Plugins ->
Plugin folder** with:

```bash
./scripts/build-linux.sh release --plugin-dir custom_plugins
./scripts/build-linux.sh release --plugin-dir /home/user/.local/share/dodev/plugins
```

Relative `--plugin-dir` paths are resolved below the build's `bin/` directory,
matching the runtime rule that relative plugin paths are resolved beside the
DoDevEditor executable. `--no-bundled-plugins` keeps the runtime plugin system
while skipping all in-tree plugin builds.

or directly with CMake:

```bash
-DDODEV_ENABLE_PLUGINS=ON
-DDODEV_BUILD_BUNDLED_PLUGINS=ON
-DDODEV_PLUGIN_OUTPUT_DIR=/desired/plugin/path
```

`DODEV_ENABLE_PLUGINS=OFF` removes the runtime plugin system entirely.

The ABI lives in `includes/plugin/DoDevPluginAPI.h`. A basic plugin does not
need wxWidgets; it talks to the editor through C function pointers. Plugins can
add menu commands, inspect or modify the active editor, enumerate/select/close
open tabs, open files, create editor tabs, access the file/Git/Symbols/bottom
panels, create host-owned text panels, and subscribe to editor/tab/workspace
events.

The editor adds a **Plugins** menu with **Reload Plugins**, **Loaded Plugins...**
and **Open Plugins Folder**. Reload safely removes plugin-owned menu commands
and panels before unloading the shared library.

Bundled plugins currently include:

- `plugins/example_plugin` — SDK/editor-access example;
- `plugins/http_editor_server` — multi-client HTTP/SSE/WebSocket server with one room/channel per open editor;


An independent example is under `plugins/example_plugin/`. Build it without
wxWidgets:

```bash
cmake -S plugins/example_plugin -B build/example-plugin -DDODEV_SDK_ROOT="$PWD"
cmake --build build/example-plugin
```

Or build/copy it together with the editor:

```bash
cmake -S . -B build -DDODEV_ENABLE_PLUGINS=ON -DDODEV_BUILD_EXAMPLE_PLUGIN=ON
```

Advanced plugins can create arbitrary wxWidgets panels with `add_custom_panel`.
Those plugins must use an ABI-compatible wxWidgets/compiler build; ordinary
host-API-only plugins do not have that requirement.

## Editor invisibles / non-printable characters

The View menu now exposes persistent controls for characters that are normally invisible in source files:

- **Show Spaces and Tabs**
- **Show End of Line**
- **Show Control Characters**
- **Show All Non-Printable Characters**

The state is stored in `config.json` under `editor_view` and is applied to newly opened editor tabs. No file content is modified by these visualization options.

## Replace dialog

The Search menu now contains:

- **Replace in Current Document...** (`Ctrl+H`)
- **Replace in Workspace Folders...** (`Ctrl+Shift+R`)

The dialog supports match case, whole word, and `std::regex` mode. Regex replacement supports capture groups such as `$1`. Preview results can be double-clicked to navigate to the match.

Current-document replacements are applied to the current editor buffer as undoable unsaved changes. Workspace replacement scans text-like files up to 4 MiB while skipping generated/cache folders such as `.git`, `.dodev`, `build`, `_deps`, `node_modules`, `target`, and similar directories. Open files use their current unsaved buffer; closed files are backed up under `.dodev/replace-backups/<timestamp>/` before being modified on disk.

## Remote SSH journal logs

When `DODEV_ENABLE_JOURNAL_LOGS=ON`, **Tools -> Remote SSH Journal Logs...** opens the SSH journal collector. The corrected SSH flow supports SSH config/agent, private keys, Unix password authentication through `SSH_ASKPASS`, an explicit Test SSH action, SSH diagnostics, remote `journalctl` validation, follow (`-f`), current boot (`-b`), unit filtering and since/from-time filtering.

## Hex / Binary Viewer

DoDevEditor can open any file in a dedicated read-only hexadecimal tab using
**File -> Open File as Hex...**. The viewer reads raw bytes rather than decoding
the file as text and displays an aligned offset, hexadecimal byte, and ASCII
representation.

Features:

- 8, 16, or 32 bytes per displayed row (16 by default).
- 64 KiB paged reads so large executables, archives, database files, images,
  firmware, and other binaries do not need to be loaded into memory at once.
- Previous/Next Page and direct offset navigation. Offsets accept decimal or
  hexadecimal forms such as `4096` or `0x1000`.
- Byte-exact search. Hex mode accepts values such as `DE AD BE EF`; text mode
  searches the UTF-8 bytes of the supplied text. Search streams through the
  complete file and wraps at EOF.
- Reload without closing the tab.
- Viewer tabs are identified with a `[Hex]` suffix and are independent from
  normal editable source tabs.
- Read-only operation: the viewer never changes the source file.

The feature is enabled by default and can be removed at compile time:

```bash
cmake -S . -B build -DDODEV_ENABLE_HEX_VIEWER=OFF
```

Build-script shortcuts:

```bash
./scripts/build-linux.sh release --hex-viewer
./scripts/build-linux.sh release --no-hex-viewer
./scripts/build-windows-mingw.sh release --hex-viewer
./scripts/build-windows-mingw.sh release --no-hex-viewer
```


## HTTP + WebSocket Editor Server plugin

A standalone plugin is provided in `plugins/http_editor_server`. It starts a small cross-platform HTTP + RFC 6455 WebSocket server implemented with native sockets only. Every open editor is represented as a room/channel, multiple clients can subscribe concurrently, and network worker threads only read cached snapshots rather than wxWidgets controls. Default bind is `0.0.0.0:9934`; configuration lives in `dodev_http_editor_server.json` beside the plugin. See `plugins/http_editor_server/API.md` for the versioned endpoint and message nomenclature.

The normal build now includes it automatically whenever bundled plugins are enabled:

```bash
./scripts/build-linux.sh release
```

It can still be controlled individually with `--http-editor-server-plugin` /
`--no-http-editor-server-plugin`, or by CMake
`-DDODEV_BUILD_HTTP_EDITOR_SERVER_PLUGIN=ON`. The primary API is versioned under
`/api/v1/...` and `/ws/v1/...`; legacy snapshot/SSE paths remain compatibility aliases.

On every successful server start/reload, every registered REST/SSE/WebSocket endpoint is written as a separate line to DoDevEditor's bottom **Logs** tab and to the existing wx log output. HTTP requests and WebSocket send/receive message metadata are also logged. The authentication token and source-text payload are never duplicated into log lines.

### Plugin settings and HTTP token

**General Settings -> Plugins** now centralizes runtime plugin configuration:

- **Plugin folder**: directory scanned for `.so`/`.dll` plugins. Changing it
  unloads the current plugins and reloads from the new directory when settings
  are saved.
- **Runtime plugins table**: every discovered native plugin is listed with an
  Enabled checkbox, runtime status and library filename. Unchecking a plugin
  keeps its `.so`/`.dll` installed but adds its basename to `plugins.disabled`
  in `config.json`, so the loader skips it before `dlopen`/`LoadLibrary`.
  **Apply Plugin Changes** unloads/reloads the plugin set immediately without
  restarting DoDevEditor. **Refresh List** rescans the configured folder.
- **HTTP/WebSocket Editor Server bind address**: defaults to `0.0.0.0`.
- **HTTP/WebSocket Editor Server port**: defaults to `9934`.
- **Authentication token**: masked settings field. Blank disables HTTP/WebSocket
  authentication.
- **Maximum editor text**: controls the largest editor snapshot exposed by the
  HTTP/WebSocket plugin.

These HTTP/WebSocket values are persisted in `config.json` under `plugins.http_editor_server`
and passed to the plugin through the existing `DODEV_HTTP_*` runtime overrides.
Saving General Settings reloads plugins so changed token/bind/port settings take
effect immediately.

Example persisted configuration:

```json
{
  "plugins": {
    "directory": "/home/user/.local/share/dodev/plugins",
    "disabled": [
      "dodev_example_plugin.so"
    ],
    "http_editor_server": {
      "bind_address": "0.0.0.0",
      "port": 9934,
      "auth_token": "change-me",
      "max_text_bytes": 4194304
    }
  }
}
```

When a token is configured, clients can use either:

```bash
curl -H "Authorization: Bearer change-me" \
  http://127.0.0.1:9934/api/v1/editors/active
```

or `X-DoDev-Token: change-me`. Because the token is intentionally persistent,
it is stored in the editor configuration as plain text; use filesystem
permissions appropriate for your environment.
