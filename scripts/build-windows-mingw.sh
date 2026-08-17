#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="Release"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 2)"
JOURNAL_LOGS="on"
FILE_COMPARE="on"
MANUAL_SYMBOLS="on"
PLUGINS="on"
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
  --manual-symbols    Enable built-in C/C++/Kotlin symbol/call parser (default)
  --no-manual-symbols Disable built-in symbol/call parser
  --plugins            Enable runtime plugin system (default)
  --no-plugins         Disable runtime plugin system
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
        --manual-symbols) MANUAL_SYMBOLS="on" ;;
        --no-manual-symbols) MANUAL_SYMBOLS="off" ;;
        --plugins) PLUGINS="on" ;;
        --no-plugins) PLUGINS="off" ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
    esac
    shift
done

for tool in cmake x86_64-w64-mingw32-gcc x86_64-w64-mingw32-g++ x86_64-w64-mingw32-windres; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "Missing required Windows cross-build tool: $tool" >&2
        exit 1
    }
done

BUILD_DIR="$ROOT_DIR/build/windows-mingw-$(printf '%s' "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')"
if (( CLEAN )); then
    rm -rf "$BUILD_DIR"
fi

journal_flag=OFF
if [[ "$JOURNAL_LOGS" == "on" ]]; then
    journal_flag=ON
fi
file_compare_flag=OFF
if [[ "$FILE_COMPARE" == "on" ]]; then
    file_compare_flag=ON
fi
manual_symbols_flag=OFF
if [[ "$MANUAL_SYMBOLS" == "on" ]]; then
    manual_symbols_flag=ON
fi
plugins_flag=OFF
if [[ "$PLUGINS" == "on" ]]; then
    plugins_flag=ON
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DBUILD_WINDOWS=ON \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DDODEV_ENABLE_LLVM=OFF \
    -DDODEV_REQUIRE_LLVM=OFF \
    -DDODEV_ENABLE_JOURNAL_LOGS="$journal_flag" \
    -DDODEV_ENABLE_FILE_COMPARE="$file_compare_flag" \
    -DDODEV_ENABLE_MANUAL_SYMBOLS="$manual_symbols_flag" \
    -DDODEV_ENABLE_PLUGINS="$plugins_flag"

cmake --build "$BUILD_DIR" --parallel "$JOBS"
echo "Windows build output: $BUILD_DIR/bin/DoDevEditor.exe"
