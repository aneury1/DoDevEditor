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
Each root can therefore keep its own repository and optional `.dodev/llvm.json` compiler configuration.

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

DoDevEditor includes a dependency-free source-analysis framework written in C++17. It does **not** use LLVM, libclang, tree-sitter, an LSP server, or another parser library. The first registered languages are:

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

F12 reparses the current unsaved buffer with the manual parser first. For C/C++, libclang can still be used as a semantic fallback when the optional LLVM feature is enabled.

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

When disabled, `src/symbols/ManualSymbolParser.cpp` is excluded from the target and `DODEV_ENABLE_MANUAL_SYMBOLS=0` removes the manual integration path from the Symbols panel. The existing optional LLVM tooling remains independent.

## Optional LLVM / Clang C/C++ analysis

DoDevEditor can optionally link against **libclang** (`clang-c/Index.h`) for
C/C++ source navigation and AST-based inspection. This dependency is optional:
the editor still configures and builds when libclang is not installed.

### Linux build modes

Normal build: auto-detect libclang and enable the feature when available:

```bash
./scripts/build-linux.sh release --clean
```

Require LLVM/libclang support (configuration fails if it cannot be found):

```bash
./scripts/build-linux.sh release --clean --llvm
```

Explicitly build without libclang:

```bash
./scripts/build-linux.sh release --clean --no-llvm
```

On Arch Linux the LLVM-enabled build can be prepared with:

```bash
sudo pacman -S --needed llvm clang
./scripts/build-linux.sh release --clean --llvm
```

The command-line **Check** and **Compile** actions only require `clang` /
`clang++` on `PATH`; they still work when the editor itself was built without
libclang. The manual parser supplies normal symbols, static callers/callees and lightweight F12 navigation without libclang. libclang remains useful for compiler-grade AST diagnostics and semantic definition resolution.

### Analysis sidebar tab

The Explorer sidebar has a `SYMBOLS` tab. When the manual parser is enabled it supports C, C++ and Kotlin; LLVM augments C/C++ when available. The tab has three views:

- **Symbols** — manual C/C++/Kotlin symbols by default; libclang symbols are used as a fallback when the manual parser is unavailable/disabled.
- **Call Hierarchy** — dependency-free callers/callees for the current file, or the legacy libclang call tree when only LLVM analysis is active.
- **Analysis** — manual parser summary plus optional libclang diagnostics and `clang` / `clang++` compile output.

Controls in this tab:

- `Manual` checkbox — quick master switch for the built-in parser (also persisted to the global runtime settings).
- `LLVM` checkbox — enables/disables libclang AST analysis for the current session/project.
- `Parse` — reparses the current C/C++/Kotlin editor buffer, including unsaved text.
- `Check` — saves the current file and runs Clang with `-fsyntax-only`.
- `Compile` — saves the current file and compiles one source translation unit
  to an object under `.dodev/llvm-obj/`.
- `Standard` — `c++17`, `c++20`, `c++23`, `c17`, or `c11`.
- `Defines` — semicolon-separated preprocessor definitions, for example
  `DEBUG;PLATFORM_LINUX=1;APP_NAME=\"DoDevEditor\"`.
- `Includes` — semicolon-separated include directories. Relative paths are
  resolved from the opened project root.
- `Extra args` — semicolon-separated additional Clang command-line arguments.
- `LLVM Auto` — automatically reparses C/C++ through libclang when switching files/tabs. Manual auto-parse is controlled in General Settings -> Code Analysis.
- `Save cfg` — saves these settings for the project.

DoDevEditor also automatically supplies the current source directory, project
root, and existing `include/`, `includes/`, and `src/` directories to Clang.

### Go to Definition

Press:

```text
F12
```

or use:

```text
Code -> Go to Definition
```

DoDevEditor first reparses the current in-memory buffer with the manual parser and uses its call/symbol index. For C/C++, if the manual result cannot resolve the symbol and libclang is enabled, the editor falls back to libclang's semantic cursor/definition lookup.

### Compile current source with Clang/LLVM

Use:

```text
Code -> LLVM Syntax Check
Code -> Compile Current Source with LLVM
```

or:

```text
Ctrl+F7
```

for object compilation. This is intentionally **per-source compilation**, not a
replacement for the project's CMake link step. For example, a `.cpp` file is
compiled approximately as:

