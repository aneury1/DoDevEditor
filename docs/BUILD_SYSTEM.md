# DoDevEditor 5.1.8 — Build system

This release reorganizes CMake, not editor behavior. wxWidgets is the desktop frontend; Raven reference/upgrade files are retained but not built. Windows builds can fetch wxWidgets when no installed package is detected; Linux/macOS still prefer system packages. See [Windows build guide](WX_5_2_9_WINDOWS_BUILD.md). CMake 3.21+ and a C++17 compiler are required.

## Build file map

| File | Responsibility |
|---|---|
| `CMakeLists.txt` | Project version, build order and module inclusion only |
| `cmake/BuildOptions.cmake` | Public feature switches, language standards, default configuration |
| `cmake/ProjectHelpers.cmake` | Target-local compiler warnings, output directories, plugin filenames, test assertions |
| `cmake/Dependencies.cmake` | Threads, POSIX PTY capability test, wxWidgets discovery and interface target |
| `cmake/CoreTargets.cmake` | Launcher, editor core, services, terminal |
| `cmake/PluginTargets.cmake` | Bundled plugin subdirectories, example modules and Docker installation |
| `cmake/WxFrontend.cmake` | wxWidgets GUI target, optional GTK3 CSS |
| `cmake/WxRuntimePath.cmake` | Detected Linux library directories retained in installed executable RUNPATH |
| `cmake/InstallTargets.cmake` | Editor, launcher, Linux desktop entry |
| `cmake/TestTargets.cmake` | C++ regression targets and CTest registration |
| `cmake/ScriptTests.cmake` | Python contracts, Linux launcher/RPATH tests, AI HTTP fixture |
| `cmake/WxGuiTests.cmake` | Optional native syntax rendering test |
| `thirdparty/*/CMakeLists.txt` | Each plugin's source files, dependencies, manifest and installation |
| `CMakePresets.json` | Reproducible Ninja configure/build/test presets |

The modules use `include()` rather than creating new build subdirectories, preserving root target names, test names, and existing relative source and binary paths. All GUI dependencies propagate via `dodev_wx_dependencies`, not global flags. The GDB core now links `dodev_services` instead of compiling duplicate JSON and task source files. Static libraries are built with PIC for shared plugins. The version displayed by the launcher is generated from `PROJECT_VERSION`.

## Install build dependencies

CachyOS / Arch Linux:

```sh
sudo pacman -S --needed base-devel cmake ninja wxwidgets-gtk3 gtk3 pkgconf curl python
```

Ubuntu / Debian (distribution with wxWidgets 3.2 packages):

```sh
sudo apt install build-essential cmake ninja-build libwxgtk3.2-dev libgtk-3-dev pkg-config libcurl4-openssl-dev python3
```

GTK3 CSS, curl HTTP and Python script tests are optional. Runtime tools such as `gdb`, `clang`, `git`, `ssh`, `sshpass`, `docker` and `graphviz` are feature-specific and do not all have to be installed to compile. On older distributions, verify that CMake >= 3.21 and wxWidgets >= 3.2 are available. Use a matching compiler/architecture for a custom wxWidgets build.

Windows: wxWidgets 3.2.6 is automatically fetched and built statically if no installed compatible package is detected. To use an existing wxWidgets 3.2+ build, match the selected MSVC/MinGW compiler, architecture and Debug/Release runtime; configure with `-DwxWidgets_ROOT_DIR=...` and, if needed, `-DwxWidgets_LIB_DIR=...`. Dynamic wx DLLs must be on PATH or beside the installed executable. Windows and macOS GUI builds are not validated by this release. The current POSIX terminal includes `<pty.h>`; platforms lacking that header need a source-level terminal port, not just another library path.

## Recommended build (Ninja)

```sh
cmake --preset desktop-release
cmake --build --preset desktop-release --parallel 4
ctest --preset desktop-release
sudo cmake --install build-desktop-release --prefix /usr/local
```

Use `desktop-debug` for development or `headless-debug` for backend tests only. `headless-minimal` disables GUI, AI HTTP, QR and example plugins (including Docker); this is a diagnostic configuration, not a full IDE. Other bundled plugin cores remain enabled.

