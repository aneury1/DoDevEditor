"""Style contract tests for the C++ embedded Raven XML (no GUI required)."""
from pathlib import Path
import re
from xml.etree import ElementTree as ET

root = Path(__file__).resolve().parents[1]
header = (root / 'resources' / 'EmbeddedUi.h').read_text(encoding='utf-8')
raw_xml = dict(re.findall(r'inline constexpr std::string_view (\w+) = R"DODEV_XML\((.*?)\)DODEV_XML";', header, re.S))
assert 'kMainWindow' in raw_xml
window = ET.fromstring(raw_xml['kMainWindow'])
assert window.tag == 'Window'

# All the user-visible named widgets still exist; migration changes only their visual attributes.
for widget in ('dock', 'documents', 'explorer_pane', 'git_pane', 'output_pane',
               'terminal_tabs', 'files', 'themes', 'search_text', 'music_backdrop'):
    assert window.find(f'.//*[@Name="{widget}"]') is not None, widget

# Preserve docking positions and command bindings.
assert window.find('.//*[@Name="explorer_pane"]').attrib['Slot.Dock'] == 'Left'
assert window.find('.//*[@Name="output_pane"]').attrib['Slot.Dock'] == 'Bottom'
assert window.find('.//*[@Name="terminal_tabs"]').tag == 'TabView'
assert window.find('.//*[@Name="documents"]').tag == 'TabView'
actions = {e.attrib['OnClicked'] for e in window.iter() if 'OnClicked' in e.attrib}
for action in ('file.new', 'file.save', 'terminal.new', 'terminal.interrupt', 'theme.next'):
    assert action in actions, action

# Required exact Raven_music palette names and styles.
expected = {
    'kMusicNocturne': ('Music Nocturne', 'Dark', '#060609', '#B79CFF'),
    'kMusicAuroraGlass': ('Music Aurora Glass', 'GlassDark', '#080912', '#7DD3FC'),
    'kMusicDaylight': ('Music Daylight', 'Light', '#F3F1EC', '#7C3AED'),
}
for symbol, (name, base, background, accent) in expected.items():
    theme = ET.fromstring(raw_xml[symbol]); assert theme.tag == 'Theme'
    assert theme.get('Name') == name
    assert theme.get('Base') == base
    assert theme.get('Accent') == accent
    assert theme.find('Colors').get('Window') == background
    assert theme.find('Syntax') is not None
    styles = {(e.get('Widget', ''), e.get('Class')) for e in theme.findall('Style')}
    for klass in ('surface', 'bar', 'music-toolbar', 'music-tabs', 'music-editor', 'music-search'):
        assert ('', klass) in styles, (name, klass)
    for klass in ('chip', 'icon', 'round', 'music-primary'):
        assert ('Button', klass) in styles, (name, klass)

# Validate *all* XML literals, not only the new themes.
for name, xml in raw_xml.items():
    ET.fromstring(xml)
assert len(raw_xml) == 14  # one interface and thirteen themes
assert 'std::array<Theme, 13> kThemes' in header
assert 'std::string theme_="Music Nocturne"' in (root/'include'/'EditorApp.h').read_text()
print(f'PASS: {len(raw_xml)-1} embedded themes + window + widget/style contract')

settings_xml = (root / 'resources' / 'EmbeddedSettingsUi.h').read_text(encoding='utf-8')
extra = dict(re.findall(r'inline constexpr std::string_view (\w+) = R"DODEV_XML\((.*?)\)DODEV_XML";',settings_xml,re.S))
for name in ('kSettingsWindow','kThemeDesignerWindow','kTemplateWindow'):
    assert name in extra
    ET.fromstring(extra[name])
for widget in ('settings_editor_font','settings_plugin_list','settings_theme_directory','settings_templates_directory'):
    assert ET.fromstring(extra['kSettingsWindow']).find(f'.//*[@Name="{widget}"]') is not None
for theme in ('kVSCodeDarkModern','kVSCodeLightModern'):
    assert theme in raw_xml
print('PASS: settings, theme designer and file-template dialogs are embedded XML')
