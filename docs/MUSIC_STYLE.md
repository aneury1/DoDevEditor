# Music-style widget skin for DoDevEditor

This release is a visual adaptation of Raven's `examples/music`, **not** a move to a music-player layout. The editor remains a dock-based IDE.

## Reference

Raven project: https://github.com/oscar-soirey/Raven/tree/master/examples/music

Theme references: `themes/nocturne.xml`, `themes/aurora.xml`, `themes/daylight.xml` and `ui/main.xml`, maintained by the Raven authors under Apache-2.0. The colors, class semantics and mesh-background technique are adapted with attribution; no album art, icons, fonts or other copyrighted assets from the demo are bundled.

## Applied look

| Palette | Window | Panel | Accent | Material |
| --- | --- | --- | --- | --- |
| Music Nocturne | `#060609` | `#111117` | `#B79CFF` | Solid dark / translucent bars |
| Music Aurora Glass | `#080912` | `#161A29CC` | `#7DD3FC` | FrostedGlass |
| Music Daylight | `#F3F1EC` | `#FFFFFF` | `#7C3AED` | Solid light |

**Widget rules:**

- `surface` styles explorer, Git, terminal and panel surfaces like the music example's cards.
- `bar` is used for the header, menu and bottom status bar.
- `chip` adds capsule styling for labeled toolbar actions.
- `icon` applies circular styling and muted text to icon-only buttons.
- `round` and `music-primary` give the New Document control the example's pill shape and accent color.
- `muted`, `eyebrow` style low-priority metadata and compact group titles.
- `music-combo`, `music-search-input` give dropdowns/search fields matching corner radii and hover semantics.
- `music-tabs`, `music-browser`, `music-list`, `music-editor` style the IDE-specific controls absent from the player.

The backdrop is a behind-the-content `Effect="mesh"` border that follows the selected music palette, and is hidden in the other eight themes. The source XML is compiled into `resources/EmbeddedUi.h` and is not loaded from disk. The editor and terminal content backgrounds remain substantially opaque for readability.

## Selecting the style

Use the **TEMA** combo box to select `Music Nocturne`, `Music Aurora Glass` or `Music Daylight`; or press `Ctrl+Shift+T` to cycle. New configurations begin with `Music Nocturne`; existing preferences are preserved and can be changed in the UI.

## Verification

`tests/test_embedded_ui.py` parses the embedded XML and checks the 11 themes, app widget identifiers, action bindings and styles. The CMake core-only build runs that test (if Python 3 is available), along with CSV/DLT/file tests and Linux PTY terminal tests. Native Raven/Vulkan rendering should be tested on a machine that can fetch the Raven dependency from GitHub. The adapted materials depend on the version of Raven fetched by CMake.
