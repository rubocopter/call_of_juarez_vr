param(
    [Parameter(Mandatory = $true)]
    [string]$LogPath,
    [Parameter(Mandatory = $true)]
    [string]$RunId,
    [Parameter(Mandatory = $true)]
    [string]$BuildManifestId,
    [switch]$AllowIncomplete
)

$ErrorActionPreference = "Stop"
$LogPath = [System.IO.Path]::GetFullPath($LogPath)
if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
    throw "Telemetry log was not found at '$LogPath'."
}

$Events = [System.Collections.Generic.List[object]]::new()
$LineNumber = 0
foreach ($Line in Get-Content -LiteralPath $LogPath) {
    $LineNumber++
    if (-not $Line.StartsWith("COJVR_EVENT ", [StringComparison]::Ordinal)) { continue }
    try {
        $Event = $Line.Substring(12) | ConvertFrom-Json
    } catch {
        throw "Malformed structured telemetry at line $LineNumber in '$LogPath'."
    }
    if ([string]$Event.run_id -ne $RunId -or
        [string]$Event.build_manifest_id -ne $BuildManifestId) {
        throw "Structured telemetry at line $LineNumber belongs to a different run/build."
    }
    $Events.Add($Event)
}
if ($Events.Count -eq 0) {
    throw "No structured telemetry was found for run '$RunId'."
}

function Get-MaxValue([string]$Property) {
    $Values = @($Events | ForEach-Object {
        if ($_.PSObject.Properties.Name -contains $Property) { [int64]$_.$Property }
    })
    if ($Values.Count -eq 0) { return 0L }
    return [int64](($Values | Measure-Object -Maximum).Maximum)
}

function Get-MaxFrom([object[]]$Items, [string]$Property) {
    $Values = @($Items | ForEach-Object {
        if ($_.PSObject.Properties.Name -contains $Property) { [int64]$_.$Property }
    })
    if ($Values.Count -eq 0) { return 0L }
    return [int64](($Values | Measure-Object -Maximum).Maximum)
}

function Get-DetailValue([object]$Event, [string]$Key) {
    if ($null -eq $Event -or [string]::IsNullOrWhiteSpace($Key)) { return "" }
    $Detail = [string]$Event.detail
    foreach ($Part in $Detail -split ";") {
        $Pair = @($Part -split "=", 2)
        if ($Pair.Count -eq 2 -and
            [string]::Equals($Pair[0], $Key, [StringComparison]::Ordinal)) {
            return [string]$Pair[1]
        }
    }
    return ""
}

$EventNames = @($Events | ForEach-Object { [string]$_.event })
$RunEnds = @($Events | Where-Object { [string]$_.event -eq "run_end" })
$Incomplete = $RunEnds.Count -ne 1
if ($Incomplete -and -not $AllowIncomplete) {
    throw "Run '$RunId' is incomplete: expected exactly one run_end event, found $($RunEnds.Count)."
}

$RequiredCoreEvents = @(
    "run_start", "build/deployment_identified", "device_created", "hook_installed",
    "callback_enter", "callback_exit", "periodic_summary"
)
$MissingCoreEvents = @($RequiredCoreEvents | Where-Object { $_ -notin $EventNames })
$DeviceEvents = @($Events | Where-Object { [string]$_.event -eq "device_created" })
$GenerationEvents = @($Events | Where-Object { [string]$_.event -eq "generation_changed" })
$HookLossEvents = @($Events | Where-Object { [string]$_.event -eq "hook_integrity_lost" })
$HookConflictEvents = @($Events | Where-Object { [string]$_.event -eq "hook_conflict" })
$HookInstallEvents = @($Events | Where-Object { [string]$_.event -eq "hook_installed" })
$InstallFailures = @($Events | Where-Object { [string]$_.event -eq "hook_install_failed" })
$RuntimeFailures = @($Events | Where-Object {
    [string]$_.event -eq "runtime_state_changed" -and
    [string]$_.runtime_result -notin @("success", "initialized")
})
$Summaries = @($Events | Where-Object { [string]$_.event -eq "periodic_summary" })
$LatestSummary = if ($Summaries.Count -gt 0) { $Summaries[-1] } else { $Events[-1] }

