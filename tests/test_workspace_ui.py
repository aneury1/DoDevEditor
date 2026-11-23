from pathlib import Path
from xml.etree import ElementTree as ET
import re
root=Path(__file__).resolve().parents[1]
s=(root/'resources/EmbeddedUi.h').read_text()
xml=ET.fromstring(re.search(r'kMainWindow = R"DODEV_XML\((.*?)\)DODEV_XML";',s,re.S).group(1))
dock=xml.find('.//DockArea')
assert dock is not None
panels={p.get('Name'):p for p in dock}
for name in ('explorer_pane','git_pane','search_pane','symbols_pane'):
    assert name in panels,name
for name in ('git_pane','search_pane','symbols_pane'):
    assert panels[name].get('Slot.Dock')=='Tab'
    assert panels[name].get('Slot.DockTo')=='explorer_pane'
assert [x.get('Name') for x in panels['search_pane'].iter('Table')]==['workspace_results']
assert [x.get('Name') for x in panels['symbols_pane'].iter('Table')]==['symbols_sidebar_results']
for name in ('workspace_query','workspace_replacement','workspace_include','workspace_exclude','workspace_regex','workspace_case','workspace_word'):
    assert xml.find(f'.//*[@Name="{name}"]') is not None,name
for action in ('workspace.search','workspace.replace_selected','workspace.replace_all','panel.search','panel.symbols'):
    assert any(w.get('OnClicked')==action or w.get('OnCommitted')==action for w in xml.iter()),action
source=(root/'src/EditorWorkspace.cpp').read_text()
assert 'search::scan' in source and 'replaceFile' in source and 'Rtext_editor_insert_text' in source
symbols=(root/'src/EditorSymbols.cpp').read_text()
assert all(c in symbols for c in ('"Entity name"','"Type"','"Filepath : line"','Rtext_editor_set_cursor','Rtable_set_activated_callback'))
print('PASS: Explorer, Git, Search, Symbols sidebar contract')
