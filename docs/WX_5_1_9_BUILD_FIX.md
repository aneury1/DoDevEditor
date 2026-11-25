# 5.1.9 — native GUI compilation hotfix

The supplied 5.1.8 log reports wxString, wxColour and wxLogWarning missing in WxNativeTheme.h, then an invalid use of auto at Frame::dockerQuery. The first header now includes wx/wx.h directly. The networkJob member template now declares its dependent return type using decltype(fn(static_cast<std::atomic<bool>*>(nullptr))). This is unevaluated and allows calls even if in-class include order changes.

When DODEV_BUILD_TESTS is enabled, the GUI target also compiles a separate translation unit that includes WxNativeTheme.h first, with identical target definitions, including optional GTK3 CSS. No display is needed for this compile check.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target DoDevEditor --parallel 2
cmake --build build --parallel 2
sudo cmake --install build
```

For an installation using the custom wx-config executable, add `-DwxWidgets_CONFIG_EXECUTABLE=/usr/local/bin/wx-config`. Do not disable GTK theme support to work around these missing declarations.

Validation: the actual changed networkJob body was compiled and exercised with a mock progress dialog and callers before/after its definition, using GCC and Clang. The native wxWidgets GUI cannot be compiled in the packaging environment (wxWidgets development headers unavailable). The standalone GUI header check is included for the developer's machine. The full inherited test suite was not rerun.

If the local source was reformatted, the patch context may not match. Apply the two changes described above manually, or use the full archive.
