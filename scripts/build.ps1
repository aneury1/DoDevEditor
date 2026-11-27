param(
    [string]$BuildDir = "",
    [string]$Configuration = "Release",
    [string]$RavenRef = "main", # Kept for compatibility; no Raven dependency.
    [int]$Jobs = 4,
    [switch]$Headless,
    [string[]]$CMakeArgs = @(),
    [string]$Generator = "",
    [string]$Architecture = "",
    [switch]$ForceWxFetch,
    [switch]$NoWxFetch,
    [string]$WxSourceArchive = "",
    [switch]$SkipTests,
    [switch]$VerboseBuild
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) { $BuildDir = Join-Path $root "build" }
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
if ($Jobs -lt 1) { throw "Jobs must be positive" }
if ($ForceWxFetch -and $NoWxFetch) { throw "ForceWxFetch and NoWxFetch are mutually exclusive" }
if ($NoWxFetch -and $WxSourceArchive) { throw "NoWxFetch conflicts with WxSourceArchive" }
Get-Command cmake -ErrorAction Stop | Out-Null
if (-not $SkipTests) { Get-Command ctest -ErrorAction Stop | Out-Null }
$logDir = Join-Path $BuildDir "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

# Stream stdout and stderr to the console and a per-stage log. CMake emits
# transfer progress on stdout; native errors must not terminate the pipeline
# before the process exit code and its diagnostic log can be collected.
function Invoke-LoggedNative {
    param([string]$Program, [string[]]$Arguments, [string]$LogFile)
    Write-Host "[LOG] $LogFile" -ForegroundColor DarkCyan
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $PSNativeCommandUseErrorActionPreference = $false
    try {
        & $Program @Arguments 2>&1 | ForEach-Object { $_.ToString() } | Tee-Object -FilePath $LogFile
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($code -ne 0) { throw "$Program failed with exit code $code. See $LogFile" }
}

$gui = if ($Headless) { "OFF" } else { "ON" }
$tests = if ($SkipTests) { "OFF" } else { "ON" }
$configure = @("-S", $root, "-B", $BuildDir, "-DCMAKE_BUILD_TYPE=$Configuration", "-DDODEV_BUILD_GUI=$gui", "-DDODEV_BUILD_TESTS=$tests", "-DDODEV_WX_FETCH_PROGRESS=ON", "--log-level=STATUS")
if ($Generator) { $configure += @("-G", $Generator) }
if ($Architecture) { $configure += @("-A", $Architecture) }
if ($ForceWxFetch) { $configure += @("-DDODEV_WX_FORCE_FETCH=ON", "-DDODEV_FETCH_WXWIDGETS=ON") }
if ($NoWxFetch) { $configure += @("-DDODEV_WX_FORCE_FETCH=OFF", "-DDODEV_FETCH_WXWIDGETS=OFF", "-DDODEV_WX_SOURCE_ARCHIVE=", "-DFETCHCONTENT_SOURCE_DIR_WXWIDGETS=") }
if ($WxSourceArchive) { $configure += "-DDODEV_WX_SOURCE_ARCHIVE=$([IO.Path]::GetFullPath($WxSourceArchive))" }
$configure += $CMakeArgs
Write-Host "`n[1/3] Configure DoDevEditor (dependency discovery and download)" -ForegroundColor Cyan
Invoke-LoggedNative -Program cmake -Arguments $configure -LogFile (Join-Path $logDir "configure.log")

$build = @("--build", $BuildDir, "--config", $Configuration, "--parallel", "$Jobs")
if ($VerboseBuild) { $build += "--verbose" }
Write-Host "`n[2/3] Compile dependencies and DoDevEditor" -ForegroundColor Cyan
Invoke-LoggedNative -Program cmake -Arguments $build -LogFile (Join-Path $logDir "build.log")

if (-not $SkipTests) {
    Write-Host "`n[3/3] Run tests" -ForegroundColor Cyan
    Invoke-LoggedNative -Program ctest -Arguments @("--test-dir", $BuildDir, "-C", $Configuration, "--output-on-failure") -LogFile (Join-Path $logDir "tests.log")
} else {
    Write-Host "`n[3/3] Tests skipped by request" -ForegroundColor Cyan
}
Write-Host "`n[OK] Build directory: $BuildDir" -ForegroundColor Green
Write-Host "[OK] Diagnostic logs: $logDir" -ForegroundColor Green
