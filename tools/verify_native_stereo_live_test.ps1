param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory
)

$ErrorActionPreference = "Stop"
$ExpectedCoJHash = "5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE"
$ExpectedChromeEngineHash = "DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8"

$Provenance = & (Join-Path $PSScriptRoot "get_run_provenance.ps1") `
    -GameDirectory $GameDirectory `
    -ExpectedDiagnosticMode "d3d9_native_stereo"
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
$OpenVrDeployment = @($Run.deployment | Where-Object { [string]$_.role -eq "openvr_runtime" })
if ($OpenVrDeployment.Count -ne 1 -or [string]$OpenVrDeployment[0].destination -ne "openvr_api.dll") {
    throw "The run manifest does not bind exactly one staged OpenVR runtime."
}
$ActionManifestDeployment = @($Run.deployment | Where-Object { [string]$_.role -eq "openvr_action_manifest" })
$SenseBindingDeployment = @($Run.deployment | Where-Object { [string]$_.role -eq "openvr_binding_psvr2_sense" })
if ($ActionManifestDeployment.Count -ne 1 -or
    [string]$ActionManifestDeployment[0].destination -ne "cojvr_openvr_input/actions.json" -or
    $SenseBindingDeployment.Count -ne 1 -or
    [string]$SenseBindingDeployment[0].destination -ne "cojvr_openvr_input/bindings/psvr2_sense.json") {
    throw "The run manifest does not bind the expected OpenVR action manifest and PS VR2 Sense profile."
}

function Assert-LogMatch([string]$Pattern, [string]$Failure) {
    if (-not [bool]($Lines -match $Pattern)) { throw $Failure }
}

function Assert-LogNotMatch([string]$Pattern, [string]$Failure) {
    if ([bool]($Lines -match $Pattern)) { throw $Failure }
}

Assert-LogMatch `
    "native_stereo_runtime: status=started backend=openvr owner=presenter_thread .*pose_semantics=eye_to_head" `
    "The native-stereo OpenVR runtime/eye configuration did not initialize."
Assert-LogMatch `
    "native_stereo_presenter: status=started owner_thread=openvr\+d3d11 mode=(latest_frame_repeat|flat_theater_to_native_stereo)" `
    "The dedicated OpenVR/D3D11 presenter thread did not initialize."
Assert-LogMatch `
    "native_stereo_d3d9_factory: status=d3d9ex_primary fallback=classic_create_device transport=d3d9ex_shared_texture_ring" `
    "The candidate did not select the D3D9Ex factory required for GPU-resident transport."
Assert-LogMatch `
    "native_stereo_device: status=observed .*device_api=d3d9ex" `
    "The game device fell back to classic D3D9 instead of D3D9Ex."

$RequireOpenVrRuntimeState = [bool]$Run.validation.requireOpenVrRuntimeState
$RequireOpenVrFocusCycle = [bool]$Run.validation.requireOpenVrFocusCycle
$RequireOpenVrDashboardCycle = if ($null -ne $Run.validation.requireOpenVrDashboardCycle) {
    [bool]$Run.validation.requireOpenVrDashboardCycle
} else {
    # Backward compatibility for manifests created before dashboard visibility
    # was separated from scene-focus ownership.
    $RequireOpenVrFocusCycle
}
$RequireProductionGpuSyncNone = [bool]$Run.validation.requireProductionGpuSyncNone
$RequireRepeatedPresentation = [bool]$Run.validation.requireRepeatedPresentation
$RequireFlatTheaterUi = if ($null -ne $Run.validation.requireFlatTheaterUi) {
    [bool]$Run.validation.requireFlatTheaterUi
} else {
    $false
}
$RequireExplicitPassthroughDisable = if ($null -ne $Run.validation.requireExplicitPassthroughDisable) {
    [bool]$Run.validation.requireExplicitPassthroughDisable
} else {
    $true
}
$RequireSubtitleState = if ($null -ne $Run.validation.requireSubtitleState) {
    [bool]$Run.validation.requireSubtitleState
} else {
    $true
}
$RequireRecenter = if ($null -ne $Run.validation.requireRecenter) {
    [bool]$Run.validation.requireRecenter
} else {
    $true
}
$RequireStereoGeometry = if ($null -ne $Run.validation.requireStereoGeometry) {
    [bool]$Run.validation.requireStereoGeometry
} else {
    $true
}
$RequirePositional6Dof = [bool]$Run.validation.requirePositional6Dof
$RequireBodyIk = [bool]$Run.validation.requireBodyIk
$RequireGameplayInput = [bool]$Run.validation.requireGameplayInput
if ($RequireProductionGpuSyncNone) {
    Assert-LogMatch `
        "openvr_gpu_handoff: native_stereo=D3D9Ex_shared_texture\+CopyResource;flat_fallback=UpdateSubresource;producer_sync=nonblocking_D3D9_event_query;consumer_sync=nonblocking_D3D11_event_query;submit=Submit_TextureWithPose;handoff=PostPresentHandoff" `
        "The production presenter did not report the expected nonblocking GPU-resident handoff."
}
if ($RequireOpenVrRuntimeState) {
    Assert-LogMatch `
        "openvr_runtime_state: phase=initialized;lifecycle=ready;initialized=true;connected=true;focused=(true|false);tracking_valid=false;presenting=false;shutdown_requested=false" `
        "The OpenVR runtime did not report a connected initialized state."
    Assert-LogMatch `
        "openvr_runtime_state: phase=transition;lifecycle=ready;initialized=true;connected=true;focused=(true|false);tracking_valid=true;presenting=(true|false);shutdown_requested=false" `
        "The OpenVR runtime never reported valid tracking."
    Assert-LogMatch `
        "openvr_runtime_state: phase=transition;lifecycle=ready;initialized=true;connected=true;focused=true;tracking_valid=true;presenting=true;shutdown_requested=false" `
        "The OpenVR runtime never reported a focused, tracking-valid presenting state."
    Assert-LogMatch `
        "openvr_scene_state: phase=first_submit;process_id=([0-9]+);scene_focus_process_id=\1;can_render_scene=true" `
        "The first successful stereo submission did not prove scene focus belonged to the game process."
}
Assert-LogMatch `
    "openvr_input: status=started action_sets=/actions/global,/actions/gameplay recenter=/actions/global/in/recenter hand_pose=/user/hand/\{left,right\}/pose/handgrip aim_pose=/user/hand/\{left,right\}/pose/tip gameplay=semantic_sense_profile owner=presenter_thread" `
    "The native-stereo OpenVR global/gameplay input action sets did not initialize."
