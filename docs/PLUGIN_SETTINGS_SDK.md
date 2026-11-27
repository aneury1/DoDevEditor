# Plugin-owned settings — 5.2.2

## Ownership

The editor's Settings dialog creates a page for each loaded plugin that registers fields or actions. It obtains the page title from the plugin manifest. There is no journal page or AI settings button hardcoded into the settings dialog. The renderer is `src/gui/wx/WxPluginSettings.inc` and is independent of plugin IDs. The core settings file owns editor, appearance, syntax, terminal, templates and plugin discovery/enablement only. Each plugin's schema, default values, descriptions and choices are registered by its native module. An old plugin using `register_setting` gets a text field. Disabled/failed plugins have no page after restart. Enable/disable still applies on restart.

Settings are persisted as strings in `<config>/plugins/<plugin-id>/settings.json`, preserving the existing v1 ABI. Structured values such as AI provider profiles remain separate plugin-owned JSON values. The generic Apply action writes changed registered settings as a batch per plugin, validating every field before writing. This is not a transaction across all plugins and core preferences. Restore defaults resets the draft values of registered fields; Apply/Save commits them. Existing tool sessions keep their initial settings; reopen them to use new defaults. Custom plugin settings actions may save their own changes immediately; the settings dialog warns before opening them and reloads that page afterward.

## SDK

The C ABI is still v1, with additive fields `register_setting_ex` and `register_settings_action` appended to `DodevHostAPI`. Use `DODEV_HOST_HAS` before calling the new functions. The C++ convenience header `dodev/sdk/PluginSettings.h` falls back to the legacy function for old hosts. A module supplies its own schema and can retrieve its values through `get_setting` as before.

```cpp
#include "dodev/sdk/PluginSettings.h"

// During the plugin's dodev_plugin_init callback:
if (!dodev::sdk::setting(host, "tabLocation", "output",
    "Where new sessions open",
    R"json({"label":"Open new sessions in","type":"choice",
      "choices":[{"value":"output","label":"Output panel"},
                 {"value":"editor","label":"Editor tabs"}]})json")) return 0;
if (!dodev::sdk::setting(host, "port", "22", "SSH port",
    R"({"type":"int","min":1,"max":65535})")) return 0;
// Register a command first, then optionally add a settings action for it:
// dodev::sdk::settingsAction(host, "Configure profiles", "myplugin.configure");
```

Supported field types: `text`, `multiline`, `bool` (true/false strings), `int` (inclusive min/max), `choice` (string values or objects with value/label). Unknown types, invalid defaults and duplicate setting names are rejected. Registered fields are validated on updates. Optional `legacyKey` identifies a previous core string setting; it is read only when the plugin does not already have that setting. The old key is left untouched for backward compatibility, and the migrated plugin file takes precedence. No password setting is registered for SSH; authentication mode is a preference and passwords remain session-only.

## Bundled plugins

- Remote Journal: plugin registers placement, authentication mode, host/user/port/unit, line count, follow, passwordless sudo and initial search, payload, service, UTC bounds, priority, case and auto-scroll. The connection and journal table use those values. Invalid timestamp or regex filters are reported by the table. Old `remoteJournal.tabLocation` is imported by the plugin registration and not used as a runtime setting thereafter.
- Serial Monitor: plugin owns port, baud, parity, data/stop bits, filtering and optional Polkit preference; explicit authorization remains required before using the privileged helper.
- Docker: module registers connection, global flags, build and CLI defaults; its manager continues to edit its own persisted connection settings.
- AI Assistant: plugin registers its settings action and defaults; provider profiles remain the authoritative configuration for profile-based requests, superseding legacy endpoint/model fields. Open its action, then Provider profiles.
- Source Symbols: parser, timeout, Clang executable and include/macro visibility are plugin settings.
- GDB, QR Code, ClangFormat and Hello: existing module-defined settings now appear in their own dynamically generated pages when loaded. GDB launch.json and tasks.json are workspace configuration, not global plugin settings.

## Native UI boundary

This release completes the generic settings registry/renderer, not a complete dynamic wxWidgets view ABI. The AI, Serial, Journal and Docker native view source files have moved into their respective plugin source directories. Their compatibility adapters are still compiled into the desktop executable; the existing v1 host hooks remain for native tools. Source Symbols and GDB also still use integrated host services. Disabling a module removes its registered settings/actions on restart, but not every legacy host-integrated view or feature. A future native-view SDK would require a generic lifetime-safe tab factory, GUI ABI/compiler compatibility policy, and unloading only after all plugin-created widgets are destroyed. No claim is made that all built-in view implementations can already be replaced without recompiling the editor.

## Build and checks

Use the existing modular CMake presets. The headless suite exercises the registry with dynamically loaded modules, legacy migration, rejected settings, batch validation, persistence across restart, disabled plugin visibility, legacy ABI registration and structured journal filters. GUI compilation and interactive rendering require wxWidgets development libraries and a display. The application does not use the old root journal setting after migration.
