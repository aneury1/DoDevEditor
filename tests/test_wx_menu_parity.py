#!/usr/bin/env python3
"""Checks exact menu hierarchy/labels/command IDs and dispatch coverage, not GUI execution."""
import pathlib, re, xml.etree.ElementTree as ET, json, sys
root=pathlib.Path(sys.argv[1]) if len(sys.argv)>1 else pathlib.Path(__file__).resolve().parents[1]
xml=(root/'resources/EmbeddedUi.h').read_text()
menu=ET.fromstring(xml[xml.index('<MenuBar'):xml.index('</MenuBar>')+10])
expected=[]
def walk(m,path):
    for el in m:
        if el.tag=='Menu': walk(el,path+'|'+el.attrib['Text'])
        elif el.tag=='Separator': expected.append([path,'',''])
        elif el.tag=='MenuItem': expected.append([path,el.attrib['Text'],el.attrib['OnClicked']])
for m in menu: walk(m,m.attrib['Text'])
spec=(root/'src/gui/wx/RavenMenuSpec.inc').read_text()
actual=[json.loads('['+s+']') for s in re.findall(r'^\s*\{(.*)\},$',spec,re.M)]
assert actual==expected,'Menu specification differs from Raven'
commands=(root/'src/gui/wx/WxCommands.inc').read_text()
core=(root/'src/editor/EditorCommands.cpp').read_text()
for _,label,action in expected:
    if not action: continue
    assert ('"'+action+'"' in commands or (action=="search.next" and 'a.rfind("search.",0)' in commands) or ('"'+action+'"' in core and action.startswith(('edit.','selection.')))),action
settings=(root/'src/gui/wx/WxSettings.inc').read_text()
assert 'ui::kThemes' in settings and 'ThemeDefinition::fromXml' in settings
for title in ['General','Editor','Appearance','Terminal','Templates','Keyboard Shortcuts','Plugins']:
    assert 'pane("'+title+'")' in settings
print(f'{sum(bool(a) for _,_,a in actual)} Raven menu entries and {len(actual)-sum(bool(a) for _,_,a in actual)} separators match; settings sections present.')
