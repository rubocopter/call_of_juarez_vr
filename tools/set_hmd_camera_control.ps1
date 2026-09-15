param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [ValidateSet("enable", "recenter", "disable")]
    [string]$Mode = "enable"
)

$ErrorActionPreference = "Stop"
$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$StageStatePath = Join-Path $GameDirectory ".cojvr-d3d9-stage.json"
if (-not (Test-Path -LiteralPath $StageStatePath -PathType Leaf)) {
    throw "No staged CoJ VR diagnostic was found."
}
$StageState = Get-Content -LiteralPath $StageStatePath -Raw | ConvertFrom-Json
if ([string]$StageState.diagnosticMode -ne "d3d9_hmd_camera") {
    throw "The staged diagnostic is '$($StageState.diagnosticMode)', not d3d9_hmd_camera."
}

$ControlPath = Join-Path $GameDirectory "cojvr-camera-control.json"
$TemporaryPath = "$ControlPath.tmp-$([Guid]::NewGuid().ToString('N'))"
$TrackingEnabled = $Mode -ne "disable"
$Control = [ordered]@{
    enabled = $false
    trackingEnabled = $TrackingEnabled
    recenter = $Mode -in @("enable", "recenter")
    yawDegrees = 0.0
    pitchDegrees = 0.0
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

Write-Host "HMD camera control updated: $ControlPath"
Write-Host "Mode: $Mode"
