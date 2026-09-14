param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory
)

$ErrorActionPreference = "Stop"

$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$OpenVrDestination = Join-Path $GameDirectory "openvr_api.dll"
$OpenVrBackup = Join-Path $GameDirectory "openvr_api.cojvr-backup.dll"
$State = Join-Path $GameDirectory ".cojvr-openvr-flat-stage.json"

if (Get-Process -Name CoJ -ErrorAction SilentlyContinue) {
    throw "Call of Juarez is running. Close it before unstaging the OpenVR flat bridge."
}
if (-not (Test-Path -LiteralPath $State -PathType Leaf)) {
    throw "No OpenVR flat staging state was found at '$State'."
}

$StageState = Get-Content -LiteralPath $State -Raw | ConvertFrom-Json
if (-not (Test-Path -LiteralPath $OpenVrDestination -PathType Leaf)) {
    throw "The staged openvr_api.dll is missing. Refusing to alter backup/state automatically."
}
$CurrentHash = (Get-FileHash -LiteralPath $OpenVrDestination -Algorithm SHA256).Hash.ToUpperInvariant()
if ($CurrentHash -ne [string]$StageState.stagedOpenVrSha256) {
    throw "openvr_api.dll changed after staging. Refusing to remove an unrecognized file."
}
if ([bool]$StageState.hadOriginalOpenVr -and
    -not (Test-Path -LiteralPath $OpenVrBackup -PathType Leaf)) {
    throw "The original openvr_api.dll backup is missing. Refusing to alter the staged files."
}

& (Join-Path $PSScriptRoot "unstage_d3d9_proxy.ps1") -GameDirectory $GameDirectory

Remove-Item -LiteralPath $OpenVrDestination -Force
if ([bool]$StageState.hadOriginalOpenVr) {
    Move-Item -LiteralPath $OpenVrBackup -Destination $OpenVrDestination
    Write-Host "Restored the original openvr_api.dll."
} else {
    Write-Host "Removed staged openvr_api.dll; no original was present."
}

Remove-Item -LiteralPath $State -Force
Write-Host "OpenVR flat diagnostic fully unstaged."
