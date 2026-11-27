# 5.3.0 — File Preview extension

File Preview registers its own Options commands and settings. `show_html_view` is an optional additive C ABI v1 callback, namespaced by PluginManager; the host only provides generic rendering and notebook placement. Native plugin source is independent of wxWidgets. See thirdparty/FilePreview/README.md.

The GUI now requires the wxWidgets `html` component along with the previously required components. Windows automatic fetch enables `wxUSE_HTML`; Linux uses installed wxWidgets with html enabled. No WebView dependency is required. Native HTML URL opening is blocked. All file reading and conversion is in the plugin. Older plugins continue to use existing callback offsets.

Test: `ctest --test-dir build -R 'file_preview|wx_dependency_fetch|wx_raven_menu_parity' --output-on-failure` after building the relevant targets.
