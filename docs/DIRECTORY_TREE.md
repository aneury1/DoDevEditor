# Directory tree (4.2.0)

Open a workspace with `dodeveditor .`. Use Archivo > Arbol de directorios or Ctrl+Shift+E. Double-click (or use the list activation key) to expand/collapse folders and open files. The root stays unchanged while navigating. The toolbar supports new file, new folder, collapse and refresh. The context menu supports existing rename/delete/copy path operations, hidden-file toggle and opening a terminal in the selected directory. Reveal active file opens its ancestors; external files switch the workspace to their parent. Hidden files are visible by default, including .vscode. Symlink directories are not recursively followed. Visible rows are capped at 20,000 and depth at 128.

This version renders an indented hierarchy using Raven ListView rather than a native TreeView. Expansion is lazy and retained during manual refresh, not across application restarts. External filesystem changes require Refresh. Drag/drop, multi-selection, Git badges, multi-root workspaces, filesystem watchers and Debug Executable context action are not implemented. GDB and detached startup are inherited unchanged from 4.1.1.

Tested: standalone C++17 directory model and embedded UI/context-menu contract checks. Full native Raven GUI compilation and interaction are not verified in the packaging environment.
