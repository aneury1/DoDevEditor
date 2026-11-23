#pragma once
#include <array>
#include <string_view>

// Todos los XML se compilan como literales C++17. No requieren ficheros .xml.
namespace dodev::ui {
inline constexpr std::string_view kMainWindow = R"DODEV_XML(
<Window Title="DoDevEditor · Glass Studio" Width="1510" Height="940" MinWidth="960" MinHeight="590" CustomHeader="true">
  <Overlay>
    <Border Name="music_backdrop" Effect="mesh" EffectColor="#3B2A6E" EffectColor2="#0E3B4A" EffectPointer="false" Opacity="0.78"/>
  <VBox Spacing="0">
    <HeaderBar Class="bar" Title="DoDevEditor · Raven Studio" TitleAlign="Left">
      <Label Text="◈   DoDevEditor" TextSize="14" Slot.Padding="8,0"/>
      <Spacer Slot.Fill="1"/>
      <Label Text="RAVEN GUI   ·   C++17" TextSize="11" Slot.Padding="12,0"/>
    </HeaderBar>
    <MenuBar Class="bar" Native="false">
      <Menu Text="Archivo" Name="file_menu">
        <MenuItem Text="Nuevo" OnClicked="file.new" Name="kb_file_new"/>
        <MenuItem Text="Nuevo desde plantilla..." OnClicked="template.show" Name="kb_template_show"/>
        <MenuItem Text="Abrir..." OnClicked="file.open" Name="kb_file_open"/>
        <MenuItem Text="Abrir carpeta..." OnClicked="folder.open" Name="kb_folder_open"/>
        <Separator/>
        <MenuItem Text="Guardar" OnClicked="file.save" Name="kb_file_save"/>
        <MenuItem Text="Guardar como..." OnClicked="file.save_as" Name="kb_file_save_as"/>
        <MenuItem Text="Cerrar pestaña" OnClicked="file.close" Name="kb_file_close"/>
        <Separator/>
        <MenuItem Text="Salir" OnClicked="app.exit" Name="kb_app_exit"/>
      </Menu>
      <Menu Text="Go" Name="go_menu">
        <MenuItem Text="Quick Open..." OnClicked="navigation.quick_open" Name="kb_navigation_quick_open"/>
        <MenuItem Text="Go to Line/Column..." OnClicked="navigation.go_to_line" Name="kb_navigation_go_to_line"/>
        <MenuItem Text="Command Palette..." OnClicked="navigation.command_palette" Name="kb_navigation_command_palette"/>
        <Separator/>
        <MenuItem Text="Keyboard Shortcuts..." OnClicked="keybindings.show" Name="kb_keybindings_show"/>
      </Menu>
      <Menu Text="Edit" Name="edit_menu">
        <MenuItem Text="Undo" OnClicked="edit.undo" Name="kb_edit_undo"/>
        <MenuItem Text="Redo" OnClicked="edit.redo" Name="kb_edit_redo"/>
        <Separator/>
        <MenuItem Text="Cut" OnClicked="edit.cut" Name="kb_edit_cut"/>
        <MenuItem Text="Copy" OnClicked="edit.copy" Name="kb_edit_copy"/>
        <MenuItem Text="Paste" OnClicked="edit.paste" Name="kb_edit_paste"/>
        <MenuItem Text="Delete" OnClicked="edit.delete" Name="kb_edit_delete"/>
        <Separator/>
        <MenuItem Text="Find" OnClicked="search.show" Name="kb_search_show"/>
        <MenuItem Text="Find Next" OnClicked="search.next" Name="kb_search_next"/>
        <MenuItem Text="Find Previous" OnClicked="search.prev" Name="kb_search_prev"/>
        <MenuItem Text="Replace" OnClicked="search.replace_show" Name="kb_search_replace_show"/>
        <MenuItem Text="Replace All (Current File)" OnClicked="search.replace_all" Name="kb_search_replace_all"/>
        <MenuItem Text="Find in Workspace" OnClicked="edit.find_workspace" Name="kb_edit_find_workspace"/>
        <Separator/>
        <MenuItem Text="Toggle Line Comment" OnClicked="edit.toggle_line_comment" Name="kb_edit_toggle_line_comment"/>
        <MenuItem Text="Toggle Block Comment" OnClicked="edit.toggle_block_comment" Name="kb_edit_toggle_block_comment"/>
        <Menu Text="Transform Text">
          <MenuItem Text="Uppercase" OnClicked="edit.uppercase" Name="kb_edit_uppercase"/>
          <MenuItem Text="Lowercase" OnClicked="edit.lowercase" Name="kb_edit_lowercase"/>
          <MenuItem Text="Title Case" OnClicked="edit.titlecase" Name="kb_edit_titlecase"/>
        </Menu>
        <Menu Text="Lines">
          <MenuItem Text="Indent Lines" OnClicked="edit.indent" Name="kb_edit_indent"/>
          <MenuItem Text="Outdent Lines" OnClicked="edit.outdent" Name="kb_edit_outdent"/>
          <MenuItem Text="Insert Line Above" OnClicked="edit.insert_above" Name="kb_edit_insert_above"/>
          <MenuItem Text="Insert Line Below" OnClicked="edit.insert_below" Name="kb_edit_insert_below"/>
          <MenuItem Text="Delete Line" OnClicked="edit.delete_line" Name="kb_edit_delete_line"/>
          <MenuItem Text="Join Lines" OnClicked="edit.join_lines" Name="kb_edit_join_lines"/>
          <MenuItem Text="Trim Trailing Whitespace" OnClicked="edit.trim_whitespace" Name="kb_edit_trim_whitespace"/>
          <MenuItem Text="Sort Lines Ascending" OnClicked="edit.sort_asc" Name="kb_edit_sort_asc"/>
          <MenuItem Text="Sort Lines Descending" OnClicked="edit.sort_desc" Name="kb_edit_sort_desc"/>
        </Menu>
      </Menu>
      <Menu Text="Selection" Name="selection_menu">
        <MenuItem Text="Select All" OnClicked="selection.all" Name="kb_selection_all"/>
        <MenuItem Text="Select Line" OnClicked="selection.line" Name="kb_selection_line"/>
        <MenuItem Text="Select Word" OnClicked="selection.word" Name="kb_selection_word"/>
        <MenuItem Text="Expand Selection" OnClicked="selection.expand" Name="kb_selection_expand"/>
        <MenuItem Text="Shrink Selection" OnClicked="selection.shrink" Name="kb_selection_shrink"/>
        <MenuItem Text="Select Next Occurrence" OnClicked="selection.next_occurrence" Name="kb_selection_next_occurrence"/>
        <MenuItem Text="Select Previous Occurrence" OnClicked="selection.prev_occurrence" Name="kb_selection_prev_occurrence"/>
        <Separator/>
        <MenuItem Text="Copy Line Up" OnClicked="edit.copy_line_up" Name="kb_edit_copy_line_up"/>
        <MenuItem Text="Copy Line Down" OnClicked="edit.copy_line_down" Name="kb_edit_copy_line_down"/>
        <MenuItem Text="Move Line Up" OnClicked="edit.move_line_up" Name="kb_edit_move_line_up"/>
        <MenuItem Text="Move Line Down" OnClicked="edit.move_line_down" Name="kb_edit_move_line_down"/>
        <MenuItem Text="Duplicate Selection" OnClicked="edit.duplicate" Name="kb_edit_duplicate"/>
        <Separator/>
        <MenuItem Text="Select to Line Start" OnClicked="selection.line_start" Name="kb_selection_line_start"/>
        <MenuItem Text="Select to Line End" OnClicked="selection.line_end" Name="kb_selection_line_end"/>
        <MenuItem Text="Select to Beginning of File" OnClicked="selection.to_start" Name="kb_selection_to_start"/>
        <MenuItem Text="Select to End of File" OnClicked="selection.to_end" Name="kb_selection_to_end"/>
        <MenuItem Text="Swap Anchor and Caret" OnClicked="selection.swap_anchor" Name="kb_selection_swap_anchor"/>
        <MenuItem Text="Move Cursor Up" OnClicked="selection.cursor_up" Name="kb_selection_cursor_up"/>
        <MenuItem Text="Move Cursor Down" OnClicked="selection.cursor_down" Name="kb_selection_cursor_down"/>
      </Menu>
      <Menu Text="Terminal" Name="terminal_menu">
        <MenuItem Text="New Terminal" OnClicked="terminal.new" Name="kb_terminal_new"/>
        <MenuItem Text="Split Terminal" OnClicked="terminal.split" Name="kb_terminal_split"/>
        <MenuItem Text="Kill Terminal" OnClicked="terminal.close" Name="kb_terminal_close"/>
        <MenuItem Text="Restart Terminal" OnClicked="terminal.restart" Name="kb_terminal_restart"/>
        <MenuItem Text="Next Terminal" OnClicked="terminal.next" Name="kb_terminal_next"/>
        <MenuItem Text="Previous Terminal" OnClicked="terminal.previous" Name="kb_terminal_previous"/>
        <MenuItem Text="Rename Terminal" OnClicked="terminal.rename" Name="kb_terminal_rename"/>
        <MenuItem Text="Focus Terminal" OnClicked="terminal.focus" Name="kb_terminal_focus"/>
        <MenuItem Text="Show Terminal Panel" OnClicked="panel.output" Name="kb_panel_output"/>
        <Separator/>
        <MenuItem Text="Run Task..." OnClicked="terminal.run_task" Name="kb_terminal_run_task"/>
        <MenuItem Text="Run Build Task" OnClicked="terminal.run_build" Name="kb_terminal_run_build"/>
        <MenuItem Text="Run Active File" OnClicked="terminal.run_file" Name="kb_terminal_run_file"/>
        <MenuItem Text="Run Selected Text" OnClicked="terminal.run_selected" Name="kb_terminal_run_selected"/>
        <MenuItem Text="Configure Tasks..." OnClicked="terminal.configure_tasks" Name="kb_terminal_configure_tasks"/>
        <MenuItem Text="Configure Default Build Task..." OnClicked="terminal.configure_build" Name="kb_terminal_configure_build"/>
        <Separator/>
        <MenuItem Text="Copy" OnClicked="terminal.copy" Name="kb_terminal_copy"/>
        <MenuItem Text="Paste" OnClicked="terminal.paste" Name="kb_terminal_paste"/>
        <MenuItem Text="Clear" OnClicked="terminal.clear" Name="kb_terminal_clear"/>
        <MenuItem Text="Interrupt (Ctrl+C)" OnClicked="terminal.interrupt" Name="kb_terminal_interrupt"/>
      </Menu>
      <Menu Text="Herramientas" Name="tools_menu">
        <MenuItem Text="Abrir CSV" OnClicked="tool.csv" Name="kb_tool_csv"/>
        <MenuItem Text="Abrir DLT" OnClicked="tool.dlt" Name="kb_tool_dlt"/>
        <MenuItem Text="Terminal integrada" OnClicked="panel.output"/>
        <MenuItem Text="Historial Git" OnClicked="git.refresh" Name="kb_git_refresh"/>
        <MenuItem Text="Data Server" OnClicked="tool.data_server" Name="kb_tool_data_server"/>
      </Menu>
      <Menu Text="Ver" Name="view_menu">
        <MenuItem Text="Explorador" OnClicked="panel.explorer" Name="kb_panel_explorer"/>
        <MenuItem Text="Git" OnClicked="panel.git" Name="kb_panel_git"/>
        <MenuItem Text="Terminal" OnClicked="panel.output"/>
        <MenuItem Text="Siguiente tema" OnClicked="theme.next" Name="kb_theme_next"/>
        <MenuItem Text="Preferencias..." OnClicked="settings.show" Name="kb_settings_show"/>
        <MenuItem Text="Diseñador de temas..." OnClicked="theme.designer" Name="kb_theme_designer"/>
      </Menu>
      <Menu Text="Plugins" Name="plugins_menu">
        <MenuItem Text="Gestionar plugins..." OnClicked="settings.show"/>
      </Menu>
      <Menu Text="Ayuda">
        <MenuItem Text="Acerca de" OnClicked="app.about" Name="kb_app_about"/>
      </Menu>
    </MenuBar>
    <Border Class="glass-toolbar music-toolbar bar" Padding="10,8" CornerRadius="14" BorderWidth="0" Slot.Padding="4,4">
      <HBox Spacing="7">
        <Button Class="music-primary round" Text="＋  Nuevo" Style="Primary" OnClicked="file.new"/>
        <Button Class="chip" Text="Abrir" Style="Flat" OnClicked="file.open"/>
        <Button Class="chip" Text="Carpeta" Style="Flat" OnClicked="folder.open"/>
        <Button Class="chip" Text="Guardar" OnClicked="file.save"/>
        <Button Class="chip" Text="Settings" Style="Flat" OnClicked="settings.show"/>
        <Separator Orientation="Vertical"/>
        <Button Class="icon" Text="↶" Tooltip="Deshacer" Style="Flat" OnClicked="edit.undo"/>
        <Button Class="icon" Text="↷" Tooltip="Rehacer" Style="Flat" OnClicked="edit.redo"/>
        <Button Class="chip" Text="Buscar" Style="Flat" OnClicked="search.show"/>
        <Spacer Slot.Fill="1"/>
        <Label Class="eyebrow" Text="TEMA" TextSize="11" Slot.VAlign="Center"/>
        <ComboBox Class="music-combo chip" Name="themes" Width="190"/>
      </HBox>
    </Border>
    <VBox Class="music-search" Name="searchbar" Visibility="Collapsed" Spacing="5" Slot.Padding="10,4">
      <HBox Spacing="7">
        <TextInput Class="music-search-input" Name="search_text" Width="270" Placeholder="Buscar en archivo..." OnCommitted="search.next"/>
        <TextInput Class="music-search-input" Name="replace_text" Width="220" Placeholder="Reemplazar por..."/>
        <Button Class="chip" Text="Siguiente" OnClicked="search.next"/>
        <Button Class="chip" Text="Reemplazar" OnClicked="search.replace"/>
        <Button Class="chip" Text="Todo" OnClicked="search.replace_all"/>
        <Button Class="icon" Text="×" Style="Flat" OnClicked="search.hide"/>
      </HBox>
    </VBox>
    <DockArea Name="dock" Slot.Fill="1" CloseHides="true">
      <TabView Class="music-tabs" Name="documents" Slot.Title="EDITOR" Slot.Closable="false" Documents="true"/>
      <VBox Name="explorer_pane" Class="glass-sidebar music-surface surface" Spacing="5" Slot.Title="ARCHIVOS" Slot.Dock="Left" Slot.DockSize="0.23" Slot.Closable="false">
        <HBox Spacing="5" Slot.Padding="7,5">
          <Label Class="eyebrow" Text="EXPLORADOR" TextSize="11" Slot.Fill="1" Slot.VAlign="Center"/>
          <Button Class="icon" Text="↻" Style="Flat" OnClicked="folder.refresh"/>
        </HBox>
        <Label Class="muted" Name="folder_label" Text="Sin carpeta" TextSize="11" Wrap="true" Slot.Padding="7,2"/>
        <FileBrowser Class="music-browser" Name="files" Controls="false" Places="false" Slot.Fill="1"/>
      </VBox>
      <VBox Name="git_pane" Class="glass-sidebar music-surface surface" Spacing="5" Slot.Title="GIT" Slot.Dock="Tab" Slot.DockTo="explorer_pane">
        <HBox Spacing="4" Slot.Padding="7,5">
          <Label Class="eyebrow" Text="COMMITS" Slot.Fill="1" TextSize="11" Slot.VAlign="Center"/>
          <Button Class="icon" Text="↻" Style="Flat" OnClicked="git.refresh"/>
        </HBox>
        <ListView Class="music-list" Name="git_commits" Slot.Fill="1" OnActivated="git.details"/>
        <TextEditor Class="music-editor" Name="git_detail" ReadOnly="true" Wrap="true" Height="145"/>
      </VBox>
      <VBox Name="output_pane" Class="glass-terminal music-surface surface" Spacing="3" Slot.Title="TERMINAL" Slot.Dock="Bottom" Slot.DockSize="0.35">
        <HBox Spacing="7" Slot.Padding="9,6">
          <Label Text="●" Color.Accent="#55E5AD" TextSize="15" Slot.VAlign="Center"/>
          <Label Class="eyebrow" Text="TERMINAL INTEGRADA" TextSize="12" Slot.VAlign="Center"/>
          <Label Class="muted" Name="terminal_state" Text="Sin sesión" TextSize="11" Slot.VAlign="Center"/>
          <Label Class="eyebrow" Text="SHELL" TextSize="10" Slot.VAlign="Center"/>
          <ComboBox Class="music-combo chip" Name="terminal_profile" Width="145"/>
          <Spacer Slot.Fill="1"/>
          <Button Class="icon" Text="＋" Style="Flat" Tooltip="Nueva terminal (Ctrl+Shift+N dentro del panel)" OnClicked="terminal.new"/>
          <Button Class="icon" Text="×" Style="Flat" Tooltip="Cerrar terminal activa" OnClicked="terminal.close"/>
          <Button Class="icon" Text="⏹" Style="Flat" Tooltip="Enviar Ctrl+C" OnClicked="terminal.interrupt"/>
          <Button Class="chip" Text="Copiar" Style="Flat" Tooltip="Copiar selección" OnClicked="terminal.copy"/>
          <Button Class="chip" Text="Pegar" Style="Flat" Tooltip="Pegar desde el portapapeles" OnClicked="terminal.paste"/>
          <Button Class="chip" Text="Limpiar" Style="Flat" OnClicked="terminal.clear"/>
        </HBox>
        <HBox Spacing="6" Slot.Padding="9,3">
          <Label Text="TASKS" TextSize="10" Slot.VAlign="Center"/>
          <ComboBox Name="terminal_task_select" Class="music-combo chip" Width="245"/>
          <Button Class="chip" Text="Run Task" OnClicked="terminal.run_task"/>
          <Button Class="chip" Text="Build" OnClicked="terminal.run_build"/>
          <Button Class="icon" Text="⊞" Tooltip="Split terminal" OnClicked="terminal.split"/>
          <Spacer Slot.Fill="1"/>
          <TextInput Name="terminal_name" Class="music-search-input" Width="145" Placeholder="Terminal name" OnCommitted="terminal.rename"/>
          <Button Class="chip" Text="Rename" OnClicked="terminal.rename"/>
        </HBox>
        <TabView Class="music-tabs" Name="terminal_tabs" Documents="true" Slot.Fill="1"/>
        <HBox Spacing="4" Slot.Padding="9,3">
          <Label Text="PTY / ConPTY · Escribe directamente · Ctrl+Shift+C/V copiar y pegar · Ctrl+C interrumpir" TextSize="10" Slot.Fill="1"/>
        </HBox>
      </VBox>
    </DockArea>
    <Border Class="glass-status music-status bar" Padding="10,6" BorderWidth="0" CornerRadius="0">
      <HBox Spacing="14">
        <Label Name="status" Text="Listo" Slot.Fill="1" TextSize="11"/>
        <Label Name="cursor" Text="Ln 1 : Col 1" TextSize="11"/>
        <Label Text="UTF-8   ·   Raven" TextSize="11"/>
      </HBox>
    </Border>
  </VBox>
  </Overlay>
</Window>
)DODEV_XML";

