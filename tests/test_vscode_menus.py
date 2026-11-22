"""Menu wiring contract: no menu action without a bound handler."""
from pathlib import Path
from xml.etree import ElementTree as ET
import re
root=Path(__file__).resolve().parents[1]
header=(root/'resources/EmbeddedUi.h').read_text(encoding='utf-8')
xml=re.search(r'kMainWindow = R"DODEV_XML\((.*?)\)DODEV_XML";',header,re.S).group(1)
window=ET.fromstring(xml)
menus={m.get('Name'):m for m in window.findall('.//MenuBar/Menu') if m.get('Name')}
assert {'edit_menu','selection_menu','terminal_menu'}.issubset(menus)
source=(root/'src/EditorApp.cpp').read_text(encoding='utf-8')
dispatch=(root/'src/EditorMenus.cpp').read_text(encoding='utf-8')
for name in ('edit_menu','selection_menu','terminal_menu'):
    items=list(menus[name].iter('MenuItem'))
    assert len(items)>=15,(name,len(items))
    for item in items:
        action=item.get('OnClicked')
        assert action,(name,item.attrib)
        assert '"'+action+'"' in source or '"'+action+'"' in dispatch,(name,action)
assert len(list(menus['selection_menu'].iter('MenuItem')))>=18
assert len(list(menus['terminal_menu'].iter('MenuItem')))>=19
assert window.find('.//*[@Name="terminal_task_select"]') is not None
assert window.find('.//*[@Name="terminal_name"]') is not None
print('PASS: Edit, Selection and Terminal menus contain mapped commands')
