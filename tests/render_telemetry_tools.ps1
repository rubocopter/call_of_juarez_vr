param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDirectory,
    [Parameter(Mandatory = $true)]
    [string]$BinaryDirectory
)

$ErrorActionPreference = "Stop"
$SourceDirectory = [System.IO.Path]::GetFullPath($SourceDirectory)
$BinaryDirectory = [System.IO.Path]::GetFullPath($BinaryDirectory)
$TestDirectory = Join-Path $BinaryDirectory "render-telemetry-tools-test"
$ResolvedPrefix = $BinaryDirectory.TrimEnd("\") + "\"
$ResolvedTest = [System.IO.Path]::GetFullPath($TestDirectory)
if (-not $ResolvedTest.StartsWith($ResolvedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to create telemetry test data outside the binary directory."
}

$RunId = "telemetry-test-run"
$ManifestId = "A" * 64
$Parser = Join-Path $SourceDirectory "tools/read_render_telemetry.ps1"

function New-Event(
    [string]$Name,
    [int64]$Timestamp,
    [int64]$Capture = 0,
    [int64]$Content = 0,
    [int64]$Upload = 0,
    [int64]$Left = 0,
    [int64]$Right = 0,
    [string]$Detail = "hooks_owned=true",
    [string]$RuntimeResult = "success",
    [string]$Stage = "none"
) {
    [ordered]@{
        schema_version = 1
        event = $Name
        run_id = $RunId
        build_manifest_id = $ManifestId
        pid = 10
        tid = 20
        monotonic_us = $Timestamp
        factory_id = 1
        device_id = 2
        swapchain_id = 3
        generation = 1
        callback_sequence = 4
        capture_sequence = $Capture
        content_sequence = $Content
        upload_sequence = $Upload
        submit_sequence = $Left + $Right
        duration_us = 10
        hresult = "0x00000000"
        runtime_result = $RuntimeResult
        callback_count = 4
        present_count = 1
        begin_scene_count = 1
        end_scene_count = 1
        swapchain_present_count = 1
        capture_count = $Capture
        content_count = $Content
        upload_count = $Upload
        submit_left_count = $Left
        submit_right_count = $Right
        capture_failure_count = 0
        upload_failure_count = 0
        wait_poses_failure_count = 0
        submit_left_failure_count = 0
        submit_right_failure_count = 0
        last_failure_stage = "none"
        device_callback_count = 4
        device_present_count = 1
        device_begin_scene_count = 1
        device_end_scene_count = 1
        device_swapchain_present_count = 1
        device_capture_count = $Capture
        device_content_count = $Content
        device_upload_count = $Upload
        device_submit_left_count = $Left
        device_submit_right_count = $Right
        stage = $Stage
        detail = $Detail
    }
}

try {
    New-Item -ItemType Directory -Path $TestDirectory -Force | Out-Null
    $LogPath = Join-Path $TestDirectory "complete.log"
    $Events = @(
        (New-Event "run_start" 1),
        (New-Event "build/deployment_identified" 2),
        (New-Event "device_created" 3 -RuntimeResult "native_identity_match"),
        (New-Event "hook_installed" 4 -Detail "interface=IDirect3D9;slot=CreateDevice;original_path=C:\Windows\System32\d3d9.dll;hooks_owned=true"),
        (New-Event "callback_enter" 5),
        (New-Event "callback_exit" 6),
        (New-Event "capture_begin" 7 -Capture 1),
        (New-Event "capture_end" 8 -Capture 1),
        (New-Event "frame_published" 9 -Capture 1 -Content 1),
        (New-Event "upload_begin" 10 -Capture 1 -Content 1 -Upload 1),
        (New-Event "upload_end" 11 -Capture 1 -Content 1 -Upload 1),
        (New-Event "wait_poses_begin" 12 -Capture 1 -Content 1 -Upload 1),
        (New-Event "wait_poses_end" 13 -Capture 1 -Content 1 -Upload 1),
        (New-Event "submit_left" 14 -Capture 1 -Content 1 -Upload 1 -Left 1),
        (New-Event "submit_right" 15 -Capture 1 -Content 1 -Upload 1 -Left 1 -Right 1),
        (New-Event "runtime_state_changed" 16 -Capture 1 -Content 1 -Upload 1 -Left 1 -Right 1),
        (New-Event "periodic_summary" 17 -Capture 1 -Content 1 -Upload 1 -Left 1 -Right 1),
        (New-Event "periodic_summary" 18 -Capture 1 -Content 1 -Upload 1 -Left 2 -Right 2),
        (New-Event "run_end" 19 -Capture 1 -Content 1 -Upload 1 -Left 2 -Right 2)
    )
    $Lines = @($Events | ForEach-Object { "COJVR_EVENT " + ($_ | ConvertTo-Json -Compress) })
    [System.IO.File]::WriteAllLines($LogPath, $Lines, [System.Text.UTF8Encoding]::new($false))

    $Report = & $Parser -LogPath $LogPath -RunId $RunId -BuildManifestId $ManifestId
    if ($Report.Incomplete -or -not $Report.NativeIdentityMatches -or
        -not $Report.HooksOwned -or $Report.Captures -ne 1 -or
        $Report.UniqueContentFrames -ne 1 -or $Report.Uploads -ne 1 -or
        $Report.SubmitLeft -ne 2 -or $Report.SubmitRight -ne 2 -or
        -not $Report.PresenterAdvancedWithoutCapture -or
        $Report.FactoryCreateDeviceOriginalPaths.Count -ne 1 -or
        $Report.FactoryCreateDeviceOriginalPaths[0] -ne "C:\Windows\System32\d3d9.dll" -or
        $Report.SteamOverlayFactoryIntercepted -or
        $Report.MissingCoreEvents.Count -ne 0) {
        throw "Complete telemetry report did not preserve exact synthetic evidence."
    }

    $OverlayPath = Join-Path $TestDirectory "overlay.log"
    $OverlayEvents = @($Events)
    $OverlayEvents[3] = New-Event "hook_installed" 4 `
        -Detail "interface=IDirect3D9;slot=CreateDevice;original_path=C:\Program Files (x86)\Steam\gameoverlayrenderer.dll;hooks_owned=true"
    $OverlayLines = @($OverlayEvents | ForEach-Object {
        "COJVR_EVENT " + ($_ | ConvertTo-Json -Compress)
    })
    [System.IO.File]::WriteAllLines(
        $OverlayPath, $OverlayLines, [System.Text.UTF8Encoding]::new($false))
    $OverlayReport = & $Parser -LogPath $OverlayPath -RunId $RunId -BuildManifestId $ManifestId
    if (-not $OverlayReport.SteamOverlayFactoryIntercepted -or
        $OverlayReport.FactoryCreateDeviceOriginalPaths.Count -ne 1 -or
        $OverlayReport.FactoryCreateDeviceOriginalPaths[0] -notlike "*gameoverlayrenderer.dll") {
        throw "Parser did not identify Steam Overlay as the original factory CreateDevice target."
    }

    $IncompletePath = Join-Path $TestDirectory "incomplete.log"
    [System.IO.File]::WriteAllLines(
        $IncompletePath, $Lines[0..($Lines.Count - 2)], [System.Text.UTF8Encoding]::new($false))
    $RejectedIncomplete = $false
    try {
        & $Parser -LogPath $IncompletePath -RunId $RunId -BuildManifestId $ManifestId | Out-Null
    } catch {
        $RejectedIncomplete = $true
    }
    if (-not $RejectedIncomplete) { throw "Parser accepted telemetry without run_end." }
    $IncompleteReport = & $Parser -LogPath $IncompletePath -RunId $RunId `
        -BuildManifestId $ManifestId -AllowIncomplete
    if (-not $IncompleteReport.Incomplete) {
        throw "AllowIncomplete did not retain the incomplete-run state."
    }

    $ContaminatedPath = Join-Path $TestDirectory "contaminated.log"
    $Contaminated = @($Lines) + @($Lines[0].Replace($RunId, "another-run"))
    [System.IO.File]::WriteAllLines(
        $ContaminatedPath, $Contaminated, [System.Text.UTF8Encoding]::new($false))
    $RejectedContamination = $false
    try {
        & $Parser -LogPath $ContaminatedPath -RunId $RunId `
            -BuildManifestId $ManifestId | Out-Null
    } catch {
        $RejectedContamination = $true
    }
    if (-not $RejectedContamination) {
        throw "Parser accepted structured events from another run."
    }

    Write-Host "Structured render telemetry parser success/failure paths passed."
} finally {
    if (Test-Path -LiteralPath $ResolvedTest -PathType Container) {
        Remove-Item -LiteralPath $ResolvedTest -Recurse -Force
    }
}