inline constexpr std::string_view kMidnight = R"DODEV_XML(<Theme Name="Midnight" Base="Dark" Accent="#579AFF" CornerRadius="8"><Colors Window="#0C1020" Panel="#131B2E" Control="#202D46" Input="#0F1930" Text="#E7F0FF" TextPlaceholder="#7C90B3" Separator="#2D3A54"/><Metrics ControlHeight="32" Spacing="7" FontSize="13"/><Syntax Keyword="#81A8FF" String="#96D8A3" Comment="#7890AC" Number="#F4B477"/></Theme>)DODEV_XML";
inline constexpr std::string_view kGraphite = R"DODEV_XML(<Theme Name="Graphite" Base="Dark" Accent="#B9B9C9" CornerRadius="6"><Colors Window="#141518" Panel="#1D1E23" Control="#2B2D32" Input="#191A1F" Text="#E8E8EC" Separator="#383B43"/><Metrics ControlHeight="32" Spacing="7"/><Syntax Keyword="#A3B8E8" String="#B5CC97" Comment="#8E939D"/></Theme>)DODEV_XML";
inline constexpr std::string_view kAurora = R"DODEV_XML(<Theme Name="Aurora" Base="Dark" Accent="#33D5BD" CornerRadius="12"><Colors Window="#091B21" Panel="#10282D" Control="#173D41" Input="#0A2227" Text="#E0FFF6" Separator="#255259"/><Metrics ControlHeight="34" Spacing="8"/><Syntax Keyword="#76E1CE" String="#FFD484" Comment="#76A3A4"/></Theme>)DODEV_XML";
inline constexpr std::string_view kAmethyst = R"DODEV_XML(<Theme Name="Amethyst" Base="Dark" Accent="#BD93F9" CornerRadius="10"><Colors Window="#15101F" Panel="#221B31" Control="#342A49" Input="#1A1527" Text="#F2EDFF" Separator="#493C60"/><Metrics ControlHeight="34" Spacing="8"/><Syntax Keyword="#D7ABFF" String="#91E3C9" Comment="#9C8CAF"/></Theme>)DODEV_XML";
inline constexpr std::string_view kPaper = R"DODEV_XML(<Theme Name="Paper" Base="Light" Accent="#3768D0" CornerRadius="9"><Colors Window="#F7F8FB" Panel="#FFFFFF" Control="#EAEFF6" Input="#FFFFFF" Text="#263249" Separator="#D7E0EC"/><Metrics ControlHeight="33" Spacing="8"/><Syntax Keyword="#3C65B6" String="#13815F" Comment="#748399"/></Theme>)DODEV_XML";
// Los dos nuevos temas aprovechan el material *real* Glass de Raven, no una simulacion CSS.
// Base GlassDark incorpora la refraccion, sombras y desenfoque nativos del renderer.
inline constexpr std::string_view kGlassStudio = R"DODEV_XML(
<Theme Name="Glass Studio" Base="GlassDark" Accent="#7E9DFF" Material="TintedGlass" Wallpaper="Dark" CornerRadius="15">
  <Colors Window="#0C1025" Panel="#1A2342C8" Control="#38476EAF" Input="#111B34D8" Text="#EFF3FF" TextPlaceholder="#A2B3D7" Separator="#B3C3FF38"/>
  <Metrics ControlHeight="36" Spacing="9" FontSize="13"/>
  <Glass Frost="0.78" Refraction="1.10" Saturation="1.22" Rim="1.10" Shadow="0.95"/>
  <Syntax Keyword="#A8B6FF" String="#8EE4BF" Comment="#93A2C5" Number="#F6C38A"/>
  <Style Class="glass-toolbar" Material="Glass" CornerRadius="15" Panel="#23345B9C"/>
  <Style Class="glass-sidebar" Material="TintedGlass" CornerRadius="16" Panel="#1A2745C4"/>
  <Style Class="glass-terminal" Material="TintedGlass" CornerRadius="16" Panel="#101D35D8"/>
  <Style Class="glass-console" Material="Solid" Panel="#080F21"/>
  <Style Class="glass-status" Material="FrostedGlass" Panel="#1C2940A0"/>
