#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
JOBS="${JOBS:-4}"
CONFIG="${CMAKE_BUILD_TYPE:-Release}"
GUI="${DODEV_BUILD_GUI:-ON}"
TESTS="${DODEV_BUILD_TESTS:-ON}"
printf '\n[1/3] Configure DoDevEditor\n'
cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE="$CONFIG" \
  -DDODEV_BUILD_GUI="$GUI" \
  -DDODEV_BUILD_TESTS="$TESTS" "$@"
printf '\n[2/3] Build DoDevEditor\n'
cmake --build "$BUILD" --config "$CONFIG" --parallel "$JOBS"
printf '\n[3/3] Run configured tests\n'
ctest --test-dir "$BUILD" -C "$CONFIG" --output-on-failure
printf '\n[OK] Build directory: %s\n' "$BUILD"
