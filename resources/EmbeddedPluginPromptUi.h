#pragma once
#include <string_view>
namespace dodev::ui {
inline constexpr std::string_view kPluginPromptWindow = R"DODEV_XML(
<Window Title="DoDevEditor · Plugin Command" Width="625" Height="245" MinWidth="440" MinHeight="210">
  <VBox Spacing="12" Slot.Padding="18,14">
    <HeaderBar Title="Plugin command prompt"/>
    <Label Name="plugin_prompt_label" Text="Enter a value" Wrap="true"/>
    <TextInput Name="plugin_prompt_input" Placeholder="Value or command" Slot.Fill="1"/>
    <Label Text="Commands are only executed after confirmation." Class="eyebrow"/>
    <HBox Spacing="10"><Spacer Slot.Fill="1"/><Button Text="Cancel" OnClicked="plugin.prompt.cancel"/><Button Text="Execute / Apply" Style="Primary" OnClicked="plugin.prompt.accept"/></HBox>
  </VBox>
</Window>
)DODEV_XML";
}
