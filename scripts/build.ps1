param(
    [string]$BuildDir = "",
    [string]$Configuration = "Release",
    [string]$RavenRef = "main",
    [int]$Jobs = 4
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) { $BuildDir = Join-Path $root "build" }
Write-Host "`n[1/3] Descarga Raven GUI desde Github con progreso de Git" -ForegroundColor Cyan
& cmake -S "$root" -B "$BuildDir" -DDODEV_BUILD_GUI=ON -DDODEV_BUILD_TESTS=ON "-DDODEV_RAVEN_GIT_TAG=$RavenRef" -DFETCHCONTENT_QUIET=OFF
if ($LASTEXITCODE -ne 0) { throw "Falló la configuración de CMake: $LASTEXITCODE" }
Write-Host "`n[2/3] Compilando Raven y DoDevEditor..." -ForegroundColor Cyan
& cmake --build "$BuildDir" --config "$Configuration" --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw "Falló la compilación: $LASTEXITCODE" }
Write-Host "`n[3/3] Ejecutando pruebas del núcleo..." -ForegroundColor Cyan
& ctest --test-dir "$BuildDir" -C "$Configuration" --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Fallaron pruebas: $LASTEXITCODE" }
Write-Host "`n[OK] Ejecutable (VS): $BuildDir\$Configuration\DoDevEditor.exe" -ForegroundColor Green
