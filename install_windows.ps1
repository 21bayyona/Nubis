# =============================================================================
#  NUBIS - copies the built VST3 bundles to the standard Windows VST3 folder.
#  Usage (from the project root):
#     powershell -ExecutionPolicy Bypass -File scripts\install_windows.ps1
# =============================================================================
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dest = Join-Path $env:CommonProgramFiles "VST3"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "Requesting administrator rights to write to $dest ..."
    Start-Process powershell -Verb RunAs -ArgumentList "-ExecutionPolicy Bypass -File `"$PSCommandPath`""
    exit
}

$bundles = @(
    (Join-Path $root "build\Nubis_artefacts\Release\VST3\Nubis.vst3"),
    (Join-Path $root "build\NubisInstrument_artefacts\Release\VST3\Nubis Instrument.vst3")
)
if (-not (Test-Path $dest)) { New-Item -ItemType Directory -Path $dest | Out-Null }

foreach ($b in $bundles) {
    if (Test-Path $b) {
        $target = Join-Path $dest (Split-Path $b -Leaf)
        if (Test-Path $target) { Remove-Item $target -Recurse -Force }
        Copy-Item $b $dest -Recurse -Force
        Write-Host "Installed: $target"
    } else {
        Write-Host "Not found (build first): $b"
    }
}
Write-Host "`nNow open FL Studio > Options > Manage plugins > Find more plugins (or 'Find installed plugins')."
Read-Host "Press Enter to close"