</Theme>
)DODEV_XML";
inline constexpr std::string_view kGlassSpotify = R"DODEV_XML(
<Theme Name="Glass Spotify" Base="GlassDark" Accent="#1ED760" Material="TintedGlass" Wallpaper="Dark" CornerRadius="18">
  <Colors Window="#080D0B" Panel="#15221BC4" Control="#274A36B0" Input="#0D2115E2" Text="#F0FFF3" TextPlaceholder="#91B99C" Separator="#74E49B38"/>
  <Metrics ControlHeight="37" Spacing="9" FontSize="13"/>
  <Glass Frost="0.88" Refraction="0.95" Saturation="1.16" Rim="0.95" Shadow="1.15"/>
  <Syntax Keyword="#79F5A0" String="#A6E6C0" Comment="#78A18A" Number="#FFE0A3"/>
  <Style Class="glass-toolbar" Material="Glass" CornerRadius="18" Panel="#1838259C"/>
  <Style Class="glass-sidebar" Material="TintedGlass" CornerRadius="17" Panel="#112C1AC4"/>
  <Style Class="glass-terminal" Material="TintedGlass" CornerRadius="17" Panel="#091A11DC"/>
  <Style Class="glass-console" Material="Solid" Panel="#080F0A"/>
  <Style Class="glass-status" Material="FrostedGlass" Panel="#142D1FA0"/>
