from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
app=(root/'src/EditorApp.cpp').read_text()
contexts=(root/'src/EditorContexts.cpp').read_text()
xml=(root/'resources/EmbeddedUi.h').read_text()
serial=(root/'src/EditorSerial.cpp').read_text()
journal=(root/'src/EditorJournal.cpp').read_text()
ai=(root/'src/EditorAI.cpp').read_text()
assert 'Rwidget_set_context_menu(browser_,explorerContextMenu_)' in contexts
assert 'Rwidget_set_context_menu(tabs_,tabContextMenu_)' in contexts
assert 'Rwidget_set_context_menu(tab->canvas,terminalContextMenu_)' in app
assert 'Rwidget_set_context_menu(d.editor,editorContextMenu_)' in app
for cmd in ['explorer.new_file','explorer.new_folder','explorer.rename','explorer.delete',
            'explorer.copy_path','tab.close_others','tab.close_right','tab.copy_path','editor.format']:
    assert f'"{cmd}"' in contexts and f'"{cmd}"' in app,cmd
for cmd in ['terminal.copy_all','terminal.save_output']:
    assert f'OnClicked="{cmd}"' in xml and f'"{cmd}"' in app
for filename,source in [('serial',serial),('journal',journal),('ai',ai)]:
    for action in [f'{filename}.copy_all' if filename!='ai' else 'ai.copy_output',
                   f'{filename}.export_all' if filename!='ai' else 'ai.export_output']:
        assert f'"{action}"' in source,action
assert 'serialCapture_.exportText(false)' in contexts
assert 'journalCapture_.exportText(false)' in contexts
assert 'screen.fullText()' in contexts
assert 'Rfile_dialog_save(&dialog)' in contexts
print('Context menus and output toolbar contract checks passed')
