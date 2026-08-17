#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="Release"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 2)"
JOURNAL_LOGS="on"
FILE_COMPARE="on"
HEX_VIEWER="on"
MANUAL_SYMBOLS="on"
PLUGINS="on"
BUNDLED_PLUGINS="on"
EXAMPLE_PLUGIN="auto"
HTTP_EDITOR_SERVER_PLUGIN="auto"
PLUGIN_DIR_ARG="${DODEV_PLUGIN_OUTPUT_DIR:-}"
USING_DEFAULT_PLUGIN_DIR=0
CLEAN=0

usage() {
    cat <<USAGE
Usage: ./scripts/build-windows-mingw.sh [release|debug] [options]

Cross-build the Windows executable from Linux using MinGW-w64.

Options:
  --clean             Remove the Windows build directory first
  --jobs N            Parallel jobs
  --journal-logs      Enable Journal Log Inspector (default)
  --no-journal-logs   Disable Journal Log Inspector
  --file-compare      Enable File Compare (default)
  --no-file-compare   Disable File Compare
  --hex-viewer         Enable binary/hex viewer tab (default)
  --no-hex-viewer      Disable binary/hex viewer tab
  --manual-symbols    Enable built-in C/C++/Kotlin symbol/call parser (default)
  --no-manual-symbols Disable built-in symbol/call parser
  --plugins            Enable runtime plugin system and bundled plugins (default)
  --no-plugins         Disable runtime plugin system and bundled plugin builds
  --bundled-plugins    Build/copy all plugins shipped under plugins/ (default)
  --no-bundled-plugins Keep runtime plugin support but do not build bundled plugins
  --example-plugin     Build/copy the SDK example plugin
  --no-example-plugin  Do not build the SDK example plugin
  --http-editor-server-plugin     Build/copy bundled HTTP editor server plugin
  --no-http-editor-server-plugin  Do not build bundled HTTP server plugin
  --plugin-dir PATH    Deploy plugins here. Relative paths are under build/bin/
  -h, --help           Show this help
USAGE
}

