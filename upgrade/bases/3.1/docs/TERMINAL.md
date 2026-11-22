# Terminal integrada de DoDevEditor 2.2

## Objetivo

Una terminal **dentro de la ventana Raven**, con la interacción básica de un panel de terminal de Visual Studio / VS Code: escribir directamente, conservar procesos y directorios, crear sesiones y detener compilaciones. No se limita a ejecutar `system()` ni a imprimir en un `TextEditor`.

## Arquitectura

```text
Raven DockArea (panel inferior TERMINAL)
  └─ TabView con una pestaña por shell
       └─ R_WIDGET_CUSTOM (pintado y entrada por eventos)
            ├─ TerminalScreen (celdas VT, estilos, cursor, scrollback)
            └─ TerminalSession (hilo lector, entrada a PTY)
                 ├─ Linux/Unix: forkpty() + /bin/sh o $SHELL
                 └─ Windows: CreatePseudoConsole() + CreateProcessW()
```

- `TerminalSession` abre una pseudoconsola, trabaja con un hilo lector y coloca los bytes recibidos en una cola protegida por mutex. **Ningún hilo lector manipula widgets Raven**.
- `EditorApp::pumpTerminals` recoge y analiza el texto en el hilo principal mediante un timer, y repinta las áreas que han cambiado.
- `TerminalScreen` mantiene una matriz de celdas, cursor, 24-bit RGB y secuencias ANSI/VT básicas, con historial de 3.000 líneas. La pantalla alternativa se utiliza para programas en modo completo.
- El widget Raven `R_WIDGET_CUSTOM` pinta fondo, selección, texto coloreado y cursor y envía teclado y ratón a la sesión.
- Al cambiar el tamaño del canvas se llama a `ioctl(TIOCSWINSZ)` o `ResizePseudoConsole()`.

## Cómo utilizar

1. Abre el panel **Ver → Terminal** o pulsa el botón de terminal de la ventana.
2. Selecciona shell en el desplegable (para la *siguiente* terminal) y pulsa **+**.
3. Haz clic dentro del área de terminal, escribe `pwd` (Linux) / `Get-Location` (PowerShell) y pulsa Enter.
4. Ejecuta `cmake --build build --parallel 4` o cualquier comando de tu proyecto. La interfaz debería seguir procesando eventos mientras el proceso trabaja.
5. Usa **⏹** o `Ctrl+C` para interrumpirlo. Crea otra pestaña con **+** si deseas conservar un segundo shell.

### Atajos de teclado cuando la terminal tiene el foco

| Atajo | Operación |
|---|---|
| `Ctrl+C` | Enviar ETX a la sesión (SIGINT en PTY POSIX) |
| `Ctrl+Shift+C` | Copiar selección o área visible |
| `Ctrl+Shift+V` | Pegar texto del portapapeles |
| `Ctrl+Shift+N` | Nueva pestaña de terminal |
| Flechas, Tab, Inicio/Fin, Supr, Retroceso | Secuencias de control enviadas a la shell |
| Rueda del ratón | Desplazamiento en scrollback |
| Arrastre izquierdo | Selección de celdas visibles para copiar |

## Compatibilidad

### Linux y otras plataformas POSIX

El backend usa `<pty.h>`, `forkpty()`, `poll()` y `waitpid()`. Es apropiado para **Linux**. La disponibilidad de `<pty.h>` y `forkpty` en otras plataformas Unix no se ha verificado. La shell se inicia en modo interactivo y dentro de la carpeta del proyecto; `$SHELL` determina el perfil por defecto.

### Windows

Se prepara ConPTY nativo: `CreatePseudoConsole`, `ResizePseudoConsole` y `ClosePseudoConsole`. Requiere **Windows 10 versión 1809 o superior**, Windows SDK compatible y shell instalado. Ofrece PowerShell 5, PowerShell 7 (si está instalado) y CMD. No se ha compilado ni ejecutado el backend Windows aquí.

### Visualización y limitaciones

Se implementan escapes CSI comunes, reposicionamiento, borrado, cursor visible/invisible, scroll regions, color ANSI 16/256 y color verdadero, además de pantalla alternativa. No se promete compatibilidad 100% con xterm: soporte parcial de secuencias privadas, teclado/ratón extendidos, algunas combinaciones Unicode, ligaduras y caracteres de anchura doble. Las apps TUI complejas pueden renderizar de modo imperfecto. Las pestañas al cerrarse finalizan sus procesos correspondientes. El scrollback está limitado para controlar memoria.

## Pruebas

`tests/test_terminal.cpp` comprueba que la pantalla posiciona texto, muestra color ANSI, restaura la pantalla alternativa, soporta RGB, borra y redimensiona. En Linux lanza una shell con PTY, escribe un comando, comprueba salida y cierra la sesión. No incluye pruebas de interfaz gráfica ni de Windows.

```bash
cmake -S . -B build-core -DDODEV_BUILD_GUI=OFF -DDODEV_BUILD_TESTS=ON
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```
