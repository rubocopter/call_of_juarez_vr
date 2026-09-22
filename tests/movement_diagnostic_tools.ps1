param(
    [Parameter(Mandatory = $true)] [string]$SourceDirectory,
    [Parameter(Mandatory = $true)] [string]$BinaryDirectory
)

$ErrorActionPreference = "Stop"
$TestDirectory = Join-Path ([IO.Path]::GetFullPath($BinaryDirectory)) "movement-diagnostic-tools-test"
$OutputPath = Join-Path $TestDirectory "summary.json"
try {
    New-Item -ItemType Directory -Path $TestDirectory -Force | Out-Null

    $CameraProbeSource = Get-Content -LiteralPath `
        (Join-Path $SourceDirectory "src/games/call_of_juarez/camera_probe.cpp") -Raw
    if ($CameraProbeSource.Contains("std::jthread") -or
        $CameraProbeSource.Contains("MovementTraceWorker") -or
        $CameraProbeSource.Contains("StartMovementTraceWorker") -or
        $CameraProbeSource.Contains("StopMovementTraceWorker")) {
        throw "Movement diagnostics reintroduced an independent JNI worker thread."
    }
    if (-not $CameraProbeSource.Contains("observer_boundary=render_camera_update") -or
        -not $CameraProbeSource.Contains("state.bridge.Refresh(&error)")) {
        throw "Movement diagnostics no longer revalidate the player on the camera-update observer boundary."
    }
    foreach ($RequiredFailureField in @(
        "phase=", "operation=", "exception=", "jvm_cached=",
        "jni_environment_observed=", "thread_attached_by_bridge=",
        "being_generation=", "last_completed_step=")) {
        if (-not $CameraProbeSource.Contains($RequiredFailureField)) {
            throw "Movement observer failure telemetry lost required field: $RequiredFailureField"
        }
    }
    if (-not $CameraProbeSource.Contains("confirmation=is_jumping_true") -or
        $CameraProbeSource.Contains("accepted_inferred_from_airborne_motion") -or
        $CameraProbeSource.Contains("now_us - state.jump_start_us > 50000ULL")) {
        throw "Movement jump diagnostics no longer require native IsJumping confirmation."
    }

    $JavaBridgeSource = Get-Content -LiteralPath `
        (Join-Path $SourceDirectory "src/games/call_of_juarez/java_player_bridge.cpp") -Raw
    $ResetStart = $JavaBridgeSource.IndexOf("void JavaPlayerBridge::Reset() noexcept")
    $ResetEnd = $JavaBridgeSource.IndexOf("void* JavaPlayerBridge::Environment", $ResetStart)
    if ($ResetStart -lt 0 -or $ResetEnd -le $ResetStart) {
        throw "JavaPlayerBridge::Reset could not be located for the JNI lifetime regression test."
    }
    $ResetBody = $JavaBridgeSource.Substring($ResetStart, $ResetEnd - $ResetStart)
    $DetachIndex = $ResetBody.IndexOf("DetachCurrentThread();")
    $ClearVmIndex = $ResetBody.IndexOf("vm_ = nullptr;")
    if ($DetachIndex -lt 0 -or $ClearVmIndex -lt 0 -or $DetachIndex -gt $ClearVmIndex) {
        throw "JavaPlayerBridge::Reset clears the VM before detaching a bridge-owned thread."
    }

    $Lines = @(
        "camera_probe_event: event=movement_trace_phase result=start detail=phase=vr-full;timestamp_us=1000000;vr_gameplay_input_enabled=true;capture_readback_enabled=true;second_eye_render_enabled=true",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1010000;game_update=1;game_delta=0.010000;actor=(0,0,0);delta=(0,0,1);horizontal_speed=100;native_vertical_speed=0;forward_speed=1;wanted_local_speed=(0,0,550);run=false;can_run=false;speed_state=129;grounded=true;jumping=false",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1020000;game_update=2;game_delta=0.010000;actor=(0,0,1);delta=(0,0,1);horizontal_speed=100;native_vertical_speed=0;forward_speed=1;wanted_local_speed=(0,0,550);run=false;can_run=false;speed_state=129;grounded=true;jumping=false",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1025000;game_update=3;game_delta=0.010000;actor=(0,0,1);delta=(0,0,0);horizontal_speed=0;native_vertical_speed=0;forward_speed=0;wanted_local_speed=(0,0,0);run=false;can_run=false;speed_state=0;grounded=true;jumping=false",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1030000;game_update=3;game_delta=0.010000;actor=(0,0,2);delta=(0,0,0.5);horizontal_speed=50;native_vertical_speed=0;forward_speed=1;wanted_local_speed=(0,0,275);run=false;can_run=false;speed_state=128;grounded=true;jumping=false",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1040000;game_update=4;game_delta=0.010000;actor=(0,0,2.5);delta=(0,0,0.5);horizontal_speed=50;native_vertical_speed=0;forward_speed=1;wanted_local_speed=(0,0,275);run=false;can_run=false;speed_state=128;grounded=true;jumping=false",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1050000;game_update=5;game_delta=0.010000;actor=(0,0,2.5);delta=(0,0,0);horizontal_speed=0;native_vertical_speed=0;forward_speed=0;wanted_local_speed=(0,0,0);run=false;can_run=false;speed_state=0;grounded=true;jumping=false",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1060000;game_update=6;game_delta=0.010000;actor=(0,0.5,2.5);delta=(0,0.5,0);horizontal_speed=0;native_vertical_speed=20;forward_speed=0;wanted_local_speed=(0,0,0);run=false;can_run=false;speed_state=0;grounded=false;jumping=false",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1070000;game_update=7;game_delta=0.010000;actor=(0,0.7,2.5);delta=(0,0.2,0);horizontal_speed=0;native_vertical_speed=-5;forward_speed=0;wanted_local_speed=(0,0,0);run=false;can_run=false;speed_state=0;grounded=true;jumping=false",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1080000;game_update=8;game_delta=0.010000;actor=(0,0,2.5);delta=(0,-0.7,0);horizontal_speed=0;native_vertical_speed=0;forward_speed=0;wanted_local_speed=(0,0,0);run=false;can_run=false;speed_state=0;grounded=true;jumping=false",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1090000;game_update=9;game_delta=0.010000;actor=(0,10,2.5);delta=(0,10,0);horizontal_speed=0;native_vertical_speed=400;forward_speed=0;wanted_local_speed=(0,0,0);run=false;can_run=false;speed_state=0;grounded=false;jumping=false",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1100000;game_update=10;game_delta=0.010000;actor=(0,42,2.5);delta=(0,32,0);horizontal_speed=0;native_vertical_speed=200;forward_speed=0;wanted_local_speed=(0,0,0);run=false;can_run=false;speed_state=0;grounded=false;jumping=true",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1110000;game_update=11;game_delta=0.010000;actor=(0,40,2.5);delta=(0,-2,0);horizontal_speed=0;native_vertical_speed=-20;forward_speed=0;wanted_local_speed=(0,0,0);run=false;can_run=false;speed_state=0;grounded=false;jumping=true",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1120000;game_update=12;game_delta=0.010000;actor=(0,0,2.5);delta=(0,-40,0);horizontal_speed=0;native_vertical_speed=10;forward_speed=0;wanted_local_speed=(0,0,0);run=false;can_run=false;speed_state=0;grounded=true;jumping=true",
        "camera_probe_event: event=movement_trace_sample result=ok detail=phase=vr-full;timestamp_us=1130000;game_update=13;game_delta=0.010000;actor=(0,0,2.5);delta=(0,0,0);horizontal_speed=0;native_vertical_speed=10;forward_speed=0;wanted_local_speed=(0,0,0);run=false;can_run=false;speed_state=0;grounded=true;jumping=false",
        "camera_probe_event: event=movement_render_sample result=ok detail=phase=vr-full;timestamp_us=1010000;left_rendered=true;right_rendered=true;submitted=true;frames_uploaded=10;new_submissions=20;repeat_submissions=2",
        "native_stereo_capture_timing: status=ok gpu_copy_queue_ms=0.200",
        "native_stereo_producer_timing: status=published ring_depth=2;producer_wait_ms=0.000;deferred_readback_ms=3.000;cpu_copy_ms=7.000",
        "native_stereo_presenter_frame: status=new consumer_gpu_copy_queue_ms=0.300;consumer_wait_ms=0.000;consumer_pending_fences=1;frame_age_ms=4.000",
        "camera_probe_event: event=movement_render_sample result=ok detail=phase=vr-full;timestamp_us=1040000;left_rendered=true;right_rendered=true;submitted=true;frames_uploaded=12;new_submissions=22;repeat_submissions=3",
        "native_stereo_capture_timing: status=ok gpu_copy_queue_ms=0.400",
        "native_stereo_producer_timing: status=published ring_depth=3;producer_wait_ms=0.000;deferred_readback_ms=5.000;cpu_copy_ms=9.000",
        "native_stereo_presenter_frame: status=new consumer_gpu_copy_queue_ms=0.500;consumer_wait_ms=0.000;consumer_pending_fences=2;frame_age_ms=6.000",
        "camera_probe_event: event=movement_jump_summary result=complete detail=phase=vr-full;timestamp_us=1130000;start_y=0;max_y=42;final_y=0;apex_height_cm=42;duration_s=0.040;time_to_apex_s=0.010;max_vertical_velocity=400;requested_jump_height=60;stair_height=35",
        "camera_probe_event: event=movement_trace_phase result=end detail=phase=vr-full;timestamp_us=2000000;duration_s=1.000000;game_updates=13;reason=disabled"
    )
    [IO.File]::WriteAllLines(
        (Join-Path $TestDirectory "cojvr.log"),
        $Lines,
        [Text.UTF8Encoding]::new($false))
    & (Join-Path $SourceDirectory "tools/summarize_movement_diagnostic.ps1") `
        -GameDirectory $TestDirectory -OutputPath $OutputPath | Out-Null
    $Report = Get-Content -LiteralPath $OutputPath -Raw | ConvertFrom-Json
    $Phase = @($Report.phases)[0]
    if ($Report.schemaVersion -ne 3 -or
        $Report.manifestType -ne "cojvr-movement-diagnostic-summary" -or
        $Phase.phase -ne "vr-full" -or $Phase.gameUpdates -ne 14 -or
        [Math]::Abs([double]$Phase.updateHz - 100.0) -gt 0.001 -or
        [Math]::Abs([double]$Phase.normalMovementCmPerSecond - 100.0) -gt 0.001 -or
        [Math]::Abs([double]$Phase.walkModifierCmPerSecond - 50.0) -gt 0.001 -or
        [Math]::Abs([double]$Phase.normalWalkRatio - 2.0) -gt 0.001 -or
        [Math]::Abs([double]$Phase.normalMovementTargetCmPerSecond - 550.0) -gt 0.001 -or
        [Math]::Abs([double]$Phase.walkModifierTargetCmPerSecond - 275.0) -gt 0.001 -or
        $null -ne $Phase.PSObject.Properties["sprintCmPerSecond"] -or
        $Phase.stereoPairs -ne 2 -or $Phase.presenterRepeatedSubmissions -ne 1 -or
        $Phase.jumps.Count -ne 1 -or $Phase.jumpApexCm.p50 -ne 42 -or
        $Phase.readbackMs.p50 -ne 3 -or $Phase.readbackMs.p95 -ne 3 -or
        $Phase.readbackMs.max -ne 5 -or $Phase.cpuCopyMs.max -ne 9 -or
        $Phase.producerGpuCopyQueueMs.max -ne 0.4 -or
        $Phase.producerWaitMs.max -ne 0 -or
        $Phase.consumerGpuCopyQueueMs.max -ne 0.5 -or
        $Phase.consumerWaitMs.max -ne 0 -or
        $Phase.endToEndFrameAgeMs.max -ne 6 -or
        $Phase.captureRingDepth.max -ne 3 -or
        $Phase.consumerPendingCopyFences.max -ne 2) {
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
