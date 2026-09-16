param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory
)

$ErrorActionPreference = "Stop"
$ExpectedCoJHash = "5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE"
$ExpectedChromeEngineHash = "DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8"

$Provenance = & (Join-Path $PSScriptRoot "get_run_provenance.ps1") `
    -GameDirectory $GameDirectory `
    -ExpectedDiagnosticMode "d3d9_hmd_camera"
$Run = $Provenance.Run
$Lines = @($Provenance.Lines)

if (([string]$Run.game.executable.sha256).ToUpperInvariant() -ne $ExpectedCoJHash -or
    -not [bool]$Run.game.executable.knownExactBuild) {
    throw "The run was not bound to the exact inspected CoJ.exe build."
}
if (([string]$Run.game.engine.sha256).ToUpperInvariant() -ne $ExpectedChromeEngineHash -or
    -not [bool]$Run.game.engine.knownInspectedBuild) {
    throw "The run was not bound to the exact inspected ChromeEngine3.dll build."
}
$OpenVrDeployment = @($Run.deployment | Where-Object { [string]$_.role -eq "openvr_runtime" })
if ($OpenVrDeployment.Count -ne 1 -or [string]$OpenVrDeployment[0].destination -ne "openvr_api.dll") {
    throw "The run manifest does not bind exactly one staged OpenVR runtime."
}

function Assert-LogMatch([string]$Pattern, [string]$Failure) {
    if (-not [bool]($Lines -match $Pattern)) { throw $Failure }
}

Assert-LogMatch `
    "camera_hmd_pose_source: status=started backend=openvr tracking_space=standing" `
    "The OpenVR HMD pose source did not initialize successfully."
Assert-LogMatch `
    "camera_probe_bootstrap: status=(installed|already_installed) system_d3d9=expected" `
    "The HMD camera candidate did not install over normal system D3D9 forwarding."
Assert-LogMatch `
    "camera_probe_event: event=camera_probe_install result=installed .*pose_source=available" `
    "The exact ChromeEngine camera profile was not connected to the XR-neutral pose source."
Assert-LogMatch `
    "camera_probe_event: event=camera_hmd_recentered result=ok" `
    "No HMD base orientation/recenter was observed."
Assert-LogMatch `
    "camera_probe_event: event=camera_probe_control_loaded result=accepted .*tracking_enabled=true" `
    "No command enabled HMD tracking for this run."

