#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
JOBS="${JOBS:-4}"
RAVEN_REF="${RAVEN_REF:-main}"
printf '\n[1/3] Descargando y configurando Raven desde GitHub (Git muestra su progreso)\n'
cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
  -DDODEV_BUILD_GUI=ON \
  -DDODEV_BUILD_TESTS=ON \
  -DDODEV_RAVEN_GIT_TAG="$RAVEN_REF" \
  -DFETCHCONTENT_QUIET=OFF
printf '\n[2/3] Compilando DoDevEditor y Raven...\n'
cmake --build "$BUILD" --parallel "$JOBS"
printf '\n[3/3] Ejecutando pruebas del nucleo...\n'
ctest --test-dir "$BUILD" --output-on-failure
printf '\n[OK] Ejecutable (segun generador): %s/DoDevEditor\n' "$BUILD"
