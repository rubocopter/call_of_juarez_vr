param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$BuildDirectory = Join-Path $PSScriptRoot "..\build\win32-debug\$Configuration"
$Probe = Join-Path $BuildDirectory "cojvr_openxr_d3d11_probe.exe"
$Loader = Join-Path $BuildDirectory "openxr_loader.dll"
$Log = Join-Path $BuildDirectory "openxr_d3d11_probe.log"

if (-not (Test-Path -LiteralPath $Probe -PathType Leaf)) {
    throw "OpenXR D3D11 probe was not found at '$Probe'. Build $Configuration first."
}
if (-not (Test-Path -LiteralPath $Loader -PathType Leaf)) {
    throw "Pinned openxr_loader.dll was not found beside the probe. Build $Configuration first."
}

$SteamVrRunning = (Get-Process -Name vrserver -ErrorAction SilentlyContinue) -or
                  (Get-Process -Name vrmonitor -ErrorAction SilentlyContinue)
if (-not $SteamVrRunning) {
    throw "SteamVR is not running. Start SteamVR manually before running this probe."
}

if (Test-Path -LiteralPath $Log) {
    Remove-Item -LiteralPath $Log -Force
}

Write-Host "Running isolated OpenXR D3D11 compositor probe ($Configuration)."
Write-Host "This does not modify or launch Call of Juarez."

& $Probe 2>&1 | Tee-Object -FilePath $Log
$ExitCode = $LASTEXITCODE
if ($ExitCode -ne 0) {
    throw "OpenXR D3D11 compositor probe failed with exit code $ExitCode. See '$Log'."
}

if (-not (Select-String -LiteralPath $Log -Pattern "OpenXR compositor probe passed; submitted_frames=180" -Quiet)) {
    throw "Expected 180-frame success marker was not found in '$Log'."
}

Write-Host "PASS - OpenXR D3D11 compositor submitted 180 projection frames."
