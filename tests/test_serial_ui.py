#!/usr/bin/env python3
import pathlib, re, xml.etree.ElementTree as ET
root = pathlib.Path(__file__).resolve().parents[1]
source = (root / 'resources/EmbeddedSerialUi.h').read_text()
match = re.search(r'R"DODEV_SERIAL_XML\((.*?)\)DODEV_SERIAL_XML"', source, re.S)
assert match, 'embedded serial XML literal is missing'
tree = ET.fromstring(match.group(1))
assert tree.tag == 'Window'
actions={x.attrib['OnClicked'] for x in tree.iter() if 'OnClicked' in x.attrib}
assert {'serial.detect','serial.start','serial.stop','serial.apply_filter','serial.export','serial.close_dialog'}<=actions
names={x.attrib['Name'] for x in tree.iter() if 'Name' in x.attrib}
assert {'serial_port','serial_filter_text','serial_filter_mode','serial_polkit','serial_only_matches'}<=names
cpp=(root/'src/EditorSerial.cpp').read_text()
assert 'Rtab_view_add_tab(terminal_tabs_,"Tasks · Serial"' in cpp
assert 'Rfile_dialog_save' in cpp
plugin=(root/'thirdparty/SerialMonitor/src/SerialMonitorPlugin.cpp').read_text()
assert '"Tools","Monitor serie..."' in plugin
print('Serial Raven dialog XML, Tasks view, save dialog and Tools menu contract passed')
