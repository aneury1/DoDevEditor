# 5.3.1 — Open as Text / Open as Preview

See `thirdparty/FilePreview/README.md` for supported formats, dependency setup, controls, security and limitations. The File menu and tree context menu are generic: plugins register extensions with `register_file_preview`; `*` is an explicit-preview fallback and does not trigger prompts on every file. The host passes unsaved text for modified open documents and otherwise a path. UI-specific reusable image, tree and optional media widgets are in `src/gui/wx/WxPreviewViews.h`. ABI additions are appended; old plugins keep their layout. Plugin unload clears handler registrations.

Targeted regression: `ctest --test-dir build -R 'file_preview|wx_dependency_fetch|wx_raven_menu_parity' --output-on-failure`. Native GUI not verified; test hooks validate the routing and payloads, not rendering. Standalone tests exercise real SQLite and Expat when installed, plus real PDF rasterization when pdftoppm exists.
