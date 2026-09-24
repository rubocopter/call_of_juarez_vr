param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory
)

$ErrorActionPreference = "Stop"
$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$Provenance = & (Join-Path $PSScriptRoot "get_run_provenance.ps1") `
    -GameDirectory $GameDirectory `
    -ExpectedDiagnosticMode "d3d9_readback"
$Lines = $Provenance.Lines
$Checks = [ordered]@{
    "known exact build" = [bool]($Lines -match "d3d9 bootstrap: host=Call of Juarez \(Direct3D 9\).*exact_build=known")
    "classic D3D9 native factory observation active" = [bool]($Lines -match "d3d9 Direct3DCreate9: native factory observation active")
    "CreateDevice observed" = [bool]($Lines -match "d3d9 CreateDevice: .*hr=0x0")
    "Present/Reset hooks active" = [bool]($Lines -match "d3d9 device hooks: Present/Reset active")
    "Present frame boundary observed" = [bool]($Lines -match "d3d9 Present: frame boundary observed")
    "classic D3D9 readback uploaded to D3D11" = [bool]($Lines -match "d3d9 classic readback -> D3D11: success success backbuffer=")
    "D3D9Ex substitution inactive" = -not [bool]($Lines -match "d3d9 D3D9Ex bridge: native Ex factory active")
    "readback diagnostic has no failure" = -not [bool]($Lines -match "d3d9 classic readback -> D3D11: failed")
}

$Failed = @($Checks.GetEnumerator() | Where-Object { -not $_.Value })
foreach ($Check in $Checks.GetEnumerator()) {
    $State = if ($Check.Value) { "PASS" } else { "FAIL" }
    Write-Host "$State - $($Check.Key)"
}

if ($Failed.Count -ne 0) {
    throw "Readback live-test log is incomplete. Do not promote the classic-D3D9 readback path to live-tested."
}

Write-Host "Readback log evidence passed. Confirm separately that rendering was visually normal and the game exited cleanly."
