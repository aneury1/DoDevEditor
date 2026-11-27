# DoDevEditor plugin-only build (5.2.4)

This directory is an export of Git History, QR Code and Simple Module with the
public SDK and the portable PTY process utility. It does not contain or build
the editor executable, wxWidgets, PluginManager or SettingsManager.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$HOME/.local"
```

The built modules and manifests are in `build/plugins/`. Copy a plugin directory
to the plugin search directory configured in DoDevEditor, then enable and restart
it. A compatible 5.2.4 host is needed to use the new table/path/secret APIs.
The plugins use the public C ABI for host functionality; they are not standalone GUI apps.
QR encryption requires OpenSSL Crypto; zlib is optional. Git History requires Git
at runtime. The process utility is statically linked into Git History, not loaded
from the editor. Serial Monitor, Remote Journal, Docker, GDB, AI and Source Symbols
are deliberately excluded because their current native GUI adapters are not yet
fully decoupled from the editor host.
