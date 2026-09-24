param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory
)

$ErrorActionPreference = "Stop"

$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$Provenance = & (Join-Path $PSScriptRoot "get_run_provenance.ps1") `
    -GameDirectory $GameDirectory `
    -ExpectedDiagnosticMode "d3d9_forwarding"
$Lines = $Provenance.Lines
$Checks = [ordered]@{
    "known exact build" = [bool]($Lines -match "d3d9 bootstrap: host=Call of Juarez \(Direct3D 9\).*exact_build=known")
    "D3D9Ex bridge active" = [bool]($Lines -match "d3d9 D3D9Ex bridge: native Ex factory active")
    "CreateDevice observed" = [bool]($Lines -match "d3d9 CreateDevice: .*hr=0x0")
    "Present/Reset hooks active" = [bool]($Lines -match "d3d9 device hooks: Present/Reset active")
    "Present frame boundary observed" = [bool]($Lines -match "d3d9 Present: frame boundary observed")
}

$Failed = @($Checks.GetEnumerator() | Where-Object { -not $_.Value })
foreach ($Check in $Checks.GetEnumerator()) {
    $State = if ($Check.Value) { "PASS" } else { "FAIL" }
    Write-Host "$State - $($Check.Key)"
}

if ($Failed.Count -ne 0) {
    throw "Live-test log is incomplete. Do not promote the D3D9Ex bridge to live-tested."
}

Write-Host "Log evidence passed. Confirm separately that rendering was visually normal and the game exited cleanly."
if ($Lines -match "d3d9 Reset: .*hr=0x0") {
    Write-Host "INFO - successful Reset was also observed."
}
