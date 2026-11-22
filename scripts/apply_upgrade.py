#!/usr/bin/env python3
"""Conflict-aware, all-or-nothing source upgrade from original Raven 3.1 or 3.2.

Run from the 3.3 full package; preserve local edits through git merge-file.
Never overwrite conflicted content. Back up changed files before writing.
"""
from __future__ import annotations
import argparse
import datetime
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT = Path(__file__).resolve()
PACKAGE = SCRIPT.parent.parent
EXCLUDES = {".git", "upgrade", "build", "build-headless", "__pycache__"}
TEXT_EXTENSIONS = {".cpp", ".c", ".cc", ".h", ".hpp", ".md", ".txt", ".json",
                   ".py", ".sh", ".ps1", ".cmake", ".xml", ".tpl", ".patch", ".yml", ".yaml"}

def list_source_files(root: Path):
    for base, dirs, files in os.walk(root):
        dirs[:] = sorted(d for d in dirs if d not in EXCLUDES and not d.startswith(".dodev-upgrade-backup-"))
        for file in sorted(files):
            source = Path(base) / file
            if source.is_symlink() or source.suffix in {".o", ".obj", ".so", ".dll", ".exe", ".a"}:
                continue
            if source.is_file():
                yield source.relative_to(root)

def three_way(current: bytes, base: bytes, new: bytes):
    if current == base:
        return new, None
    if current == new:
        return current, None
    # Preserve non-UTF-8 and binary files; never try a line merge on them.
    try:
        for blob in (current, base, new):
            if b"\x00" in blob: raise ValueError("binary content")
            blob.decode("utf-8")
    except (ValueError, UnicodeError):
        return None, b"Local binary/non-UTF8 modification; manual merge required.\n"
    if not shutil.which("git"):
        return None, b"Git is required to merge locally modified text files.\n"
    with tempfile.TemporaryDirectory(prefix="dodev-merge-") as work:
        a,b,c = (Path(work)/k for k in ("current", "base", "new"))
        a.write_bytes(current);b.write_bytes(base);c.write_bytes(new)
        process=subprocess.run(["git","merge-file","-p",str(a),str(b),str(c)],
                               stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=False)
    if process.returncode==0:
        return process.stdout, None
    return None, process.stdout or process.stderr or b"Unknown merge conflict.\n"

def main():
    parser=argparse.ArgumentParser(description="Upgrade DoDevEditor 3.1/3.2 to 3.3, preserving local changes")
    parser.add_argument("--target",required=True,type=Path,help="Existing project directory")
    parser.add_argument("--from",dest="base_version",choices=("3.1","3.2"),help="Original baseline version")
    parser.add_argument("--dry-run",action="store_true",help="Check only; do not change anything")
    options=parser.parse_args()
    target=options.target.resolve()
    if not target.is_dir() or target==PACKAGE or not (target/"CMakeLists.txt").is_file():
        parser.error("target must be a different, existing DoDevEditor project directory")
    detected=None
    cmake=(target/"CMakeLists.txt").read_text(encoding="utf-8",errors="replace")
    version=re.search(r"project\(DoDevEditor\s+VERSION\s+(\d+\.\d+)",cmake)
    if version:detected=version.group(1)
    base_version=options.base_version or detected
    if base_version not in {"3.1","3.2"}:
        parser.error(f"Could not identify a supported 3.1/3.2 base (detected {detected!r}). Pass --from.")
    if detected and detected not in {base_version,"3.3"}:
        parser.error(f"Target appears to be {detected}, not {base_version}. Choose matching --from or use full package.")
    base_root=PACKAGE/"upgrade"/"bases"/base_version
    if not base_root.is_dir():
        parser.error(f"Missing base snapshot {base_root} (use the complete 3.3 ZIP)")
    updates=[]; conflicts=[]; unchanged=0
    for rel in list_source_files(PACKAGE):
        source=PACKAGE/rel
        dest=target/rel
        if dest.is_symlink() or (dest.parent.exists() and dest.parent.resolve()!=target and
                                target not in dest.parent.resolve().parents):
            conflicts.append((rel,b"Destination traverses an external symlink; refusing update.\n"));continue
        new=source.read_bytes()
        old=base_root/rel
        if not dest.exists():
            updates.append((rel,None,new));continue
        if not dest.is_file():
            conflicts.append((rel,b"Destination exists and is not a regular file.\n"));continue
        current=dest.read_bytes()
        if current==new:unchanged+=1;continue
        if not old.is_file():
            conflicts.append((rel,b"File exists locally but did not exist in original baseline. Manual merge required.\n"));continue
        merged,conflict=three_way(current,old.read_bytes(),new)
        if conflict is not None:conflicts.append((rel,conflict))
        elif merged!=current:updates.append((rel,current,merged))
        else:unchanged+=1
    print(f"Upgrade from {base_version}: {len(updates)} pending changes, {unchanged} unchanged, {len(conflicts)} conflicts")
    if conflicts:
        for rel,data in conflicts:
            print(f"  CONFLICT: {rel}",file=sys.stderr)
            if not options.dry_run:
                marker=target/(str(rel)+".dodev-merge-conflict")
                marker.parent.mkdir(parents=True,exist_ok=True)
                marker.write_bytes(data)
        print("No project source file was changed. Inspect the .dodev-merge-conflict outputs.",file=sys.stderr)
        return 2
    if options.dry_run:
        for rel,_,_ in updates:print("  WOULD UPDATE",rel)
        return 0
    if not updates:
        print("Already up to date.");return 0
    stamp=datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    backup=target/(".dodev-upgrade-backup-"+stamp)
    backup.mkdir(parents=True,exist_ok=False)
    # All merged contents have been calculated before writing. Save originals first.
    for rel,previous,_ in updates:
        if previous is not None:
            location=backup/rel;location.parent.mkdir(parents=True,exist_ok=True)
            location.write_bytes(previous)
    (backup/"UPGRADE_NOTES.txt").write_text(
        f"Baseline {base_version} -> Raven 3.3\n"
        "Files absent from this backup were newly created.\n"
        "If necessary, restore individual backed-up files before retrying.\n",encoding="utf-8")
    for rel,_,new in updates:
        location=target/rel
        location.parent.mkdir(parents=True,exist_ok=True)
        temp=location.with_name(location.name+".dodev-updating")
        temp.write_bytes(new)
        temp.replace(location)
    print(f"Updated {len(updates)} files. Backups: {backup}")
    return 0

if __name__=="__main__":
    sys.exit(main())
