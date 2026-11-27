# Simple Module — native plugin tutorial

One C++ source file demonstrates four extension points: Options menu, Settings page, editor tab and Output panel. The plugin is a normal dynamically loaded SDK v1 shared library. It contains no wxWidgets dependencies. Requires the DoDevEditor 5.2.3 host update: 5.2.2 rejects the Options menu and routes generic views to the sidebar.

## Build independently

The standalone archive includes the two SDK headers. In the editor tree the CMake project instead finds the editor's include directory.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

For an external SDK, pass `-DDODEV_SDK_INCLUDE=/path/to/DoDevEditor/include`.

Copy **both** the library and manifest (the entire `build/plugins/simple-module` folder) into the directory configured in Settings > General/Plugins (default `~/.config/DoDevEditor/plugins` on Linux). Alternatively, `cmake --install build --prefix /usr/local` with appropriate permissions installs the plugin next to the system-installed executable. Match the installed editor's prefix.

Open Settings > Plugins, enable Simple Module, save and restart. Options now contains three entries. Settings contains a Simple Module page. Change Greeting message and Severity, save; open Options > Simple Module: Open editor tab or Open output panel. Both views have buttons to demonstrate command dispatch and cross-view navigation. Reopening an existing view selects it; close and reopen to refresh its snapshot of the settings.

## Study the source

1. `sayHello`: registered command callback, reads settings at execution time.
2. `editorView`: factory called by `register_tab`; creates a label, read-only text and buttons.
3. `outputView`: factory called by `register_panel(..., "Bottom", ...)`; reads both settings and creates selectable sample output.
4. `shutdown`: releases resources (none in this example).
5. `dodev_plugin_init`: validates the API, declares settings schema, registers command, views and menu entries. Each return value is checked.
6. `plugin.json`: stable ID, display name and platform-specific library name.

`register_menu` can target a registered command or `plugin.view.<view-id>` host action. The host builds the settings page from `register_setting_ex` and persists values in `plugins/<plugin-id>/settings.json`. Do not store the UI builder/context after its factory returns; copy returned settings strings before another SDK call. Use unique IDs for each plugin. The example doesn't write any global editor settings.

## Scope

This is a small teaching plugin, not a live logging framework. The current builder exposes labels, buttons and read-only text; there is no live append/update or arbitrary native widget factory in this ABI. Existing plugin-specific graphical adapters are not migrated by this release. A host upgrade is required once for the generic placement and Options support; afterwards this example builds independently.
