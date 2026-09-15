param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory
)

$ErrorActionPreference = "Stop"

$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$OpenVrDestination = Join-Path $GameDirectory "openvr_api.dll"
$OpenVrBackup = Join-Path $GameDirectory "openvr_api.cojvr-backup.dll"
$State = Join-Path $GameDirectory ".cojvr-openvr-flat-stage.json"
$D3D9Destination = Join-Path $GameDirectory "d3d9.dll"
$D3D9State = Join-Path $GameDirectory ".cojvr-d3d9-stage.json"

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

if (Test-Path -LiteralPath $D3D9State -PathType Leaf) {
    & (Join-Path $PSScriptRoot "unstage_d3d9_proxy.ps1") -GameDirectory $GameDirectory
} elseif (Test-Path -LiteralPath $D3D9Destination -PathType Leaf) {
    throw "The D3D9 staging state is missing but d3d9.dll is still present. Refusing to alter an unrecognized proxy."
} else {
    Write-Host "The D3D9 proxy was already unstaged; continuing OpenVR flat cleanup."
}

Remove-Item -LiteralPath $OpenVrDestination -Force
if ([bool]$StageState.hadOriginalOpenVr) {
    Move-Item -LiteralPath $OpenVrBackup -Destination $OpenVrDestination
    Write-Host "Restored the original openvr_api.dll."
} else {
    Write-Host "Removed staged openvr_api.dll; no original was present."
}

Remove-Item -LiteralPath $State -Force
Write-Host "OpenVR flat diagnostic fully unstaged."
