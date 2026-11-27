# DoDevEditor 5.2.6 — Git and Journal workspace views

## Activation

The native plugins remain opt-in. In Settings > Plugins enable Git History and Remote Journal (SSH), apply, restart, and open a project. The registered Workspace pages are automatically present; their factories execute lazily on tab selection. The Files, Search, Symbols and Calls tabs are unchanged in purpose. There is no separate legacy text-only Git history tab.

## Git

Workspace > Git shows a report table containing subject, full hash, committer and commit date. Choose a row (mouse or keyboard) to populate the single reusable Output > Git commit details tab. It includes the source workspace, full hash, parents, author and committer identities/dates, complete commit message and changed-file status. The `showPatch` plugin setting controls the patch (true by default for newly initialized settings; existing explicit choices are preserved). Refresh history reloads the current HEAD history. History is capped by the plugin `historyLimit` setting (1–1000). The queries remain synchronous, bounded by the existing process timeout/output limit. A workspace change recreates Workspace views; previous Output content remains available and is labeled with its source workspace until another commit is selected.

## Journal Logs

Workspace > Journal Logs contains an inline JSON editor for project profiles. Use Save profiles and then Connect / Start capture. Choose the saved profile or a manual connection in the login dialog. Save validates the JSON and profile names and checks for external edits before overwriting. Reload discards unsaved edits only after confirmation; switching projects and closing the application also protect unsaved profile edits. Password authentication continues to prompt at connection time; do not enter credentials into profiles.json.

Configuration file: `.dodev/plugins/com.dodev.remotejournal/profiles.json`.

Example:

```json
{"profiles":[{"name":"Development","host":"server.example.com","user":"developer","port":22,"unit":"","lines":200,"authentication":"password","follow":true,"sudoNonInteractive":false,"filterSetFile":".dodev/plugins/com.dodev.remotejournal/development-filters.json"}]}
```

Profiles can override global defaults such as `applicationId`, `payload`, `payloadRegex`, `fromUtc`, `toUtc`, `tableForeground`, and `tableBackground`. The existing N-rule engine, include/exclude/marker behavior and export remain intact. Each capture opens in the editor notebook as its own Journal tab regardless of an obsolete `tabLocation` setting. Existing captures are not moved or disconnected by changing profiles.

## Generic SDK

The additive `DodevUiBuilder::workspace_profiles` callback renders and persists a plugin-supplied profiles JSON document in the calling registered Workspace view. A plugin must check `struct_size` before calling this optional function; older hosts use the existing `open_workspace_config` fallback. Registration via `register_panel(..., "Workspace", ...)` requests an automatically discoverable Workspace tab. No plugin-specific fields are introduced into the host settings renderer. Global settings remain in plugin-owned Settings pages.

## Validation and boundaries

Headless integration tests use the native plugin manager and a temporary real Git repository with two commits. They verify table rows, successive selection callbacks into the same Output ID, complete commit bodies, changed files, and patch output. Journal tests verify profile registration/schema, validation, removed placement option and the existing multi-filter model. The wxWidgets GUI and live SSH were not compiled or exercised in the development environment. The Journal capture GUI still uses its legacy host adapter; this release does not claim complete dynamic plugin independence.
