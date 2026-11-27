# DoDevEditor 5.2.7 — Journal workspace filters and SQLite export

## Layout

- Workspace / Journal Logs: edit and save SSH connection profiles as before.
- Workspace / Journal Filters: choose a capture or the workspace default rule set. Add, edit, duplicate, delete, reorder, enable/disable, import and export positive/negative/marker rules. Quick filters (payload, regex, service, application ID, UTC bounds, priority) are available when an individual capture is selected.
- Editor: live capture table, details, auto-scroll, pause, disconnect, copy, export, clear and close. The Workspace filters button selects the corresponding capture in Workspace. If the capture belongs to another workspace, reopen that workspace to edit its filters. Disconnected captures remain selectable until closed.

Each capture has an independent filter state. Workspace controls edit a copy and only commit a valid quick filter; invalid regex/time bounds leave the prior active filter in place. Rule sets may be saved or loaded as JSON. Save as workspace defaults writes `.dodev/plugins/com.dodev.remotejournal/filters.json`; new captures use it if no explicit `filterSetFile` is configured in the profile or global settings. Saving defaults persists the multi-rule set, not transient quick search fields. Changes to default rules do not change existing sessions. Save defaults explicitly before changing workspace. Existing profile-specific filter sets remain supported.

## Export

Use Export in the editor capture tab. Select visible (filtered) rows or all locally buffered rows, then TXT, CSV, or SQLite. While paused, visible exports contain the displayed snapshot; all exports include current buffered records. The SQLite option appears only if CMake detects SQLite3 development headers/libraries. No database server is necessary. For CachyOS/Arch install `sqlite`; Ubuntu/Debian install `libsqlite3-dev sqlite3`. Reconfigure and build after installing. CMake prints `Remote Journal: SQLite export enabled` or a disabled diagnostic.

SQLite creates a **new** database; existing files are not overwritten. Uses prepared statements and a transaction, staging the completed database before copying it to the destination. Includes `journal_logs` and `export_metadata`; `timestamp_us` is UTC microseconds since epoch or NULL if unknown. Unknown priority is -1. Raw records are BLOBs to preserve their captured bytes. Indexes cover time, application, service and priority. The export is unencrypted, contains captured payloads, and may contain sensitive application data; store securely. A capture only retains the existing bounded buffer; this is snapshot export, not continuous archival or retrieval of remote history.

```sql
SELECT timestamp_utc, application_id, service, payload
FROM journal_logs
WHERE priority BETWEEN 0 AND 3
  AND application_id = 'PAYMENT'
ORDER BY timestamp_us;
```

## Architecture and verification

Filter UI, filter state and SQLite export belong to RemoteJournal. The existing native Journal adapter is still compiled into the editor; this is not yet a fully dynamically independent graphical plugin. GUI compilation and live SSH not verified in the headless test environment. Focused test: `ctest --test-dir build -R 'remote_journal|workspace_plugin' --output-on-failure`.
