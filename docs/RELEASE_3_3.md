# DoDevEditor Raven 3.3 — Release notes

## Added

- Extensive 900-line+ Markdown manual in `docs/CUSTOMIZATION_MANUAL.md` describing configuration, themes, fonts, file templates, keybindings, terminal, tasks, plugins and upgrade recovery.
- New `com.dodev.clangformat` C++17 native plugin, with per-plugin JSON settings (`executable`, `style`), a style input prompt, formatting of the active buffer, undo-friendly editor replacement and Edit/Tools menu integration.
- New `com.dodev.docker` C++17 native plugin with CLI argument prompt, containers, images, version, Compose Up/Down, build, logs and run commands in the integrated PTY/ConPTY terminal.
- Additive SDK v1 host function pointers: `get_active_document`, `replace_active_document`, `execute_terminal_command`, and `prompt_input`. No original function pointers were moved.
- Embedded Raven input dialog XML, no external XML required.
- Build targets for all three example native plugins (Hello, clang-format and Docker).
- Two source patches, for original 3.1 and original 3.2 baselines, plus conflict-aware upgrade script preserving local edits and making backups.

## Compatibility and prerequisites

- Plugins are explicit opt-in under Settings → Plugins and need an application restart after enabling.
- The clang-format plugin expects an external trusted `clang-format` executable; it is **not bundled**.
- The Docker plugin expects the Docker CLI and a usable daemon; it does **not** install Docker.
- Plugin settings are stored in plain-text JSON; do not put secrets there.
- The full GUI depends on a compatible GitHub Raven revision and a Vulkan environment.

## Verification

- CMake headless build completed with GCC/C++17 in this environment.
- 12 automated tests passed: services, plugin loader, two new native plugins, terminal PTY, tasks, settings, themes, keybindings, editor commands, XML contract.
- Generated 3.1→3.3 and 3.2→3.3 patches both passed clean `git apply --check` and were applied to pristine extracted baselines.
- Conflict-aware updater was tested with a non-conflicting README local edit (preserved) and a conflicting README title edit (no project sources changed).
- **The full Raven/Vulkan GUI was not compiled or run in this environment**; visual GUI and Windows ConPTY behavior require validation on a machine that can fetch Raven and render windows.