if ($RequireSubtitleState) {
    Assert-LogMatch `
        "camera_probe_event: event=subtitle_state result=observed detail=.*;subtitles_enabled=(true|false);dialog_playing=(true|false);current_line_visible=(true|false);dialog_subtitle_visible=(true|false);diagnostic=(disabled|idle|java_hidden|java_visible)" `
        "The run did not capture the native Java subtitle setting/dialog visibility state."
}
if ($RequireFlatTheaterUi) {
    Assert-LogMatch `
        "native_stereo_presenter_transition: status=content_mode mode=flat_theater" `
        "The run never presented intro/menu content through flat_theater."
    Assert-LogMatch `
        "flat_ui_pointer: status=hit;hand=(left|right);pose=tip_with_grip_fallback;.*;beam_origin=[0-9]+,[0-9]+;.*;visual=cyan_beam_reticle;route=flat_theater_menu_pointer" `
        "No valid Sense ray produced a flat-theater beam from the projected controller origin."
    Assert-LogMatch `
        "camera_probe_event: event=flat_ui_pointer result=active detail=active=true;hand=(left|right);source=[0-9]+x[0-9]+;route=win32_cursor_position\+SendInput_absolute\+WM_MOUSEMOVE" `
        "The flat-theater pointer did not reach the real Windows mouse-input path."
    Assert-LogNotMatch `
        "camera_probe_event: event=flat_ui_pointer_game_route result=applied" `
        "The flat-menu pointer had simultaneous Win32 and logical-Java cursor owners."
    Assert-LogMatch `
        "camera_probe_event: event=flat_ui_select result=applied detail=source=(left_l2|right_r2);pointer_active=true;.*;route=(GameWithMenu.sm_cMenuModule.GetCurrentUI.Enter|LawmanGame.sm_cActiveGameModule.cMenu.GetCurrentUI.Enter|GameWithMenu.sm_cIntroModule.OnInputKey)" `
        "No Sense trigger press reached an exact CoJ UI/select route."
    Assert-LogMatch `
        "camera_probe_event: event=flat_ui_select result=applied detail=source=right_cross;pointer_active=true;.*;route=(GameWithMenu.sm_cMenuModule.GetCurrentUI.Enter|LawmanGame.sm_cActiveGameModule.cMenu.GetCurrentUI.Enter|GameWithMenu.sm_cIntroModule.OnInputKey)" `
        "Cross did not reach an exact CoJ accept/select route."
    Assert-LogMatch `
        "camera_probe_event: event=flat_ui_back result=applied detail=source=right_circle;route=(GameWithMenu\.sm_cMenuModule\.GetCurrentUI|LawmanGame\.sm_cActiveGameModule\.cMenu\.GetCurrentUI)\.CallOnInputKeyGlobal\(Escape\)" `
        "Circle did not reach the active UI's normal Escape press/release route for menu back."
    Assert-LogMatch `
        "camera_probe_event: event=flat_ui_select result=applied detail=.*;route=GameWithMenu.sm_cIntroModule.OnInputKey" `
        "No Sense trigger press skipped a startup video through IntroModule.OnInputKey."
    Assert-LogMatch `
        "camera_probe_event: event=flat_ui_select result=applied detail=.*;route=LawmanGame.sm_cActiveGameModule.cMenu.GetCurrentUI.Enter" `
        "No Sense trigger press selected the in-game Escape menu through LawmanModule.cMenu."
    Assert-LogMatch `
        "camera_probe_event: event=blocking_ui_select result=applied detail=.*game_timer_valid=true;game_timer_frozen=true;current_ui_is_loading=true;gameplay_suppressed=true;body_mutation_suppressed=true;fire_suppressed_until_release=true;route=GameUILoading.OnInputKey" `
        "No Sense select press reached GameUILoading.OnInputKey while the loading timer was frozen."
    Assert-LogMatch `
        "camera_probe_event: event=blocking_ui_resume result=observed detail=.*game_timer_valid=true;game_timer_frozen=false;trigger_released=true;route=LawmanModule.TimerStart" `
        "The run did not prove that the blocking UI resumed only after trigger release."
    Assert-LogMatch `
        "native_stereo_presenter_transition: status=content_mode mode=native_stereo" `
        "The run did not transition from flat-theater UI to native stereo gameplay."
}
Assert-LogMatch `
    "native_stereo_factory_hook: status=installed" `
    "The candidate did not install its CreateDevice-only D3D9 observation hook."
Assert-LogMatch `
    "native_stereo_device: status=observed" `
    "No native D3D9 device was observed through CreateDevice."
Assert-LogMatch `
    "camera_probe_bootstrap: status=(installed|already_installed) system_d3d9=expected" `
    "The native-stereo candidate did not install over normal system D3D9 forwarding."
Assert-LogMatch `
    "camera_probe_event: event=camera_probe_install result=installed .*native_stereo=available" `
    "The exact ChromeEngine render-view profile was not enabled for native stereo."
Assert-LogMatch `
    "camera_probe_event: event=camera_probe_control_loaded result=accepted .*tracking_enabled=true" `
    "No command enabled HMD tracking for this run."
if ($RequireRecenter) {
    Assert-LogMatch `
        "camera_probe_event: event=camera_hmd_recentered result=ok .*stereo=true" `
        "No stereo HMD base orientation/recenter was observed."
    Assert-LogMatch `
        "openvr_input_event: action=recenter result=pressed source=global_action" `
        "No PS VR2 Sense/global-action recenter press was observed."
    Assert-LogMatch `
        "camera_probe_event: event=camera_hmd_recenter_requested result=ok .*source=openvr_global_action" `
        "The controller recenter press did not reach the XR-neutral camera recenter boundary."
}
if ($RequireBodyIk) {
    Assert-LogMatch `
        "openvr_controller_pose: source=handgrip;left_active=true;right_active=true;raw_role_fallback=false" `
        "Body IK never received both PS VR2 Sense handgrip action poses."
    Assert-LogMatch `
        "camera_probe_event: event=body_tracking_input result=observed detail=.*controller_pose_source=handgrip;raw_role_fallback=false;.*left_position_valid=true;left_orientation_valid=true;.*right_position_valid=true;right_orientation_valid=true;" `
        "Body IK tracking telemetry did not prove valid left/right handgrip poses without raw-role fallback."
    Assert-LogMatch `
        "camera_probe_event: event=body_arm_recovery result=ok .*transaction_active=false.*calibration_preserved=true" `
        "Create recenter did not safely preserve the arm target across the recovery boundary."
}
if ($RequireStereoGeometry) {
    Assert-LogMatch `
        "native_stereo_capture: status=source eye=left .*viewport=[0-9]+,[0-9]+,[0-9]+,[0-9]+,[-+0-9.eE]+,[-+0-9.eE]+" `
        "The left eye did not report a valid D3D9 viewport at its capture boundary."
    Assert-LogMatch `
        "native_stereo_capture: status=source eye=right .*viewport=[0-9]+,[0-9]+,[0-9]+,[0-9]+,[-+0-9.eE]+,[-+0-9.eE]+" `
        "The right eye did not report a valid D3D9 viewport at its capture boundary."
}
Assert-LogMatch `
    "native_stereo_capture_timing: status=ok .*eye=left .*transport=d3d9ex_shared_texture_ring.*capture_pool=default.*gpu_copy_queue_ms=[0-9.]+" `
    "The left eye did not use the D3D9Ex shared DEFAULT-pool transport."
