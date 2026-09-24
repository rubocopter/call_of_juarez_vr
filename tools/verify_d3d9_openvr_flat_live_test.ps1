param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,
    [switch]$AllowIncomplete,
    [switch]$RequireSteamOverlayAbsent
)

$ErrorActionPreference = "Stop"
$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$Provenance = & (Join-Path $PSScriptRoot "get_run_provenance.ps1") `
    -GameDirectory $GameDirectory `
    -ExpectedDiagnosticMode "d3d9_openvr_flat"
$Report = & (Join-Path $PSScriptRoot "read_render_telemetry.ps1") `
    -LogPath (Join-Path $GameDirectory "cojvr.log") `
    -RunId $Provenance.RunId `
    -BuildManifestId $Provenance.BuildManifestId `
    -AllowIncomplete:$AllowIncomplete
$OverlayAbsenceRequired = $RequireSteamOverlayAbsent -or
    [bool]$Provenance.Run.validation.requireSteamOverlayAbsent

$Checks = [ordered]@{
    "structured core event set complete" = $Report.MissingCoreEvents.Count -eq 0
    "at least one native D3D9 factory/device identity observed" =
        $Report.FactoryIds.Count -gt 0 -and $Report.DeviceIds.Count -gt 0
    "native GetDirect3D factory identity preserved" = $Report.NativeIdentityMatches
    "implicit swapchain identity observed" = $Report.SwapchainIds.Count -gt 0
    "all installed hook slots remained owned" = $Report.HooksOwned
    "no hook installation conflict" = $Report.HookConflicts -eq 0
    "no hook installation failure" = $Report.HookInstallFailures -eq 0
    "Present/BeginScene/EndScene callback activity measured" =
        $Report.PresentCallbacks -gt 0 -and
        $Report.BeginSceneCallbacks -gt 0 -and
        $Report.EndSceneCallbacks -gt 0
    "at least one exact device activity stream identified" =
        @($Report.DeviceProgress | Where-Object { $_.Callbacks -gt 0 }).Count -gt 0
    "capture progression observed" = $Report.Captures -gt 0
    "new game content observed" = $Report.UniqueContentFrames -gt 0
    "every capture reached upload" = $Report.Uploads -eq $Report.Captures
    "balanced left/right submissions observed" =
        $Report.SubmitLeft -gt 0 -and $Report.SubmitLeft -eq $Report.SubmitRight
    "capture/upload/wait/submission stages reported no failure" =
        $Report.CaptureFailures -eq 0 -and
        $Report.UploadFailures -eq 0 -and
        $Report.WaitPosesFailures -eq 0 -and
        $Report.SubmitLeftFailures -eq 0 -and
        $Report.SubmitRightFailures -eq 0 -and
        $Report.RuntimeFailures -eq 0
    "no stage remained active at final summary" = $Report.LastActiveStage -eq "none"
    "D3D9Ex substitution inactive" =
        -not [bool]($Provenance.Lines -match "d3d9 D3D9Ex bridge: native Ex factory active")
}
if ($OverlayAbsenceRequired) {
    $Checks["factory CreateDevice original target excludes Steam Overlay"] =
        $Report.FactoryCreateDeviceOriginalPaths.Count -gt 0 -and
        -not $Report.SteamOverlayFactoryIntercepted
}

foreach ($Check in $Checks.GetEnumerator()) {
    $State = if ($Check.Value) { "PASS" } else { "FAIL" }
    Write-Host "$State - $($Check.Key)"
}

Write-Host "Run/build - $($Report.RunId) / $($Report.BuildManifestId)"
Write-Host "Identity - factories: $($Report.FactoryIds -join ','); devices: $($Report.DeviceIds -join ','); swapchains: $($Report.SwapchainIds -join ','); generations: $($Report.Generations -join ',')"
Write-Host "Factory CreateDevice original paths - $($Report.FactoryCreateDeviceOriginalPaths -join '; ')"
Write-Host "Steam Overlay absence required - $OverlayAbsenceRequired"
foreach ($Device in $Report.DeviceProgress) {
    Write-Host "Device $($Device.DeviceId) - factory: $($Device.FactoryId); swapchains: $($Device.SwapchainIds -join ','); generations: $($Device.Generations -join ','); callbacks P/B/E/S: $($Device.Present)/$($Device.BeginScene)/$($Device.EndScene)/$($Device.SwapchainPresent); capture/content/upload/submitL/submitR: $($Device.Captures)/$($Device.Content)/$($Device.Uploads)/$($Device.SubmitLeft)/$($Device.SubmitRight); active stage: $($Device.ActiveStage)"
}
Write-Host "Callbacks - total: $($Report.Callbacks); Present: $($Report.PresentCallbacks); BeginScene: $($Report.BeginSceneCallbacks); EndScene: $($Report.EndSceneCallbacks); SwapChainPresent: $($Report.SwapchainPresentCallbacks)"
Write-Host "Pipeline - captures: $($Report.Captures); unique content: $($Report.UniqueContentFrames); repeated captures: $($Report.RepeatedCaptureFrames); uploads: $($Report.Uploads); submit L/R: $($Report.SubmitLeft)/$($Report.SubmitRight)"
Write-Host "Continuity - hook losses: $($Report.HookLosses); conflicts: $($Report.HookConflicts); active stage: $($Report.LastActiveStage); last failed stage: $($Report.LastFailureStage)"
Write-Host "Clock evidence - submit after last capture event: $($Report.SubmitObservedAfterLastCapture); presenter advanced while capture count was fixed: $($Report.PresenterAdvancedWithoutCapture)"

$Failed = @($Checks.GetEnumerator() | Where-Object { -not $_.Value })
if ($Failed.Count -ne 0) {
    throw "The run is reproducible but the game-observation gate failed. Use the reported identity/stage/counter evidence; do not promote the integration."
}
if ($Report.Incomplete) {
    throw "The run evidence is incomplete because no unique run_end was recorded."
}

Write-Host "Structured D3D9/OpenVR game-observation evidence passed. A headset-visible result is not part of this gate."
