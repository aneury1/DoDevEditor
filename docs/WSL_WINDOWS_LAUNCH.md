# Launching DoDevEditor (WSL) from Windows

DoDevEditor is a Linux GUI application. On Windows 11, use WSL 2 with WSLg to show its window on the Windows desktop. No Windows native build or third-party X server is required when WSLg works.

## Check WSL

In Windows PowerShell:

```powershell
wsl --version
wsl -l -v
wsl --update
```

Replace `Ubuntu` with the exact distribution name shown by `wsl -l -v`.

In the Linux distribution:

```bash
printf 'DISPLAY=%s\nWAYLAND_DISPLAY=%s\n' "$DISPLAY" "$WAYLAND_DISPLAY"
command -v dodeveditor || true
ls -l "$HOME/.local/bin/dodeveditor"
```

Both executable names detach by default. The launcher is optional. If it is not installed, point Windows to the absolute Linux path of the built `DoDevEditor` binary.

## From PowerShell

For an installed editor:

```powershell
wsl.exe -d Ubuntu -- bash -lc '~/.local/bin/dodeveditor'
```

For a development build:

```powershell
wsl.exe -d Ubuntu --exec /home/YOUR_LINUX_USER/projects/DoDevEditor/build/DoDevEditor
```

If the editor uses Vulkan, WSLg and the installed Linux graphics/Vulkan stack must expose a compatible Vulkan device; test with `vulkaninfo --summary` when launch fails.

## Windows desktop shortcut

1. Right-click desktop > New > Shortcut.
2. Use the following as the target, updating distribution and Linux path:

```text
C:\Windows\System32\wsl.exe -d Ubuntu --exec /home/YOUR_LINUX_USER/.local/bin/dodeveditor
```

3. Name it `DoDevEditor (WSL)`.

If `dodeveditor` is unavailable, use the full path to `DoDevEditor` instead. For a Linux application registered through a `.desktop` file, check the distro's application group in the Windows Start menu; WSLg can expose entries there.

## Troubleshooting

- WSL 1: update the distribution to WSL 2 (`wsl --set-version Ubuntu 2`).
- No GUI display: update WSL (`wsl --update`) and restart it (`wsl --shutdown`). Confirm `$DISPLAY` or `$WAYLAND_DISPLAY` is set.
- Binary not found: confirm the Linux path with `realpath ./build/DoDevEditor` or `command -v dodeveditor`.
- Missing library: run the binary with `--foreground` from the WSL terminal and inspect the error; use `ldd` for missing shared objects.
- Vulkan issues: install `vulkan-tools` in the distro and run `vulkaninfo --summary`; update Windows GPU drivers and WSLg if necessary.
- To keep diagnostic logs, run `DoDevEditor --foreground` directly. Use `dodeveditor` for the detached application launcher when present.

Microsoft documentation: https://learn.microsoft.com/windows/wsl/tutorials/gui-apps
