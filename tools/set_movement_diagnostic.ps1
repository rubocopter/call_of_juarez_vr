param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [Parameter(Mandatory = $true)]
    [ValidateSet("off", "vanilla", "vr-full", "vr-input-off", "vr-readback-off", "vr-single-eye")]
    [string]$Mode
)

$ErrorActionPreference = "Stop"
$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$StageStatePath = Join-Path $GameDirectory ".cojvr-d3d9-stage.json"
if (-not (Test-Path -LiteralPath $StageStatePath -PathType Leaf)) {
    throw "No staged CoJ diagnostic was found."
}
$StageState = Get-Content -LiteralPath $StageStatePath -Raw | ConvertFrom-Json
$SupportedModes = @("d3d9_camera_probe", "d3d9_native_stereo")
if ([string]$StageState.diagnosticMode -notin $SupportedModes) {
    throw "The staged diagnostic '$($StageState.diagnosticMode)' does not support movement tracing."
}
if ($Mode -eq "vanilla" -and [string]$StageState.diagnosticMode -ne "d3d9_camera_probe") {
    throw "The vanilla phase requires d3d9_camera_probe so no VR runtime owns presentation or input."
}
if ($Mode -like "vr-*" -and [string]$StageState.diagnosticMode -ne "d3d9_native_stereo") {
    throw "The '$Mode' phase requires d3d9_native_stereo."
}

$ControlPath = Join-Path $GameDirectory "cojvr-camera-control.json"
$Existing = if (Test-Path -LiteralPath $ControlPath -PathType Leaf) {
    Get-Content -LiteralPath $ControlPath -Raw | ConvertFrom-Json
} else { $null }
$BodyIkEnabled = if ($null -ne $Existing -and
    $null -ne $Existing.PSObject.Properties['bodyIkEnabled']) {
    [bool]$Existing.bodyIkEnabled
} else { $false }

$TraceEnabled = $Mode -ne "off"
$IsVr = $Mode -like "vr-*"
$Control = [ordered]@{
    enabled = $false
    trackingEnabled = $IsVr
    bodyIkEnabled = $BodyIkEnabled
    recenter = $false
    yawDegrees = 0.0
    pitchDegrees = 0.0
    movementTraceEnabled = $TraceEnabled
    movementTracePhase = if ($TraceEnabled) { $Mode } else { "off" }
    vrGameplayInputEnabled = $Mode -ne "vr-input-off"
    captureReadbackEnabled = $Mode -notin @("vr-readback-off", "vr-single-eye")
    secondEyeRenderEnabled = $Mode -ne "vr-single-eye"
}

$TemporaryPath = "$ControlPath.tmp-$([Guid]::NewGuid().ToString('N'))"
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

Write-Host "Movement diagnostic control updated: $ControlPath"
Write-Host "Phase: $($Control.movementTracePhase)"
Write-Host "VR gameplay input: $($Control.vrGameplayInputEnabled.ToString().ToLowerInvariant())"
Write-Host "Capture/readback: $($Control.captureReadbackEnabled.ToString().ToLowerInvariant())"
Write-Host "Second eye render: $($Control.secondEyeRenderEnabled.ToString().ToLowerInvariant())"
if ($Mode -eq "vr-input-off") {
    Write-Host "Native keyboard/gamepad ownership is exclusive. Release all controls before entering this phase."
}
