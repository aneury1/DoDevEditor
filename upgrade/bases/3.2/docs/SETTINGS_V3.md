# DoDevEditor 3.0 — Settings specification

The application uses `SettingsManager` (`include/dodev/settings/SettingsManager.h`) with a built-in C++ JSON default object. The configuration directory is selected using `DODEV_CONFIG_DIR`, then platform defaults (`XDG_CONFIG_HOME` / `~/.config` on Linux; `%APPDATA%` on Windows; `~/Library/Application Support` on macOS).

No external configuration file is required for application startup. If `settings.json` is absent, default values are used and legacy `settings.ini` keys `theme` and `folder` are optionally imported. Missing font files fall back to Raven's available default font. Missing custom theme, template or plugin directories are ignored.

If `settings.json` is syntactically malformed, the user-provided file remains untouched; defaults are used in memory and writes to this malformed file are refused until it is repaired or renamed. Individual invalid known values are replaced with defaults, and numeric settings are clamped to supported ranges. Unknown JSON keys are preserved. The serializer writes to a temporary file and renames it (the platform's rename semantics apply).

## Settings UI

Access **Settings** via the menu or toolbar. The UI is embedded in `resources/EmbeddedSettingsUi.h`; it has tabs for General, Editor & Fonts, Appearance, Terminal, Templates and Plugins. **Apply / Save** copies values into SettingsManager and persists JSON. **Restore defaults** uses the built-in baseline; **Cancel** dismisses unsaved changes. Changing native plugin activation requires restart.

| Setting key | Default | Notes |
| --- | --- | --- |
| `schemaVersion` | `1` | Current schema |
| `general.lastFolder` | `""` | Last workspace path |
| `general.restoreSession` | `true` | Session preference |
| `editor.fontFile` | `""` | Optional `.ttf`/`.otf` font path |
| `editor.fontSize` | `14` | 8–64 |
| `editor.tabSize` | `4` | 1–16 |
| `editor.wordWrap` | `false` | Editor wrapping |
| `editor.lineNumbers` | `true` | Editor gutter |
| `editor.insertSpaces` | `true` | Soft tabs |
| `appearance.theme` | `Music Nocturne` | Embedded or custom theme name |
| `appearance.uiFontFile` | `""` | Optional UI font path |
| `appearance.uiFontSize` | `13` | 9–36 |
| `appearance.customThemesDirectory` | `""` | Defaults to `<config>/themes` |
| `appearance.preferredThemeFormat` | `json` | `json` or `xml` |
| `terminal.fontFile` | `""` | Optional terminal font path |
| `terminal.fontSize` | `14` | 8–48 |
| `terminal.defaultShell` | `""` | Use system shell when empty |
| `terminal.scrollbackLines` | `3000` | 100–200000 |
| `templates.directory` | `""` | Defaults to `<config>/templates` |
| `templates.associations` | mapping | Extension → template ID |
| `plugins.enabled` | `true` | Global plugin loader flag |
| `plugins.safeMode` | `false` | Suppresses native plugin execution |
| `plugins.directory` | `""` | Defaults to `<config>/plugins` |
| `plugins.enabledIds` | `[]` | Explicit opt-in plugin IDs |
| `plugins.disabled` | `[]` | Blocklist |

`editor.fontFamily` and `terminal.fontFamily` exist in the JSON schema but the implemented user interface loads fonts from file paths; these family-name fields are reserved for future font family enumeration.

Plugin-specific settings live in `<config>/plugins/<plugin-id>/settings.json`, regardless of the location of the plugin's executable. They are optional and separately persisted from application settings.

## Caveats

General Settings currently exposes a **Plugins** tab, not an installed-extension marketplace. Configuration changes applied to dynamically loaded native plugins take effect on the next restart. The existing editor and terminal stay available without plugins.
