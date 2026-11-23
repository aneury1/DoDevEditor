#pragma once
#include <string_view>
namespace dodev::ui {
inline constexpr std::string_view kSettingsWindow = R"DODEV_XML(
<Window Title="DoDevEditor · Settings" Width="1020" Height="760" MinWidth="700" MinHeight="560">
  <VBox Spacing="8">
    <HeaderBar Title="Settings · General / Editor / Appearance / Terminal / Plugins"/>
    <TabView Name="settings_sections" Slot.Fill="1">
      <VBox Slot.Title="General" Spacing="10" Slot.Padding="18,14">
        <Label Text="GENERAL" Class="eyebrow" TextSize="16"/>
        <Label Name="settings_config_directory" Text="Configuration folder" Wrap="true"/>
        <CheckBox Name="settings_restore" Text="Restore the previous session"/>
        <CheckBox Name="settings_auto_save" Text="Automatically save documents"/>
        <Label Text="Settings are optional. If a file is missing, built-in defaults are used." Wrap="true"/>
      </VBox>
      <VBox Slot.Title="Editor &amp; Fonts" Spacing="9" Slot.Padding="18,14">
        <Label Text="EDITOR FONT" Class="eyebrow" TextSize="16"/>
        <Label Text="Font file (.ttf/.otf) — leave blank for Raven monospace"/>
        <HBox Spacing="5"><TextInput Name="settings_editor_font" Placeholder="/path/to/JetBrainsMono-Regular.ttf" Slot.Fill="1"/><Button Text="Browse..." OnClicked="settings.browse_editor_font"/></HBox>
        <HBox Spacing="8"><Label Text="Font size"/><TextInput Name="settings_editor_size" Width="72"/><Label Text="Tab width"/><TextInput Name="settings_tab_size" Width="72"/></HBox>
        <CheckBox Name="settings_wrap" Text="Word wrap"/>
        <CheckBox Name="settings_line_numbers" Text="Show line numbers"/>
        <CheckBox Name="settings_insert_spaces" Text="Insert spaces when pressing Tab"/>
        <Label Text="Editor preview (actual font applies when you press Apply):"/>
        <TextEditor Name="settings_font_preview" Height="175"/>
      </VBox>
      <VBox Slot.Title="Appearance" Spacing="9" Slot.Padding="18,14">
        <Label Text="WINDOW FONTS &amp; THEMES" Class="eyebrow" TextSize="16"/>
        <Label Text="User interface font file (optional)"/>
        <HBox Spacing="5"><TextInput Name="settings_ui_font" Placeholder="/path/to/font.ttf" Slot.Fill="1"/><Button Text="Browse..." OnClicked="settings.browse_ui_font"/></HBox>
        <HBox Spacing="6"><Label Text="UI font size"/><TextInput Name="settings_ui_size" Width="72"/><Label Text="Theme file format"/><ComboBox Name="settings_theme_format" Width="130"/></HBox>
        <Label Text="Directory for custom .json/.xml themes"/>
        <HBox Spacing="5"><TextInput Name="settings_theme_directory" Slot.Fill="1"/><Button Text="Folder..." OnClicked="settings.browse_themes"/></HBox>
        <HBox Spacing="7"><Button Text="Create / edit theme" Style="Primary" OnClicked="theme.designer"/><Button Text="Import theme" OnClicked="theme.import"/><Button Text="Export current" OnClicked="theme.export"/></HBox>
      </VBox>
      <VBox Slot.Title="Terminal" Spacing="9" Slot.Padding="18,14">
        <Label Text="TERMINAL" Class="eyebrow" TextSize="16"/>
        <Label Text="Terminal font file (optional)"/>
        <HBox Spacing="5"><TextInput Name="settings_terminal_font" Slot.Fill="1"/><Button Text="Browse..." OnClicked="settings.browse_terminal_font"/></HBox>
        <HBox Spacing="8"><Label Text="Font size"/><TextInput Name="settings_terminal_size" Width="72"/><Label Text="Scrollback lines"/><TextInput Name="settings_scrollback" Width="100"/></HBox>
        <Label Text="Shell profile (empty = system default)"/>
        <TextInput Name="settings_shell" Placeholder="bash, zsh, fish, powershell, pwsh, cmd"/>
      </VBox>
      <VBox Slot.Title="Templates" Spacing="9" Slot.Padding="18,14">
        <Label Text="FILE TEMPLATES" Class="eyebrow" TextSize="16"/>
        <Label Text="Optional directory containing templates.json + .tpl files"/>
        <HBox Spacing="5"><TextInput Name="settings_templates_directory" Slot.Fill="1"/><Button Text="Folder..." OnClicked="settings.browse_templates"/></HBox>
        <Label Text="Extension associations are configured in settings.json (templates.associations)." Wrap="true"/>
        <Button Text="New file from template..." OnClicked="template.show"/>
      </VBox>
      <VBox Slot.Title="Keybindings" Spacing="8" Slot.Padding="18,14">
        <Label Text="KEYBOARD SHORTCUTS" Class="eyebrow" TextSize="16"/>
        <Label Text="Select a command, enter its shortcut and Apply. Supported: Ctrl, Shift, Alt, Super, arrows, F1–F12." Wrap="true"/>
        <TextInput Name="kb_filter" Placeholder="Filter commands (e.g. terminal, navigate, save)"/>
        <ListView Name="kb_command_list" Height="240"/>
        <HBox Spacing="6"><Label Text="Shortcut"/><TextInput Name="kb_shortcut" Width="225" Placeholder="Ctrl+Shift+P"/><Button Text="Record keys" OnClicked="keybindings.record"/><Button Text="Apply" Style="Primary" OnClicked="keybindings.apply"/></HBox>
        <HBox Spacing="6"><Button Text="Unbind" OnClicked="keybindings.unbind"/><Button Text="Reset selected" OnClicked="keybindings.reset"/><Button Text="Reset all" OnClicked="keybindings.reset_all"/></HBox>
        <Label Name="kb_status" Text="Stored in optional keybindings.json. Conflicts are rejected." Wrap="true"/>
      </VBox>
      <VBox Slot.Title="Plugins" Spacing="8" Slot.Padding="18,14">
        <Label Text="NATIVE PLUGINS · RESTART REQUIRED TO ACTIVATE/DEACTIVATE" Class="eyebrow" TextSize="15" Wrap="true"/>
        <CheckBox Name="settings_plugins_enabled" Text="Enable plugin support"/>
        <CheckBox Name="settings_safe_mode" Text="Safe mode — do not execute native plugins"/>
        <Label Text="Plugin directory (contains folders with plugin.json)"/>
        <HBox Spacing="5"><TextInput Name="settings_plugin_directory" Slot.Fill="1"/><Button Text="Folder..." OnClicked="settings.browse_plugins"/></HBox>
        <ListView Name="settings_plugin_list" Height="155"/>
        <HBox Spacing="6"><Button Text="Enable selected" OnClicked="plugin.enable"/><Button Text="Disable selected" OnClicked="plugin.disable"/><Button Text="Rescan" OnClicked="plugin.rescan"/></HBox>
        <HBox Spacing="6"><Label Text="Plugin setting"/><ComboBox Name="settings_plugin_key" Width="240"/><TextInput Name="settings_plugin_value" Width="240"/><Button Text="Save value" OnClicked="plugin.setting.save"/></HBox>
        <Label Text="Plugin binaries are trusted code with your user privileges; explicitly approve each plugin." Wrap="true"/>
      </VBox>
    </TabView>
    <HBox Spacing="8" Slot.Padding="12,8">
      <Label Name="settings_status" Text="Change values and click Apply" Slot.Fill="1" Wrap="true"/>
      <Button Text="Restore defaults" OnClicked="settings.defaults"/>
      <Button Text="Cancel" OnClicked="settings.cancel"/>
      <Button Text="Apply / Save" Style="Primary" OnClicked="settings.apply"/>
    </HBox>
  </VBox>
</Window>
)DODEV_XML";

