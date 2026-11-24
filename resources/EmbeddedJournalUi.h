#pragma once
#include <string_view>
namespace dodev::ui {
inline constexpr std::string_view kRemoteJournalWindow = R"JOURNAL(
<Window Title="Remote Journal · DoDevEditor" Width="740" Height="515" MinWidth="570" MinHeight="420">
 <VBox Spacing="10" Slot.Padding="15,13">
  <HeaderBar Title="Remote Systemd Journal (SSH)"/>
  <Label Text="Read systemd journal logs from a remote Linux server using your configured SSH keys." Wrap="true"/>
  <HBox Spacing="8">
   <Label Text="Hostname / IP" Slot.VAlign="Center"/>
   <TextInput Name="journal_host" Width="265" Placeholder="server.example.com"/>
   <Label Text="SSH user" Slot.VAlign="Center"/>
   <TextInput Name="journal_user" Width="170" Placeholder="operator"/>
  </HBox>
  <HBox Spacing="8">
   <Label Text="SSH port" Slot.VAlign="Center"/>
   <TextInput Name="journal_port" Width="85" Placeholder="22"/>
   <Label Text="Unit" Slot.VAlign="Center"/>
   <TextInput Name="journal_unit" Width="265" Placeholder="nginx.service (optional)"/>
   <Label Text="Last lines" Slot.VAlign="Center"/>
   <TextInput Name="journal_lines" Width="95" Placeholder="200"/>
  </HBox>
  <HBox Spacing="12">
   <CheckBox Name="journal_follow" Text="Follow live log (-f)"/>
   <CheckBox Name="journal_sudo" Text="Use sudo -n (non-interactive, if configured)"/>
  </HBox>
  <Label Text="Output filter" Class="eyebrow"/>
  <HBox Spacing="8">
   <ComboBox Name="journal_filter_mode" Width="145"/>
   <TextInput Name="journal_filter_text" Placeholder="Fixed text or regular expression" Slot.Fill="1"/>
   <Button Class="chip" Text="Apply Filter" OnClicked="journal.apply_filter"/>
  </HBox>
  <Label Name="journal_status" Text="Not connected" Wrap="true" Class="muted"/>
  <HBox Spacing="8">
   <Button Text="Connect" Style="Primary" OnClicked="journal.connect"/>
   <Button Text="Disconnect" OnClicked="journal.disconnect"/>
   <Button Text="View Tasks" OnClicked="journal.open_tasks"/>
   <Button Text="Export TXT..." OnClicked="journal.export"/>
   <Button Text="Clear" OnClicked="journal.clear"/>
   <Spacer Slot.Fill="1"/>
   <Button Text="Close" OnClicked="journal.close_dialog"/>
  </HBox>
  <Label Text="Security: validates remote host, user and unit; uses OpenSSH with host-key verification and BatchMode. The application never stores SSH passwords. First connect using ssh in Terminal to accept the host key." Wrap="true" TextSize="11"/>
 </VBox>
</Window>
)JOURNAL";
}
