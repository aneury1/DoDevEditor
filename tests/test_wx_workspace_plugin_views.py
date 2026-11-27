"""Source contract only: actual wxWidgets event handling requires a desktop build."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
main = (root / 'src/gui/wx/Main.cpp').read_text()
commands = (root / 'src/gui/wx/WxCommands.inc').read_text()
profiles = (root / 'src/gui/wx/WxWorkspaceProfiles.inc').read_text()
login = (root / 'thirdparty/RemoteJournal/src/wx/JournalLogin.inc').read_text()
assert 'p=page(side,"Git",s)' not in main
assert 'refreshGitView' not in commands
assert 'resetWorkspacePluginTabs();' in main
assert 'loadSelectedWorkspacePlugin();' in main
assert 'isWorkspacePluginView(v)' in commands
assert 'dodev.plugin.pending:' in commands
assert 'b.workspace_profiles=' in commands
assert 'Save profiles' in profiles and 'Reload' in profiles
assert 'auto* notebook=editors;' in login
assert 'get("tabLocation")' not in login
assert 'notebook->AddPage(panel' in login
print('Workspace registration, Git routing, inline profiles and capture placement source contract verified')
