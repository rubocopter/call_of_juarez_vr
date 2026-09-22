param(
    [Parameter(Mandatory = $true)] [string]$SourceDirectory,
    [Parameter(Mandatory = $true)] [string]$BinaryDirectory
)

$ErrorActionPreference = "Stop"
$TestDirectory = Join-Path ([IO.Path]::GetFullPath($BinaryDirectory)) "movement-diagnostic-tools-test"
$OutputPath = Join-Path $TestDirectory "summary.json"
try {
    New-Item -ItemType Directory -Path $TestDirectory -Force | Out-Null
    $Lines = @(
        "camera_probe_event: event=movement_trace_phase result=start detail=phase=vr-full;timestamp_us=1000000;vr_gameplay_input_enabled=true;capture_readback_enabled=true;second_eye_render_enabled=true",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1010000;game_update=1;game_delta=0.010000;actor=(0,0,0);delta=(0,0,1);horizontal_speed=100;run=false;grounded=true",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1020000;game_update=2;game_delta=0.010000;actor=(0,0,1);delta=(0,0,1);horizontal_speed=100;run=false;grounded=true",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1030000;game_update=3;game_delta=0.010000;actor=(0,0,3);delta=(0,0,2);horizontal_speed=200;run=true;grounded=true",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1040000;game_update=4;game_delta=0.010000;actor=(0,0,5);delta=(0,0,2);horizontal_speed=200;run=true;grounded=true",
        "camera_probe_event: event=movement_render_sample result=ok detail=phase=vr-full;timestamp_us=1010000;left_rendered=true;right_rendered=true;submitted=true;frames_uploaded=10;new_submissions=20;repeat_submissions=2",
        "native_stereo_producer_timing: status=published deferred_readback_ms=3.000;cpu_copy_ms=7.000",
        "camera_probe_event: event=movement_render_sample result=ok detail=phase=vr-full;timestamp_us=1040000;left_rendered=true;right_rendered=true;submitted=true;frames_uploaded=12;new_submissions=22;repeat_submissions=3",
        "native_stereo_producer_timing: status=published deferred_readback_ms=5.000;cpu_copy_ms=9.000",
        "camera_probe_event: event=movement_jump_summary result=complete detail=phase=vr-full;timestamp_us=1040000;apex_height_cm=42;duration_s=0.700;time_to_apex_s=0.300;max_vertical_velocity=160",
        "camera_probe_event: event=movement_trace_phase result=end detail=phase=vr-full;timestamp_us=2000000;duration_s=1.000000;game_updates=4;reason=disabled"
    )
    [IO.File]::WriteAllLines(
        (Join-Path $TestDirectory "cojvr.log"),
        $Lines,
        [Text.UTF8Encoding]::new($false))
    & (Join-Path $SourceDirectory "tools/summarize_movement_diagnostic.ps1") `
        -GameDirectory $TestDirectory -OutputPath $OutputPath | Out-Null
    $Report = Get-Content -LiteralPath $OutputPath -Raw | ConvertFrom-Json
    $Phase = @($Report.phases)[0]
    if ($Report.manifestType -ne "cojvr-movement-diagnostic-summary" -or
        $Phase.phase -ne "vr-full" -or $Phase.gameUpdates -ne 4 -or
        [Math]::Abs([double]$Phase.updateHz - 100.0) -gt 0.001 -or
        [Math]::Abs([double]$Phase.walkCmPerSecond - 100.0) -gt 0.001 -or
        [Math]::Abs([double]$Phase.sprintCmPerSecond - 200.0) -gt 0.001 -or
        [Math]::Abs([double]$Phase.sprintWalkRatio - 2.0) -gt 0.001 -or
        $Phase.stereoPairs -ne 2 -or $Phase.presenterRepeatedSubmissions -ne 1 -or
        $Phase.jumps.Count -ne 1 -or $Phase.jumpApexCm.p50 -ne 42 -or
        $Phase.readbackMs.p50 -ne 3 -or $Phase.readbackMs.p95 -ne 3 -or
        $Phase.readbackMs.max -ne 5 -or $Phase.cpuCopyMs.max -ne 9) {
        throw "Movement diagnostic summary did not preserve the synthetic phase metrics."
    }

    @{ diagnosticMode = "d3d9_native_stereo" } | ConvertTo-Json | Set-Content `
        -LiteralPath (Join-Path $TestDirectory ".cojvr-d3d9-stage.json") -Encoding utf8
    & (Join-Path $SourceDirectory "tools/set_movement_diagnostic.ps1") `
        -GameDirectory $TestDirectory -Mode vr-input-off | Out-Null
    $Control = Get-Content -LiteralPath (Join-Path $TestDirectory "cojvr-camera-control.json") `
        -Raw | ConvertFrom-Json
    if (-not $Control.trackingEnabled -or -not $Control.movementTraceEnabled -or
        $Control.movementTracePhase -ne "vr-input-off" -or
        $Control.vrGameplayInputEnabled -or -not $Control.captureReadbackEnabled -or
        -not $Control.secondEyeRenderEnabled) {
        throw "VR input-off control did not preserve native-exclusive ownership."
    }
    & (Join-Path $SourceDirectory "tools/set_movement_diagnostic.ps1") `
        -GameDirectory $TestDirectory -Mode vr-readback-off | Out-Null
    $Control = Get-Content -LiteralPath (Join-Path $TestDirectory "cojvr-camera-control.json") `
        -Raw | ConvertFrom-Json
    if ($Control.captureReadbackEnabled -or -not $Control.secondEyeRenderEnabled -or
        -not $Control.vrGameplayInputEnabled) {
        throw "Readback-off control changed an unrelated diagnostic subsystem."
    }
    Write-Host "Movement diagnostic phase summary passed."
} finally {
    if (Test-Path -LiteralPath $TestDirectory -PathType Container) {
        Remove-Item -LiteralPath $TestDirectory -Recurse -Force
    }
}
