#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="Release"
CLEAN=0
DETECTED_JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 2)"
# wxWidgets is a large build. Cap the default parallelism to reduce the chance
# of cc1/cc1plus being killed on machines with many cores but limited RAM.
if (( DETECTED_JOBS > 8 )); then
    JOBS=8
else
    JOBS="$DETECTED_JOBS"
fi
GENERATOR=""
VERBOSE=0
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

usage() {
    cat <<USAGE
Usage: ./scripts/build-linux.sh [release|debug|relwithdebinfo|minsizerel] [options]

Options:
  --clean            Delete this configuration's build directory first
  --jobs N           Parallel build jobs (default: detected CPU count: ${JOBS})
  --ninja            Use Ninja when installed
  --make              Force Unix Makefiles
  --verbose           Show every compiler/linker command
  --journal-logs       Enable SSH journal log inspector (default)
  --no-journal-logs    Disable journal log inspector at compile time
  --file-compare       Enable side-by-side file comparison (default)
  --no-file-compare    Disable file comparison at compile time
  --hex-viewer          Enable binary/hex viewer tab (default)
  --no-hex-viewer       Disable binary/hex viewer tab at compile time
  --manual-symbols     Enable built-in C/C++/Kotlin symbol/call parser (default)
  --no-manual-symbols  Disable built-in symbol/call parser at compile time
  --plugins            Enable runtime plugin system and bundled plugins (default)
  --no-plugins         Disable runtime plugin system and bundled plugin builds
  --bundled-plugins    Build/copy all plugins shipped under plugins/ (default)
  --no-bundled-plugins Keep runtime plugin support but do not build bundled plugins
  --example-plugin     Build/copy the SDK example plugin
  --no-example-plugin  Do not build the SDK example plugin
  --http-editor-server-plugin     Build/copy bundled HTTP editor server plugin
  --no-http-editor-server-plugin  Do not build bundled HTTP server plugin
  --plugin-dir PATH    Deploy built plugins here. Relative paths are under build/bin/
  -h, --help          Show this help

Examples:
  ./scripts/build-linux.sh
  ./scripts/build-linux.sh debug
  ./scripts/build-linux.sh release --clean
  ./scripts/build-linux.sh relwithdebinfo --jobs 8 --ninja
  ./scripts/build-linux.sh release --clean --plugins
USAGE
}

fail() {
    printf '\n\033[31m[FAIL]\033[0m %s\n' "$*" >&2
    exit 1
}

info() {
    printf '\033[36m[INFO]\033[0m %s\n' "$*"
}

ok() {
    printf '\033[32m[ OK ]\033[0m %s\n' "$*"
}

while (($#)); do
    case "${1,,}" in
        release) BUILD_TYPE="Release" ;;
        debug) BUILD_TYPE="Debug" ;;
        relwithdebinfo) BUILD_TYPE="RelWithDebInfo" ;;
        minsizerel) BUILD_TYPE="MinSizeRel" ;;
        --clean) CLEAN=1 ;;
        --jobs)
            shift
            [[ $# -gt 0 ]] || fail "--jobs requires a number"
            JOBS="$1"
            [[ "$JOBS" =~ ^[1-9][0-9]*$ ]] || fail "Invalid job count: $JOBS"
            ;;
        --ninja) GENERATOR="Ninja" ;;
        --make) GENERATOR="Unix Makefiles" ;;
        --verbose) VERBOSE=1 ;;
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
            [[ $# -gt 0 ]] || fail "--plugin-dir requires a path"
            PLUGIN_DIR_ARG="$1"
            ;;
        -h|--help) usage; exit 0 ;;
        *) fail "Unknown argument: $1. Run with --help." ;;
    esac
    shift
done

if [[ "$PLUGINS" != "on" ]]; then
    BUNDLED_PLUGINS="off"
fi

# Resolve per-plugin build switches. Explicit per-plugin switches override the
# aggregate bundled setting.
if [[ "$EXAMPLE_PLUGIN" == "auto" ]]; then
    EXAMPLE_PLUGIN="$BUNDLED_PLUGINS"
fi
if [[ "$HTTP_EDITOR_SERVER_PLUGIN" == "auto" ]]; then
    HTTP_EDITOR_SERVER_PLUGIN="$BUNDLED_PLUGINS"
fi
if [[ "$PLUGINS" != "on" && ( "$EXAMPLE_PLUGIN" == "on" || "$HTTP_EDITOR_SERVER_PLUGIN" == "on" ) ]]; then
    fail "Bundled plugins require --plugins"
fi

command -v cmake >/dev/null 2>&1 || fail "cmake is not installed"
command -v git >/dev/null 2>&1 || fail "git is not installed"
command -v c++ >/dev/null 2>&1 || fail "a C++ compiler is not installed"
command -v pkg-config >/dev/null 2>&1 || fail "pkg-config/pkgconf is not installed"