Assert-LogMatch `
    "native_stereo_capture_timing: status=ok .*eye=right .*transport=d3d9ex_shared_texture_ring.*capture_pool=default.*gpu_copy_queue_ms=[0-9.]+" `
    "The right eye did not use the D3D9Ex shared DEFAULT-pool transport."
Assert-LogMatch `
    "native_stereo_producer_timing: status=published .*transport=d3d9ex_shared_texture_ring.*producer_wait_ms=0.000;deferred_readback_ms=0.000;cpu_copy_ms=0.000;producer_collect_ms=[0-9.]+" `
    "The producer did not prove zero-wait, zero-readback GPU-resident publication."
Assert-LogNotMatch `
    "native_stereo_producer_timing: status=published .*transport=classic_d3d9_locked_systemmem_fallback" `
    "The native-stereo producer fell back to the classic D3D9 CPU path."
Assert-LogMatch `
    "native_stereo_presenter_timing: status=ok .*render_pose_sequence=[1-9][0-9]*;pose_mode=explicit_render_pose;content=new.*left_result=0;right_result=0.*wait_pose_ms=[0-9.]+;submit_ms=[0-9.]+" `
    "The presenter did not report a successful new-frame OpenVR submission bound to its exact render pose."

if ($RequireStereoGeometry) {
    $StereoLines = @($Lines | Where-Object {
        $_ -match "camera_probe_event: event=camera_native_stereo_frame result=ok"
    })
    if ($StereoLines.Count -lt 2) {
        throw "Too few successfully submitted native-stereo frames were recorded."
    }
    foreach ($Line in $StereoLines) {
    if ($Line -notmatch "left_camera_applied=true" -or
        $Line -notmatch "left_projection_applied=true" -or
        $Line -notmatch "left_captured=true" -or
        $Line -notmatch "left_state_restored=true" -or
        $Line -notmatch "right_rendered=true" -or
        $Line -notmatch "right_full_view_pass=true" -or
        $Line -notmatch "right_view_guard_restored=true" -or
        $Line -notmatch "right_captured=true" -or
        $Line -notmatch "right_state_restored=true" -or
        $Line -notmatch "submitted=true" -or
        $Line -notmatch "transport_accepted=true" -or
        $Line -notmatch "content_hash_deferred=true" -or
        $Line -notmatch "left_renderer_camera_match=true" -or
        $Line -notmatch "right_renderer_camera_match=true" -or
        ($RequirePositional6Dof -and $Line -notmatch "head_position_valid=true") -or
        ($RequirePositional6Dof -and $Line -notmatch "positional_6dof=true") -or
        $Line -notmatch "render_view_rva=0x30fb0" -or
        $Line -notmatch "render_core_rva=0x30e00") {
        throw "A submitted stereo frame did not prove two complete ChromeEngine render-view passes/captures and transactional camera/view-guard restoration."
    }

    $LeftEyeMatch = [regex]::Match($Line, "left_eye_x=([-+0-9.eE]+)")
    $RightEyeMatch = [regex]::Match($Line, "right_eye_x=([-+0-9.eE]+)")
    if (-not $LeftEyeMatch.Success -or -not $RightEyeMatch.Success) {
        throw "A stereo frame did not report both eye-to-head translations."
    }
    $LeftEyeX = [double]::Parse(
        $LeftEyeMatch.Groups[1].Value,
        [System.Globalization.CultureInfo]::InvariantCulture)
    $RightEyeX = [double]::Parse(
        $RightEyeMatch.Groups[1].Value,
        [System.Globalization.CultureInfo]::InvariantCulture)
    if (-not [double]::IsFinite($LeftEyeX) -or -not [double]::IsFinite($RightEyeX) -or
        ($RightEyeX - $LeftEyeX) -lt 0.02) {
        throw "OpenVR eye separation was missing, reversed or implausibly small."
    }

    $ScaleMatch = [regex]::Match($Line, "game_units_per_meter=([-+0-9.eE]+)")
    $LeftEyePositionMatch = [regex]::Match(
        $Line,
        "left_eye_position=\(([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+)\)")
    $RightEyePositionMatch = [regex]::Match(
        $Line,
        "right_eye_position=\(([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+)\)")
    $LeftAppliedMatch = [regex]::Match(
        $Line,
        "left_applied_position=\(([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+)\)")
    $RightAppliedMatch = [regex]::Match(
        $Line,
        "right_applied_position=\(([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+)\)")
    if (-not $ScaleMatch.Success -or -not $LeftEyePositionMatch.Success -or
        -not $RightEyePositionMatch.Success -or -not $LeftAppliedMatch.Success -or
        -not $RightAppliedMatch.Success) {
        throw "A stereo frame did not report the runtime and applied eye positions needed to verify world scale."
    }

    $Scale = [double]::Parse(
        $ScaleMatch.Groups[1].Value,
        [System.Globalization.CultureInfo]::InvariantCulture)
    if (-not [double]::IsFinite($Scale) -or [Math]::Abs($Scale - 100.0) -gt 0.001) {
        throw "The Call of Juarez adapter did not use the proven 100 game-units-per-metre scale."
    }

    function Parse-VectorMatch([System.Text.RegularExpressions.Match]$Match) {
        return @(1..3 | ForEach-Object {
            [double]::Parse(
                $Match.Groups[$_].Value,
                [System.Globalization.CultureInfo]::InvariantCulture)
        })
    }
    $LeftRuntime = Parse-VectorMatch $LeftEyePositionMatch
    $RightRuntime = Parse-VectorMatch $RightEyePositionMatch
    $LeftApplied = Parse-VectorMatch $LeftAppliedMatch
    $RightApplied = Parse-VectorMatch $RightAppliedMatch
    $RuntimeDistance = [Math]::Sqrt(
        [Math]::Pow($RightRuntime[0] - $LeftRuntime[0], 2) +
        [Math]::Pow($RightRuntime[1] - $LeftRuntime[1], 2) +
        [Math]::Pow($RightRuntime[2] - $LeftRuntime[2], 2))
    $AppliedDistance = [Math]::Sqrt(
        [Math]::Pow($RightApplied[0] - $LeftApplied[0], 2) +
        [Math]::Pow($RightApplied[1] - $LeftApplied[1], 2) +
        [Math]::Pow($RightApplied[2] - $LeftApplied[2], 2))
    $ExpectedAppliedDistance = $RuntimeDistance * $Scale
    $DistanceTolerance = [Math]::Max(0.02, $ExpectedAppliedDistance * 0.005)
    if (-not [double]::IsFinite($RuntimeDistance) -or $RuntimeDistance -lt 0.02 -or
        -not [double]::IsFinite($AppliedDistance) -or
        [Math]::Abs($AppliedDistance - $ExpectedAppliedDistance) -gt $DistanceTolerance) {
        throw "The applied ChromeEngine eye baseline does not match the XR eye baseline converted from metres to centimetres."
    }

    $LeftFrustumMatch = [regex]::Match(
        $Line,
        "left_frustum=([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+)")
    $RightFrustumMatch = [regex]::Match(
        $Line,
        "right_frustum=([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+)")
        if (-not $LeftFrustumMatch.Success -or -not $RightFrustumMatch.Success) {
            throw "A stereo frame did not report both applied off-center frusta."
        }
        $LeftFrustum = @(1..6 | ForEach-Object {
            [double]::Parse($LeftFrustumMatch.Groups[$_].Value, [System.Globalization.CultureInfo]::InvariantCulture)
        })
        $RightFrustum = @(1..6 | ForEach-Object {
            [double]::Parse($RightFrustumMatch.Groups[$_].Value, [System.Globalization.CultureInfo]::InvariantCulture)
        })
        if ($LeftFrustum[4] -le 0 -or $LeftFrustum[5] -le $LeftFrustum[4] -or
            [Math]::Abs($LeftFrustum[4] - $RightFrustum[4]) -gt 0.0001 -or
            [Math]::Abs($LeftFrustum[5] - $RightFrustum[5]) -gt 0.01 -or
            ([Math]::Abs($LeftFrustum[0] - $RightFrustum[0]) -lt 0.0001 -and
             [Math]::Abs($LeftFrustum[1] - $RightFrustum[1]) -lt 0.0001)) {
            throw "The applied per-eye frusta were invalid, inconsistent, or not horizontally asymmetric."
        }
    }
}

