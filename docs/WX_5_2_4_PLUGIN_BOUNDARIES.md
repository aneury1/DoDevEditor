# DoDevEditor 5.2.4 — workspace profiles and plugin boundaries

This is an incremental migration, not a fully independent plugin architecture.
All plugins must own their settings, profile schemas, functionality and GUI logic.
The host may expose only generic SDK services: settings, workspace/document
context, menus, Editor/Output/Workspace views, dialogs, clipboard and lifecycle.
The application must eventually build and run with all optional plugins removed.
No host code may include plugin-specific UI files or link plugin-specific cores.

## Global and workspace settings

Global settings are registered by the native modules using the existing schema
API, and stored by plugin ID. Workspace views provide a Configure profiles button
that opens `.dodev/plugins/<plugin-id>/profiles.json` in the editor. Save the
file before opening a capture. Profile data is loaded only on explicit user
action; do not put credentials in workspace profiles or commit credentials.

### Serial Monitor

Options > SerialMonitor: Workspace profiles. Edit the profiles and select one
or several ports. Each capture owns its session and opens an Editor tab.
`captureAsAdministrator` is a global preference, disabled by default. On Linux
this uses the existing Polkit helper with explicit authorization; do not run the
whole editor as root. `allowPolkit` allows permission-denied fallback.

```json
{"profiles":[{"name":"Device A","port":"/dev/ttyUSB0","baud":115200,"dataBits":8,"stopBits":1,"parity":"N"},{"name":"Device B","port":"/dev/ttyUSB1","baud":9600}]}
```

### Remote Journal

Options > RemoteJournal: Workspace profiles. Each profile may specify host,
user, port, unit, lines, authentication (key/password), follow and sudoNonInteractive.
New sessions default to Editor; global tabLocation accepts editor/output/workspace.
No password is persisted. Table supports text, payload substring or regular
expression, service, application ID, priority, UTC date/time range, and copy/export.
APP_ID or APPLICATION_ID is used if present, otherwise SYSLOG_IDENTIFIER/_COMM.
Colors tableForeground and tableBackground accept #RRGGBB, blank inherits theme.
This resembles a DLT Viewer table; it is not an AUTOSAR DLT transport/parser.
Filters only cover locally buffered records. Regex matching is std::regex.

### Source Symbols

Global `captureKinds` accepts comma-separated symbol kinds, e.g.
`function,method,class,struct,namespace`. Empty captures all. The selection is
included in the cache fingerprint. Actual kinds depend on the chosen parser.

### QR Code

Tools > QR Code: Capture File / Capture Directory. A directory is serialized
with its relative paths and file data into the versioned DQA1 bundle format.
The payload envelope DQP1 uses optional zlib compression, optional AES-256-GCM
with PBKDF2-HMAC-SHA256 (600000 iterations, random salt and nonce), and base64
with prefix DODEVQR1:. OpenSSL is required for encryption; a masked password
prompt occurs at export. No passwords are saved. The exported PNG/SVG is a
single QR; an oversized encoded payload fails explicitly, never truncates.
A 4 MiB pre-compression guard is not a QR capacity guarantee. There is no
multi-QR segmentation or graphical file extraction tool yet. The public
unwrapPayload function verifies/decrypts and returns the original binary bundle.

### Git History

Enable Git History, restart and choose Options > Git: Commit history. The native
plugin registers a Workspace table of commit hash, committer, commit date and
subject. Select a row to show fuller metadata, changed filenames and optional
patch in Output. The plugin executes Git with literal argv and a bounded query.
Refresh is manual; query execution currently blocks the invoking UI thread.

## What is actually independent?

Git History, QR Code and Simple Module use the public C ABI and generic host
functions. Build them in an external source tree using:

```
python3 scripts/export_standalone_plugins.py /tmp/dodev-plugin-sdk
cmake -S /tmp/dodev-plugin-sdk -B /tmp/dodev-plugin-sdk/build
cmake --build /tmp/dodev-plugin-sdk/build --parallel 4
```

Serial Monitor and Remote Journal still call the legacy host-specific open hooks.
Their wx views live in plugin folders but are compiled into the main executable.
Source Symbols and call hierarchy still have native editor adapters; AI, Docker
and GDB also retain host-specific integrations. Git's legacy core panel remains
as a fallback, so the complete host is not yet free of plugin-specific logic.
The next architecture stage must replace these legacy integrations with generic
view factories, scoped session lifetimes, generic native or declarative controls,
async update dispatch and a generic command binding system. Plugin UI resources
must be destroyed before unloading a native module. Preserve the existing ABI
prefix and use struct_size checks for additive functions.

## Verification

Linux headless build and test suite; independent Git/QR/Simple build and QR tests.
Native wxWidgets GUI, real SSH/serial hardware and other operating systems are
not verified in the build environment. See the attached test results.