if ! command -v curl >/dev/null 2>&1; then
    printf '\033[33m[WARN]\033[0m curl is not installed; OpenAI/Claude/OpenAI-compatible/Ollama AI Chat providers will be unavailable at runtime.\n' >&2
fi
if ! command -v secret-tool >/dev/null 2>&1; then
    printf '\033[33m[WARN]\033[0m secret-tool is not installed; AI OS-keyring secret storage will be unavailable (environment/session secret still work).\n' >&2
fi
if [[ "$JOURNAL_LOGS" == "on" ]] && ! command -v ssh >/dev/null 2>&1; then
    printf '\033[33m[WARN]\033[0m ssh is not installed; SSH Journal Logs will still compile but live collection will be unavailable at runtime.\n' >&2
fi

if ! pkg-config --exists zlib; then
    printf '\nThe system zlib development files are required for the Linux wxWidgets build.\n\n' >&2
    if command -v pacman >/dev/null 2>&1; then
        printf 'Arch Linux:  sudo pacman -S --needed zlib\n' >&2
    elif command -v apt-get >/dev/null 2>&1; then
        printf 'Debian/Ubuntu: sudo apt install zlib1g-dev\n' >&2
    elif command -v dnf >/dev/null 2>&1; then
        printf 'Fedora: sudo dnf install zlib-devel\n' >&2
    fi
    fail "system zlib development package not found"
fi

if ! pkg-config --exists gtk+-3.0; then
    printf '\nGTK3 development files are required to build wxWidgets.\n\n' >&2
    if command -v pacman >/dev/null 2>&1; then
        printf 'Arch Linux:  sudo pacman -S --needed base-devel cmake git pkgconf gtk3\n' >&2
    elif command -v apt-get >/dev/null 2>&1; then
        printf 'Debian/Ubuntu: sudo apt install build-essential cmake git pkg-config libgtk-3-dev\n' >&2
    elif command -v dnf >/dev/null 2>&1; then
        printf 'Fedora: sudo dnf install gcc-c++ make cmake git pkgconf-pkg-config gtk3-devel\n' >&2
    fi
    fail "GTK3 development package not found"
fi

if [[ "$GENERATOR" == "Ninja" ]] && ! command -v ninja >/dev/null 2>&1; then
    fail "--ninja was requested, but ninja is not installed"
fi

if [[ -z "$GENERATOR" ]] && command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
fi
if [[ -z "$GENERATOR" ]]; then
    GENERATOR="Unix Makefiles"
fi

build_slug="$(printf '%s' "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')"
BUILD_DIR="$ROOT_DIR/build/linux-$build_slug"
if [[ -z "$PLUGIN_DIR_ARG" ]]; then
    PLUGIN_OUTPUT_DIR="$BUILD_DIR/bin/plugins"
    USING_DEFAULT_PLUGIN_DIR=1