$PresentedStereoLines = @($Lines | Where-Object {
    $_ -match "native_stereo_presenter_frame: status=new" -and
    $_ -match "transport=d3d9ex_shared_texture_ring"
})
if ($PresentedStereoLines.Count -lt 2) {
    throw "Too few GPU-resident native-stereo frames reached the OpenVR presenter."
}
foreach ($Line in $PresentedStereoLines) {
    if ($Line -notmatch "transport=d3d9ex_shared_texture_ring" -or
        $Line -notmatch "distinct_eye_content=not_cpu_sampled" -or
        $Line -notmatch "eye_resources_distinct=true" -or
        $Line -notmatch "distinct_check=separate_shared_eye_resources" -or
        $Line -notmatch "hash_mode=disabled_gpu_resident" -or
        $Line -notmatch "consumer_gpu_copy_queue_ms=[0-9.]+" -or
        $Line -notmatch "consumer_wait_ms=0.000" -or
        $Line -notmatch "consumer_pending_fences=[0-9]+" -or
        $Line -notmatch "frame_age_ms=[0-9.]+") {
        throw "A sampled presenter frame did not prove separate GPU eye resources, zero consumer wait and frame-age telemetry."
    }
}

if ($RequireStereoGeometry) {
    $OrientationLines = @($Lines | Where-Object {
        $_ -match "camera_probe_event: event=camera_hmd_orientation_applied result=ok"
    })
    if ($OrientationLines.Count -lt 2) {
        throw "Too few HMD-driven camera samples were recorded during native stereo."
    }
    foreach ($Line in $OrientationLines) {
    if ($Line -notmatch "native_homogeneous_layout=true" -or
        $Line -notmatch "roll_mode=native_camera_basis" -or
        $Line -notmatch "leveled_forward=\(" -or
        $Line -notmatch "leveled_up=\(" -or
        $Line -notmatch "source_world_homogeneous_layout=true" -or
        $Line -notmatch "source_view_homogeneous_layout=true" -or
        $Line -notmatch "injected_view_homogeneous_layout=true" -or
        $Line -notmatch "restore_deferred=true;stereo=true;renderer_camera_match=true") {
        throw "An HMD stereo camera sample violated the native matrix/restoration contract."
    }
    $DeterminantMatch = [regex]::Match($Line, "applied_determinant=([-+0-9.eE]+)")
    if (-not $DeterminantMatch.Success) {
        throw "An HMD stereo sample did not report its applied determinant."
    }
    $AppliedDeterminant = [double]::Parse(
        $DeterminantMatch.Groups[1].Value,
        [System.Globalization.CultureInfo]::InvariantCulture)
    if (-not [double]::IsFinite($AppliedDeterminant) -or
        [Math]::Abs($AppliedDeterminant - 1.0) -gt 0.02) {
        throw "An HMD stereo sample used a reflected or non-rigid camera basis."
    }
    }
    if (-not ($OrientationLines | Where-Object {
        $_ -match "view_matrix_changed=true" -and
        $_ -match "projection_matrix_changed=true" -and
        $_ -match "view_projection_changed=true"
    })) {
        throw "Stereo telemetry never observed view, asymmetric projection and view-projection changes together."
    }

    $YawSamples = @($OrientationLines | ForEach-Object {
        if ($_ -match "yaw_degrees=([-+0-9.eE]+)") { [double]$Matches[1] }
    })
    $PitchSamples = @($OrientationLines | ForEach-Object {
        if ($_ -match "pitch_degrees=([-+0-9.eE]+)") { [double]$Matches[1] }
    })
    $RollSamples = @($OrientationLines | ForEach-Object {
        if ($_ -match "roll_degrees=([-+0-9.eE]+)") { [double]$Matches[1] }
    })
    if (-not ($YawSamples | Where-Object { [Math]::Abs($_) -ge 5.0 })) {
        throw "Telemetry did not capture a meaningful physical yaw movement."
    }
    if (-not ($PitchSamples | Where-Object { [Math]::Abs($_) -ge 3.0 })) {
        throw "Telemetry did not capture a meaningful physical pitch movement."
    }
    if (-not ($RollSamples | Where-Object { [Math]::Abs($_) -ge 3.0 })) {
        throw "Telemetry did not capture a meaningful physical head tilt through the native camera roll path."
    }
}
if ($RequirePositional6Dof) {
    Assert-LogMatch `
        "camera_probe_event: event=body_player_reconciliation result=camera_only detail=.*actor_write=false;.*mode=camera_only_collision_safe;collision_owner=native_actor" `
        "Room-scale translation did not preserve native actor collision ownership."
    Assert-LogMatch `
        "camera_probe_event: event=body_yaw_tracking result=applied detail=.*route=PlayerBeing.RotateHorizontally;commit_after_stereo_restore=true" `
        "Physical HMD yaw never transferred to the native player body root."
    $MeaningfulPositionObserved = $false
    foreach ($Line in $OrientationLines) {
        $PositionMatch = [regex]::Match(
            $Line,
            "relative_head_position=\(([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+)\)")
        if (-not $PositionMatch.Success -or $Line -notmatch "head_position_valid=true") {
            continue
        }
        $X = [double]::Parse($PositionMatch.Groups[1].Value, [System.Globalization.CultureInfo]::InvariantCulture)
        $Y = [double]::Parse($PositionMatch.Groups[2].Value, [System.Globalization.CultureInfo]::InvariantCulture)
        $Z = [double]::Parse($PositionMatch.Groups[3].Value, [System.Globalization.CultureInfo]::InvariantCulture)
        $Magnitude = [Math]::Sqrt($X * $X + $Y * $Y + $Z * $Z)
        if ([double]::IsFinite($Magnitude) -and $Magnitude -ge 0.03) {
            $MeaningfulPositionObserved = $true
            break
        }
    }
    if (-not $MeaningfulPositionObserved) {
        throw "Telemetry did not capture at least 3 cm of valid physical HMD translation for the 6DOF gate."
    }

    $RoomScaleBodyLines = @($Lines | Where-Object {
        $_ -match "camera_probe_event: event=body_visual_roomscale result=applied"
    })
    foreach ($Line in $RoomScaleBodyLines) {
        $WorldOffsetMatch = [regex]::Match(
            $Line,
            "world_offset=\(([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+)\)")
        if (-not $WorldOffsetMatch.Success) {
            throw "A visual body room-scale sample did not report its world offset."
        }
        $VerticalOffset = [double]::Parse(
            $WorldOffsetMatch.Groups[2].Value,
            [System.Globalization.CultureInfo]::InvariantCulture)
        if (-not [double]::IsFinite($VerticalOffset) -or [Math]::Abs($VerticalOffset) -gt 0.001) {
            throw "Visual body room-scale compensation applied vertical HMD translation to the pelvis."
        }
    }
}

