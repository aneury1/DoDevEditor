# File Preview 1.1.0 / DoDevEditor 5.3.1+

An independently buildable C++17 plugin. All extension recognition, format parsing, settings, SQLite queries and external renderer invocation are in the plugin. The host offers generic registered file handlers and HTML, bitmap, tree and optional native media views; it does not identify individual file formats.

## Open a file
- File (Archivo) > Open...: after choosing a recognized file, choose Open as Text or Open as Preview. Unsupported extensions default to text. File > Open as Text... or Open as Preview... bypasses the choice. File > Preview selected workspace file uses the file selected in the tree.
- Workspace Files: double-click a recognized file to choose; right-click Open as Text or Open as Preview to select explicitly. Internal navigation, e.g. GDB or source symbol references, still opens source directly.
- Options > Preview active file renders the current source buffer, including unsaved edits; Preview file... reads from disk. For binary files, use the disk preview. The explicit preview fallback renders otherwise unrecognized extensions as hex. All previews are separate editor tabs, never source edits.

## Formats
| Type | Presentation / prerequisites |
|---|---|
| HTML/Markdown | Static formatted text; no scripts, embedded resources or CSS. Markdown is a subset. |
| CSV/TSV | HTML table; quoted cells, CRLF and multiline cells; 2,000 rows, 80 columns. |
| JSON/XML | Expandable native tree; lexical JSON preserves large integers; XML requires Expat, rejects DTDs. Turn off structuredTree for formatted text. |
| YAML/INI/TOML/TXT/LOG | Escaped text. |
| PNG/JPEG/BMP/GIF/ICO/TIFF | Native bitmap with scroll and zoom; first frame only. wxWidgets image handlers must be available. |
| SQLite/DB | Read-only table list and selected table or one SELECT query. Requires SQLite3. Use sqliteTable or sqliteQuery in Settings; refresh preview after changing. 200 rows, 80 columns; BLOBs summarized. Query authorizer disallows writes, ATTACH, PRAGMA, unapproved functions; instruction budget. |
| Binary | Hex + ASCII, first 64 KiB, explicit truncation notice. |
| PDF | Optional Poppler pdftoppm rasterizes one selected page (pdfPage setting); source <=64 MiB. |
| SVG | Optional rsvg-convert rasterizes to PNG. |
| PlantUML | Optional plantuml executable, PNG output, SECURE profile requested. |
| Mermaid | Optional mmdc executable, PNG output. |
| WAV/MP3/OGG/FLAC | Native wxMediaCtrl Play/Pause/Stop if host has media component and suitable OS codecs; otherwise explanatory error tab. |

External renderers are disabled by default; enable externalRenderers only for trusted files. Configure executable paths in Settings > File Preview. They receive literal arguments, use temporary copies, time out after 30 seconds; they are not sandboxed and may access resources, including network, depending on renderer. They can block the GUI while rendering. On Windows specify actual executable, not .cmd/.bat; Java/Node wrappers must be installed as executable launchers. Relative resources may not work due to temporary copies. PDF is a single-page raster preview, not a document navigation engine. SVG is raster, not an interactive vector canvas. Native media support is optional; current fetch enables it on Windows but no Windows or GUI execution was tested. No audio autoplay.

## Build
Standalone ZIP includes public sdk/include/dodev/sdk. Or build this folder using `-DDODEV_SDK_INCLUDE=/path/to/DoDevEditor/include`. SQLite3 and Expat are optional compile-time dependencies. Image and media widgets are host capabilities only. No plugin links against wxWidgets or editor-internal libraries.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
cmake --install build --prefix /usr/local
```

CachyOS/Arch: install `sqlite expat poppler librsvg` for SQLite, XML, PDF and SVG. Install PlantUML and Mermaid CLI separately. Ubuntu/Debian: `libsqlite3-dev libexpat1-dev poppler-utils librsvg2-bin`; media support depends on installed wxWidgets library and codecs. After upgrading the host once, the plugin is separately buildable/installable. Earlier hosts reject initialization because they lack the new additive SDK functions.

Limits: text 2 MiB, image encoded bytes 64 MiB (decoded dimensions checked after decoding), HTML output 16 MiB, 10,000 tree nodes, 80 levels. Files are snapshots, no live sync. Native wx rendering and optional external tools other than pdftoppm remain untested here.