$OrientationLines = @($Lines | Where-Object {
    $_ -match "camera_probe_event: event=camera_hmd_orientation_applied result=ok"
})
if ($OrientationLines.Count -lt 2) {
    throw "Too few HMD-driven camera samples were recorded."
}
foreach ($Line in $OrientationLines) {
    if ($Line -notmatch "native_homogeneous_layout=true") {
        throw "An HMD camera sample did not preserve the native homogeneous source-matrix layout."
    }
    if ($Line -notmatch "source_world_homogeneous_layout=true" -or
        $Line -notmatch "source_view_homogeneous_layout=true" -or
        $Line -notmatch "injected_view_homogeneous_layout=true") {
        throw "An HMD camera sample did not preserve the complete native world/view homogeneous matrix contract."
    }
    $NaturalDeterminantMatch = [regex]::Match($Line, "natural_determinant=([-+0-9.eE]+)")
    if (-not $NaturalDeterminantMatch.Success) {
        throw "An HMD camera sample did not report its natural source-basis determinant."
    }
    $NaturalDeterminant = [double]::Parse(
        $NaturalDeterminantMatch.Groups[1].Value,
        [System.Globalization.CultureInfo]::InvariantCulture)
    if (-not [double]::IsFinite($NaturalDeterminant) -or
        [Math]::Abs($NaturalDeterminant - 1.0) -gt 0.02) {
        throw "An HMD camera sample started from a reflected or non-rigid natural source basis (determinant=$NaturalDeterminant)."
    }
    $DeterminantMatch = [regex]::Match($Line, "applied_determinant=([-+0-9.eE]+)")
    if (-not $DeterminantMatch.Success) {
        throw "An HMD camera sample did not report its applied source-basis determinant."
    }
    $AppliedDeterminant = [double]::Parse(
        $DeterminantMatch.Groups[1].Value,
        [System.Globalization.CultureInfo]::InvariantCulture)
    if ([Math]::Abs($AppliedDeterminant - 1.0) -gt 0.02) {
        throw "An HMD camera sample used a reflected or non-rigid source basis (determinant=$AppliedDeterminant)."
    }
    if ($Line -notmatch "render_basis_observed=true.*view_matrix_observed=true.*restored=true;renderer_camera_match=true") {
        throw "An HMD camera sample did not prove culling/view observation, restoration and renderer-camera identity."
    }
}
if (-not ($OrientationLines | Where-Object { $_ -match "render_basis_changed=true" })) {
    throw "HMD camera telemetry never observed the engine-derived render basis change after source-basis injection."
}
if (-not ($OrientationLines | Where-Object {
    $_ -match "view_matrix_changed=true" -and $_ -match "view_projection_changed=true"
})) {
    throw "HMD camera telemetry never observed both the actual view matrix and view-projection matrix change."
}
$Sequences = @($OrientationLines | ForEach-Object {
    if ($_ -match "pose_sequence=([0-9]+)") { [uint64]$Matches[1] }
} | Sort-Object -Unique)
if ($Sequences.Count -lt 2) {
    throw "HMD camera telemetry did not advance across distinct pose samples."
}
$YawSamples = @($OrientationLines | ForEach-Object {
    if ($_ -match "yaw_degrees=([-+0-9.eE]+)") { [double]$Matches[1] }
})
$PitchSamples = @($OrientationLines | ForEach-Object {
    if ($_ -match "pitch_degrees=([-+0-9.eE]+)") { [double]$Matches[1] }
})
if (-not ($YawSamples | Where-Object { [Math]::Abs($_) -ge 5.0 })) {
    throw "Telemetry did not capture a meaningful physical yaw movement."
}
if (-not ($PitchSamples | Where-Object { [Math]::Abs($_) -ge 3.0 })) {
    throw "Telemetry did not capture a meaningful physical pitch movement."
}

Assert-LogMatch `
    "camera_probe_event: event=camera_probe_passthrough result=disabled" `
    "Tracking was not explicitly disabled back to natural camera passthrough."
Assert-LogMatch `
    "camera_probe_event: event=camera_probe_control_loaded result=accepted .*tracking_enabled=false" `
    "No command explicitly disabled HMD tracking for this run."
$LastHmdIndex = -1
$DisabledIndex = -1
for ($Index = 0; $Index -lt $Lines.Count; ++$Index) {
    if ($Lines[$Index] -match "event=camera_hmd_orientation_applied result=ok") {
        $LastHmdIndex = $Index
    }
    if ($Lines[$Index] -match "event=camera_probe_passthrough result=disabled") {
        $DisabledIndex = $Index
    }
}
if ($LastHmdIndex -lt 0 -or $DisabledIndex -le $LastHmdIndex) {
    throw "Natural camera passthrough was not observed after HMD-driven orientation."
}
Assert-LogMatch `
    "camera_probe_event: event=camera_probe_restore result=restored" `
    "Camera vtable hooks were not cleanly restored on normal exit."
Assert-LogMatch `
    "camera_hmd_pose_source: status=stopped" `
    "The OpenVR pose source did not shut down cleanly."
$EscapedRunId = [Regex]::Escape([string]$Provenance.RunId)
Assert-LogMatch "run_end: run_id=$EscapedRunId(?:\s|$)" "The run did not end normally."

Write-Host "PASS - exact CoJ/ChromeEngine/OpenVR deployment identities verified."
Write-Host "PASS - HMD pose advanced through recentered XR-neutral camera orientation."
Write-Host "PASS - engine-derived render-basis change, renderer identity and source-basis restoration verified."
Write-Host "PASS - tracking disable, hook restore, XR shutdown and run end verified."