while (($#)); do
    case "${1,,}" in
        release) BUILD_TYPE="Release" ;;
        debug) BUILD_TYPE="Debug" ;;
        --clean) CLEAN=1 ;;
        --jobs)
            shift
            [[ $# -gt 0 ]] || { echo "--jobs requires a value" >&2; exit 2; }
            JOBS="$1"
            ;;
        --journal-logs) JOURNAL_LOGS="on" ;;
        --no-journal-logs) JOURNAL_LOGS="off" ;;
        --file-compare) FILE_COMPARE="on" ;;
        --no-file-compare) FILE_COMPARE="off" ;;
        --hex-viewer) HEX_VIEWER="on" ;;
        --no-hex-viewer) HEX_VIEWER="off" ;;
        --manual-symbols) MANUAL_SYMBOLS="on" ;;
        --no-manual-symbols) MANUAL_SYMBOLS="off" ;;
        --plugins) PLUGINS="on" ;;
        --no-plugins) PLUGINS="off" ;;
        --bundled-plugins) BUNDLED_PLUGINS="on" ;;
        --no-bundled-plugins) BUNDLED_PLUGINS="off" ;;
        --example-plugin) EXAMPLE_PLUGIN="on" ;;
        --no-example-plugin) EXAMPLE_PLUGIN="off" ;;
        --http-editor-server-plugin) HTTP_EDITOR_SERVER_PLUGIN="on" ;;
        --no-http-editor-server-plugin) HTTP_EDITOR_SERVER_PLUGIN="off" ;;
        --plugin-dir)
            shift
            [[ $# -gt 0 ]] || { echo "--plugin-dir requires a path" >&2; exit 2; }
            PLUGIN_DIR_ARG="$1"
            ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
    esac
    shift
done

if [[ "$PLUGINS" != "on" ]]; then
    BUNDLED_PLUGINS="off"
fi
if [[ "$EXAMPLE_PLUGIN" == "auto" ]]; then EXAMPLE_PLUGIN="$BUNDLED_PLUGINS"; fi
if [[ "$HTTP_EDITOR_SERVER_PLUGIN" == "auto" ]]; then HTTP_EDITOR_SERVER_PLUGIN="$BUNDLED_PLUGINS"; fi
if [[ "$PLUGINS" != "on" && ( "$EXAMPLE_PLUGIN" == "on" || "$HTTP_EDITOR_SERVER_PLUGIN" == "on" ) ]]; then
    echo "Bundled plugins require --plugins" >&2
    exit 2
fi

for tool in cmake x86_64-w64-mingw32-gcc x86_64-w64-mingw32-g++ x86_64-w64-mingw32-windres; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "Missing required Windows cross-build tool: $tool" >&2
        exit 1
    }
done

BUILD_DIR="$ROOT_DIR/build/windows-mingw-$(printf '%s' "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')"
if [[ -z "$PLUGIN_DIR_ARG" ]]; then
    PLUGIN_OUTPUT_DIR="$BUILD_DIR/bin/plugins"
    USING_DEFAULT_PLUGIN_DIR=1
elif [[ "$PLUGIN_DIR_ARG" = /* ]]; then
    PLUGIN_OUTPUT_DIR="$PLUGIN_DIR_ARG"
else
    PLUGIN_OUTPUT_DIR="$BUILD_DIR/bin/$PLUGIN_DIR_ARG"
fi
if (( CLEAN )); then
    rm -rf "$BUILD_DIR"
fi

if [[ "$PLUGINS" == "on" && "$USING_DEFAULT_PLUGIN_DIR" == "1" && -d "$PLUGIN_OUTPUT_DIR" ]]; then
    find "$PLUGIN_OUTPUT_DIR" -maxdepth 1 -type f \
        \( -name 'dodev_*.dll' -o -name 'dodev_*.so' \) \
        -delete 2>/dev/null || true
fi

journal_flag=OFF
if [[ "$JOURNAL_LOGS" == "on" ]]; then
    journal_flag=ON
fi
file_compare_flag=OFF
if [[ "$FILE_COMPARE" == "on" ]]; then
    file_compare_flag=ON
fi
hex_viewer_flag=OFF
if [[ "$HEX_VIEWER" == "on" ]]; then
    hex_viewer_flag=ON
fi
manual_symbols_flag=OFF
if [[ "$MANUAL_SYMBOLS" == "on" ]]; then
    manual_symbols_flag=ON
fi
plugins_flag=OFF
if [[ "$PLUGINS" == "on" ]]; then
    plugins_flag=ON
fi
example_plugin_flag=OFF
if [[ "$EXAMPLE_PLUGIN" == "on" ]]; then
    example_plugin_flag=ON
fi
http_editor_server_plugin_flag=OFF
if [[ "$HTTP_EDITOR_SERVER_PLUGIN" == "on" ]]; then
    http_editor_server_plugin_flag=ON
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DBUILD_WINDOWS=ON \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DDODEV_ENABLE_JOURNAL_LOGS="$journal_flag" \
    -DDODEV_ENABLE_FILE_COMPARE="$file_compare_flag" \
    -DDODEV_ENABLE_HEX_VIEWER="$hex_viewer_flag" \
    -DDODEV_ENABLE_MANUAL_SYMBOLS="$manual_symbols_flag" \
    -DDODEV_ENABLE_PLUGINS="$plugins_flag" \
    -DDODEV_BUILD_BUNDLED_PLUGINS=OFF \
    -DDODEV_BUILD_EXAMPLE_PLUGIN="$example_plugin_flag" \
    -DDODEV_BUILD_HTTP_EDITOR_SERVER_PLUGIN="$http_editor_server_plugin_flag" \
    -DDODEV_PLUGIN_OUTPUT_DIR="$PLUGIN_OUTPUT_DIR"

echo "Plugins: $PLUGINS"
echo "Bundled plugins: $BUNDLED_PLUGINS"
echo "Example plugin: $EXAMPLE_PLUGIN"
echo "HTTP editor server plugin: $HTTP_EDITOR_SERVER_PLUGIN"
echo "Plugin output: $PLUGIN_OUTPUT_DIR"

cmake --build "$BUILD_DIR" --parallel "$JOBS"
echo "Windows build output: $BUILD_DIR/bin/DoDevEditor.exe"
if [[ "$PLUGINS" == "on" && -d "$PLUGIN_OUTPUT_DIR" ]]; then
    echo "Plugin deployment directory: $PLUGIN_OUTPUT_DIR"
    find "$PLUGIN_OUTPUT_DIR" -maxdepth 1 -type f -name '*.dll' -printf '  %f\n' 2>/dev/null || true
fi
