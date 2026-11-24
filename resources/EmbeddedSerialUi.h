#pragma once
#include <string_view>
namespace dodev::ui {
// Embedded XML; no external UI files required.
inline constexpr std::string_view kSerialMonitorWindow = R"DODEV_SERIAL_XML(
<Window Title="Serial Monitor · DoDevEditor" Width="790" Height="555" MinWidth="600" MinHeight="470">
  <VBox Spacing="11" Slot.Padding="16,13">
    <HeaderBar Title="Serial Monitor"/>
    <Label Text="Read a serial device and stream its output into TASKS next to Terminal." Wrap="true"/>
    <HBox Spacing="7">
      <Label Text="Port" Width="55" Slot.VAlign="Center"/>
      <TextInput Name="serial_port" Width="245" Placeholder="/dev/ttyUSB0 or COM3"/>
      <ComboBox Name="serial_ports_list" Width="250"/>
      <Button Class="chip" Text="Detect" OnClicked="serial.detect"/>
    </HBox>
    <HBox Spacing="8">
      <Label Text="Baud rate" Slot.VAlign="Center"/>
      <ComboBox Name="serial_baud" Width="132"/>
      <Label Text="Data bits" Slot.VAlign="Center"/>
      <ComboBox Name="serial_data_bits" Width="90"/>
      <Label Text="Parity" Slot.VAlign="Center"/>
      <ComboBox Name="serial_parity" Width="90"/>
      <Label Text="Stop bits" Slot.VAlign="Center"/>
      <ComboBox Name="serial_stop_bits" Width="85"/>
    </HBox>
    <Label Text="Interactive console: click Focus Keyboard in Tasks and type directly (Enter, arrows, Ctrl+C). The command field can send full lines." Wrap="true"/>
    <Label Text="Live log filter" Class="eyebrow"/>
    <HBox Spacing="8">
      <ComboBox Name="serial_filter_mode" Width="145"/>
      <TextInput Name="serial_filter_text" Placeholder="Text or regex pattern" Slot.Fill="1"/>
      <Button Class="chip" Text="Apply filter" OnClicked="serial.apply_filter"/>
    </HBox>
    <HBox Spacing="12">
      <CheckBox Name="serial_case_sensitive" Text="Case-sensitive"/>
      <CheckBox Name="serial_only_matches" Text="Display / export matching lines only"/>
    </HBox>
    <CheckBox Name="serial_polkit" Text="If access is denied, request elevated access via Polkit (Linux)"/>
    <Label Name="serial_detected" Class="muted" Text="Select a serial port. Device permissions remain unchanged." Wrap="true"/>
    <Label Name="serial_status" Class="muted" Text="Not connected" Wrap="true"/>
    <HBox Spacing="8">
      <Button Text="Start capture" Style="Primary" OnClicked="serial.start"/>
      <Button Text="Stop" OnClicked="serial.stop"/>
      <Button Text="Open Tasks" OnClicked="serial.show_tasks"/>
      <Button Text="Export TXT..." OnClicked="serial.export"/>
      <Button Text="Clear capture" OnClicked="serial.clear"/>
      <Spacer Slot.Fill="1"/>
      <Button Text="Close" OnClicked="serial.close_dialog"/>
    </HBox>
    <Label Text="The app never collects your sudo password. If permission is denied, a system Polkit agent may prompt. Configure dialout/uucp group membership as a permanent alternative." Wrap="true" TextSize="11"/>
  </VBox>
</Window>
)DODEV_SERIAL_XML";
}  // namespace dodev::ui
