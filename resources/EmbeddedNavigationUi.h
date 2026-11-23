#pragma once
#include <string_view>
namespace dodev::ui {
inline constexpr std::string_view kGoToLineWindow = R"DODEV_XML(
<Window Title="Go to Line / Column" Width="510" Height="190" MinWidth="390" MinHeight="150">
  <VBox Spacing="9" Slot.Padding="16,12">
    <HeaderBar Title="Go to Line · Ctrl+G"/>
    <Label Text="Line number, optionally followed by :column (e.g. 42:8)"/>
    <TextInput Name="goto_position" Placeholder="42:1"/>
    <HBox Spacing="8"><Label Name="goto_status" Text="Lines are numbered from 1" Slot.Fill="1"/><Button Text="Go" Style="Primary" OnClicked="navigation.goto_submit"/><Button Text="Cancel" OnClicked="navigation.goto_close"/></HBox>
  </VBox>
</Window>
)DODEV_XML";
inline constexpr std::string_view kQuickOpenWindow = R"DODEV_XML(
<Window Title="DoDevEditor · Quick Open" Width="750" Height="520" MinWidth="490" MinHeight="340">
  <VBox Spacing="8" Slot.Padding="13,10">
    <HeaderBar Title="Quick Open · Ctrl+P"/>
    <TextInput Name="quick_query" Placeholder="Search project files..."/>
    <ListView Name="quick_list" Slot.Fill="1"/>
    <HBox Spacing="8"><Label Name="quick_status" Text="Type to filter files" Slot.Fill="1"/><Button Text="Open" Style="Primary" OnClicked="navigation.quick_accept"/><Button Text="Close" OnClicked="navigation.quick_close"/></HBox>
  </VBox>
</Window>
)DODEV_XML";
} // namespace dodev::ui