</Theme>
)DODEV_XML";

inline constexpr std::string_view kMusicNocturne = R"DODEV_XML(
<Theme Name="Music Nocturne" Base="Dark" Accent="#B79CFF" CornerRadius="8">
  <Colors Window="#060609" Panel="#111117" Popup="#1B1B24" PopupBorder="#FFFFFF1A" Header="#060609"
    Input="#1C1C25" Text="#F4F4F8" TextPlaceholder="#8A8A99" Separator="#FFFFFF14" Border="#FFFFFF12"
    Control="#FFFFFF12" ControlHovered="#FFFFFF1F" ControlPressed="#FFFFFF2B" ControlBorder="#00000000"
    RowHovered="#FFFFFF10" Track="#FFFFFF33" Handle="#FFFFFF" HandleHovered="#FFFFFF" Tooltip="#26262F"/>
  <Metrics ControlHeight="34" FontSize="14" SliderHandleSize="12" Spacing="8"/>
  <Syntax Keyword="#B7A5FF" Type="#82BEFF" Function="#E8C3FF" String="#9FDDB5" Number="#F2C58D" Comment="#808094"/>
  <Style Class="surface" Background="#0D0D12C8" BorderColor="#FFFFFF0D" BorderWidth="1"/>
  <Style Class="bar" Background="#050508E6"/>
  <Style Class="music-toolbar" Background="#14141BBE" BorderColor="#FFFFFF16" BorderWidth="1" CornerRadius="14"/>
  <Style Class="music-status" Background="#0A0A10F2"/>
  <Style Class="music-search" Background="#191922D8" CornerRadius="12"/>
  <Style Class="music-search-input" Input="#1C1C25" ControlBorder="#FFFFFF16" CornerRadius="100"/>
  <Style Class="music-browser" Panel="#111117" RowHovered="#FFFFFF10"/>
  <Style Class="music-list" Panel="#111117" RowHovered="#FFFFFF10"/>
  <Style Class="music-editor" Panel="#0C0C12" Input="#0C0C12" Text="#F4F4F8"/>
  <Style Class="music-tabs" Panel="#111117" Control="#FFFFFF12" ControlHovered="#FFFFFF24"/>
  <Style Widget="Label" Class="muted" Text="#A3A3B4"/>
  <Style Widget="Label" Class="eyebrow" Text="#D6D6E2" FontSize="12"/>
  <Style Widget="Button" Class="icon" Text="#C4C4D2" CornerRadius="100"/>
  <Style Widget="Button" Class="icon" State="Hovered" Foreground="#FFFFFF"/>
  <Style Widget="Button" Class="round" CornerRadius="100"/>
  <Style Widget="Button" Class="chip" CornerRadius="100" Control="#FFFFFF14" ControlHovered="#FFFFFF24"/>
  <Style Widget="Button" Class="music-primary" CornerRadius="100" Accent="#B79CFF" AccentText="#060609"/>
  <Style Widget="ComboBox" Class="music-combo" CornerRadius="100" Control="#FFFFFF14" ControlHovered="#FFFFFF24"/>
