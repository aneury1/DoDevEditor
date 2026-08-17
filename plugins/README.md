# DoDevEditor Plugin SDK

Plugins are native shared libraries loaded from the `plugins` directory beside
DoDevEditor (`.so` on Linux, `.dll` on Windows). The public ABI is:

    includes/plugin/DoDevPluginAPI.h

A normal plugin only needs that header and does not need wxWidgets. It can:

- add/remove menu commands;
- enumerate/select/close open tabs;
- access the active tab and active source editor;
- read/replace/insert editor text;
- read/update caret and selection;
- open files or create new source tabs;
- obtain opaque handles for built-in panels/notebooks;
- create host-owned text panels in the sidebar, bottom area, or editor area;
- receive active-tab/editor/workspace events.

For completely custom wxWidgets UI, `add_custom_panel()` passes the notebook
parent as an opaque `wxWindow*`. That advanced mode requires a plugin built with
an ABI-compatible wxWidgets/toolchain.

## Standalone example build

Linux:

    cmake -S plugins/example_plugin -B build/example-plugin \
      -DDODEV_SDK_ROOT="$PWD"
    cmake --build build/example-plugin
    cp build/example-plugin/dodev_example_plugin.so /path/to/DoDevEditor/bin/plugins/

Windows (native or MinGW): build the same CMake project and copy
`dodev_example_plugin.dll` beside `DoDevEditor.exe` under `plugins/`.

You can also build it together with the editor using:

    -DDODEV_BUILD_EXAMPLE_PLUGIN=ON


## HTTP + WebSocket Editor Server

`plugins/http_editor_server` is a dependency-free REST/SSE/WebSocket plugin. It exposes every open editor as a file room/channel, supports multiple concurrent WebSocket clients, and defaults to `0.0.0.0:9934`. The WebSocket implementation uses native sockets only; no third-party HTTP/WebSocket library is linked. See `plugins/http_editor_server/API.md` for the versioned `/api/v1/...` and `/ws/v1/...` protocol. Build it standalone or enable `DODEV_BUILD_HTTP_EDITOR_SERVER_PLUGIN=ON` to copy it beside the editor automatically.


## Building all bundled plugins with DoDevEditor

The general Linux and Windows build wrappers build every plugin shipped under
`plugins/` when plugin support is enabled. The resulting shared libraries are
deployed to the runtime plugin directory automatically.

    ./scripts/build-linux.sh release --plugins
    ./scripts/build-linux.sh release --plugin-dir custom_plugins

Direct CMake builds can use `DODEV_BUILD_BUNDLED_PLUGINS=ON` and
`DODEV_PLUGIN_OUTPUT_DIR=<path>`.
