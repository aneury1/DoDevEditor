#!/usr/bin/env python3
"""Export only the public SDK and the independently buildable plugins.

The legacy plugins requiring host-specific graphical adapters are deliberately
not included. The export can be built with no editor source tree present.
"""
import argparse
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('destination', type=Path)
out = p.parse_args().destination.resolve()
if out.exists():
    raise SystemExit('Choose a new destination directory (will not overwrite).')
out.mkdir(parents=True)
for name in ('thirdparty/GitHistory', 'thirdparty/QRCode', 'plugins/examples/SimpleModule'):
    shutil.copytree(root / name, out / name)
for name in ('dodev_plugin_api.h', 'PluginSettings.h'):
    target = out / 'include/dodev/sdk' / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / 'include/dodev/sdk' / name, target)
shutil.copy2(root / 'include/TerminalSession.h', out / 'include/TerminalSession.h')
(out / 'support').mkdir()
shutil.copy2(root / 'src/TerminalSession.cpp', out / 'support/TerminalSession.cpp')
(out / 'cmake').mkdir()
shutil.copy2(root / 'cmake/ProjectHelpers.cmake', out / 'cmake/ProjectHelpers.cmake')
for name in ('CMakeLists.txt', 'README.md'):
    shutil.copy2(root / 'sdk/standalone' / name, out / name)
print(out)
