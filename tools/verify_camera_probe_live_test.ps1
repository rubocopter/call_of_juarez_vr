param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory
)

$ErrorActionPreference = "Stop"
$ExpectedCoJHash = "5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE"
$ExpectedChromeEngineHash = "DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8"

$Provenance = & (Join-Path $PSScriptRoot "get_run_provenance.ps1") `
    -GameDirectory $GameDirectory `
    -ExpectedDiagnosticMode "d3d9_camera_probe"

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

function Assert-LogMatch([string]$Pattern, [string]$Failure) {
    if (-not [bool]($Lines -match $Pattern)) { throw $Failure }
}

Assert-LogMatch `
    "camera_probe_bootstrap: status=(installed|already_installed) system_d3d9=expected" `
    "The exact-build camera probe did not install over normal system D3D9 forwarding."
Assert-LogMatch `
    "camera_probe_event: event=camera_probe_install result=installed" `
    "The ChromeEngine camera-vtable profile was not accepted."
Assert-LogMatch `
    "camera_probe_event: event=camera_probe_control_loaded result=accepted" `
    "No external camera-control command was accepted."
Assert-LogMatch `
    "camera_probe_event: event=camera_probe_orientation_applied result=ok .*restored=true;renderer_camera_match=true" `
    "No reproducible render-camera orientation override with state restoration was observed."
Assert-LogMatch `
    "camera_probe_event: event=camera_probe_fov_applied result=ok" `
    "No render-camera FOV override was observed."
Assert-LogMatch `
    "camera_probe_event: event=camera_probe_restore result=restored" `
    "Camera vtable hooks were not cleanly restored on normal exit."
$EscapedRunId = [Regex]::Escape([string]$Provenance.RunId)
Assert-LogMatch "run_end: run_id=$EscapedRunId(?:\s|$)" "The run did not end normally."

Write-Host "PASS - exact CoJ/ChromeEngine build identities verified."
Write-Host "PASS - external control reached the renderer camera update and FOV path."
Write-Host "PASS - natural camera basis was restored after the render update."
Write-Host "PASS - camera probe hooks restored on normal process exit."