if ($RequireBodyIk) {
    Assert-LogMatch `
        "camera_probe_event: event=camera_probe_control_loaded result=accepted .*tracking_enabled=true;body_ik_enabled=true" `
        "The run never enabled the controller-driven body IK overlay."
    Assert-LogMatch `
        "camera_probe_event: event=body_tracking_input result=observed detail=.*left_position_valid=true;left_orientation_valid=true;.*right_position_valid=true;right_orientation_valid=true;" `
        "The body IK gate never observed valid tracked poses for both Sense controllers."
    $BodyIkLines = @($Lines | Where-Object {
        $_ -match "camera_probe_event: event=body_arm_tracking result=applied"
    })
    foreach ($Side in @("left", "right")) {
        $SideLines = @($BodyIkLines | Where-Object { $_ -match ";side=$Side;" })
        if ($SideLines.Count -lt 1) {
            throw "The body IK gate did not apply a valid $Side arm from the tracked Sense pose."
        }
        foreach ($Line in $SideLines) {
            if ($Line -notmatch ";plan_valid=true;" -or
                $Line -notmatch ";rotation_plan_valid=true;" -or
                $Line -notmatch ";write_enabled=true;write_allowed=true;write_ok=true;" -or
                $Line -notmatch ";hand_orientation=calibrated_controller_delta_bounded_hand_residual;" -or
                $Line -notmatch ";tracking_basis=level_recenter_minus_actor_yaw;" -or
                $Line -notmatch ";controller_orientation_valid=true;" -or
                $Line -notmatch ";orientation_calibration_recenter_sequence=[0-9]+;" -or
                $Line -notmatch ";upper_element_position=\(" -or
                $Line -notmatch ";forearm_element_position=\(" -or
                $Line -notmatch ";foretwist_element_position=\(" -or
                $Line -notmatch ";hand_element_position=\(" -or
                $Line -notmatch ";hand_target_up=\(" -or
                $Line -notmatch ";hand_target_forward=\(" -or
                $Line -notmatch ";upper_target_position=\(" -or
                $Line -notmatch ";forearm_target_position=\(" -or
                $Line -notmatch ";upper_rotation_axis=\(" -or
                $Line -notmatch ";forearm_rotation_axis=\(" -or
                $Line -notmatch ";upper_native_axis=\(" -or
                $Line -notmatch ";forearm_native_axis=\(" -or
                $Line -notmatch ";forearm_twist_native_axis=\(" -or
                $Line -notmatch ";twist_owner=foretwist_element;" -or
                $Line -notmatch ";hand_residual_source=post_foretwist_observed_basis;" -or
                $Line -notmatch ";hand_residual_native_axis=\(" -or
                $Line -notmatch ";hand_residual_degrees=[-+0-9.eE]+;" -or
                $Line -notmatch ";hand_rotation_mode=bounded_hand_residual;" -or
                $Line -notmatch ";hand_rotation_limit_degrees=30(?:\.0+)?;" -or
                $Line -notmatch ";hand_residual_rejected=(?:true|false);" -or
                $Line -notmatch ";reach_write_safe=true;" -or
                $Line -notmatch ";hand_native_axis=\(" -or
                $Line -notmatch ";hand_rotation_degrees=[-+0-9.eE]+;" -or
                $Line -notmatch ";hand_rotation_no_op=(?:true|false);" -or
                $Line -notmatch ";forearm_twist_limit_degrees=0;" -or
                $Line -notmatch ";forearm_twist_no_op=true;" -or
                $Line -notmatch ";skinning_contract=foretwist_sibling_swing_plus_bounded_hand_residual;" -or
                $Line -notmatch ";skinning_frames_reached=true;" -or
                $Line -notmatch ";native_axis_space=element_local;" -or
                $Line -notmatch ";targets_reached=true;" -or
                $Line -notmatch ";hand_orientation_reached=(?:true|false);" -or
                $Line -notmatch ";rollback_attempted=false;rollback_ok=true;" -or
                $Line -notmatch ";reach_adjustment=0(?:\.0+)?;reach_adjusted=false;reach_policy=native_chain_hard_clamp;" -or
                $Line -notmatch ";tracking_forward=-z_to_negative_native_forward;" -or
                $Line -notmatch ";basis_source=GetElementPos/GetElementLeftVector/GetElementUpVector;writer=RotateElementWithChildren") {
                throw "A $Side arm IK application did not prove the measured-skeleton/native-writer contract."
            }
            $SkinErrorMatch = [regex]::Match($Line, ";skinning_axis_error=([-+0-9.eE]+);")
            if (-not $SkinErrorMatch.Success) { throw "Missing sibling skinning axis error." }
            $SkinError = [double]::Parse($SkinErrorMatch.Groups[1].Value, [System.Globalization.CultureInfo]::InvariantCulture)
            if (-not [double]::IsFinite($SkinError) -or $SkinError -lt 0 -or $SkinError -gt 0.02) {
                throw "Sibling skinning frame mismatch exceeded tolerance."
            }
            $HandRotationMatch = [regex]::Match($Line, ";hand_rotation_degrees=([-+0-9.eE]+);")
            if (-not $HandRotationMatch.Success) { throw "Missing bounded hand residual angle." }
            $HandRotation = [double]::Parse($HandRotationMatch.Groups[1].Value, [System.Globalization.CultureInfo]::InvariantCulture)
            if (-not [double]::IsFinite($HandRotation) -or $HandRotation -lt 0 -or $HandRotation -gt 30.001) {
                throw "Controller-driven hand residual exceeded the 30-degree safety bound."
            }
            $HandResidualMatch = [regex]::Match($Line, ";hand_residual_degrees=([-+0-9.eE]+);")
            $HandNoOp = $Line -match ";hand_rotation_no_op=true;"
            $HandRejected = $Line -match ";hand_residual_rejected=true;"
            if (-not $HandResidualMatch.Success) { throw "Missing observed hand residual angle." }
            $HandResidual = [double]::Parse($HandResidualMatch.Groups[1].Value, [System.Globalization.CultureInfo]::InvariantCulture)
            if ($HandResidual -gt 30.001 -and
                (-not $HandRejected -or -not $HandNoOp -or $HandRotation -gt 0.001)) {
                throw "An excessive controller hand mismatch was partially applied instead of preserving the native wrist pose."
            }
            foreach ($RotationField in @("upper_rotation_degrees", "forearm_rotation_degrees")) {
                $RotationMatch = [regex]::Match($Line, ";$RotationField=([-+0-9.eE]+);")
                if (-not $RotationMatch.Success) { throw "Missing $RotationField arm safety telemetry." }
                $RotationDegrees = [double]::Parse(
                    $RotationMatch.Groups[1].Value,
                    [System.Globalization.CultureInfo]::InvariantCulture)
                if (-not [double]::IsFinite($RotationDegrees) -or
                    $RotationDegrees -lt 0 -or $RotationDegrees -gt 120.001) {
                    throw "A tracked arm write exceeded the 120-degree native-animation takeover bound."
                }
            }
        }
    }
    $BodyWriteProbes = @($Lines | Where-Object {
        $_ -match "camera_probe_event: event=body_arm_write_probe"
    })
    $BodyRenderProbes = @($Lines | Where-Object {
        $_ -match "camera_probe_event: event=body_arm_render_probe"
    })
    foreach ($Side in @("left", "right")) {
        if (-not ($BodyWriteProbes | Where-Object {
            $_ -match "result=natural .*;side=$Side;phase=before_write;" -and
            $_ -match ";writer=RotateElementWithChildren"
        })) {
            throw "The body IK gate did not capture the natural $Side arm immediately before the element overlay."
        }
        if (-not ($BodyWriteProbes | Where-Object {
            $_ -match "result=changed .*;side=$Side;phase=after_write;expects_change=true;changed_from_natural=true;targets_reached=true;.*;hand_orientation_reached=(?:true|false);" -and
            $_ -match ";writer=RotateElementWithChildren"
        })) {
            throw "The body IK gate did not prove that the $Side element writer reached the solved arm position."
        }
        foreach ($Phase in @("left_eye_complete", "right_eye_complete")) {
            if (-not ($BodyRenderProbes | Where-Object {
                $_ -match "result=changed .*;side=$Side;phase=$Phase;changed_from_natural=true;" -and
                $_ -match ";post_write_geometry_valid=true;matches_post_write=true;" -and
                $_ -match ";writer=RotateElementWithChildren"
            })) {
                throw "The body IK gate did not preserve the changed $Side arm through $Phase."
            }
        }
    }
    if ($BodyWriteProbes | Where-Object {
        $_ -match ";phase=after_write;expects_change=true;" -and
        $_ -notmatch "result=changed"
    }) {
        throw "The body IK gate recorded a non-mutating element write for a non-zero arm rotation."
    }
    $BodyRestoreLines = @($Lines | Where-Object {
        $_ -match "camera_probe_event: event=body_arm_restore result=ok"
    })
    foreach ($Side in @("left", "right")) {
        $RestoreLines = @($BodyRestoreLines | Where-Object { $_ -match ";side=$Side;" })
        if ($RestoreLines.Count -lt 1 -or
            -not ($RestoreLines | Where-Object {
                $_ -match ";hand_restored=true;forearm_twist_restored=true;forearm_restored=true;upper_restored=true;geometry_read=true;geometry_restored=true;writer=RotateElementWithChildren;twist_owner=foretwist_element;transaction=post_stereo_capture;" -and
                $_ -match ";restore_joint_error=[-+0-9.eE]+;restore_element_position_error=[-+0-9.eE]+;restore_axis_error=[-+0-9.eE]+"
            })) {
            throw "The body IK gate did not restore the natural $Side element hierarchy after stereo capture."
        }
        foreach ($Line in $RestoreLines) {
            $JointMatch = [regex]::Match($Line, "restore_joint_error=([-+0-9.eE]+)")
            $PositionMatch = [regex]::Match($Line, "restore_element_position_error=([-+0-9.eE]+)")
            $AxisMatch = [regex]::Match($Line, "restore_axis_error=([-+0-9.eE]+)")
            if (-not $JointMatch.Success -or -not $PositionMatch.Success -or
                -not $AxisMatch.Success) {
                throw "A $Side arm restore omitted its measured geometry error."
            }
            $JointError = [double]::Parse($JointMatch.Groups[1].Value, [System.Globalization.CultureInfo]::InvariantCulture)
            $PositionError = [double]::Parse($PositionMatch.Groups[1].Value, [System.Globalization.CultureInfo]::InvariantCulture)
            $AxisError = [double]::Parse($AxisMatch.Groups[1].Value, [System.Globalization.CultureInfo]::InvariantCulture)
            if (-not [double]::IsFinite($JointError) -or $JointError -gt 0.02 -or
                -not [double]::IsFinite($PositionError) -or $PositionError -gt 0.02 -or
                -not [double]::IsFinite($AxisError) -or $AxisError -gt 0.001) {
                throw "A $Side arm restore exceeded the float-aware natural-geometry tolerance."
            }
        }
    }
    if ($Lines | Where-Object {
        $_ -match "camera_probe_event: event=body_arm_restore result=failed"
    }) {
        throw "The body IK gate recorded a failed element-hierarchy restore."
    }
    Assert-LogMatch `
        "camera_probe_event: event=body_visual_roomscale result=applied detail=.*;actor_write=false;scope=stereo_render_only;restore=after_both_eyes" `
        "The body IK gate did not move the local visual body with room-scale HMD translation."
    Assert-LogMatch `
        "camera_probe_event: event=body_visual_roomscale_restore result=ok detail=.*;reason=stereo_complete" `
        "The room-scale local-body overlay did not restore after both eye renders."
    Assert-LogNotMatch `
        "camera_probe_event: event=body_visual_roomscale_restore result=failed" `
        "The room-scale local-body overlay recorded a failed pelvis restoration."

    $LowerBodyLines = @($Lines | Where-Object {
        $_ -match "camera_probe_event: event=body_lower_tracking result=observed"
    })
    foreach ($Side in @("left", "right")) {
        $SideLines = @($LowerBodyLines | Where-Object { $_ -match ";side=$Side;" })
        if ($SideLines.Count -lt 1) {
            throw "The body IK gate did not observe a valid $Side lower-body chain."
        }
        foreach ($Line in $SideLines) {
            if ($Line -notmatch ";pelvis_target=\(" -or
                $Line -notmatch ";plan_valid=true;" -or
                $Line -notmatch ";knee_plane_valid=true;" -or
                $Line -notmatch ";write_enabled=false;" -or
                $Line -notmatch ";foot_orientation=natural;" -or
                $Line -notmatch ";basis_source=GetBoneDirVector/GetBonePerpVector;writer=disabled_preflight") {
                throw "A $Side lower-body observation did not prove the measured pelvis/leg preflight contract."
            }
        }
    }
}