</Theme>
)DODEV_XML";

inline constexpr std::string_view kMusicAuroraGlass = R"DODEV_XML(
<Theme Name="Music Aurora Glass" Base="GlassDark" Accent="#7DD3FC" Material="FrostedGlass" CornerRadius="12">
  <Colors Window="#080912" Panel="#161A29CC" Header="#0B1021C8" Text="#F7F7FB" TextPlaceholder="#B4B4C6"
    Separator="#FFFFFF1F" Border="#FFFFFF1C" Control="#FFFFFF1A" ControlHovered="#FFFFFF2A"
    ControlPressed="#FFFFFF3B" RowHovered="#FFFFFF14" Track="#FFFFFF38"/>
  <Metrics ControlHeight="34" FontSize="14" SliderHandleSize="12" Spacing="8"/>
  <Glass Frost="0.7" Refraction="0.6" Saturation="1.5" Rim="1.0" Shadow="1.0"/>
  <Syntax Keyword="#A1D8FF" Type="#9FADFF" Function="#D7B5F4" String="#ACF2D8" Number="#F5D69C" Comment="#9BA7C5"/>
  <Style Class="surface" Material="FrostedGlass" CornerRadius="14"/>
  <Style Class="bar" Material="FrostedGlass"/>
  <Style Class="music-toolbar" Material="FrostedGlass" CornerRadius="14"/>
  <Style Class="music-status" Material="TintedGlass"/>
  <Style Class="music-search" Material="FrostedGlass" CornerRadius="14"/>
  <Style Class="music-search-input" Input="#17223DC4" CornerRadius="100"/>
  <Style Class="music-browser" Material="TintedGlass"/>
  <Style Class="music-list" Material="TintedGlass"/>
  <Style Class="music-editor" Material="Solid" Panel="#0C1221" Input="#0C1221"/>
  <Style Class="music-tabs" Material="TintedGlass" CornerRadius="10"/>
  <Style Widget="Label" Class="muted" Text="#C8C8D8"/>
  <Style Widget="Label" Class="eyebrow" Text="#E4E4EE" FontSize="12"/>
  <Style Widget="Button" Class="icon" Text="#DCDCE8" CornerRadius="100"/>
  <Style Widget="Button" Class="round" CornerRadius="100"/>
  <Style Widget="Button" Class="chip" CornerRadius="100" Control="#FFFFFF1C" ControlHovered="#FFFFFF31"/>
  <Style Widget="Button" Class="music-primary" CornerRadius="100" Accent="#7DD3FC" AccentText="#06121C"/>
  <Style Widget="ComboBox" Class="music-combo" CornerRadius="100"/>