$FactoryIds = @($DeviceEvents | ForEach-Object { [int64]$_.factory_id } | Sort-Object -Unique)
$DeviceIds = @($DeviceEvents | ForEach-Object { [int64]$_.device_id } | Sort-Object -Unique)
$SwapchainIds = @($Events | Where-Object { [int64]$_.swapchain_id -gt 0 } |
    ForEach-Object { [int64]$_.swapchain_id } | Sort-Object -Unique)
$Generations = @($Events | Where-Object { [int64]$_.generation -gt 0 } |
    ForEach-Object { "device=$([int64]$_.device_id):generation=$([int64]$_.generation)" } |
    Sort-Object -Unique)
$FactoryCreateDeviceHooks = @($HookInstallEvents | Where-Object {
    (Get-DetailValue $_ "interface") -eq "IDirect3D9" -and
    (Get-DetailValue $_ "slot") -eq "CreateDevice"
})
$FactoryCreateDeviceOriginalPaths = @($FactoryCreateDeviceHooks | ForEach-Object {
    Get-DetailValue $_ "original_path"
} | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
$SteamOverlayFactoryIntercepted = @($FactoryCreateDeviceOriginalPaths | Where-Object {
    [string]::Equals(
        [System.IO.Path]::GetFileName([string]$_),
        "gameoverlayrenderer.dll",
        [StringComparison]::OrdinalIgnoreCase)
}).Count -gt 0

$Callbacks = Get-MaxValue "callback_count"
$PresentCallbacks = Get-MaxValue "present_count"
$BeginSceneCallbacks = Get-MaxValue "begin_scene_count"
$EndSceneCallbacks = Get-MaxValue "end_scene_count"
$SwapchainPresentCallbacks = Get-MaxValue "swapchain_present_count"
$Captures = Get-MaxValue "capture_count"
$Content = Get-MaxValue "content_count"
$Uploads = Get-MaxValue "upload_count"
$SubmitLeft = Get-MaxValue "submit_left_count"
$SubmitRight = Get-MaxValue "submit_right_count"
$CaptureFailures = Get-MaxValue "capture_failure_count"
$UploadFailures = Get-MaxValue "upload_failure_count"
$WaitPosesFailures = Get-MaxValue "wait_poses_failure_count"
$SubmitLeftFailures = Get-MaxValue "submit_left_failure_count"
$SubmitRightFailures = Get-MaxValue "submit_right_failure_count"

$LastCaptureEnd = @($Events | Where-Object { [string]$_.event -eq "capture_end" } |
    Sort-Object { [int64]$_.monotonic_us } | Select-Object -Last 1)
$SubmitAfterCaptureStopped = $false
if ($LastCaptureEnd.Count -eq 1) {
    $LastCaptureTime = [int64]$LastCaptureEnd[0].monotonic_us
    $SubmitAfterCaptureStopped = @($Events | Where-Object {
        ([string]$_.event -eq "submit_left" -or [string]$_.event -eq "submit_right") -and
        [int64]$_.monotonic_us -gt $LastCaptureTime
    }).Count -gt 0
}

$PresenterAdvancedWithoutCapture = $false
for ($Index = 1; $Index -lt $Summaries.Count; $Index++) {
    $Previous = $Summaries[$Index - 1]
    $Current = $Summaries[$Index]
    $PreviousSubmits = [int64]$Previous.submit_left_count + [int64]$Previous.submit_right_count
    $CurrentSubmits = [int64]$Current.submit_left_count + [int64]$Current.submit_right_count
    if ([int64]$Previous.capture_count -eq [int64]$Current.capture_count -and
        $CurrentSubmits -gt $PreviousSubmits) {
        $PresenterAdvancedWithoutCapture = $true
        break
    }
}

$LatestStage = [string]$LatestSummary.stage
if ([string]::IsNullOrWhiteSpace($LatestStage)) { $LatestStage = "none" }
$IdentityMismatches = @($DeviceEvents | Where-Object {
    [string]$_.runtime_result -ne "native_identity_match"
})
$HooksOwned = $HookLossEvents.Count -eq 0 -and
    $HookConflictEvents.Count -eq 0 -and
    $InstallFailures.Count -eq 0 -and
    ([string]$LatestSummary.detail -match "(?:^|;)hooks_owned=true(?:;|$)")

$DeviceProgress = @($DeviceIds | ForEach-Object {
    $DeviceId = [int64]$_
    $ForDevice = @($Events | Where-Object { [int64]$_.device_id -eq $DeviceId })
    $DeviceSummaries = @($ForDevice | Where-Object { [string]$_.event -eq "periodic_summary" })
    $DeviceLatest = if ($DeviceSummaries.Count -gt 0) { $DeviceSummaries[-1] } else { $ForDevice[-1] }
    [pscustomobject]@{
        DeviceId = $DeviceId
        FactoryId = [int64]$ForDevice[0].factory_id
        SwapchainIds = @($ForDevice | Where-Object { [int64]$_.swapchain_id -gt 0 } |
            ForEach-Object { [int64]$_.swapchain_id } | Sort-Object -Unique)
        Generations = @($ForDevice | Where-Object { [int64]$_.generation -gt 0 } |
            ForEach-Object { [int64]$_.generation } | Sort-Object -Unique)
        Callbacks = Get-MaxFrom $ForDevice "device_callback_count"
        Present = Get-MaxFrom $ForDevice "device_present_count"
        BeginScene = Get-MaxFrom $ForDevice "device_begin_scene_count"
        EndScene = Get-MaxFrom $ForDevice "device_end_scene_count"
        SwapchainPresent = Get-MaxFrom $ForDevice "device_swapchain_present_count"
        Captures = Get-MaxFrom $ForDevice "device_capture_count"
        Content = Get-MaxFrom $ForDevice "device_content_count"
        Uploads = Get-MaxFrom $ForDevice "device_upload_count"
        SubmitLeft = Get-MaxFrom $ForDevice "device_submit_left_count"
        SubmitRight = Get-MaxFrom $ForDevice "device_submit_right_count"
        ActiveStage = [string]$DeviceLatest.stage
    }
})

[pscustomobject]@{
    RunId = $RunId
    BuildManifestId = $BuildManifestId
    Incomplete = $Incomplete
    MissingCoreEvents = $MissingCoreEvents
    FactoryIds = $FactoryIds
    DeviceIds = $DeviceIds
    DeviceProgress = $DeviceProgress
    SwapchainIds = $SwapchainIds
    Generations = $Generations
    DeviceCreations = $DeviceEvents.Count
    GenerationChanges = $GenerationEvents.Count
    NativeIdentityMatches = $IdentityMismatches.Count -eq 0 -and $DeviceEvents.Count -gt 0
    FactoryCreateDeviceOriginalPaths = $FactoryCreateDeviceOriginalPaths
    SteamOverlayFactoryIntercepted = $SteamOverlayFactoryIntercepted
    HooksOwned = $HooksOwned
    HookLosses = $HookLossEvents.Count
    HookConflicts = $HookConflictEvents.Count
    HookInstallFailures = $InstallFailures.Count
    RuntimeFailures = $RuntimeFailures.Count
    Callbacks = $Callbacks
    PresentCallbacks = $PresentCallbacks
    BeginSceneCallbacks = $BeginSceneCallbacks
    EndSceneCallbacks = $EndSceneCallbacks
    SwapchainPresentCallbacks = $SwapchainPresentCallbacks
    Captures = $Captures
    UniqueContentFrames = $Content
    RepeatedCaptureFrames = [Math]::Max(0L, $Captures - $Content)
    Uploads = $Uploads
    SubmitLeft = $SubmitLeft
    SubmitRight = $SubmitRight
    CaptureFailures = $CaptureFailures
    UploadFailures = $UploadFailures
    WaitPosesFailures = $WaitPosesFailures
    SubmitLeftFailures = $SubmitLeftFailures
    SubmitRightFailures = $SubmitRightFailures
    LastFailureStage = [string]$LatestSummary.last_failure_stage
    LastActiveStage = $LatestStage
    SubmitObservedAfterLastCapture = $SubmitAfterCaptureStopped
    PresenterAdvancedWithoutCapture = $PresenterAdvancedWithoutCapture
    EventCount = $Events.Count
    Events = $Events
}