if ($RequireGameplayInput) {
    Assert-LogMatch `
        "camera_probe_event: event=native_stereo_ui_back result=applied detail=.*;source=right_circle;route=LawmanGame\.sm_cActiveGameModule\.OnInputKey\(Escape\)" `
        "Circle did not open the shipped in-game Escape menu from native-stereo gameplay."
    Assert-LogMatch `
        "camera_probe_event: event=controller_aim result=applied detail=.*left_visual_origin_written=true;right_visual_origin_written=true;.*tracking_basis=level_recenter_minus_actor_yaw;direction_owner=m_avLookDirDevForHand;fire_origin=controller_tip_attack_transition;visual_origin_owner=m_avAimFromPoint;fire_origin_release=native_UpdateLookAndAimPoints;native_accuracy_spread=preserved" `
        "Controller tip direction/origin did not reach the exact per-hand CoJ aiming fields with deferred weapon-state ownership."
    Assert-LogMatch `
        "camera_probe_event: event=controller_aim_geometry result=observed detail=.*left_grip_to_tip_distance_m=.*;left_dot_neg_z=.*;right_grip_to_tip_distance_m=.*;right_dot_neg_z=.*;axis_reference=grip_to_tip_tracking_space" `
        "The gameplay gate did not capture grip-to-tip geometry needed to validate the runtime tip-axis convention."
    Assert-LogMatch `
        "camera_probe_event: event=controller_aim_fire_transition result=applied detail=.*;side=(left|right);.*;direction=\(.*\);.*;origin=\(.*\);.*;grip_to_tip_direction=\(.*\);.*;dot_neg_z=.*;aim_axis_assumption=negative_z;direction_owner=m_avLookDirDevForHand;fire_origin_owner=Being.m_vLookFromPoint" `
        "A fire press did not preserve the exact controller aim/origin and tip-axis diagnostics at the native attack transition."
    Assert-LogMatch `
        "camera_probe_event: event=gameplay_input result=applied detail=.*;active=true;.*;crouch=false;physical_crouch=true;.*;locomotion_policy=coj_inputanalog_per_axis_deadzone_0.04;route=GameInputController.InputAction.Translate" `
        "The gameplay gate did not keep physical crouch separate from the native crouch action."
    Assert-LogNotMatch `
        "renderer=LaserPointer\.PointFromTo" `
        "The run created the old diagnostic LaserPointer during normal gameplay."
    $GameplayLines = @($Lines | Where-Object {
        $_ -match "camera_probe_event: event=gameplay_input result=applied"
    })
    if ($GameplayLines.Count -lt 1) {
        throw "The gameplay-input gate did not reach the exact-game InputAction.Translate route."
    }
    $MeaningfulGameplayInput = $false
    foreach ($Line in $GameplayLines) {
        if ($Line -notmatch ";active=true;" -or
            $Line -notmatch ";locomotion_policy=coj_inputanalog_per_axis_deadzone_0.04;" -or
            $Line -notmatch ";route=GameInputController.InputAction.Translate") {
            continue
        }
        $MoveMatch = [regex]::Match($Line, ";move=([-+0-9.eE]+),([-+0-9.eE]+)")
        $TurnMatch = [regex]::Match($Line, ";turn=([-+0-9.eE]+),([-+0-9.eE]+)")
        $DigitalPressed = $Line -match ";(fire_left|fire_right|jump|reload|run|crouch|interact|weapon_next|weapon_previous|kick)=true;"
        $AxisActive = $false
        foreach ($Match in @($MoveMatch, $TurnMatch)) {
            if (-not $Match.Success) { continue }
            $First = [Math]::Abs([double]::Parse($Match.Groups[1].Value, [System.Globalization.CultureInfo]::InvariantCulture))
            $Second = [Math]::Abs([double]::Parse($Match.Groups[2].Value, [System.Globalization.CultureInfo]::InvariantCulture))
            if ($First -ge 0.15 -or $Second -ge 0.15) { $AxisActive = $true }
        }
        if ($AxisActive -and
            $Line -notmatch ";analog_transaction=LockApplyControllerState\+dispatch\+UnlockApplyControllerState\+ApplyControllerState;") {
            throw "Active analog locomotion did not reproduce the shipped controller-state transaction."
        }
        if ($AxisActive -or $DigitalPressed) {
            $MeaningfulGameplayInput = $true
            break
        }
    }
    if (-not $MeaningfulGameplayInput) {
        throw "The gameplay-input gate did not observe any non-neutral Sense gameplay action."
    }
}