inline constexpr std::string_view kThemeDesignerWindow = R"DODEV_XML(
<Window Title="DoDevEditor · Theme Designer" Width="910" Height="780" MinWidth="620" MinHeight="560">
  <VBox Spacing="9">
    <HeaderBar Title="Theme Designer · Raven XML + JSON"/>
    <VBox Spacing="8" Slot.Padding="16,12" Slot.Fill="1">
      <HBox Spacing="8"><Label Text="Theme name"/><TextInput Name="designer_name" Width="320"/><Label Text="Base"/><ComboBox Name="designer_base" Width="150"/></HBox>
      <Label Text="Enter colors as #RRGGBB or #RRGGBBAA"/>
      <HBox Spacing="8"><Label Text="Accent" Width="115"/><TextInput Name="designer_accent" Width="170"/><Label Text="Window" Width="115"/><TextInput Name="designer_window" Width="170"/></HBox>
      <HBox Spacing="8"><Label Text="Panel" Width="115"/><TextInput Name="designer_panel" Width="170"/><Label Text="Control" Width="115"/><TextInput Name="designer_control" Width="170"/></HBox>
      <HBox Spacing="8"><Label Text="Input" Width="115"/><TextInput Name="designer_input" Width="170"/><Label Text="Text" Width="115"/><TextInput Name="designer_text" Width="170"/></HBox>
      <HBox Spacing="8"><Label Text="Separator" Width="115"/><TextInput Name="designer_separator" Width="170"/><Label Text="Corner radius" Width="115"/><TextInput Name="designer_radius" Width="170"/></HBox>
      <Label Text="Syntax highlighting"/>
      <HBox Spacing="8"><Label Text="Keyword" Width="115"/><TextInput Name="designer_keyword" Width="170"/><Label Text="String" Width="115"/><TextInput Name="designer_string" Width="170"/></HBox>
      <HBox Spacing="8"><Label Text="Comment" Width="115"/><TextInput Name="designer_comment" Width="170"/></HBox>
      <Label Text="Changes can be previewed immediately and saved in a user-selected location." Wrap="true"/>
      <TextEditor Name="designer_preview" Height="155" ReadOnly="true"/>
      <Label Name="designer_status" Text="Duplicate the current theme and customize it" Wrap="true"/>
    </VBox>
    <HBox Spacing="8" Slot.Padding="16,9"><Button Text="Preview" OnClicked="designer.preview"/><Button Text="Save as JSON..." OnClicked="designer.save_json"/><Button Text="Save as XML..." OnClicked="designer.save_xml"/><Spacer Slot.Fill="1"/><Button Text="Close" OnClicked="designer.close"/></HBox>
  </VBox>
</Window>
)DODEV_XML";

inline constexpr std::string_view kTemplateWindow = R"DODEV_XML(
<Window Title="DoDevEditor · New from Template" Width="810" Height="650" MinWidth="610" MinHeight="480">
  <VBox Spacing="9">
    <HeaderBar Title="New document from template"/>
    <VBox Spacing="9" Slot.Fill="1" Slot.Padding="16,12">
      <HBox Spacing="8"><Label Text="File name"/><TextInput Name="template_filename" Width="285" Placeholder="MyClass.cpp"/><Label Text="Template"/><ComboBox Name="template_selection" Width="250"/></HBox>
      <Label Text="Preview (placeholders filled when the document is created):"/>
      <TextEditor Name="template_preview" Slot.Fill="1" ReadOnly="true"/>
      <Label Name="template_status" Text="Select a template, then create a new unsaved document."/>
    </VBox>
    <HBox Spacing="8" Slot.Padding="16,10"><Spacer Slot.Fill="1"/><Button Text="Cancel" OnClicked="template.cancel"/><Button Text="Create document" Style="Primary" OnClicked="template.create"/></HBox>
  </VBox>
</Window>
)DODEV_XML";
} // namespace dodev::ui
