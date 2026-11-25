param(
    [string]$BuildDir = "",
    [string]$Configuration = "Release",
    [string]$RavenRef = "main", # Kept for compatibility; no Raven dependency.
    [int]$Jobs = 4,
    [switch]$Headless,
    [string[]]$CMakeArgs = @()
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) { $BuildDir = Join-Path $root "build" }
$gui = if ($Headless) { "OFF" } else { "ON" }
Write-Host "`n[1/3] Configure DoDevEditor" -ForegroundColor Cyan
& cmake -S "$root" -B "$BuildDir" "-DCMAKE_BUILD_TYPE=$Configuration" "-DDODEV_BUILD_GUI=$gui" -DDODEV_BUILD_TESTS=ON @CMakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed: $LASTEXITCODE" }
Write-Host "`n[2/3] Build DoDevEditor" -ForegroundColor Cyan
& cmake --build "$BuildDir" --config "$Configuration" --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
Write-Host "`n[3/3] Run tests" -ForegroundColor Cyan
& ctest --test-dir "$BuildDir" -C "$Configuration" --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Tests failed: $LASTEXITCODE" }
Write-Host "`n[OK] Build directory: $BuildDir" -ForegroundColor Green
