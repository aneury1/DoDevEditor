# DoDevEditor Raven 3.1 — Edit, Selection and Terminal menus

This release adds three top-level menus inspired by Visual Studio Code. The
**intent is practical feature coverage**, not a claim of complete VS Code
feature parity. Actions use a native Raven `TextEditor` and VT terminal.

## Edit (26 items)

- Undo/redo via Raven's native undo stack, Cut/Copy/Paste/Delete, Find, Find
  Previous/Next, Replace and Replace All in the current document.
- Find in Workspace scans a chosen folder and presents matching paths/line numbers
  in a new editable results tab. Binary files and giant files are skipped; at
  most 2,000 files and 20 MB are scanned, excluding `.git`, `build`,
  `node_modules`, `.cache`. It does not mutate source files.
- Toggle line/block comments, transform case, indent/outdent, insert/delete
  lines, join, trim whitespace and sort ascending/descending.
- Transformations are applied with `Rtext_editor_insert_text` rather than
  resetting the document, so native undo/redo works on each invocation.

## Selection (19 items)

- Select all, line, word, next/previous occurrence, expand/shrink, line/file
  boundaries, swap anchor/caret, move caret above/below.
- Copy/move lines up/down and duplicate the selected range or current line.
- **Current Raven limitation:** The Raven native text editor API exposes a
  single contiguous selection, not multiple disjoint cursors. Therefore
  VS Code commands requiring *multi-cursor editing*, rectangular column
  selection, and syntax-tree-aware expand selection are **not included**;
  expand/shrink works at word/line/document granularity. Do not represent
  the next-occurrence command as multi-cursor: it moves one selection.

## Terminal (19 items)

- New Terminal: creates a persistent shell and an independent tab.
- Split Terminal: creates an **additional independent PTY/ConPTY session**
  as a second Raven custom canvas in the active terminal's horizontal group.
  Mouse focus selects the live session that receives text.
- Kill, restart, next/previous tab, rename, focus, copy, paste, clear,
  interrupt, and show the terminal panel.
- Run Selected Text: sends the current editor selection to the active shell;
  multiline input prompts for confirmation.
- Run Active File: supports `.py`, `.js`, `.mjs`, `.sh`, `.ps1`, `.rb`,
  `.pl`, `.bat`, `.cmd`, `.exe`. Unsupported languages require a task.
- Run Task, Run Build Task, Configure Tasks, Configure Default Build Task:
  reads optional workspace `.vscode/tasks.json`, falling back to
  `<settings directory>/tasks.json`; no file is required for startup.
  The terminal toolbar has a task selector and Run/Build buttons.

### Optional tasks.json

```json
{
  "version": "2.0.0",
  "tasks": [
    {"label":"Build","type":"shell","command":"cmake --build build", "group":"build"},
    {"label":"Test","type":"shell","command":"ctest --test-dir build --output-on-failure"}
  ]
}
```

Configure Tasks creates a sample in the chosen workspace only if one does not
already exist. Task files are optional, and malformed JSON produces a warning
rather than preventing startup. `shell` and `process` task entries with labels,
commands, argument arrays, and working directory are recognized. This is a
**subset** of VS Code's Task schema; variable substitution, problem matchers,
compound dependencies, platform overrides, and background tasks are not yet
implemented. The task's shell command is executed with the user's privileges;
only run tasks from trusted projects.

### Keyboard shortcuts and implementation boundaries

Shortcuts are declared through Raven XML's `Shortcut` attribute. Raven's
runtime decides whether a particular key combination is recognized on the
platform; the command remains available from the menu if the binding is not.
The terminal canvas also handles Ctrl+Shift+C/V and Ctrl+C, using its normal
PTY/ConPTY keyboard path.

## Files changed

- `resources/EmbeddedUi.h` — menus, task selector and rename controls
- `include/EditorApp.h`, `src/EditorApp.cpp` — action bindings and split groups
- `src/EditorMenus.cpp` — GUI command handlers and task runner
- `include/dodev/editor/EditorCommands.h`, `src/editor/EditorCommands.cpp` —
  testable text transformations
- `include/dodev/terminal/TerminalTasks.h`, `src/terminal/TerminalTasks.cpp` —
  task parsing and command preparation
- CMake, tests and documentation

## Build and test

```bash
cmake -S . -B build -DDODEV_BUILD_GUI=ON
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

For headless tests without Raven / Vulkan:

```bash
cmake -S . -B build-tests -DDODEV_BUILD_GUI=OFF
cmake --build build-tests -j 4
ctest --test-dir build-tests --output-on-failure
```

Raven is fetched at configure time with `FetchContent` and visible git progress.
