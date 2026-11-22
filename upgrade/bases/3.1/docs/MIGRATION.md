# Notas de migración — wxWidgets a Raven GUI

## Inventario del proyecto adjunto

El código original contenía `src/Frame.cpp`, `TabContainer.cpp`, `TextEditor.cpp`, `CSVEditor.cpp`, `DLTViewer.cpp`, `GitPanel.cpp`, `FileExplorerTab.cpp`, `DataServerEditor.cpp` e `ImageEditor.cpp`. Además, `src/main.cpp` abría una ventana mínima de prueba de Tree-sitter distinta de `WindowFrame`. Se conservaron los originales en `legacy/`, pero **no participan en el nuevo build**.

| Componente anterior | Reemplazo Raven | Estado |
|---|---|---|
| wxFrame, wxAUI, wxMenus | `Window`, `HeaderBar`, `MenuBar`, `DockArea`, `TabView` XML | Migrado a Raven |
| wxStyledTextCtrl | `Rtext_editor_*` | Editor de texto funcional |
| wxFile dialogs / wxTree | `Rfile_dialog_*`, `FileBrowser` | Apertura, guardado y carpeta |
| CSVEditor | `Rtable` + `Rtext_editor` y parser C++17 | Vista y edición del texto original |
| DLTViewer | `Rtable` + parser de storage header DLT | Vista básica y filtro APID |
| GitPanel | `Rlist_view` + proceso `git log` | Historial de commits |
| Terminal integrada | `R_WIDGET_CUSTOM` + `TerminalScreen` + `TerminalSession` | Shells persistentes PTY/ConPTY, pestañas, entrada directa, ANSI/VT básico |
| BinaryEditor / HEX | `Rtext_editor` solo lectura | HEX hasta 512 KiB |
| DataServerEditor | Panel Raven | API de red del original no implementada |
| ImageEditor | Fuente en legacy | Original vacío, no implementado |
| Theme wx y Settings | 11 `Theme` XML en memoria, incluyendo Music Nocturne/Aurora Glass/Daylight | Migrado |

## Separación de responsabilidades

- `EditorData.cpp` evita cualquier dependencia de Raven. Parseo CSV, DLT, Git y E/S se prueban con CTest.
- `EditorApp.cpp` contiene toda la lógica UI; incorpora `Raven.h`, crea pestañas y asocia acciones por nombre.
- `EmbeddedUi.h` conserva XML de layout y temas como constantes `std::string_view` de duración estática. No hay rutas a XML externos.
- `legacy/` permite comprobar diferencias, pero su código wxWidgets está fuera de CMake.

## Compilación y validación

Comprobado: `cmake -DDODEV_BUILD_GUI=OFF`, compilación C++17 con CTest, tres pruebas: CSV/DLT/HEX/archivos, terminal ANSI con shell PTY interactiva en Linux y validacion estructural XML de los temas y widgets.

No comprobado en este entorno: FetchContent real desde GitHub, compilación del target `DoDevEditor` contra la copia actual de Raven y apertura de su ventana Vulkan. Se consultaron los encabezados públicos del repositorio para contrastar las firmas. En el primer build conectado se deben ejecutar configuración, compilación y prueba manual de los comandos y temas.

## Plan para implementar módulos que antes eran incompletos

1. Data Server: definir contratos de servidor/cliente, operaciones HTTP, cierre, cancelación, seguridad y pruebas; después vincular los botones Raven.
2. Visor de imágenes: cargar PNG/JPEG con la API de texturas e imágenes de Raven si se necesita.
3. Emulación avanzada xterm: ampliar escapes/Unicode/ratón y validar ConPTY y programas TUI en Windows y Linux.
4. DLT completo: integrar la especificación y verificar cabeceras, formatos extendidos, timestamps y payload no verboso.

No se debe confundir la migración del contenedor UI con la implementación desde cero de módulos que eran stubs.
