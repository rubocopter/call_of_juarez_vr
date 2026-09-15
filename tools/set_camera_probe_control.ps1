param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [ValidateSet("enable", "disable")]
    [string]$Mode = "enable",

    [double]$FovDegrees = 110.0,
    [double]$YawDegrees = 20.0,
    [double]$PitchDegrees = -10.0
)

$ErrorActionPreference = "Stop"

if ($FovDegrees -lt 30.0 -or $FovDegrees -gt 140.0) {
    throw "FovDegrees must be between 30 and 140."
}
if ($YawDegrees -lt -60.0 -or $YawDegrees -gt 60.0) {
    throw "YawDegrees must be between -60 and 60."
}
if ($PitchDegrees -lt -45.0 -or $PitchDegrees -gt 45.0) {
    throw "PitchDegrees must be between -45 and 45."
}

$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$StageStatePath = Join-Path $GameDirectory ".cojvr-d3d9-stage.json"
if (-not (Test-Path -LiteralPath $StageStatePath -PathType Leaf)) {
    throw "No staged CoJ VR diagnostic was found."
}
$StageState = Get-Content -LiteralPath $StageStatePath -Raw | ConvertFrom-Json
if ([string]$StageState.diagnosticMode -ne "d3d9_camera_probe") {
    throw "The staged diagnostic is '$($StageState.diagnosticMode)', not d3d9_camera_probe."
}

$ControlPath = Join-Path $GameDirectory "cojvr-camera-control.json"
$TemporaryPath = "$ControlPath.tmp-$([Guid]::NewGuid().ToString('N'))"
$Control = if ($Mode -eq "enable") {
    [ordered]@{
        enabled = $true
        fovDegrees = $FovDegrees
        yawDegrees = $YawDegrees
        pitchDegrees = $PitchDegrees
    }
} else {
    [ordered]@{
        enabled = $false
        yawDegrees = 0.0
        pitchDegrees = 0.0
    }
}

try {
    [System.IO.File]::WriteAllText(
        $TemporaryPath,
        ($Control | ConvertTo-Json) + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $TemporaryPath -Destination $ControlPath -Force
} finally {
    if (Test-Path -LiteralPath $TemporaryPath -PathType Leaf) {
        Remove-Item -LiteralPath $TemporaryPath -Force
    }
}

Write-Host "Camera probe control updated: $ControlPath"
Write-Host "Mode: $Mode"
if ($Mode -eq "enable") {
    Write-Host "FOV/yaw/pitch: $FovDegrees / $YawDegrees / $PitchDegrees degrees"
}
