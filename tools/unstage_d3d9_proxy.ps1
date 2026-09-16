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
$CameraControl = Join-Path $GameDirectory "cojvr-camera-control.json"
$CameraControlBackup = Join-Path $GameDirectory "cojvr-camera-control.cojvr-backup.json"
$OpenVrDestination = Join-Path $GameDirectory "openvr_api.dll"
$OpenVrBackup = Join-Path $GameDirectory "openvr_api.cojvr-backup.dll"
$OpenVrInputDestination = Join-Path $GameDirectory "cojvr_openvr_input"
$OpenVrInputBackup = Join-Path $GameDirectory "cojvr_openvr_input.cojvr-backup"

if (-not (Test-Path -LiteralPath $State -PathType Leaf)) {
    if (Test-Path -LiteralPath $BridgeMarker -PathType Leaf) {
        Remove-Item -LiteralPath $BridgeMarker -Force
        Write-Host "Removed the CoJ VR D3D9Ex bridge marker."
        return
    }
    throw "No CoJ VR staging state was found. Refusing to modify d3d9.dll."
}

$StageState = Get-Content -LiteralPath $State -Raw | ConvertFrom-Json
$CameraControlManaged = [bool]$StageState.cameraControlManaged
$OpenVrRuntimeManaged = [bool]$StageState.openVrRuntimeManaged
$OpenVrInputManaged = [bool]$StageState.openVrInputManaged
if ($CameraControlManaged -and [bool]$StageState.hadOriginalCameraControl -and
    -not (Test-Path -LiteralPath $CameraControlBackup -PathType Leaf)) {
    throw "The original camera-control backup is missing. Refusing to unstage incompletely."
}
if (-not (Test-Path -LiteralPath $Destination -PathType Leaf)) {
    throw "The staged d3d9.dll is missing. Refusing to alter the backup/state automatically."
}
if ($OpenVrRuntimeManaged) {
    if (-not (Test-Path -LiteralPath $OpenVrDestination -PathType Leaf)) {
        throw "The staged openvr_api.dll is missing. Refusing to unstage incompletely."
    }
    $CurrentOpenVrHash = (Get-FileHash -LiteralPath $OpenVrDestination -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($CurrentOpenVrHash -ne [string]$StageState.stagedOpenVrSha256) {
        throw "openvr_api.dll changed after staging. Refusing to remove an unrecognized file."
    }
    if ([bool]$StageState.hadOriginalOpenVr -and
        -not (Test-Path -LiteralPath $OpenVrBackup -PathType Leaf)) {
        throw "The original openvr_api.dll backup is missing. Refusing to unstage incompletely."
    }
}
if ($OpenVrInputManaged) {
    $ActionManifest = Join-Path $OpenVrInputDestination "actions.json"
    $SenseBinding = Join-Path $OpenVrInputDestination "bindings\psvr2_sense.json"
    if (-not (Test-Path -LiteralPath $ActionManifest -PathType Leaf) -or
        -not (Test-Path -LiteralPath $SenseBinding -PathType Leaf)) {
        throw "The staged OpenVR input assets are incomplete. Refusing to unstage incompletely."
    }
    if ((Get-FileHash -LiteralPath $ActionManifest -Algorithm SHA256).Hash.ToUpperInvariant() -ne
            ([string]$StageState.stagedOpenVrActionManifestSha256).ToUpperInvariant() -or
        (Get-FileHash -LiteralPath $SenseBinding -Algorithm SHA256).Hash.ToUpperInvariant() -ne
            ([string]$StageState.stagedOpenVrSenseBindingSha256).ToUpperInvariant()) {
        throw "OpenVR input assets changed after staging. Refusing to remove unrecognized files."
    }
    if ([bool]$StageState.hadOriginalOpenVrInput -and
        -not (Test-Path -LiteralPath $OpenVrInputBackup -PathType Container)) {
        throw "The original OpenVR input backup is missing. Refusing to unstage incompletely."
    }
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

if ($CameraControlManaged) {
    if (Test-Path -LiteralPath $CameraControl -PathType Leaf) {
        Remove-Item -LiteralPath $CameraControl -Force
    }
    if ([bool]$StageState.hadOriginalCameraControl) {
        Move-Item -LiteralPath $CameraControlBackup -Destination $CameraControl
        Write-Host "Restored the original camera-control file."
    } else {
        Write-Host "Removed the staged camera-control file."
    }
}

if ($OpenVrRuntimeManaged) {
    Remove-Item -LiteralPath $OpenVrDestination -Force
    if ([bool]$StageState.hadOriginalOpenVr) {
        Move-Item -LiteralPath $OpenVrBackup -Destination $OpenVrDestination
        Write-Host "Restored the original openvr_api.dll."
    } else {
        Write-Host "Removed the staged OpenVR runtime."
    }
}
if ($OpenVrInputManaged) {
    Remove-Item -LiteralPath $OpenVrInputDestination -Recurse -Force
    if ([bool]$StageState.hadOriginalOpenVrInput) {
        Move-Item -LiteralPath $OpenVrInputBackup -Destination $OpenVrInputDestination
        Write-Host "Restored the original OpenVR input directory."
    } else {
        Write-Host "Removed the staged OpenVR input assets."
    }
}

Remove-Item -LiteralPath $State -Force
if (Test-Path -LiteralPath $BridgeMarker -PathType Leaf) {
    Remove-Item -LiteralPath $BridgeMarker -Force
    Write-Host "Removed the CoJ VR D3D9Ex bridge marker."
}
