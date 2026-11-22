from pathlib import Path
from xml.etree import ElementTree as ET
root=Path(__file__).resolve().parents[1]
source=(root/"resources/EmbeddedPluginPromptUi.h").read_text(encoding="utf-8")
a=source.index('R"DODEV_XML(')+len('R"DODEV_XML(')
b=source.index(')DODEV_XML"',a)
xml=ET.fromstring(source[a:b].strip())
assert xml.tag=="Window"
assert xml.find('.//TextInput[@Name="plugin_prompt_input"]') is not None
assert xml.find('.//Button[@OnClicked="plugin.prompt.accept"]') is not None
assert xml.find('.//Button[@OnClicked="plugin.prompt.cancel"]') is not None
app=(root/"src/EditorApp.cpp").read_text(encoding="utf-8")
assert '"plugin.prompt.accept"' in app and '"plugin.prompt.cancel"' in app
settings=(root/"src/EditorSettings.cpp").read_text(encoding="utf-8")
for ident in ('showPluginPrompt','closePluginPrompt','Rtext_editor_select_all','Rwindow_set_title'):
    assert ident in settings
print('Embedded plugin prompt XML and event routing verified')
