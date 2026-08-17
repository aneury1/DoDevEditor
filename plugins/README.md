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
