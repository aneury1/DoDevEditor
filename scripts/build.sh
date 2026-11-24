#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
JOBS="${JOBS:-4}"
printf '\n[1/3] Configurando DoDevEditor wxWidgets\n'
cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
  -DDODEV_BUILD_GUI=ON \
  -DDODEV_BUILD_TESTS=ON
printf '\n[2/3] Compilando DoDevEditor wxWidgets...\n'
cmake --build "$BUILD" --parallel "$JOBS"
printf '\n[3/3] Ejecutando pruebas del nucleo...\n'
ctest --test-dir "$BUILD" --output-on-failure
printf '\n[OK] Ejecutable (segun generador): %s/DoDevEditor\n' "$BUILD"