```bash
clang++ -std=c++17 \
  -DDEBUG \
  -DPLATFORM_LINUX=1 \
  -Iinclude \
  -Wall -Wextra \
  -c src/main.cpp \
  -o .dodev/llvm-obj/main_<path-hash>.o
```

Headers are checked with `-fsyntax-only` instead of producing an object file.
C sources use `clang` and a C standard; C++ sources use `clang++` and a C++
standard.

### Project configuration

`Save cfg` writes:

```text
.dodev/llvm.json
```

Example:

```json
{
  "enabled": true,
  "autoParse": true,
  "standard": "c++20",
  "defines": [
    "DEBUG",
    "PLATFORM_LINUX=1"
  ],
  "includeDirs": [
    "include",
    "thirdparty/mylib/include"
  ],
  "extraArgs": [
    "-Wno-unused-parameter"
  ]
}
```

Generated `.dodev/llvm-obj/` files are ignored by Git, while `llvm.json` can be
committed if the analysis configuration should be shared with the project.

### Current scope

The call tree is translation-unit based: it shows calls discovered while
parsing the current source file. It is not yet a whole-project reverse-call
index. Correct parsing and definition resolution depend on supplying the same
important `-D`, `-I`, language-standard, and other compile arguments used by
the real project build.

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
- dependency-free C/C++/Kotlin symbols and static call hierarchy, plus LLVM/libclang diagnostics for active C/C++ when enabled.

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
key/API endpoint. The normal Current File, Selection, Git Diff and LLVM context
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

## C/C++ clang-format context menu

Right-click an open C/C++ source/header tab to access:

- **Format Document (clang-format)**
- **Format Selection (clang-format)** when text is selected

Formatting runs against the current in-memory editor buffer. The editor passes the real source path using `--assume-filename`, so `clang-format --style=file` can discover the nearest `.clang-format` / `_clang-format` configuration. If no project format file exists, LLVM style is used as the fallback.

Formatting does **not** save the document. The formatted text remains a normal modified editor buffer and can be reverted with Undo or saved through the normal Local History / Git workflow.

Runtime requirement:

```bash
clang-format --version
```

On Arch Linux it is normally supplied by the `clang` package.

## File menu: Recent Files and Recent Folders

The **File** menu contains two persistent IDE-style history submenus:

- **Recent Files** — up to 15 recently opened/saved files, newest first.
- **Recent Folders** — up to 15 recently opened workspace folders, newest first.

Entries are stored in `config.json` as `recent_files` and `recent_folders`. Reopening an entry moves it to the top. Missing paths are pruned automatically when the menu refreshes. Each submenu includes a **Clear Recent Files/Folders** action. The first nine entries also have numbered menu mnemonics for quicker keyboard access.


## Journal Log Inspector (optional)

DoDevEditor can be built with an optional cross-platform Journal Log Inspector. It adds **Tools → SSH Journal Logs...** and **Tools → Import Journal Logs...**.

The SSH tab launches the local OpenSSH client and runs `journalctl -o json --no-pager` on the remote Linux/systemd host. Controls include host, port, user, private-key path, optional systemd unit/application, **Follow (`-f`)**, **Current boot (`-b`)**, and a **Since** value such as `2026-08-16 08:00:00` or `-2 hours`. Authentication is intentionally delegated to normal OpenSSH keys, agent, and `~/.ssh/config`; passwords are not stored by the editor.

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

DoDevEditor can load native plugins from the `plugins/` directory beside the
executable. The feature is enabled by default and can be compiled out:

```bash
./scripts/build-linux.sh release --plugins
./scripts/build-linux.sh release --no-plugins
```

or directly with CMake:

```bash
-DDODEV_ENABLE_PLUGINS=ON
-DDODEV_ENABLE_PLUGINS=OFF
```

The ABI lives in `includes/plugin/DoDevPluginAPI.h`. A basic plugin does not
need wxWidgets; it talks to the editor through C function pointers. Plugins can
add menu commands, inspect or modify the active editor, enumerate/select/close
open tabs, open files, create editor tabs, access the file/Git/Symbols/bottom
panels, create host-owned text panels, and subscribe to editor/tab/workspace
events.

The editor adds a **Plugins** menu with **Reload Plugins**, **Loaded Plugins...**
and **Open Plugins Folder**. Reload safely removes plugin-owned menu commands
and panels before unloading the shared library.

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