if ($RequireExplicitPassthroughDisable) {
    Assert-LogMatch `
        "camera_probe_event: event=camera_probe_passthrough result=disabled" `
        "Tracking was not explicitly disabled back to natural camera passthrough."
    Assert-LogMatch `
        "camera_probe_event: event=camera_probe_control_loaded result=accepted .*tracking_enabled=false" `
        "No command explicitly disabled HMD tracking for this run."
}
Assert-LogMatch `
    "camera_probe_event: event=camera_probe_restore result=restored .*view_restored_slots=1" `
    "The camera/render-view hooks were not cleanly restored on normal exit."
Assert-LogMatch `
    "native_stereo_factory_hook: status=restored" `
    "The CreateDevice observation hook was not cleanly restored."
Assert-LogMatch `
    "native_stereo_presenter_stop: shutdown_complete=true" `
    "The joined presenter did not prove that its owning OpenVR runtime completed shutdown."
Assert-LogMatch `
    "native_stereo_shutdown: stage=presenter_end" `
    "The proxy did not join the presenter during finalization."
Assert-LogMatch `
    "native_stereo_transport_summary: .*frames_collected=[1-9][0-9]*.*shared_frames_published=[1-9][0-9]*.*cpu_fallback_frames=0.*fallback_activations=0.*frames_uploaded=[1-9][0-9]*.*new_submissions=[1-9][0-9]*.*shared_frames_copied=[1-9][0-9]*.*shared_open_failures=0.*shared_copy_failures=0.*submit_failures=0" `
    "The transport summary did not prove a zero-fallback GPU-resident capture/copy/submission path."
