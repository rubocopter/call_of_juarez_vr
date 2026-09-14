param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory
)

$ErrorActionPreference = "Stop"
$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$Log = Join-Path $GameDirectory "cojvr.log"

if (-not (Test-Path -LiteralPath $Log -PathType Leaf)) {
    throw "cojvr.log was not found. The readback diagnostic has not produced live-test evidence."
}

$Lines = Get-Content -LiteralPath $Log
$Checks = [ordered]@{
    "known exact build" = [bool]($Lines -match "d3d9 bootstrap: host=Call of Juarez \(Direct3D 9\).*exact_build=known")
    "classic D3D9 forwarding active" = [bool]($Lines -match "d3d9 Direct3DCreate9: forwarding wrapper active")
    "CreateDevice observed" = [bool]($Lines -match "d3d9 CreateDevice: .*hr=0x0")
    "Present/Reset hooks active" = [bool]($Lines -match "d3d9 device hooks: Present/Reset active")
    "Present frame boundary observed" = [bool]($Lines -match "d3d9 Present: frame boundary observed")
    "classic D3D9 readback uploaded to D3D11" = [bool]($Lines -match "d3d9 classic readback -> D3D11: success success backbuffer=")
    "D3D9Ex substitution inactive" = -not [bool]($Lines -match "d3d9 D3D9Ex bridge: forwarding wrapper active")
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
