# Remote Journal 5.2.1

## Filter controls

Each connection keeps its own filter and its own buffer. General text searches across the displayed fields. Payload searches only the decoded journal `MESSAGE` field (or diagnostic message). Service searches the unit column (`_SYSTEMD_UNIT`, with `_SYSTEMD_USER_UNIT` fallback). Both use substring matching; the Case sensitive toggle applies to all text filters. Priority, general text, payload, service and time are combined with AND. Reset filters removes all five restrictions and resets case sensitivity without clearing the buffer or changing pause/autoscroll.

The From and To fields use UTC, matching the Timestamp (UTC) column. They accept:

* `2026-11-25` (whole day for To)
* `2026-11-25 14:30:00` (whole second for To)
* `2026-11-25T14:30:00.123456Z` (optional T and Z, up to six fractional digits; partial fractional upper bounds include the entire specified precision interval)

Blank means open-ended. Both bounds are inclusive. The filter compares original microsecond timestamps; display and CSV now show six fractional digits. Invalid or reversed ranges display an error and retain the previous valid filter. Records without a valid journal timestamp (including SSH diagnostics) are excluded when either time bound is set. Clear the bounds to see them again. Accepted civil dates are 1970–9999, with no timezone offsets or leap seconds.

Filtering is local: changing it does not reconnect or fetch history. The existing 20,000-record / 16 MiB session cap still applies. If you want to filter several services, leave the systemd unit blank at SSH login. If the connection requested only one unit, the local filter cannot recover other units. Copy visible and filtered export follow the active filter. All-buffered export ignores it.

## Placement

Open Settings > Remote Journal and choose Output panel (default) or Editor tabs. Apply or Save & Close. The preference is saved as `remoteJournal.tabLocation` (`output` or `editor`) in settings.json. Existing settings are merged with the default and invalid placement values fall back to output. New SSH sessions open in the selected notebook; existing sessions stay in place. No restart is required. Close Tab and the editor's close commands stop the corresponding SSH session. Source-document session restore skips journal tabs; credentials are not persisted. The Output pane can be resized using its splitter.

## Validation

Headless targeted CTest targets: `remote_journal_ssh_filter`, `remote_journal_structured_table`, and the settings regression. Tests cover combined text filters, case sensitivity, timestamp boundaries, invalid/reversed ranges, leap dates, missing timestamps, UTC microseconds, persistence and invalid settings fallback. Native wxWidgets compilation and live SSH testing must be performed on a desktop with the dependencies installed.
