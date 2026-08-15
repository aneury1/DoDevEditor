# Journal Log Diff

A wxWidgets C++17 desktop application for comparing two `journalctl` or syslog export files side by side. The layout and navigation follow the main concepts of Beyond Compare without copying its branding or assets.

## Features

- Side-by-side aligned log comparison.
- Reference/older lines removed or changed are red.
- New/candidate lines added or changed are green.
- Checkbox to ignore journal timestamps while keeping the original timestamps visible.
- Recognizes ISO, syslog, precise, monotonic, and bracketed timestamp prefixes.
- Optional whitespace and case-insensitive comparison.
- **Differences only** filter to hide unchanged rows.
- **Newest only** filter to show only added or modified candidate lines and hide removed reference-only rows.
- Synchronized vertical and horizontal scrolling.
- Previous/next difference navigation with `F7` and `F8`.
- Drag a file directly onto either comparison pane.
- Myers line-diff algorithm; no third-party diff library.
- Accepts two paths on the command line.

## Linux dependencies

### Debian / Ubuntu

```bash
sudo apt update
sudo apt install build-essential cmake libwxgtk3.2-dev
```

### Arch Linux

```bash
sudo pacman -S --needed base-devel cmake wxwidgets-gtk3
```

### Fedora

```bash
sudo dnf install gcc-c++ cmake wxGTK-devel
```

## Build

```bash
./scripts/build-linux.sh
```

Or manually:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run:

```bash
./build/journal-log-diff
```

Open two files directly:

```bash
./build/journal-log-diff old-journal.log new-journal.log
```

Try the included example:

```bash
./build/journal-log-diff examples/reference.log examples/candidate.log
```

## Export journal logs

Examples:

```bash
journalctl -u my-service --since "2026-08-04 08:00" -o short-iso-precise > old-journal.log
journalctl -u my-service --since "2026-08-04 12:00" -o short-iso-precise > new-journal.log
```

For a previous boot and the current boot:

```bash
journalctl -u my-service -b -1 -o short-iso-precise > previous-boot.log
journalctl -u my-service -b 0  -o short-iso-precise > current-boot.log
```

## Timestamp behavior

When **Ignore journal timestamps** is enabled, only a recognized timestamp prefix is removed from the comparison key. The original complete line is still rendered in the editor. Hostname, unit/process name, PID, priority text, and message content remain part of the comparison.

## Windows build

Install wxWidgets and point CMake to it, for example:

```powershell
cmake -S . -B build -DwxWidgets_ROOT_DIR=C:/Libraries/wxWidgets
cmake --build build --config Release
```

The exact wxWidgets configuration depends on whether the library was built with MSVC or MinGW.

## View filters

- **Differences only** removes equal rows from both panes while retaining added, removed, and modified rows.
- **Newest only** shows candidate-side additions and modifications. Rows that exist only in the reference file are omitted.
- The filters operate on the cached comparison result, so toggling them does not reread or recompute the files.
