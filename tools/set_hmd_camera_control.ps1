param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [ValidateSet("enable", "recenter", "disable", "body-enable", "body-disable")]
    [string]$Mode = "enable"
)

$ErrorActionPreference = "Stop"
$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$StageStatePath = Join-Path $GameDirectory ".cojvr-d3d9-stage.json"
if (-not (Test-Path -LiteralPath $StageStatePath -PathType Leaf)) {
    throw "No staged CoJ VR diagnostic was found."
}
$StageState = Get-Content -LiteralPath $StageStatePath -Raw | ConvertFrom-Json
$SupportedModes = @("d3d9_hmd_camera", "d3d9_native_stereo")
if ([string]$StageState.diagnosticMode -notin $SupportedModes) {
    throw "The staged diagnostic '$($StageState.diagnosticMode)' does not support HMD camera control."
}

$ControlPath = Join-Path $GameDirectory "cojvr-camera-control.json"
$TemporaryPath = "$ControlPath.tmp-$([Guid]::NewGuid().ToString('N'))"
$ExistingControl = if (Test-Path -LiteralPath $ControlPath -PathType Leaf) {
    Get-Content -LiteralPath $ControlPath -Raw | ConvertFrom-Json
} else {
    $null
}
$ExistingTracking = if ($null -ne $ExistingControl -and
    $null -ne $ExistingControl.PSObject.Properties['trackingEnabled']) {
    [bool]$ExistingControl.trackingEnabled
} else {
    $false
}
$ExistingBodyIk = if ($null -ne $ExistingControl -and
    $null -ne $ExistingControl.PSObject.Properties['bodyIkEnabled']) {
    [bool]$ExistingControl.bodyIkEnabled
} else {
    $false
}
$BodyMode = $Mode -in @("body-enable", "body-disable")
$TrackingEnabled = if ($BodyMode) { $ExistingTracking } else { $Mode -ne "disable" }
$BodyIkEnabled = if ($Mode -eq "body-enable") {
    $true
} elseif ($Mode -eq "body-disable") {
    $false
} else {
    $ExistingBodyIk
}
$Control = [ordered]@{
    enabled = $false
    trackingEnabled = $TrackingEnabled
    bodyIkEnabled = $BodyIkEnabled
    recenter = (-not $BodyMode) -and ($Mode -in @("enable", "recenter"))
    yawDegrees = 0.0
    pitchDegrees = 0.0
    movementTraceEnabled = if ($null -ne $ExistingControl -and
        $null -ne $ExistingControl.PSObject.Properties['movementTraceEnabled']) {
        [bool]$ExistingControl.movementTraceEnabled
    } else { $false }
    movementTracePhase = if ($null -ne $ExistingControl -and
        $null -ne $ExistingControl.PSObject.Properties['movementTracePhase']) {
        [string]$ExistingControl.movementTracePhase
    } else { "off" }
    vrGameplayInputEnabled = if ($null -ne $ExistingControl -and
        $null -ne $ExistingControl.PSObject.Properties['vrGameplayInputEnabled']) {
        [bool]$ExistingControl.vrGameplayInputEnabled
    } else { $true }
    captureReadbackEnabled = if ($null -ne $ExistingControl -and
        $null -ne $ExistingControl.PSObject.Properties['captureReadbackEnabled']) {
        [bool]$ExistingControl.captureReadbackEnabled
    } else { $true }
    secondEyeRenderEnabled = if ($null -ne $ExistingControl -and
        $null -ne $ExistingControl.PSObject.Properties['secondEyeRenderEnabled']) {
        [bool]$ExistingControl.secondEyeRenderEnabled
    } else { $true }
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
Write-Host "Body IK enabled: $($BodyIkEnabled.ToString().ToLowerInvariant())"
