# Bundled QR Code plugin — build, install and logging

DoDevEditor 3.4 includes the complete offline QR Code extension in
`thirdparty/QRCode/`. No separate QR plugin archive, `FetchContent` call, or
manual copy of the QR plugin is required. Raven GUI is still fetched from
GitHub by the main `CMakeLists.txt` when `DODEV_BUILD_GUI=ON`.

## Files and build targets

| Source | Description |
|---|---|
| `thirdparty/QRCode/src/QRMatrix.cpp` | QR encoding and PNG/SVG rendering |
| `thirdparty/QRCode/src/QRCodePlugin.cpp` | DoDevEditor SDK v1 integration |
| `thirdparty/QRCode/include/QRMatrix.h` | QR internal types |
| `thirdparty/QRCode/plugin.json` | Native module manifest |
| `thirdparty/QRCode/CMakeLists.txt` | Build/install target |
| `src/plugins/PluginPaths.cpp` | Resolves bundled plugin directory from executable path |
| `src/plugins/PluginManager.cpp` | Discovers user + bundled plugins, logs actual paths/status |

Targets: `dodev_qr_core` (static library) and `dodev_qrcode_plugin`
(native shared module). C++17; no external QR encoding dependency.

## Build and install (Linux)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build
# With Unix Makefiles you can alternatively run: make -C build install
```

**Typical output:**

```text
-- [DoDevEditor][QR] Bundled source: .../thirdparty/QRCode
-- [DoDevEditor][QR] Build output: .../build/plugins/qrcode
[DoDevEditor][QR][build] Binary: .../build/plugins/qrcode/libdodev_qrcode_plugin.so
[DoDevEditor][QR][build] Manifest: .../build/plugins/qrcode/plugin.json
-- Installing: /home/user/.local/bin/plugins/qrcode/libdodev_qrcode_plugin.so
-- Installing: /home/user/.local/bin/plugins/qrcode/plugin.json
-- [DoDevEditor][QR][install] Plugin directory: /home/user/.local/bin/plugins/qrcode
```

On Windows the library is `dodev_qrcode_plugin.dll`; on macOS
`libdodev_qrcode_plugin.dylib`. The manifest contains the platform-specific
filename. All platforms install under `<prefix>/<bindir>/plugins/qrcode`.
The built module is also available at `build/plugins/qrcode`.

### Where is it installed?

The final location is controlled by `CMAKE_INSTALL_PREFIX` and
`CMAKE_INSTALL_BINDIR`. For example:

```sh
cmake --install build --prefix /opt/dodev
ls -lh /opt/dodev/bin/plugins/qrcode
```

The plugin is **not** installed in a generic system-wide plugin directory.
It is installed next to the editor executable inside `bin/plugins/`, so both
installed and portable application layouts can locate it.

To verify without installing the GUI or fetching Raven, use:

```sh
cmake -S . -B build-headless -DDODEV_BUILD_GUI=OFF \
  -DDODEV_BUILD_QRCODE_PLUGIN=ON -DDODEV_BUILD_TESTS=ON
