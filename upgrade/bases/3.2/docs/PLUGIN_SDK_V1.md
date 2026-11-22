# Native Plugin SDK v1 — Public contract

**Header:** `include/dodev/sdk/dodev_plugin_api.h`.

The plugin binary interface is C-compatible; the main host and bundled example use C++17. Each plugin has a `plugin.json` manifest (`id`, `name`, `version`, `apiVersion=1`, and platform-specific `entry`) and exports:

```cpp
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(
    const DodevHostAPI* host, DodevPluginAPI* plugin);
```

Return `1` on successful activation. The plugin should check `host->api_version`, `host->struct_size`, and `plugin->struct_size` before accessing their fields. The `plugin->shutdown` callback is optional. The SDK's C-compatible structs expose no standard-library or Raven ABI types.

## Host contribution API

| Function | Purpose |
| --- | --- |
| `register_command` | Named action callable by Raven and menus |
| `register_menu` | Add menu item under File, Edit, View, Tools or Plugins |
| `register_tab` | Register on-demand tab factory |
| `register_panel` | Register on-demand dock panel factory (`Left`, `Right`, `Bottom`, `Tab`) |
| `register_template` | Register extension template |
| `register_theme_xml` | Register a Raven theme XML literal |
| `register_setting` | Declare a string setting/default/description |
| `get_setting` / `set_setting` | Read/write per-plugin JSON settings |
| `subscribe_event` | Subscribe to defined host events |
| `log` | Send diagnostic messages to host |

A tab/panel factory receives a short-lived `DodevUiBuilder` object with `label`, `button` and `read_only_text` helpers. The host creates and owns Raven widgets and handles visual themes. Do not retain the builder pointer or its context after the callback returns. More widget types can be added in API version 2.

Emitted events currently include `app.ready`, `app.shutting_down`, `document.opened`, `document.saved`, and `theme.changed`. Data-bearing events send a JSON string. Plugin event callbacks and UI construction run in the host process; do not execute blocking work on the UI thread.

## Plugin activation and settings

1. Install `<plugin-directory>/<folder>/plugin.json` and the required `.so` or `.dll` in that folder. Manifest `entry` must be a relative path and cannot traverse `..`.
2. The application discovers manifests, but **does not load native code by default**.
3. Select the plugin under **Settings → Plugins**, enable it and restart. Safe mode overrides per-plugin enablement.
4. The plugin initializes and registers contributions. Clicking menu entries or opening a plugin tab invokes registered callbacks.
5. A plugin's settings are stored under `<config>/plugins/<plugin-id>/settings.json` (not next to the plugin library); missing settings use plugin-defined defaults.
6. On shutdown, the host drops registered callbacks and executes the plugin's `shutdown` callback before unloading the native library.

### Build and install the bundled example

```bash
cmake -S . -B build-core -DDODEV_BUILD_GUI=OFF \
  -DDODEV_BUILD_PLUGIN_EXAMPLES=ON
cmake --build build-core --parallel
# Sample manifest and libhello_plugin.so are in build-core/plugin-examples/hello/
mkdir -p "${XDG_CONFIG_HOME:-$HOME/.config}/DoDevEditor/plugins/hello"
cp build-core/plugin-examples/hello/{plugin.json,libhello_plugin.so} \
   "${XDG_CONFIG_HOME:-$HOME/.config}/DoDevEditor/plugins/hello/"
```

On Windows, copy `hello_plugin.dll` alongside `plugin.json`, then enable it through the same Settings UI.

### Limitations and safety

Plugins are **trusted in-process native code**, not sandboxed. Do not enable an untrusted binary. A malicious or crashing plugin may terminate or compromise the editor. Current API supports dynamically registered menu items, on-demand tabs and dock panels, and string-valued settings; it does not yet support hot-unloading on disable, transactional rollback of all contributions from a failed plugin, language servers, compiler debugging interfaces, or arbitrary Raven widget trees. Activation/deactivation is intended to take effect on restart.

The bundled `plugins/examples/HelloPlugin/HelloPlugin.cpp` is the canonical runnable example. `tests/test_plugin.cpp` verifies the example plugin can be discovered, explicitly enabled, loaded, invoked, configured and shut down in the headless core build.
