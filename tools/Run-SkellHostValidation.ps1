# SkellCore Windows local host-readiness / CPU baseline runner.
# Runs the same verified JUCE processor tests as GitHub CI, but measures
# on YOUR machine without copying plugins into FL Studio directories.
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\tools\Run-SkellHostValidation.ps1
#   powershell -ExecutionPolicy Bypass -File .\tools\Run-SkellHostValidation.ps1 -SkipBuild
#   powershell -ExecutionPolicy Bypass -File .\tools\Run-SkellHostValidation.ps1 -WithDSPTests
param(
    [string]$BuildDir = "",
    [string]$ReportDir = "",
    [switch]$SkipBuild,
    [switch]$WithDSPTests
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $BuildDir) { $BuildDir = Join-Path $repo "build-host-local" }
if (-not $ReportDir) { $ReportDir = Join-Path $repo "host-reports" }
New-Item -ItemType Directory -Path $ReportDir -Force | Out-Null

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake was not found. Install CMake and the Visual Studio C++ toolchain."
}
if (-not $SkipBuild) {
    $configure = @("-S", $repo, "-B", $BuildDir, "-A", "x64",
                   "-DSKELLCORE_BUILD_HOST_READINESS=ON")
    if ($WithDSPTests) {
        $configure += "-DSKELLCORE_BUILD_TESTS=ON"
        $configure += "-DSKELLCORE_BUILD_BENCHMARKS=ON"
    }
    & cmake @configure
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
    & cmake --build $BuildDir --config Release --parallel 2
    if ($LASTEXITCODE -ne 0) { throw "Windows SkellCore build failed" }
}

$exes = @(Get-ChildItem $BuildDir -Recurse -File -Filter "SkellCoreHostReadiness.exe")
if ($exes.Count -eq 0) {
    throw "SkellCoreHostReadiness.exe was not found. Rebuild without -SkipBuild."
}
$exe = $exes[0].FullName
$logs = Join-Path $ReportDir "host-readiness.log"
$output = @(& $exe 2>&1 | ForEach-Object { "$_" })
$output | Set-Content -Path $logs -Encoding UTF8
$output | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) { throw "SkellCore host-readiness checks failed (see $logs)" }

$lines = @($output | Where-Object { $_ -match '^SKELL_MATRIX_CSV,' })
if ($lines.Count -ne 37) {
    throw "Expected 36 benchmark cases plus CSV header, found $($lines.Count - 1)"
}
$matrixPath = Join-Path $ReportDir "host-matrix.csv"
$lines | ForEach-Object { $_ -replace '^SKELL_MATRIX_CSV,', '' } |
    Set-Content -Path $matrixPath -Encoding UTF8

if ($WithDSPTests) {
    & ctest --test-dir $BuildDir -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Additional portable DSP tests failed" }
}

$gitSha = "unavailable"
if (Get-Command git -ErrorAction SilentlyContinue) {
    $sha = @(& git -C $repo rev-parse HEAD 2>$null)
    if ($LASTEXITCODE -eq 0 -and $sha.Count -gt 0) { $gitSha = "$($sha[0])".Trim() }
}
$cpu = "unknown"
try {
    $cpuData = Get-CimInstance Win32_Processor -ErrorAction Stop | Select-Object -First 1
    if ($cpuData) { $cpu = $cpuData.Name }
} catch { $cpu = "unavailable" }

$pluginFiles = @(Get-ChildItem $BuildDir -Recurse -File -Filter "*.vst3" |
    Where-Object { $_.FullName -match 'VST3' })
$plugin = if ($pluginFiles.Count -gt 0) { $pluginFiles[0] } else { $null }
$binaryHash = if ($plugin) { (Get-FileHash -Path $plugin.FullName -Algorithm SHA256).Hash } else { "missing" }
$summary = [ordered]@{
    schema = "skellcore-local-host-readiness/1"
    date_utc = (Get-Date).ToUniversalTime().ToString("o")
    repo_commit = $gitSha
    processor = $cpu
    os = [Environment]::OSVersion.VersionString
    machine = $env:COMPUTERNAME
    test_status = "PASS"
    sample_rates = @(44100, 48000, 96000)
    block_sizes = @(64, 128, 512)
    instance_counts = @(1, 4, 8, 16)
    matrix_cases = 36
    vst3_binary = if ($plugin) { $plugin.FullName } else { $null }
    vst3_binary_sha256 = $binaryHash
    notes = "Direct JUCE processor test, NOT an FL Studio host-compatibility or measured DAW CPU test."
}
$reportPath = Join-Path $ReportDir "host-summary.json"
$summary | ConvertTo-Json -Depth 5 | Set-Content -Path $reportPath -Encoding UTF8

Write-Host ""
Write-Host "SkellCore Windows host readiness: PASS" -ForegroundColor Green
Write-Host "CPU: $cpu"
Write-Host "Commit: $gitSha"
Write-Host "CSV: $matrixPath"
Write-Host "Report: $reportPath"
Write-Host "VST3 binary SHA256: $binaryHash"
Write-Host "Next: use docs/FL_STUDIO_HOST_ACCEPTANCE.md to verify inside actual FL Studio."
