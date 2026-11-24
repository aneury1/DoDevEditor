# DoDevEditor 4.0 — VS Code-like Context Menus and Output Export

## File Explorer context menu

Right-click in the Explorer tree to create a file or subdirectory, open an entry, rename, delete (with confirmation), copy its absolute path, or refresh the file list. Creation uses the selected folder or the parent directory of the selected file. New files are empty. Entry-name validation rejects path traversal and names containing directory separators. Deleting the active project root is blocked, and dirty open files block deletion of their containing path.

## Text editor / document tab context menus

Right-click the editor for Undo, Redo, Cut, Copy, Paste, Select All, Search, Find in Files, Go to Line, Toggle Line Comment, or Format Document through the optional clang-format plugin. Right-click a document tab for Save, Close, Close Others, Close Tabs to the Right, Copy Path and Reveal in Explorer.

## Full output export

Terminal, Serial Monitor, Remote Journal and AI Assistant panels expose Copy All and Save TXT controls. Save TXT invokes the system file-save dialog to choose an output path. The Terminal export includes retained scrollback; content already evicted from the scrollback buffer cannot be recovered. Serial and Journal export the retained log buffer without applying the current display filter.

## Usage from Windows via WSLg

See [WSL_WINDOWS_LAUNCH.md](WSL_WINDOWS_LAUNCH.md). WSLg is a WSL2 desktop integration feature; the Raven/Vulkan binary must have a working Vulkan stack in the WSL distribution.

## Verification and limitations

- 32/32 headless tests pass in this environment, including filesystem operations, context menu UI contracts, the terminal PTY, and plugin regressions.
- GUI rendering against Raven/Vulkan was not tested end to end in this environment. Test right-click target selection and native file dialogs on your desktop.
- Destructive Explorer deletion currently permanently removes the selected file/folder; it is not a desktop Trash operation. Confirm the target carefully.
- Terminal/Serial/Journal/AI export saves retained buffered output; it cannot recover discarded output or output outside the current capture session.
