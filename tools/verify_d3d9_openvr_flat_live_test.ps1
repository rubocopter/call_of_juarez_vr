param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory
)

$ErrorActionPreference = "Stop"
$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$Log = Join-Path $GameDirectory "cojvr.log"
$Proxy = Join-Path $GameDirectory "d3d9.dll"
$StageStatePath = Join-Path $GameDirectory ".cojvr-d3d9-stage.json"
$ExpectedProxySha256 = "369754A6D93A1A93C87B157E9480F8F82518A1F703B67ADCB8C56F889A14A6AF"

if (-not (Test-Path -LiteralPath $Log -PathType Leaf)) {
    throw "cojvr.log was not found. The OpenVR flat bridge has not produced live-test evidence."
}

$ProxyHashMatches = $false
$StageStateMatches = $false
if (Test-Path -LiteralPath $Proxy -PathType Leaf) {
    $ProxyHashMatches =
        (Get-FileHash -LiteralPath $Proxy -Algorithm SHA256).Hash.ToUpperInvariant() -eq
        $ExpectedProxySha256
}
if (Test-Path -LiteralPath $StageStatePath -PathType Leaf) {
    try {
        $StageState = Get-Content -LiteralPath $StageStatePath -Raw | ConvertFrom-Json
        $StageStateMatches =
            ([string]$StageState.stagedProxySha256).ToUpperInvariant() -eq $ExpectedProxySha256
    } catch {
        $StageStateMatches = $false
    }
}

$Lines = Get-Content -LiteralPath $Log

$MaxBeginSceneCallbacks = 0L
$MaxPresentCallbacks = 0L
$MaxPresentCallbackReturns = 0L
$MaxEndSceneCallbacks = 0L
$MaxEndSceneCallbackReturns = 0L
$MaxSubmittedFrames = 0L
$LastPhaseCallback = 0L
$LastPhase = "none"
$HookContinuity = "none"
foreach ($Line in $Lines) {
    if ($Line -match "d3d9 Present: callback_count=([0-9]+)") {
        $MaxPresentCallbacks = [Math]::Max($MaxPresentCallbacks, [int64]$Matches[1])
    }
    if ($Line -match "d3d9 Present: callback_return_count=([0-9]+)") {
        $MaxPresentCallbackReturns = [Math]::Max($MaxPresentCallbackReturns, [int64]$Matches[1])
    }
    if ($Line -match "d3d9 BeginScene: callback_count=([0-9]+)") {
        $MaxBeginSceneCallbacks = [Math]::Max($MaxBeginSceneCallbacks, [int64]$Matches[1])
    }
    if ($Line -match "d3d9 EndScene: callback_count=([0-9]+)") {
        $MaxEndSceneCallbacks = [Math]::Max($MaxEndSceneCallbacks, [int64]$Matches[1])
    }
    if ($Line -match "d3d9 EndScene: callback_return_count=([0-9]+)") {
        $MaxEndSceneCallbackReturns = [Math]::Max($MaxEndSceneCallbackReturns, [int64]$Matches[1])
    }
    if ($Line -match "d3d9 OpenVR flat bridge: submitted_frames=([0-9]+)") {
        $MaxSubmittedFrames = [Math]::Max($MaxSubmittedFrames, [int64]$Matches[1])
    }
    if ($Line -match "d3d9 OpenVR flat bridge: callback=([0-9]+) phase=([a-z0-9_]+)") {
        $LastPhaseCallback = [int64]$Matches[1]
        $LastPhase = [string]$Matches[2]
    }
    if ($Line -match "d3d9 device hook continuity: (overwritten|stable_10s)") {
        $HookContinuity = [string]$Matches[1]
    }
}

if ($MaxBeginSceneCallbacks -eq 0 -and
    [bool]($Lines -match "d3d9 BeginScene: frame boundary observed")) {
    $MaxBeginSceneCallbacks = 1
}
if ($MaxPresentCallbacks -eq 0 -and
    [bool]($Lines -match "d3d9 Present: frame boundary observed")) {
    $MaxPresentCallbacks = 1
}
if ($MaxEndSceneCallbacks -eq 0 -and
    [bool]($Lines -match "d3d9 EndScene: frame boundary observed")) {
    $MaxEndSceneCallbacks = 1
}
if ($MaxSubmittedFrames -eq 0 -and
    [bool]($Lines -match "d3d9 OpenVR flat bridge: first stereo submission success")) {
    $MaxSubmittedFrames = 1
}

$Checks = [ordered]@{
    "expected diagnostic d3d9.dll is staged" = $ProxyHashMatches
    "staging state matches expected diagnostic" = $StageStateMatches
    "known exact build" = [bool]($Lines -match "d3d9 bootstrap: host=Call of Juarez \(Direct3D 9\).*exact_build=known")
    "classic D3D9 forwarding active" = [bool]($Lines -match "d3d9 Direct3DCreate9: forwarding wrapper active")
    "CreateDevice observed" = [bool]($Lines -match "d3d9 CreateDevice: .*hr=0x0")
    "Present/Reset hooks active" = [bool]($Lines -match "d3d9 device hooks: Present/Reset active")
    "Present frame boundary observed" = [bool]($Lines -match "d3d9 Present: frame boundary observed")
    "BeginScene frame boundary observed" = [bool]($Lines -match "d3d9 BeginScene: frame boundary observed")
    "EndScene hook active" = [bool]($Lines -match "d3d9 device hook: EndScene active")
    "EndScene frame boundary observed" = [bool]($Lines -match "d3d9 EndScene: frame boundary observed")
    "OpenVR phase diagnostics observed" = $LastPhaseCallback -gt 0
    "at least 300 EndScene callbacks observed" = $MaxEndSceneCallbacks -ge 300
    "first OpenVR stereo submission succeeded" = [bool]($Lines -match "d3d9 OpenVR flat bridge: first stereo submission success")
    "at least 300 game frames reached SteamVR" = $MaxSubmittedFrames -ge 300
    "D3D9Ex substitution inactive" = -not [bool]($Lines -match "d3d9 D3D9Ex bridge: forwarding wrapper active")
    "OpenVR flat bridge has no logged failure" = -not [bool]($Lines -match "d3d9 OpenVR flat bridge: failure")
}

$Failed = @($Checks.GetEnumerator() | Where-Object { -not $_.Value })
foreach ($Check in $Checks.GetEnumerator()) {
    $State = if ($Check.Value) { "PASS" } else { "FAIL" }
    Write-Host "$State - $($Check.Key)"
}

Write-Host "Observed progress - Present callbacks: $MaxPresentCallbacks; Present callback returns: $MaxPresentCallbackReturns; BeginScene callbacks: $MaxBeginSceneCallbacks; EndScene callbacks: $MaxEndSceneCallbacks; EndScene callback returns: $MaxEndSceneCallbackReturns; OpenVR submissions: $MaxSubmittedFrames"
Write-Host "Last OpenVR bridge phase - callback: $LastPhaseCallback; phase: $LastPhase"
Write-Host "Device hook continuity - $HookContinuity"

if ($Failed.Count -ne 0) {
    throw "OpenVR flat live-test evidence is incomplete. Do not promote the in-game bridge."
}

Write-Host "OpenVR flat bridge log evidence passed. Confirm headset-visible behavior and gameplay separately."
