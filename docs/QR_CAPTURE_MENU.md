# Capturar un código QR desde Herramientas

## Qué hace

`Herramientas → Capturar QR del archivo actual...` genera una **imagen QR** a
partir del contenido completo del documento de texto activo. Incluye cambios
que todavía no se han guardado. Esta acción **no** lee códigos QR desde una
cámara ni captura una imagen del escritorio.

## Primer uso

1. Compilar e instalar DoDevEditor y sus plugins incluidos:

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
   cmake --build build --parallel
   cmake --install build
   ```

2. Abrir `~/.local/bin/DoDevEditor`.
3. En `Settings → Plugins`, seleccionar `QR Code Generator`
   (`com.dodev.qrcode`) y pulsar **Enable**. Verificar que no esté activo Safe Mode
   y que los plugins estén habilitados globalmente.
4. Reiniciar DoDevEditor para activar el módulo nativo.
5. Abrir o crear un documento de **texto no vacío**.
6. Seleccionar `Herramientas → Capturar QR del archivo actual...`.
7. Indicar un nombre de archivo terminado en `.png` o `.svg` cuando se solicite.
   El formato inicial se elige mediante la opción `preferredFormat` del plugin;
   por defecto es PNG.

Para abrir el resultado, use `Herramientas → QR Code: View Last Image`.

## Por qué aparece el menú aunque no se haya activado el plugin

La opción `Capturar QR del archivo actual...` forma parte del menú integrado
(en C++/XML). Si el módulo QR está desactivado, muestra un diálogo con las
instrucciones de activación y su estado de descubrimiento. Al activarse el
plugin, esa misma opción invoca `qrcode.generate`. Las otras opciones de QR
(PNG, SVG y último archivo) se añaden dinámicamente por el plugin.

## Ubicación del módulo

Con un prefijo de instalación `$HOME/.local`:

```text
~/.local/bin/DoDevEditor
~/.local/bin/plugins/qrcode/plugin.json
~/.local/bin/plugins/qrcode/libdodev_qrcode_plugin.so
```

En Windows el módulo usa la extensión `.dll`. El programa busca los plugins
instalados relativos a la ruta del **ejecutable que realmente se está
utilizando**. Si se ejecuta un binario desde `build/` en lugar del instalado,
se buscará en el directorio `build/plugins` y no en el prefijo de instalación.

## Logs y diagnóstico

En Linux:

```bash
~/.local/bin/DoDevEditor 2>&1 | tee dodev-qr.log
```

Buscar líneas `[DoDevEditor][Plugins]` (rutas, estado `Active`/`Disabled` o
`Failed`) y `[plugin com.dodev.qrcode]` (resultado de exportación). Al pulsar
el menú con el plugin inactivo aparece `[DoDevEditor][QR] Capture unavailable`.

Si el documento está vacío, si no es texto, si la ruta de salida es incorrecta
o si el documento supera la capacidad de un único QR, el plugin registra un
error explícito. No recorta el contenido para hacerlo caber.

## Límites actuales

- Genera el QR del documento completo; la API del SDK v1 todavía no expone la
  selección de texto.
- El visor de imágenes se abre con el programa predeterminado del sistema; el
  SDK v1 no expone aún un widget para previsualizar imágenes dentro del editor.
- Los plugins nativos instalados requieren activación explícita por seguridad.
