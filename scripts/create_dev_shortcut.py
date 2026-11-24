#!/usr/bin/env python3
"""Register a development-tree DoDevEditor launcher on Linux (no sudo/install)."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

APP_ID = "dodeveditor-development.desktop"
ICON_NAME = "dodeveditor-development.png"


def desktop_exec_quote(path: Path) -> str:
    """Quote a single Exec argument using Freedesktop Desktop Entry rules."""
    value = str(path)
    if "\n" in value or "\r" in value or "\0" in value:
        raise ValueError("The project path contains an unsupported character")
    value = value.replace("\\", "\\\\").replace('"', '\\"').replace("`", "\\`").replace("$", "\\$")
    return '"' + value.replace("%", "%%") + '"'


def desktop_location() -> Path | None:
    if shutil.which("xdg-user-dir"):
        try:
            result = subprocess.run(["xdg-user-dir", "DESKTOP"], capture_output=True,
                                    text=True, timeout=3, check=True)
            path = Path(result.stdout.strip()).expanduser()
            if path.is_dir():
                return path
        except (OSError, subprocess.SubprocessError):
            pass
    for name in ("Desktop", "Escritorio"):
        path = Path.home() / name
        if path.is_dir():
            return path
    return None


def find_launchers(project: Path, build_dir: Path | None) -> tuple[Path, Path]:
    dirs = [build_dir] if build_dir else [project / name for name in (
        "build", "build-release", "build-debug", "cmake-build-debug", "cmake-build-release")]
    for folder in dirs:
        if folder is None:
            continue
        folder = folder.resolve()
        for candidate in (folder, folder / "bin", folder / "Release", folder / "Debug"):
            launcher, editor = candidate / "dodeveditor", candidate / "DoDevEditor"
            if launcher.is_file() and os.access(launcher, os.X_OK) and editor.is_file():
                return launcher, editor
    searched = ", ".join(str(d) for d in dirs)
    raise FileNotFoundError(
        "Could not find built dodeveditor + DoDevEditor executables together in: " + searched +
        "\nBuild with: cmake -S . -B build && cmake --build build --parallel\n"
        "Or run: python3 scripts/create_dev_shortcut.py --build-dir /path/to/build"
    )


def remove_if_exists(path: Path) -> bool:
    if path.exists():
        path.unlink()
        print("[DoDevEditor][Dev Shortcut] Removed:", path)
        return True
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Create/remove DoDevEditor development shortcuts")
    parser.add_argument("--project", type=Path, help="Project source directory (default: parent of this script)")
    parser.add_argument("--build-dir", type=Path, help="CMake build directory (default: auto-detect)")
    parser.add_argument("--no-desktop", action="store_true", help="Only register the Applications menu entry")
    parser.add_argument("--remove", action="store_true", help="Remove development shortcuts")
    args = parser.parse_args()
    if not sys.platform.startswith("linux"):
        parser.error("This shortcut installer is for Linux desktop environments")

    project = (args.project or Path(__file__).resolve().parent.parent).expanduser().resolve()
    build_dir = args.build_dir.expanduser().resolve() if args.build_dir else None
    data_home = Path(os.environ.get("XDG_DATA_HOME") or (Path.home() / ".local/share")).expanduser()
    app_entry = data_home / "applications" / APP_ID
    icon_path = data_home / "icons" / ICON_NAME
    desktop = desktop_location()
    desktop_entry = desktop / APP_ID if desktop else None

    if args.remove:
        remove_if_exists(app_entry)
        if desktop_entry:
            remove_if_exists(desktop_entry)
        remove_if_exists(icon_path)
        return 0

    try:
        launcher, editor = find_launchers(project, build_dir)
        quoted_launcher = desktop_exec_quote(launcher)
    except (ValueError, FileNotFoundError) as exc:
        print("[DoDevEditor][Dev Shortcut] ERROR:", exc, file=sys.stderr)
        return 1

    icon_source = project / "legacy" / "doDevLogo1.png"
    if icon_source.is_file():
        icon_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(icon_source, icon_path)
        icon_value = str(icon_path)
    else:
        icon_value = "accessories-text-editor"

    entry = "\n".join([
        "[Desktop Entry]",
        "Version=1.0",
        "Type=Application",
        "Name=DoDevEditor (Development)",
        "Name[es]=DoDevEditor (Desarrollo)",
        "GenericName=Source Code Editor",
        "Comment=Launch the development build without installing it",
        "Comment[es]=Abrir la compilación de desarrollo sin instalarla",
        f"Exec={quoted_launcher} %F",
        f"Path={project}",
        f"Icon={icon_value}",
        "Terminal=false",
        "StartupNotify=true",
        "Categories=Development;IDE;TextEditor;",
        "MimeType=text/plain;inode/directory;text/x-csrc;text/x-c++src;text/x-chdr;text/x-c++hdr;text/x-python;application/json;application/xml;text/xml;text/markdown;",
        "Keywords=DoDevEditor;C++;IDE;development;",
        "",
    ])
    app_entry.parent.mkdir(parents=True, exist_ok=True)
    app_entry.write_text(entry, encoding="utf-8")
    app_entry.chmod(0o644)
    print("[DoDevEditor][Dev Shortcut] Menu entry:", app_entry)
    if desktop and not args.no_desktop:
        assert desktop_entry is not None
        desktop_entry.write_text(entry, encoding="utf-8")
        desktop_entry.chmod(0o755)
        if shutil.which("gio"):
            subprocess.run(["gio", "set", str(desktop_entry), "metadata::trusted", "true"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        print("[DoDevEditor][Dev Shortcut] Desktop entry:", desktop_entry)
    elif not desktop and not args.no_desktop:
        print("[DoDevEditor][Dev Shortcut] No Desktop/Escritorio directory; added Applications entry only")
    print("[DoDevEditor][Dev Shortcut] GUI executable:", editor)
    print("[DoDevEditor][Dev Shortcut] Logs: ~/.local/state/DoDevEditor/launcher.log")
    print("[DoDevEditor][Dev Shortcut] Run without sudo. Re-run after moving the build directory.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
