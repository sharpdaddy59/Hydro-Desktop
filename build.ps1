# build.ps1 — arduino-cli build/upload/monitor wrapper for hydro-dash.
#
# This project uses the mainstream esp32:esp32 core via arduino-cli. The
# CYD doesn't have the OPI-PSRAM init issues that pushed cores3-hydro
# onto M5Stack's fork, so the stock core is fine.
#
# Examples:
#   .\build.ps1                          # compile only
#   .\build.ps1 -Upload                  # compile + auto-detect + flash
#   .\build.ps1 -Upload -Monitor         # ... + serial @ 115200
#   .\build.ps1 -Upload -Port COM8       # specify port
#   .\build.ps1 -Strict                  # warnings=all
#
# Pre-reqs (run setup.ps1 once):
#   - arduino-cli on PATH
#   - esp32:esp32 core installed
#   - LovyanGFX, WiFiManager, ArduinoJson installed

[CmdletBinding()]
param(
    [string]$Port = "",
    [switch]$Upload,
    [switch]$Monitor,
    [switch]$Strict
)

# Generic ESP32 dev board FQBN — works for the WROOM-32 module on the CYD.
# If you find your CYD revision needs PSRAM enabled, change to:
#   m5stack -> not applicable; use:
#   esp32:esp32:esp32:PSRAM=enabled
$Fqbn = "esp32:esp32:esp32"

function Find-SketchDir {
    foreach ($c in @($PWD.Path, $PSScriptRoot) | Select-Object -Unique) {
        $leaf = Split-Path -Leaf $c
        if (Test-Path (Join-Path $c "$leaf.ino")) { return $c }
    }
    return $null
}
$SketchDir = Find-SketchDir
if (-not $SketchDir) {
    Write-Host "[build] No matching .ino found. arduino-cli requires <dir-leaf-name>/<dir-leaf-name>.ino."
    exit 1
}

$warn = if ($Strict) { "all" } else { "default" }
Write-Host "[build] Compiling (FQBN: $Fqbn, warnings=$warn)"

$compileArgs = @(
    "compile"
    "--fqbn"; $Fqbn
    "--warnings"; $warn
    "--export-binaries"
    $SketchDir
)
& arduino-cli @compileArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$binDir  = Join-Path $SketchDir "build\$($Fqbn -replace ':', '.')"
$binPath = Join-Path $binDir "$(Split-Path -Leaf $SketchDir).ino.bin"
if (Test-Path $binPath) {
    Write-Host "[build] Firmware .bin: $binPath"
}

if (-not $Upload) {
    Write-Host "[build] Compile OK."
    return
}

if (-not $Port) {
    Write-Host "[build] Auto-detecting ESP32 port..."
    $raw = arduino-cli board list --format json | ConvertFrom-Json
    $boards = if ($raw.detected_ports) { $raw.detected_ports } else { $raw }

    $candidates = @()
    foreach ($b in $boards) {
        if ($b.port.protocol -ne "serial") { continue }
        $fqbns = @()
        if ($b.matching_boards) { $fqbns = @($b.matching_boards.fqbn) }
        $score = 0
        if ($fqbns -contains $Fqbn)                            { $score = 3 }
        elseif ($fqbns | Where-Object { $_ -like "esp32:*" })  { $score = 1 }
        if ($score -gt 0) {
            $candidates += [pscustomobject]@{ Port = $b.port.address; Score = $score; Fqbns = $fqbns }
        }
    }
    if (-not $candidates) {
        Write-Host "[build] No ESP32 serial port detected. Plug in the CYD or pass -Port COMx."
        exit 1
    }
    $best = $candidates | Sort-Object -Property Score -Descending | Select-Object -First 1
    $Port = $best.Port
    Write-Host "[build] Found ESP32 device on $Port"
}

Write-Host "[build] Uploading to $Port..."
arduino-cli upload -p $Port --fqbn $Fqbn $SketchDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Monitor) {
    Write-Host "[build] Opening serial monitor on $Port at 115200. Ctrl+C to exit."
    arduino-cli monitor -p $Port -c baudrate=115200
}