</Theme>
)DODEV_XML";

inline constexpr std::string_view kMusicDaylight = R"DODEV_XML(
<Theme Name="Music Daylight" Base="Light" Accent="#7C3AED" CornerRadius="8">
  <Colors Window="#F3F1EC" Panel="#FFFFFF" Header="#F3F1EC" Text="#16161D" TextPlaceholder="#7A7A88"
    Separator="#0000001A" Border="#00000017" Control="#0000000D" ControlHovered="#00000017"
    ControlPressed="#00000021" ControlBorder="#00000000" RowHovered="#0000000D" Track="#00000026"
    Handle="#16161D" HandleHovered="#16161D"/>
  <Metrics ControlHeight="34" FontSize="14" SliderHandleSize="12" Spacing="8"/>
  <Syntax Keyword="#6E39AE" Type="#365CAB" Function="#8F4AA7" String="#176B55" Number="#996515" Comment="#84848F"/>
  <Style Class="surface" Background="#FFFFFFD9" BorderColor="#0000000F" BorderWidth="1"/>
  <Style Class="bar" Background="#FFFFFFE6"/>
  <Style Class="music-toolbar" Background="#FFFFFFE6" BorderColor="#00000014" BorderWidth="1" CornerRadius="12"/>
  <Style Class="music-status" Background="#F4F1EBE8"/>
  <Style Class="music-search" Background="#FFFFFF" BorderColor="#00000014" BorderWidth="1" CornerRadius="12"/>
  <Style Class="music-search-input" Input="#FFFFFF" CornerRadius="100"/>
  <Style Class="music-browser" Panel="#FFFFFF" RowHovered="#0000000D"/>
  <Style Class="music-list" Panel="#FFFFFF" RowHovered="#0000000D"/>
  <Style Class="music-editor" Panel="#FFFFFF" Input="#FFFFFF" Text="#16161D"/>
  <Style Class="music-tabs" Panel="#FFFFFF" Control="#0000000D" ControlHovered="#00000017"/>
  <Style Widget="Label" Class="muted" Text="#5E5E6C"/>
  <Style Widget="Label" Class="eyebrow" Text="#3A3A46" FontSize="12"/>
  <Style Widget="Button" Class="icon" Text="#3A3A46" CornerRadius="100"/>
  <Style Widget="Button" Class="round" CornerRadius="100"/>
  <Style Widget="Button" Class="chip" CornerRadius="100" Control="#0000000D" ControlHovered="#00000017"/>
  <Style Widget="Button" Class="music-primary" CornerRadius="100" Accent="#7C3AED" AccentText="#FFFFFF"/>
  <Style Widget="ComboBox" Class="music-combo" CornerRadius="100"/>