Assert-LogMatch `
    "native_stereo_gpu_transport_summary: frames_copied=[1-9][0-9]*;resources_opened=[1-9][0-9]*;copy_fences_completed=[1-9][0-9]*;.*open_failures=0;copy_failures=0" `
    "The presenter did not retire shared-copy fences without transport failures."
if ($RequireRepeatedPresentation) {
    Assert-LogMatch `
        "native_stereo_transport_summary: .*repeat_submissions=[1-9][0-9]*" `
        "The run did not physically exercise repeated-frame presentation while waiting for new game content."
}
Assert-LogMatch `
    "native_stereo_runtime: status=stopped" `
    "The native-stereo OpenVR runtime did not shut down cleanly."

if ($RequireOpenVrRuntimeState) {
    $RuntimeShutdownState = [bool]($Lines -match
        "openvr_runtime_state: phase=shutdown_complete;lifecycle=shutdown_complete;initialized=false;connected=false;focused=false;tracking_valid=false;presenting=false;shutdown_requested=false")
    $JoinedShutdownState = [bool]($Lines -match
        "native_stereo_presenter_stop: shutdown_complete=true")
    if (-not $RuntimeShutdownState -and -not $JoinedShutdownState) {
        throw "The OpenVR presenter did not prove a complete final shutdown state."
    }
}

if ($RequireOpenVrDashboardCycle) {
    $ActiveIndex = -1
    $DashboardOpenedIndex = -1
    $DashboardClosedIndex = -1
    $PresentationResumed = $false
    for ($Index = 0; $Index -lt $Lines.Count; ++$Index) {
        $Line = $Lines[$Index]
        if ($ActiveIndex -lt 0 -and
            $Line -match "^openvr_runtime_state:.*;focused=true;tracking_valid=true;presenting=true;") {
            $ActiveIndex = $Index
            continue
        }
        if ($ActiveIndex -ge 0 -and $DashboardOpenedIndex -lt 0 -and
            $Line -match "^openvr_scene_state: phase=dashboard_opened;.*dashboard_visible=true") {
            $DashboardOpenedIndex = $Index
            continue
        }
        if ($DashboardOpenedIndex -ge 0 -and $DashboardClosedIndex -lt 0 -and
            $Line -match "^openvr_scene_state: phase=dashboard_closed;.*dashboard_visible=false") {
            $DashboardClosedIndex = $Index
            continue
        }
        if ($DashboardClosedIndex -ge 0 -and
            $Line -match "^native_stereo_presenter_timing: status=ok ") {
            $PresentationResumed = $true
            break
        }
    }
    if ($ActiveIndex -lt 0 -or $DashboardOpenedIndex -lt 0 -or
        $DashboardClosedIndex -lt 0 -or -not $PresentationResumed) {
        throw "The run did not prove a SteamVR dashboard open/close cycle followed by resumed stereo presentation."
    }
}

if ($Lines -match "d3d9_hook_event:.*(Present|BeginScene|EndScene|Reset)") {
    throw "Native stereo unexpectedly depended on a D3D9 frame/device hook."
}

$EscapedRunId = [Regex]::Escape([string]$Provenance.RunId)
Assert-LogMatch "run_end: run_id=$EscapedRunId(?:\s|$)" "The run did not end normally."

Write-Host "PASS - exact CoJ/ChromeEngine/OpenVR deployment identities verified."
if ($RequireRecenter) {
    Write-Host "PASS - PS VR2 Sense Create recenter action reached the camera pose boundary."
}
Write-Host "PASS - ChromeEngine eye passes were queued through the deferred D3D9 ring and distinct frames reached the presenter."
if ($RequireStereoGeometry) {
    Write-Host "PASS - metre-to-centimetre eye baseline, viewport, asymmetric frusta and HMD camera matrices verified."
}
if ($RequirePositional6Dof) {
    Write-Host "PASS - valid recentered HMD translation and at least 3 cm of physical 6DOF movement verified."
}
if ($RequireBodyIk) {
    Write-Host "PASS - both tracked Sense poses drove measured two-bone arm IK through the exact native element-basis writer."
    Write-Host "PASS - pelvis plus both measured leg chains produced read-only lower-body IK preflight telemetry with no lower-body writes."
}
Write-Host "PASS - dedicated presenter submission, passthrough, hook restoration and same-owner XR shutdown verified."
if ($RequireFlatTheaterUi) {
    Write-Host "PASS - flat-theater Sense ray/cursor, exact CoJ UI select, loading-gate release and native-stereo transition verified."
}
if ($RequireOpenVrRuntimeState) {
    Write-Host "PASS - OpenVR connected/tracking/presenting state and joined shutdown-complete state verified."
}
if ($RequireOpenVrDashboardCycle) {
    Write-Host "PASS - SteamVR dashboard open/close and resumed presentation verified in the same physical run."
}