Without Ninja use ordinary CMake commands:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$HOME/.local"
```

Use a fresh build directory when transferring to another PC; do not copy CMakeCache.txt, build objects or the compile database between systems. Do not use sudo to configure or compile. Install to a system prefix only with appropriate privileges.

## Options

| Option | Default | Effect |
|---|---|---|
| `DODEV_BUILD_GUI` | ON | wxWidgets editor and desktop install |
| `DODEV_BUILD_TESTS` | ON | Regression tests |
| `DODEV_BUILD_PLUGIN_EXAMPLES` | ON | Hello, ClangFormat and Docker example modules; Docker installed |
| `DODEV_BUILD_QRCODE_PLUGIN` | ON | QR plugin and its tests |
| `DODEV_WX_GTK_THEME` | ON | GTK3 CSS when compatible development files are available |
| `DODEV_WX_SYNTAX_GUI_TEST` | OFF | Native GUI regression test; display needed |
| `DODEV_ENABLE_AI_HTTP` | ON | AI network transport when libcurl is found |
| `DODEV_ENABLE_WARNINGS` | ON | Additional compiler warnings on core targets |

If the curl linker dependencies are broken, try `-DDODEV_ENABLE_AI_HTTP=OFF` to isolate the problem; AI network functionality is disabled in that build. `DODEV_WX_GTK_THEME=OFF` isolates native GTK styling. CMake prints a summary of compiler, architecture, build type, wxWidgets library paths and actual optional feature availability.

## Diagnose a failing PC

1. Check `cmake --version`, `c++ --version`, `wx-config --version`, `wx-config --cxxflags`, `wx-config --libs core,base,aui,stc,adv`.
2. If wxWidgets is custom-installed, configure a clean build using the correct wx-config executable:

   ```sh
   cmake -S . -B build-diagnostic -DCMAKE_BUILD_TYPE=Debug \
     -DwxWidgets_CONFIG_EXECUTABLE=/usr/local/bin/wx-config
   cmake --build build-diagnostic --target DoDevEditor --parallel 2 --verbose
   ```

3. If configuration fails, inspect CMake's first error and summary. If compilation/linking fails, capture the **first error**, not just the last make failure:

   ```bash
   cmake -S . -B build-diagnostic -DCMAKE_BUILD_TYPE=Debug 2>&1 | tee configure.log
   cmake --build build-diagnostic --target DoDevEditor --parallel 2 --verbose 2>&1 | tee build.log
   ```

4. Run a fresh headless build to separate core issues from GUI/dependency issues: `cmake --preset headless-debug && cmake --build --preset headless-debug && ctest --preset headless-debug`.
5. For an installed Linux executable, check `ldd /usr/local/bin/DoDevEditor` and `readelf -d /usr/local/bin/DoDevEditor | grep -E 'RPATH|RUNPATH'`. The existing RPATH helper is retained. Missing libraries or broken SONAME symlinks still require installing/repairing wxWidgets; `sudo ldconfig` refreshes the system cache after appropriate library installation.
6. For a low-memory PC, build with `--parallel 2` or `--parallel 1`. Avoid an unbounded parallel build.

To pass CMake options through the shell script: `bash scripts/build.sh -DDODEV_WX_GTK_THEME=OFF`. The PowerShell script accepts `-CMakeArgs` and `-Headless`.

## Output layout

Linux: `build/DoDevEditor` and `build/dodeveditor`. Windows and Apple builds use `dodeveditor-cli` (with `.exe` on Windows) to avoid case-insensitive filename collisions with the GUI executable; CMake target names are unchanged. Both executables and the `plugins/` directory are kept together. Plugin suffix/prefix matches each manifest. In multi-config generators, use a separate build tree per configuration: runtime output directories are deliberately shared inside a build tree to keep plugins discoverable, so switching configurations can overwrite binaries.

## Validation scope

A headless Linux build is not a native GUI build. Review the release notes for the actual test runs. No native wxWidgets GUI or Windows/macOS build is claimed. The exact failure on the other PC was not supplied, so no claim is made that all issues on that PC are fixed.
