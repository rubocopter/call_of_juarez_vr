param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory
)

$ErrorActionPreference = "Stop"

$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$Destination = Join-Path $GameDirectory "d3d9.dll"
$Backup = Join-Path $GameDirectory "d3d9.cojvr-backup.dll"
$State = Join-Path $GameDirectory ".cojvr-d3d9-stage.json"
$BridgeMarker = Join-Path $GameDirectory ".cojvr-d3d9-ex-bridge"

if (-not (Test-Path -LiteralPath $State -PathType Leaf)) {
    if (Test-Path -LiteralPath $BridgeMarker -PathType Leaf) {
        Remove-Item -LiteralPath $BridgeMarker -Force
        Write-Host "Removed the CoJ VR D3D9Ex bridge marker."
        return
    }
    throw "No CoJ VR staging state was found. Refusing to modify d3d9.dll."
}

$StageState = Get-Content -LiteralPath $State -Raw | ConvertFrom-Json
if (-not (Test-Path -LiteralPath $Destination -PathType Leaf)) {
    throw "The staged d3d9.dll is missing. Refusing to alter the backup/state automatically."
}

$CurrentHash = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash.ToUpperInvariant()
if ($CurrentHash -ne [string]$StageState.stagedProxySha256) {
    throw "d3d9.dll changed after staging. Refusing to remove an unrecognized file."
}

Remove-Item -LiteralPath $Destination -Force

if ([bool]$StageState.hadOriginalD3D9) {
    if (-not (Test-Path -LiteralPath $Backup -PathType Leaf)) {
        throw "The original d3d9.dll backup is missing."
    }
    Move-Item -LiteralPath $Backup -Destination $Destination
    Write-Host "Restored the original d3d9.dll."
} else {
    Write-Host "Removed the CoJ VR proxy. No original d3d9.dll backup was present."
}

Remove-Item -LiteralPath $State -Force
if (Test-Path -LiteralPath $BridgeMarker -PathType Leaf) {
    Remove-Item -LiteralPath $BridgeMarker -Force
    Write-Host "Removed the CoJ VR D3D9Ex bridge marker."
}