elif [[ "$PLUGIN_DIR_ARG" = /* ]]; then
    PLUGIN_OUTPUT_DIR="$PLUGIN_DIR_ARG"
else
    PLUGIN_OUTPUT_DIR="$BUILD_DIR/bin/$PLUGIN_DIR_ARG"
fi

if (( CLEAN )); then
    info "Removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

# The default plugin deployment directory belongs to the build output. Remove
# previously deployed DoDevEditor libraries so deleted bundled plugins cannot
# survive an incremental rebuild. A custom plugin directory is never cleaned.
if [[ "$PLUGINS" == "on" && "$USING_DEFAULT_PLUGIN_DIR" == "1" && -d "$PLUGIN_OUTPUT_DIR" ]]; then
    find "$PLUGIN_OUTPUT_DIR" -maxdepth 1 -type f \
        \( -name 'dodev_*.so' -o -name 'dodev_*.dll' -o -name 'dodev_*.dylib' \) \
        -delete 2>/dev/null || true
fi

mkdir -p "$BUILD_DIR"

CMAKE_VERSION="$(cmake --version | awk 'NR==1 {print $3}')"
CMAKE_MAJOR="${CMAKE_VERSION%%.*}"

CMAKE_ARGS=(
    -S "$ROOT_DIR"
    -B "$BUILD_DIR"
    -G "$GENERATOR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)

if [[ "$JOURNAL_LOGS" == "on" ]]; then
    CMAKE_ARGS+=( -DDODEV_ENABLE_JOURNAL_LOGS=ON )
else
    CMAKE_ARGS+=( -DDODEV_ENABLE_JOURNAL_LOGS=OFF )
fi

if [[ "$FILE_COMPARE" == "on" ]]; then
    CMAKE_ARGS+=( -DDODEV_ENABLE_FILE_COMPARE=ON )
else
    CMAKE_ARGS+=( -DDODEV_ENABLE_FILE_COMPARE=OFF )
fi

if [[ "$HEX_VIEWER" == "on" ]]; then
    CMAKE_ARGS+=( -DDODEV_ENABLE_HEX_VIEWER=ON )
else
    CMAKE_ARGS+=( -DDODEV_ENABLE_HEX_VIEWER=OFF )
fi

if [[ "$MANUAL_SYMBOLS" == "on" ]]; then
    CMAKE_ARGS+=( -DDODEV_ENABLE_MANUAL_SYMBOLS=ON )
else
    CMAKE_ARGS+=( -DDODEV_ENABLE_MANUAL_SYMBOLS=OFF )
fi

if [[ "$PLUGINS" == "on" ]]; then
    CMAKE_ARGS+=( -DDODEV_ENABLE_PLUGINS=ON )
else
    CMAKE_ARGS+=( -DDODEV_ENABLE_PLUGINS=OFF )
fi
# The wrapper resolves the aggregate switch itself so explicit per-plugin
# overrides remain meaningful. Direct CMake users can use
# DODEV_BUILD_BUNDLED_PLUGINS=ON.
CMAKE_ARGS+=( -DDODEV_BUILD_BUNDLED_PLUGINS=OFF )
if [[ "$EXAMPLE_PLUGIN" == "on" ]]; then
    CMAKE_ARGS+=( -DDODEV_BUILD_EXAMPLE_PLUGIN=ON )
else
    CMAKE_ARGS+=( -DDODEV_BUILD_EXAMPLE_PLUGIN=OFF )
fi
if [[ "$HTTP_EDITOR_SERVER_PLUGIN" == "on" ]]; then
    CMAKE_ARGS+=( -DDODEV_BUILD_HTTP_EDITOR_SERVER_PLUGIN=ON )
else
    CMAKE_ARGS+=( -DDODEV_BUILD_HTTP_EDITOR_SERVER_PLUGIN=OFF )
fi
CMAKE_ARGS+=( "-DDODEV_PLUGIN_OUTPUT_DIR=$PLUGIN_OUTPUT_DIR" )

# CMake 4 removed compatibility with policy versions older than 3.5.  This
# cache option is intentionally supplied by the build wrapper (rather than set
# inside the project) so older third-party CMake files can still configure.
if [[ "$CMAKE_MAJOR" =~ ^[0-9]+$ ]] && (( CMAKE_MAJOR >= 4 )); then
    CMAKE_ARGS+=( -DCMAKE_POLICY_VERSION_MINIMUM=3.5 )
    info "CMake $CMAKE_VERSION detected: enabling third-party policy compatibility"
fi

info "Project:    $ROOT_DIR"
info "Build:      $BUILD_DIR"
info "Type:       $BUILD_TYPE"
info "Generator:  $GENERATOR"
info "Jobs:       $JOBS"
info "Journal logs: $JOURNAL_LOGS"
info "File compare: $FILE_COMPARE"
info "Manual symbols: $MANUAL_SYMBOLS"
info "Plugins:    $PLUGINS"
info "Bundled plugins: $BUNDLED_PLUGINS"
info "Example plugin: $EXAMPLE_PLUGIN"
info "HTTP editor server plugin: $HTTP_EDITOR_SERVER_PLUGIN"
info "Plugin output: $PLUGIN_OUTPUT_DIR"

info "Configuring..."
cmake "${CMAKE_ARGS[@]}"

info "Building..."
BUILD_LOG="$BUILD_DIR/build.log"
BUILD_CMD=(cmake --build "$BUILD_DIR" --parallel "$JOBS")
if (( VERBOSE )); then
    BUILD_CMD+=(--verbose)
fi

set +e
"${BUILD_CMD[@]}" 2>&1 | tee "$BUILD_LOG"
build_status=${PIPESTATUS[0]}
set -e

if (( build_status != 0 )); then
    printf '\n\033[31m[BUILD ERROR SUMMARY]\033[0m\n' >&2
    # Ninja often prints many harmless third-party warnings before the actual
    # failure. Surface the useful diagnostics immediately.
    grep -nEi '(^|[ :])(fatal error:|error:|undefined reference|collect2: error|killed signal|out of memory|internal compiler error)' "$BUILD_LOG" \
        | head -n 40 >&2 || true
    printf '\nFull build log: %s\n' "$BUILD_LOG" >&2
    printf 'Retry verbosely with:\n  %q release --clean --jobs 2 --verbose\n' "$ROOT_DIR/scripts/build-linux.sh" >&2
    exit "$build_status"
fi

BIN="$BUILD_DIR/bin/DoDevEditor"
if [[ -x "$BIN" ]]; then
    ok "Build completed: $BIN"
    printf '\nRun it with:\n  %q\n' "$BIN"
else
    ok "Build completed"
    printf 'Check build output under: %s\n' "$BUILD_DIR"
fi

if [[ "$PLUGINS" == "on" && -d "$PLUGIN_OUTPUT_DIR" ]]; then
    ok "Plugin deployment directory: $PLUGIN_OUTPUT_DIR"
    find "$PLUGIN_OUTPUT_DIR" -maxdepth 1 -type f \
        \( -name '*.so' -o -name '*.dylib' -o -name '*.dll' \) -printf '  %f\n' 2>/dev/null || true
fi