</Theme>
)DODEV_XML";

inline constexpr std::string_view kCopper = R"DODEV_XML(<Theme Name="Copper" Base="Dark" Accent="#ED9B62" CornerRadius="9"><Colors Window="#1D1512" Panel="#2B211B" Control="#48372B" Input="#251B17" Text="#FFF1DB" Separator="#594536"/><Metrics ControlHeight="33" Spacing="8"/><Syntax Keyword="#F3B777" String="#C4DEA1" Comment="#AC937C"/></Theme>)DODEV_XML";

inline constexpr std::string_view kVSCodeDarkModern = R"DODEV_XML(
<Theme Name="VS Code Dark Modern" Base="Dark" Accent="#007ACC" CornerRadius="5">
 <Colors Window="#181818" Panel="#1F1F1F" Header="#181818" Control="#2D2D30" Input="#252526" Text="#D4D4D4" TextPlaceholder="#858585" Separator="#333333" ControlHovered="#36363B" RowHovered="#2A2D2E"/>
 <Metrics FontSize="13" ControlHeight="31" Spacing="6"/>
 <Syntax Keyword="#569CD6" Type="#4EC9B0" Function="#DCDCAA" String="#CE9178" Number="#B5CEA8" Comment="#6A9955"/>
 <Style Class="music-editor" Panel="#1F1F1F" Input="#1F1F1F" Text="#D4D4D4"/>
 <Style Class="music-toolbar" Background="#252526" CornerRadius="5"/>
 <Style Class="music-tabs" Panel="#252526" Control="#2D2D30" ControlHovered="#383838"/>
 <Style Widget="Button" Class="chip" CornerRadius="4"/>
