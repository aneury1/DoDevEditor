from pathlib import Path
import re
from xml.etree import ElementTree as ET
root=Path(__file__).resolve().parents[1]
main=(root/'resources/EmbeddedUi.h').read_text()
xml=ET.fromstring(re.search(r'kMainWindow = R"DODEV_XML\((.*?)\)DODEV_XML";',main,re.S).group(1))
assert xml.find('.//*[@Name="go_menu"]') is not None
for action in ('navigation.go_to_line','navigation.quick_open','navigation.command_palette','keybindings.show'):
    assert any(i.get('OnClicked')==action for i in xml.iter('MenuItem'))
assert not any('Shortcut' in i.attrib for i in xml.iter('MenuItem')), 'static shortcuts conflict with runtime settings'
settings=(root/'resources/EmbeddedSettingsUi.h').read_text()
settings_xml=ET.fromstring(re.search(r'kSettingsWindow=R"DODEV_XML\((.*?)\)DODEV_XML";',settings,re.S).group(1)) if 'kSettingsWindow=R"' in settings else ET.fromstring(re.search(r'kSettingsWindow = R"DODEV_XML\((.*?)\)DODEV_XML";',settings,re.S).group(1))
for item in ('kb_filter','kb_command_list','kb_shortcut','kb_status'):
    assert settings_xml.find(f'.//*[@Name="{item}"]') is not None
navigation=(root/'resources/EmbeddedNavigationUi.h').read_text()
for name in ('kGoToLineWindow','kQuickOpenWindow'):
    raw=re.search(name+r'=R"DODEV_XML\((.*?)\)DODEV_XML"',navigation,re.S).group(1)
    ET.fromstring(raw)
source=(root/'src/EditorKeybindings.cpp').read_text()
assert 'Rwindow_register_shortcut' in source and 'Rwindow_unregister_shortcut' in source
assert 'Rtext_editor_set_cursor' in source
assert 'Rlist_view_add_item' in source
assert 'keybindingEventCb' in source and 'Rwidget_set_event_callback' in (root/'src/EditorSettings.cpp').read_text()
print('PASS: dynamic keyboard shortcuts UI, Quick Open, Go to Line, embedded XML')