cmake --build build-headless --parallel
ctest --test-dir build-headless --output-on-failure
cmake --install build-headless --prefix /tmp/dodev-test-install
ls /tmp/dodev-test-install/bin/plugins/qrcode
```

## Runtime discovery and activation

On startup, DoDevEditor searches **two locations** in priority order:

1. User plugin root, default `$XDG_CONFIG_HOME/DoDevEditor/plugins`
   (typically `~/.config/DoDevEditor/plugins` on Linux), or the custom
   `plugins.directory` from `settings.json`.
2. Bundled plugin root, resolved from the executable location as
   `<executable_directory>/plugins`.

Missing directories are normal and never prevent startup. Duplicate plugin
IDs are skipped, with the user directory taking precedence.

For safety, native plugins **are not enabled automatically** just because
`make install` has installed their `.so`/`.dll`. Open
**Settings → Plugins**, select **QR Code Generator**, choose **Enable**, then
restart DoDevEditor. The editor stores the opt-in in its optional
`settings.json`, under `plugins.enabledIds`:

```json
{
  "plugins": {
    "enabled": true,
    "safeMode": false,
    "enabledIds": ["com.dodev.qrcode"],
    "disabled": []
  }
}
```

Merge the fragment into your existing `settings.json` rather than replacing
other settings. Settings → Plugins is the preferred method.

After activation, use **Tools → QR Code: Generate from Editor**, or the
**QR Code Generator** plugin tab. It exports the full active document to
PNG/SVG, entirely offline. Plugin-specific settings are stored in
`<config>/plugins/com.dodev.qrcode/settings.json`.

## Runtime terminal logs

Start the editor from an OS terminal:

```sh
"$HOME/.local/bin/DoDevEditor" 2>&1 | tee dodev.log
```

Example startup log:

```text
[DoDevEditor][Plugins] User plugin root: "/home/user/.config/DoDevEditor/plugins"
[DoDevEditor][Plugins] Installed plugin root: "/home/user/.local/bin/plugins"
[DoDevEditor][Plugins] Searching: /home/user/.local/bin/plugins
[DoDevEditor][Plugins] com.dodev.qrcode : Active | manifest=/home/user/.local/bin/plugins/qrcode/plugin.json | module=/home/user/.local/bin/plugins/qrcode/libdodev_qrcode_plugin.so
[DoDevEditor][Plugins] Activated 1 plugin(s) from 2 location(s)
[plugin com.dodev.qrcode] QR code created: /path/to/file.qr.png (...)
```

If not yet enabled, the status is:

```text
[DoDevEditor][Plugins] com.dodev.qrcode : Disabled (enable in Settings; restart to load)
```

These logs go to the **process terminal/stdout/stderr** where DoDevEditor was
launched, not to an already-running interactive shell tab inside the editor.

## Configuration switches

| CMake option | Default | Purpose |
|---|---|---|
| `DODEV_BUILD_QRCODE_PLUGIN` | `ON` | Build and install the bundled extension |
| `DODEV_BUILD_GUI` | `ON` | Build main Raven/Vulkan application |
| `DODEV_BUILD_TESTS` | `ON` | Run core + QR tests |
| `DODEV_BUILD_PLUGIN_EXAMPLES` | `ON` | Build old example plugins |
| `CMAKE_INSTALL_PREFIX` | Platform dependent | Install prefix |
| `CMAKE_INSTALL_BINDIR` | `bin` normally | Executable/plugin base directory |

Run `cmake -S . -B build -DDODEV_BUILD_QRCODE_PLUGIN=OFF` only when you
intentionally want to exclude QR from subsequent builds and installs.

## Diagnostics

- **Plugin missing in Settings:** check that `plugin.json` and `.so`/`.dll`
  exist together under `<prefix>/bin/plugins/qrcode`. Confirm that you are
  running the *installed* executable, not an old copy from another directory.
- **Disabled:** enable `com.dodev.qrcode` and restart, or check `safeMode`.
- **Failed: Library missing:** verify filename and architecture against
  `plugin.json` and host executable.
- **Wrong installed location:** pass `--prefix` to `cmake --install` and
  inspect the `[install] Plugin directory` line in terminal output.
- **No `DoDevEditor` installed with headless config:** `DODEV_BUILD_GUI=OFF`
  intentionally suppresses the GUI target. QR installation can be tested
  independently with that switch, but the editor executable is only installed
  with the full GUI build.
- **Existing user plugin with same ID:** user plugin root takes precedence.
  Rename/remove the duplicate if the bundled plugin is not selected.

## Validation scope

The QR C++ module, its manifest, `make install` / `cmake --install` layout,
and headless plugin discovery are covered by the supplied CTest cases.
Full Raven/Vulkan GUI integration requires Raven dependency access and a
supported graphical environment.