</Theme>
)DODEV_XML";
inline constexpr std::string_view kVSCodeLightModern = R"DODEV_XML(
<Theme Name="VS Code Light Modern" Base="Light" Accent="#007ACC" CornerRadius="5">
 <Colors Window="#F3F3F3" Panel="#FFFFFF" Header="#F3F3F3" Control="#EDEDED" Input="#FFFFFF" Text="#333333" TextPlaceholder="#777777" Separator="#D4D4D4" ControlHovered="#E5E5E5" RowHovered="#E8E8E8"/>
 <Metrics FontSize="13" ControlHeight="31" Spacing="6"/>
 <Syntax Keyword="#0000FF" Type="#267F99" Function="#795E26" String="#A31515" Number="#098658" Comment="#008000"/>
 <Style Class="music-editor" Panel="#FFFFFF" Input="#FFFFFF" Text="#333333"/>
 <Style Class="music-toolbar" Background="#F3F3F3" CornerRadius="5"/>
 <Style Class="music-tabs" Panel="#EDEDED" Control="#EDEDED" ControlHovered="#E1E1E1"/>
 <Style Widget="Button" Class="chip" CornerRadius="4"/>
</Theme>
)DODEV_XML";

struct Theme { std::string_view name; std::string_view xml; };
inline constexpr std::array<Theme, 13> kThemes{{
    {"Midnight", kMidnight}, {"Graphite", kGraphite}, {"Aurora", kAurora},
    {"Amethyst", kAmethyst}, {"Paper", kPaper}, {"Copper", kCopper},
    {"Glass Studio", kGlassStudio}, {"Glass Spotify", kGlassSpotify},
    {"Music Nocturne", kMusicNocturne}, {"Music Aurora Glass", kMusicAuroraGlass},
    {"Music Daylight", kMusicDaylight},
    {"VS Code Dark Modern", kVSCodeDarkModern}, {"VS Code Light Modern", kVSCodeLightModern}
}};
} // namespace dodev::ui
