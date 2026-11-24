# GDB debugger plugin — DoDevEditor 4.1.0

Native C++17 plugin: `com.dodev.gdb`. Uses GDB's interactive CLI in its own PTY/ConPTY, not Microsoft's C++ extension, DAP or an MI state machine. Requires GDB with embedded Python support (`gdb --configuration` / `gdb -batch -ex 'python print(1)'`). Install GDB and a C++ compiler; on Windows use a compatible GDB build with Python. Only enable trusted native plugins and launch configurations: tasks and setup commands execute local code.

## Installation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "$(nproc)"
cmake --install build --prefix "$HOME/.local"
```

Enable **GDB / C++ Run and Debug** in Settings > Plugins and restart. The native module and manifest are installed under `bin/plugins/gdb-debugger`. Start the editor normally with `dodeveditor .`. Detached application startup from 4.0.1 is preserved.

## Workflow

1. Open a project folder. Choose Run > GDB: Open launch.json to create or edit `.vscode/launch.json`; existing files are not replaced. Save before launching.
2. Set `program` to the executable built with debug symbols (e.g. CMake Debug, `-g -O0`). Set `cwd`, `args`, and `miDebuggerPath` as required.
3. Run > GDB: Select Configuration selects by exact profile name; selection is a plugin preference and falls back to the first GDB profile if absent in another workspace.
4. Open Run > Run and Debug, or press Ctrl+Shift+D. Press F9 on a source line to persist a breakpoint. Press F5 to start or continue. Output, locals, threads, stack, and evaluation results appear in the separate GDB Debug terminal.
5. Pause a running program before stepping or inspecting. The prompt guard only sends panel commands when the GDB CLI prompt is available. The console remains interactive for direct commands and program input. Do not change the `(dodev-gdb)` prompt, or use GDB TUI mode when using panel controls.
6. Use Shift+F5 to stop (interrupt first when running, then quit at the next debugger prompt). For a local launch, restart runs the existing program again without rebuilding. Stop and launch again to rerun preLaunchTask and reload changed settings. Close the debug session before closing the editor; forcibly closing a terminal is not a graceful GDB detach.

## VSCode configuration compatibility

Reads JSONC (line/block comments and trailing commas). Supports `type: cppdbg`, `MIMode: gdb`, `name`, `request: launch|attach`, `program`, `args`, `cwd`, `environment`, `stopAtEntry`, `miDebuggerPath`, `externalConsole: false`, `setupCommands` with `ignoreFailures`, `preLaunchTask`, `processId` (numeric PID or `${command:pickProcess}`, which prompts for a PID), `miDebuggerServerAddress` (existing gdbserver, `target remote`), `stopAtConnect`, `sourceFileMap` (string-to-string substitution), and `linux`, `windows`, `osx` overrides. No special VSCode extension is required. `stopAtEntry` uses GDB's `start` command for local launches (main function); remote connections use stop/continue behavior. Attach requires OS permission. GDB must have Python support for bootstrap and setup ordering.

Variables: `${workspaceFolder}`, `${workspaceRoot}`, `${workspaceFolderBasename}`, `${file}`, `${fileDirname}`, `${fileBasename}`, `${fileBasenameNoExtension}`, `${fileExtname}`, `${relativeFile}`, `${relativeFileDirname}`, `${cwd}`, `${pathSeparator}`, `${/}`, `${env:NAME}`. Variables referencing active files need a saved active document. Unsupported variable forms produce an error. Multi-root, input variables and extension-specific commands are not implemented.

Pre-launch tasks: supports single `process`, `cppbuild` and simple foreground `shell` tasks in `.vscode/tasks.json` with `command`, `args`, `options.cwd`, `options.env` and platform overrides. Process/cppbuild use literal argv; shell tasks execute the configured trusted shell command (Windows shell task arguments are rejected; use process tasks). Failed builds stop automatic launch; no dependency graphs, background/watch tasks, custom shells, problem matcher UI, or compound launch configurations. Environment and configuration values may be visible in the debugger process command line; don't put secrets in these profiles.

Unsupported explicit options include externalConsole=true, miDebuggerArgs, pipeTransport, debugServerPath, coreDumpPath, visualizerFile, logging, customLaunchSetupCommands, launchCompleteCommand and postDebugTask; a descriptive error is provided. This is not complete VSCode debug-adapter parity. There are no editor-gutter glyphs, automatic source navigation on stop, structured watch/variable trees, or MI/DAP event synchronization. F9 stores breakpoints in `.vscode/dodev-breakpoints.json`; when stopped it attempts to update live breakpoints, otherwise changes are applied on a new launch. GDB can be used to add conditional breakpoints, watchpoints and registers manually. The CLI prompt heuristic is intended for trusted programs and ordinary GDB output, not an isolated machine protocol.

## Shortcuts

| Shortcut | Command |
|---|---|
| Ctrl+Shift+D | Run and Debug sidebar |
| F5 | Start / Continue |
| F9 | Toggle source breakpoint |
| F10 | Step over |
| F11 | Step into |
| Shift+F11 | Step out |
| Shift+F5 | Stop |
| Ctrl+Shift+F5 | Restart local launch |

## Sample workspace

Open `thirdparty/GdbDebugger/examples`. Configure once with:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

On Windows use a single-configuration generator (e.g. Ninja or MinGW Makefiles) with GDB and the matching compiler. Then select `C++: GDB Launch`. F5 builds the example and stops at main. Try `info locals`, `print result`, `backtrace`, `info threads`, or the panel buttons.

## Headless verification

```bash
cmake -S . -B build-core -DDODEV_BUILD_GUI=OFF -DDODEV_BUILD_QRCODE_PLUGIN=OFF
cmake --build build-core --target dodev_gdb_tests dodev_terminal_tests -j4
ctest --test-dir build-core -R 'gdb_configuration|terminal_pty_interactive' --output-on-failure
```

These tests exercise JSONC, profile resolution, variable expansion, preLaunchTask generation, persisted breakpoints, attach PID validation, prompt gating, plugin registration/commands with mock host hooks, and literal direct PTY execution. Native plugin and headless Linux targets were compiled; full Raven GUI, real GDB inferior execution, and Windows ConPTY runtime were not verified in the build environment.
