#!/usr/bin/env python3
from pathlib import Path
import re,xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[1]
text=(root/'resources/EmbeddedJournalUi.h').read_text()
m=re.search(r'R"([A-Za-z0-9_]{1,16})\((.*?)\)\1"',text,re.S)
assert m
xml=ET.fromstring(m.group(2))
assert xml.tag=='Window'
actions={x.attrib['OnClicked'] for x in xml.iter() if 'OnClicked' in x.attrib}
assert {'journal.connect','journal.disconnect','journal.export','journal.apply_filter','journal.close_dialog'}<=actions
serial=(root/'src/EditorSerial.cpp').read_text()
assert 'serialConsoleEventCb' in serial and 'serialSend' in serial and 'serialTxCommitCb' in serial
symbols=(root/'src/EditorSymbols.cpp').read_text()
assert 'symbols_sidebar_results' in symbols and 'Rtable_set_activated_callback' in symbols
menus=(root/'resources/EmbeddedUi.h').read_text()
assert 'terminal.export_all' in menus and 'terminal.export' in menus
print('Raven serial console, source symbols table, SSH journal dialog, terminal export UI contracts passed')
