# 5.2.0 — Persistent Remote Journal output tabs

Open Plugins > Remote Journal (SSH), enter the host, user, optional systemd unit,
SSH port and initial line count. Select key/agent or password authentication.
Connect opens a dedicated `Journal: user@host / unit` page in the Output dock.
The connection proceeds asynchronously; authentication errors and remote
journalctl diagnostics are visible in the status and table. A successful launch
of the SSH process is not itself proof of successful authentication.

Each page owns its SSH Session, timer, model, filters and selection. Open another
connection to monitor another server/unit concurrently. Switching pages or hiding
Output does not stop streaming. Disconnect stops streaming without removing the
rows. Close tab (or its notebook close control) stops and removes the session;
application shutdown stops all sessions. Reconnect by opening Remote Journal again.

## Table controls

* UTC timestamp, actual journal priority, host, systemd unit, process, PID, message.
* Text filter across the displayed fields, case-sensitive option, exact priority
  selector including unknown/SSH diagnostics. Filtering is local and non-destructive.
* Ctrl+A selects visible rows; Ctrl+C copies selected log lines; Shift/Ctrl multi-select.
* Right-click: copy full rows, messages only, raw JSON/SSH output or visible rows.
* Select a row to read multiline content in the details box.
* Auto-scroll follows the newest matching row when there is no selected row.
* Pause view freezes the displayed table, not the SSH connection. Resume catches up
  with retained events. A separate status reports total received and evicted rows.
* Clear resets the local buffer; it never clears the remote system journal.
* Export visible filtered rows or all retained rows as CSV or text. During pause,
  visible export uses the frozen view; all-buffer export uses the live buffer.
  CSV retains untrusted message content verbatim: import columns as text in spreadsheets.

## Implementation and constraints

The existing OpenSSH/sshpass transport retains strict host key checking and does
not intentionally persist passwords. It remains POSIX-only. `journalctl --output=json
--output-fields=...` requires a systemd journalctl supporting these options. Older
servers without support produce a diagnostic row; no automatic legacy fallback.
Timestamp is UTC, milliseconds; missing journal metadata remains blank/unknown.
Journalctl may omit oversized fields or encode binary fields as JSON arrays; those
are represented rather than invented. Non-JSON SSH output is retained as diagnostics.
The session retains up to 20,000 complete rows and 16 MiB of raw line content, evicting
oldest records; a line is capped at 32 KiB and marked when truncated. The view
retains its own parsed copy. Pausing may therefore miss entries evicted before resume.
No disk history, automatic reconnect or remote journal modification is performed.

## Build and verify

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build -R 'remote_journal|new_symbol' --output-on-failure
sudo cmake --install build
```

Native GUI smoke test: connect to a trusted SSH host with a running systemd service;
verify rows arrive, filter by priority and text, select multiple rows and copy, switch
to another output tab, pause/resume, export CSV, disconnect, close, and open two
connections. Verify wrong password/host key errors remain visible. Test with the
existing theme switcher. Headless tests use simulated SSH and do not establish a
real remote connection.
