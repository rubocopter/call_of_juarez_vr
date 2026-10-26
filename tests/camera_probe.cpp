#include "games/call_of_juarez/camera_probe.hpp"
#include "games/call_of_juarez/scoped_rifle_view.hpp"
#include "games/call_of_juarez/body_adapter.hpp"
#include "games/call_of_juarez/java_player_bridge.hpp"
#include "games/call_of_juarez/ppfx_resource_observation.hpp"
#include "runtime/vr_math.hpp"
#include <windows.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {

bool Near(float lhs, float rhs, float epsilon = 0.001F) {
    return std::fabs(lhs - rhs) <= epsilon;
}

float Dot(
    const cojvr::games::call_of_juarez::CameraProbeVector& lhs,
    const cojvr::games::call_of_juarez::CameraProbeVector& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

float Length(const cojvr::games::call_of_juarez::CameraProbeVector& value) {
    return std::sqrt(Dot(value, value));
}

cojvr::games::call_of_juarez::CameraProbeVector Cross(
    const cojvr::games::call_of_juarez::CameraProbeVector& lhs,
    const cojvr::games::call_of_juarez::CameraProbeVector& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

bool NearVector(
    const cojvr::games::call_of_juarez::CameraProbeVector& lhs,
    const cojvr::games::call_of_juarez::CameraProbeVector& rhs,
    float epsilon = 0.001F) {
    return Near(lhs.x, rhs.x, epsilon) && Near(lhs.y, rhs.y, epsilon) &&
        Near(lhs.z, rhs.z, epsilon);
}

cojvr::runtime::Quaternion AxisAngle(
    float x,
    float y,
    float z,
    float degrees) {
    constexpr float kDegreesToRadians = 0.01745329251994329577F;
    const float half = degrees * kDegreesToRadians * 0.5F;
    const float sine = std::sin(half);
    return cojvr::runtime::NormalizeQuaternion({x * sine, y * sine, z * sine, std::cos(half)});
}

cojvr::runtime::Quaternion MultiplyQuaternion(
    const cojvr::runtime::Quaternion lhs,
    const cojvr::runtime::Quaternion rhs) {
    return cojvr::runtime::NormalizeQuaternion({
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
    });
}

} // namespace

int main() {
    using namespace cojvr::games::call_of_juarez;

    SetLastError(0x7134);
    const auto uninstalled_ppfx = ObserveInstalledCoJPostprocessResources();
    if (uninstalled_ppfx.valid ||
        uninstalled_ppfx.status != CoJPostprocessObservationStatus::unknown_build ||
        GetLastError() != 0x7134) {
        std::cerr << "native PPFX observation admitted an uninstalled/unverified engine\n";
        return 1;
    }

    if (ShouldRetainCoJBodyRestoreTransaction(true) ||
        !ShouldRetainCoJBodyRestoreTransaction(false)) {
        std::cerr << "failed body restore no longer retains its retry transaction\n";
        return 1;
    }

    if (std::string_view(CoJSubtitleDiagnosticStateName(false, false, false, false)) !=
            "unavailable" ||
        std::string_view(CoJSubtitleDiagnosticStateName(true, false, true, false)) !=
            "disabled" ||
        std::string_view(CoJSubtitleDiagnosticStateName(true, true, false, false)) !=
            "idle" ||
        std::string_view(CoJSubtitleDiagnosticStateName(true, true, true, false)) !=
            "java_hidden" ||
        std::string_view(CoJSubtitleDiagnosticStateName(true, true, true, true)) !=
            "java_visible") {
        std::cerr << "subtitle diagnostic state did not distinguish settings, dialog and Java visibility\n";
        return 1;
    }

    CameraProbeCommand command{};
    std::string error;
    if (!ParseCameraProbeCommand(
            R"({"enabled":true,"fovDegrees":110,"yawDegrees":30,"pitchDegrees":-10})",
            command, &error) ||
        !command.enabled || !command.override_fov || !Near(command.fov_degrees, 110.0F) ||
        !Near(command.yaw_degrees, 30.0F) || !Near(command.pitch_degrees, -10.0F) ||
        command.movement_trace_enabled || command.reload_trace_enabled || command.reload_wait_probe_installed || command.movement_trace_phase != "off" ||
        !command.vr_gameplay_input_enabled || !command.capture_readback_enabled ||
        !command.second_eye_render_enabled) {
        std::cerr << "valid control command was not parsed: " << error << '\n';
        return 1;
    }

    if (!ParseCameraProbeCommand(R"({"enabled":false,"reloadTraceEnabled":true})", command, &error) ||
        !command.reload_trace_enabled ||
        ParseCameraProbeCommand(R"({"enabled":false,"reloadTraceEnabled":1})", command, &error)) {
        std::cerr << "reload trace opt-in/type validation failed\n";
        return 1;
    }
    if (!ParseCameraProbeCommand(R"({"enabled":false,"reloadWaitProbeInstalled":true})", command, &error) ||
        !command.reload_wait_probe_installed ||
        ParseCameraProbeCommand(R"({"enabled":false,"reloadWaitProbeInstalled":1})", command, &error)) {
        std::cerr << "reload wait probe marker/type validation failed\n";
        return 1;
    }
    if (!ParseCameraProbeCommand(R"({"enabled":false,"manualReloadInstalled":true})", command, &error) ||
        !command.manual_reload_installed ||
        ParseCameraProbeCommand(R"({"enabled":false,"manualReloadInstalled":1})", command, &error)) {
        std::cerr << "manual reload marker invalid\n"; return 1;
    }

    if (!ParseCameraProbeCommand(R"({"enabled":false,"renderResourceProbeEnabled":true})",command,&error) ||
        !command.render_resource_probe_enabled ||
        ParseCameraProbeCommand(R"({"enabled":false,"renderResourceProbeEnabled":1})",command,&error)) {
        std::cerr << "invalid resource probe control\n"; return 1;
    }
    if (!ParseCameraProbeCommand(R"({"enabled":false,"frameLumaProbeEnabled":true})",command,&error) ||
        !command.frame_luma_probe_enabled ||
        ParseCameraProbeCommand(R"({"enabled":false,"frameLumaProbeEnabled":1})",command,&error) ||
        ParseCameraProbeCommand(R"({"enabled":false,"frameLumaProbeEnabled":truejunk})",command,&error) ||
        ParseCameraProbeCommand(R"({"enabled":false,"frameLumaProbeEnabled":false1})",command,&error) ||
        ParseCameraProbeCommand(R"({"enabled":false,"frameLumaProbeEnabled":true junk})",command,&error) ||
        !ParseCameraProbeCommand(R"({"enabled":false})",command,&error) || command.frame_luma_probe_enabled) {
        std::cerr << "invalid luma probe opt-in/default\n"; return 1;
    }
    if (ParseCameraProbeCommand(R"({"enabled":true,"fovDegrees":200})", command, &error) ||
        ParseCameraProbeCommand(R"({"yawDegrees":10})", command, &error) ||
        ParseCameraProbeCommand(R"({"enabled":true,"pitchDegrees":nan})", command, &error) ||
        ParseCameraProbeCommand(R"({"enabled":false,"trackingEnabled":1})", command, &error)) {
        std::cerr << "invalid control command was accepted\n";
        return 1;
    }
    if(!ParseCameraProbeCommand(R"({"enabled":false,"independentEyeResolution":"2560x1920"})",command,&error)||
        command.independent_eye_height!=1920 ||
        !ParseCameraProbeCommand(R"({"enabled":false,"independentEyeResolution":"2560x1440"})",command,&error)||
        command.independent_eye_height!=1440 ||
        !ParseCameraProbeCommand(R"({"enabled":false})",command,&error)||command.independent_eye_height!=0 ||
        ParseCameraProbeCommand(R"({"enabled":false,"independentEyeResolution":"2804x2860"})",command,&error)||
        ParseCameraProbeCommand(R"({"enabled":false,"independentEyeResolution":1920})",command,&error)) {
        std::cerr<<"invalid startup independent eye extent control\n";return 1;
    }

    if (!ParseCameraProbeCommand(
            R"({"enabled":false,"trackingEnabled":true,"bodyIkEnabled":true,"independentHandsEnabled":true})",
            command, &error) || !command.independent_hands_enabled ||
        ParseCameraProbeCommand(R"({"enabled":false,"independentHandsEnabled":"true"})",command,&error)) {
        std::cerr << "independent-hand presentation did not require an explicit boolean\n";
        return 1;
    }

    if (!ParseCameraProbeCommand(
            R"({"enabled":false,"trackingEnabled":true,"bodyIkEnabled":true,"recenter":true})",
            command, &error) ||
        command.enabled || !command.tracking_enabled || !command.body_ik_enabled ||
        !command.recenter) {
        std::cerr << "tracking control command was not parsed: " << error << '\n';
        return 1;
    }

    if (!ParseCameraProbeCommand(
            R"({"enabled":true,"trackingEnabled":true,"movementTraceEnabled":true,"movementTracePhase":"vr-input-off","vrGameplayInputEnabled":false,"captureReadbackEnabled":false,"secondEyeRenderEnabled":false})",
            command, &error) ||
        !command.movement_trace_enabled || command.movement_trace_phase != "vr-input-off" ||
        command.vr_gameplay_input_enabled || command.capture_readback_enabled ||
        command.second_eye_render_enabled) {
        std::cerr << "movement diagnostic controls were not parsed: " << error << '\n';
        return 1;
    }

    if (ParseCameraProbeCommand(
            R"({"enabled":true,"movementTracePhase":"bad phase"})",
            command, &error)) {
        std::cerr << "invalid movement phase label was accepted\n";
        return 1;
    }

    if (std::string_view(CoJMovementSpeedStateName(0x80)) != "walk" ||
        std::string_view(CoJMovementSpeedStateName(0x81)) != "trot" ||
        std::string_view(CoJMovementSpeedStateName(0x82)) != "run" ||
        std::string_view(CoJMovementSpeedStateName(0x83)) != "sprint" ||
        std::string_view(CoJMovementSpeedStateName(11)) != "other" ||
        !CoJOdeWalkStateGrounded(0) || CoJOdeWalkStateGrounded(2)) {
        std::cerr << "movement state diagnostics do not preserve exact-build semantics\n";
        return 1;
    }

    CoJMovementJumpPhase jump_phase = CoJMovementJumpPhase::grounded_stable;
    auto jump_step = AdvanceCoJMovementJumpPhase(jump_phase, false, false, 25.0F);
    jump_phase = jump_step.phase;
    if (!jump_step.begin_candidate || jump_step.confirm_jump ||
        jump_phase != CoJMovementJumpPhase::awaiting_native_confirmation) {
        std::cerr << "ground loss did not remain an unconfirmed jump candidate\n";
        return 1;
    }
    jump_step = AdvanceCoJMovementJumpPhase(jump_phase, false, false, -10.0F);
    jump_phase = jump_step.phase;
    jump_step = AdvanceCoJMovementJumpPhase(jump_phase, true, false, 20.0F);
    jump_phase = jump_step.phase;
    if (!jump_step.reject_candidate || jump_step.confirm_jump ||
        jump_phase != CoJMovementJumpPhase::grounded_stable) {
        std::cerr << "micro-airborne terrain contact was accepted as a jump\n";
        return 1;
    }

    jump_step = AdvanceCoJMovementJumpPhase(jump_phase, false, false, 420.0F);
    jump_phase = jump_step.phase;
    jump_step = AdvanceCoJMovementJumpPhase(jump_phase, false, true, 390.0F);
    jump_phase = jump_step.phase;
    if (!jump_step.confirm_jump ||
        jump_phase != CoJMovementJumpPhase::airborne_ascending) {
        std::cerr << "native IsJumping did not confirm the physical jump\n";
        return 1;
    }
    jump_step = AdvanceCoJMovementJumpPhase(jump_phase, false, true, -5.0F);
    jump_phase = jump_step.phase;
    if (!jump_step.reached_apex ||
        jump_phase != CoJMovementJumpPhase::airborne_descending) {
        std::cerr << "confirmed jump did not enter descent at the apex\n";
        return 1;
    }
    jump_step = AdvanceCoJMovementJumpPhase(jump_phase, true, true, 10.0F);
    jump_phase = jump_step.phase;
    if (jump_step.complete_jump ||
        jump_phase != CoJMovementJumpPhase::landed_waiting_release) {
        std::cerr << "landing completed before native jump state released\n";
        return 1;
    }
    jump_step = AdvanceCoJMovementJumpPhase(jump_phase, true, false, 10.0F);
    if (!jump_step.complete_jump ||
        jump_step.phase != CoJMovementJumpPhase::grounded_stable) {
        std::cerr << "landed jump did not complete on native jump release\n";
        return 1;
    }

    if (ParseCameraProbeCommand(
            R"({"enabled":false,"trackingEnabled":true,"bodyIkEnabled":1})",
            command, &error)) {
        std::cerr << "invalid body IK control value was accepted\n";
        return 1;
    }

    if (!IsSupportedChromeEngineHash(kInspectedChromeEngine3Sha256) ||
        !IsSupportedChromeEngineHash(
            "db69bc35919fe57187766771a2452aca11090474f6d63df1a85a80eded131ec8") ||
        IsSupportedChromeEngineHash(
            "AB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8")) {
        std::cerr << "ChromeEngine exact-build hash gate failed\n";
        return 1;
    }

    const CameraProbeBasis yawed = ApplyCameraProbeOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, 30.0F, -10.0F);
    if (!Near(Length(yawed.forward), 1.0F) || !Near(Length(yawed.up), 1.0F) ||
        !Near(Length(yawed.right), 1.0F) || !Near(Dot(yawed.forward, yawed.up), 0.0F) ||
        !Near(Dot(yawed.forward, yawed.right), 0.0F) || !Near(Dot(yawed.up, yawed.right), 0.0F) ||
        !Near(CameraProbeBasisDeterminant(yawed), 1.0F) ||
        !IsCameraProbeBasisRigidRightHanded(yawed) || yawed.forward.x <= 0.4F) {
        std::cerr << "camera orientation transform did not preserve an orthonormal basis\n";
        return 1;
    }

    const CameraProbeBasis native_identity{
        {0.0F, 0.0F, 1.0F},
        {0.0F, 1.0F, 0.0F},
        {1.0F, 0.0F, 0.0F},
    };
    const CameraProbeBasis reflected_identity{
        native_identity.forward,
        native_identity.up,
        {-1.0F, 0.0F, 0.0F},
    };
    if (!Near(CameraProbeBasisDeterminant(native_identity), 1.0F) ||
        !IsCameraProbeBasisRigidRightHanded(native_identity) ||
        !Near(CameraProbeBasisDeterminant(reflected_identity), -1.0F) ||
        IsCameraProbeBasisRigidRightHanded(reflected_identity)) {
        std::cerr << "camera handedness guard did not distinguish native and reflected bases\n";
        return 1;
    }

    if (!IsMovementTraceCameraReady(native_identity, true, true, true) ||
        IsMovementTraceCameraReady({}, true, true, true) ||
        IsMovementTraceCameraReady(native_identity, false, true, true) ||
        IsMovementTraceCameraReady(native_identity, true, false, true) ||
        IsMovementTraceCameraReady(native_identity, true, true, false)) {
        std::cerr << "movement trace camera gate accepted a loading/incomplete camera\n";
        return 1;
    }

    const CameraProbeBasis scaled_identity{
        native_identity.forward,
        native_identity.up,
        {1.1F, 0.0F, 0.0F},
    };
    const CameraProbeBasis skewed_identity{
        native_identity.forward,
        {0.1F, 1.0F, 0.0F},
        native_identity.right,
    };
    CameraProbeBasis nonfinite_identity = native_identity;
    nonfinite_identity.forward.x = std::numeric_limits<float>::quiet_NaN();
    if (IsCameraProbeBasisRigidRightHanded(scaled_identity) ||
        IsCameraProbeBasisRigidRightHanded(skewed_identity) ||
        IsCameraProbeBasisRigidRightHanded(nonfinite_identity)) {
        std::cerr << "camera rigidity guard accepted scaled, skewed or non-finite basis\n";
        return 1;
    }

    CameraProbeFrustum stereo_frustum{};
    const cojvr::runtime::EyeFov asymmetric_fov{
        std::atan(-1.2F), std::atan(0.8F), std::atan(1.1F), std::atan(-0.9F)};
    if (!BuildCameraProbeFrustum(asymmetric_fov, 0.25F, 500.0F, stereo_frustum) ||
        !Near(stereo_frustum.left, -0.30F) ||
        !Near(stereo_frustum.right, 0.20F) ||
        !Near(stereo_frustum.bottom, -0.225F) ||
        !Near(stereo_frustum.top, 0.275F) ||
        !Near(stereo_frustum.near_plane, 0.25F) ||
        !Near(stereo_frustum.far_plane, 500.0F)) {
        std::cerr << "asymmetric OpenVR FOV did not map to the native off-center frustum\n";
        return 1;
    }
    const CameraProbeFrustum valid_stereo_frustum = stereo_frustum;
    // Exercise the focus render copies through the same native frustum and
    // controller-reticle projection used by both world passes. UI retains
    // the original optics; loss/recovery must not accumulate scale or IPD.
    std::array<cojvr::runtime::EyeView,2> focus_optics{};
    for(int i=0;i<2;++i){focus_optics[i].eye=i?cojvr::runtime::Eye::right:cojvr::runtime::Eye::left;
        focus_optics[i].fov=asymmetric_fov;focus_optics[i].eye_to_head.position.x=i?.032F:-.032F;
        focus_optics[i].eye_to_head.position_valid=true;}
    auto focus_render=focus_optics;
    for(auto zoom:{CoJFocusZoom{1,2,2,true},CoJFocusZoom{},CoJFocusZoom{1,2,2,true},CoJFocusZoom{0,2,1,true}}){
        const float scale=zoom.valid?zoom.magnification:1;
        (void)BuildCoJFocusRenderEyes(focus_optics,zoom,focus_render);
        for(int i=0;i<2;++i){
            CameraProbeFrustum focused{};CameraProbeReticlePoint point{};
            const float eye_x=(i?3.2F:-3.2F)/scale;
            if(!BuildCameraProbeFrustum(focus_render[i].fov,.25F,500,focused)||
                !Near(focused.left,valid_stereo_frustum.left/scale)||
                !Near(focused.right,valid_stereo_frustum.right/scale)||
                !ProjectWorldAimRayToEyeReticle({13,4,5},{0,0,-1},{3+eye_x,4,5},native_identity,focused,100,point)||
                !Near(point.u,.6F+(10-eye_x)*scale/200)||!Near(point.v,.55F)||
                !Near(focus_render[i].eye_to_head.position.x,focus_optics[i].eye_to_head.position.x/scale)||
                !Near(focus_optics[i].fov.angle_left,asymmetric_fov.angle_left)){
                std::cerr<<"focus render/reticle optics or physical UI/IPD recovery mismatch\n";return 1;
            }
        }
    }
    cojvr::runtime::EyeFov invalid_fov = asymmetric_fov;
    invalid_fov.angle_left = std::numeric_limits<float>::quiet_NaN();
    if (BuildCameraProbeFrustum(invalid_fov, 0.25F, 500.0F, stereo_frustum) ||
        BuildCameraProbeFrustum(asymmetric_fov, 0.0F, 500.0F, stereo_frustum) ||
        BuildCameraProbeFrustum(asymmetric_fov, 1.0F, 0.5F, stereo_frustum)) {
        std::cerr << "invalid stereo frustum input was accepted\n";
        return 1;
    }

    // A tracked but unarmed left tip must not steal the sole right gun's cross.
    // These rays are the measured frame-2160 discrepancy (40 degrees / 70 cm).
    const CameraProbeWeaponRay unarmed_left_tip{
        {39675.625F, 3636.834717F, 29483.429688F},
        {0.546377F, -0.013014F, 0.837438F}, true, true, false};
    const CameraProbeWeaponRay right_gun{
        {39617.503906F, 3636.127197F, 29522.767578F},
        {-0.096656F, -0.209404F, 0.973040F}, true, true, true};
    if (SelectWeaponReticleHands(unarmed_left_tip, right_gun) !=
        CoJWeaponReticleHands::right) {
        std::cerr << "unarmed left tip stole the verified right gun reticle during left fire\n";
        return 1;
    }
    auto left_gun = unarmed_left_tip;
    left_gun.muzzle_verified = true;
    auto unarmed_right_tip = right_gun;
    unarmed_right_tip.muzzle_verified = false;
    auto right_whip=unarmed_right_tip;
    right_whip.whip_aim_verified=true;
    if (SelectWeaponReticleHands(unarmed_left_tip,right_whip)!=CoJWeaponReticleHands::right) {
        std::cerr<<"published whip aim must retain its right-hand reticle for either trigger\n";return 1;
    }
    right_whip.origin_valid=false;
    if (SelectWeaponReticleHands(unarmed_left_tip,right_whip)!=CoJWeaponReticleHands::none) {
        std::cerr<<"invalid whip pose retained a reticle\n";return 1;
    }
    auto invalid_direction = right_gun;
    invalid_direction.direction_valid = false;
    auto invalid_origin = right_gun;
    invalid_origin.origin_valid = false;
    struct ReticleSelectionCase {
        const char* name;
        CameraProbeWeaponRay left;
        CameraProbeWeaponRay right;
        CoJWeaponReticleHands expected;
    };
    const ReticleSelectionCase reticle_selection_cases[] = {
        {"right only", unarmed_left_tip, right_gun, CoJWeaponReticleHands::right},
        {"left only", left_gun, unarmed_right_tip, CoJWeaponReticleHands::left},
        {"both without firing", left_gun, right_gun, CoJWeaponReticleHands::both},
        {"unarmed valid tips", unarmed_left_tip, unarmed_right_tip, CoJWeaponReticleHands::none},
        {"invalid direction tracking", unarmed_left_tip, invalid_direction, CoJWeaponReticleHands::none},
        {"invalid origin tracking", unarmed_left_tip, invalid_origin, CoJWeaponReticleHands::none},
        {"invalid right preserves left", left_gun, invalid_direction, CoJWeaponReticleHands::left},
        {"invalid left preserves right", invalid_origin, right_gun, CoJWeaponReticleHands::right},
    };
    for (const auto& test : reticle_selection_cases) {
        if (SelectWeaponReticleHands(test.left, test.right) != test.expected) {
            std::cerr << "weapon reticle selection failed: " << test.name << '\n';
            return 1;
        }
    }

    CameraProbeReticlePoint reticle{};
    if (!ProjectWorldAimRayToEyeReticle(
            {13.0F, 4.0F, 5.0F},
            {0.0F, 0.0F, -1.0F},
            {3.0F, 4.0F, 5.0F},
            native_identity,
            valid_stereo_frustum,
            100.0F,
            reticle) ||
        !reticle.valid || !Near(reticle.u, 0.65F) || !Near(reticle.v, 0.55F)) {
        std::cerr << "world-space weapon ray did not project through the exact asymmetric eye frustum\n";
        return 1;
    }
    if (ProjectWorldAimRayToEyeReticle(
            {3.0F, 4.0F, 5.0F},
            {0.0F, 0.0F, 1.0F},
            {3.0F, 4.0F, 5.0F},
            native_identity,
            valid_stereo_frustum,
            100.0F,
            reticle) ||
        ProjectWorldAimRayToEyeReticle(
            {403.0F, 4.0F, 5.0F},
            {0.0F, 0.0F, -1.0F},
            {3.0F, 4.0F, 5.0F},
            native_identity,
            valid_stereo_frustum,
            100.0F,
            reticle)) {
        std::cerr << "reticle projection accepted a backwards or off-screen weapon ray\n";
        return 1;
    }

    // A stationary native barrel must project in both eyes, including elevation;
    // its reference is a world ray and does not snap to the headset center.
    std::array<CameraProbeReticlePoint,2> level_cannon{},raised_cannon{};
    for(unsigned eye=0;eye<2;++eye){
        const CameraProbeVector position{eye==0?-3.2F:3.2F,0,0};
        if(!ProjectWorldAimRayToEyeReticle({10,-5,-50},{0,0,-1},position,native_identity,
                valid_stereo_frustum,400,level_cannon[eye])||
           !ProjectWorldAimRayToEyeReticle({10,-5,-50},{0,.2F,-.9797959F},position,native_identity,
                valid_stereo_frustum,400,raised_cannon[eye])||
           !(raised_cannon[eye].v<level_cannon[eye].v)){
            std::cerr<<"elevated stationary barrel did not move the stereo reticle upwards\n";return 1;
        }
    }
    if(!(level_cannon[0].u>level_cannon[1].u)){
        std::cerr<<"stationary barrel reticle lost binocular projection\n";return 1;
    }

    // A centred binocular scope uses the actual weapon ray, not a screen-space
    // cross pasted over unrelated head-centred zoom. Both asymmetric eye views
    // preserve tangent spans and the headset's asymmetric principal points.
    {
        std::array<cojvr::runtime::EyeView,2> physical{},zoomed{},scoped{};
        cojvr::runtime::Pose head{};head.position_valid=head.orientation_valid=true;
        head.orientation.w=1;
        for(unsigned i=0;i<2;++i){
            physical[i].fov=i?cojvr::runtime::EyeFov{-.67F,.91F,.72F,-.65F}:
                cojvr::runtime::EyeFov{-.91F,.67F,.72F,-.65F};
            physical[i].eye_to_head.position_valid=true;
            physical[i].eye_to_head.position.x=i? .032F:-.032F;
            zoomed[i]=physical[i];
            zoomed[i].eye_to_head.position.x/=8.F;
            auto scale=[](float x){return std::atan(std::tan(x)/8.F);};
            auto f=physical[i].fov;zoomed[i].fov={scale(f.angle_left),scale(f.angle_right),scale(f.angle_up),scale(f.angle_down)};
        }
        CameraProbeWeaponRay ray{{15,-8,-40},{.1F,.03F,-1},true,true,true};
        auto scene_head=head;
        if(!BuildCoJScopedRifleRenderEyes(native_identity,{},head,0,physical,zoomed,ray,1,500,scoped,scene_head)){
            std::cerr<<"scoped rifle view did not centre both eye frusta on verified aim\n";return 1;
        }
        const auto scope_orientation=scene_head.orientation;
        const auto scene_basis=ApplyCameraPoseOrientation(native_identity.forward,native_identity.up,scope_orientation,0);
        const auto followed_body=ApplyCameraProbeOrientation(native_identity.forward,native_identity.up,35,0);
        auto compensated=zoomed;
        if(!BuildCoJScopedRifleRenderEyes(followed_body,{},head,35,physical,zoomed,ray,1,500,compensated,scene_head)||
            !Near(compensated[0].fov.angle_left,scoped[0].fov.angle_left)||
            !Near(compensated[0].fov.angle_up,scoped[0].fov.angle_up)||
            !Near(scene_head.orientation.y,scope_orientation.y)){
            std::cerr<<"body-follow compensation leaked head yaw into rifle scope\n";return 1;
        }
        // Scope aim belongs to the rifle. With a stationary controller, yaw,
        // pitch and roll of the HMD must leave the rendered scope unchanged.
        for(auto orientation:std::array<cojvr::runtime::Quaternion,3>{{
            {0,.173648F,0,.984808F},{.173648F,0,0,.984808F},{0,0,.173648F,.984808F}}}){
            auto turned=head;turned.orientation=orientation;
            auto moved=zoomed;
            if(!BuildCoJScopedRifleRenderEyes(native_identity,{},turned,0,physical,zoomed,ray,1,500,moved,scene_head)||
                !Near(moved[0].fov.angle_left,scoped[0].fov.angle_left)||
                !Near(moved[0].fov.angle_up,scoped[0].fov.angle_up)||
                !Near(scene_head.orientation.x,scope_orientation.x)||!Near(scene_head.orientation.y,scope_orientation.y)||
                !Near(scene_head.orientation.z,scope_orientation.z)||!Near(scene_head.orientation.w,scope_orientation.w)||
                !Near(turned.orientation.w,orientation.w)){
                std::cerr<<"head rotation changed the controller-owned scope view\n";return 1;
            }
        }
        for(unsigned axis=0;axis<2;++axis){
            auto aimed=ray;auto moved=zoomed;
            if(axis==0)aimed.direction.x+=.05F;else aimed.direction.y+=.05F;
            if(!BuildCoJScopedRifleRenderEyes(native_identity,{},head,0,physical,zoomed,aimed,1,500,moved,scene_head))return 1;
            const auto aimed_basis=ApplyCameraPoseOrientation(native_identity.forward,native_identity.up,scene_head.orientation,0);
            if(axis==0?aimed_basis.forward.x>=scene_basis.forward.x:aimed_basis.forward.y>=scene_basis.forward.y){
                std::cerr<<"controller yaw/pitch failed to move scope in the aim direction\n";return 1;
            }
        }
        for(auto direction:std::array<CameraProbeVector,4>{{{1,0,0},{0,0,1},{-1,.5F,0},{0,.8F,-.6F}}}){
            auto sideways=ray;sideways.origin={};sideways.direction=direction;
            auto side_eyes=zoomed;
            if(!BuildCoJScopedRifleRenderEyes(native_identity,{},head,0,physical,zoomed,sideways,1,500,side_eyes,scene_head)){
                std::cerr<<"rifle-controlled scope was limited to the original head heading\n";return 1;
            }
            const auto aimed=ApplyCameraPoseOrientation(native_identity.forward,native_identity.up,scene_head.orientation,0);
            if(-Dot(aimed.forward,direction)/Length(direction)<.9999F){
                std::cerr<<"scope camera did not follow the verified controller ray\n";return 1;
            }
        }
        for(unsigned eye=0;eye<2;++eye){
            CameraProbeFrustum f{},before{};CameraProbeReticlePoint point{};
            const auto eye_pos=ApplyCameraEyeOffset({},scene_basis,scoped[eye].eye_to_head.position);
            if(!BuildCameraProbeFrustum(scoped[eye].fov,1,500,f)||
                !BuildCameraProbeFrustum(zoomed[eye].fov,1,500,before)||
                !ProjectWorldAimRayToEyeReticle(ray.origin,ray.direction,eye_pos,scene_basis,f,475,point)||
                !Near(f.right-f.left,before.right-before.left)||
                !Near(f.top-f.bottom,before.top-before.bottom)||
                !Near(scoped[eye].eye_to_head.position.x,zoomed[eye].eye_to_head.position.x)){
                std::cerr<<"scope centre, zoom span or physical IPD changed\n";return 1;
            }
        }
        // Inverse the physical headset projection: texture UV .5 is NOT the
        // common optical axis of a mirrored asymmetric lens pair. At infinity
        // both retinal horizontal angles must coincide; at finite depths their
        // disparity must equal the unzoomed physical baseline, not 8x baseline.
        for(float depth:{80.F,200.F,475.F,499.F}){
            float retinal[2]{};
            for(unsigned eye=0;eye<2;++eye){
                CameraProbeFrustum f{},o{};CameraProbeReticlePoint p{};
                if(!BuildCameraProbeFrustum(scoped[eye].fov,1,500,f)||
                    !BuildCameraProbeFrustum(physical[eye].fov,1,500,o))return 1;
                const auto eye_pos=ApplyCameraEyeOffset({},scene_basis,scoped[eye].eye_to_head.position);
                if(!ProjectWorldAimRayToEyeReticle({0,0,0},{.1F,.03F,-1},eye_pos,scene_basis,f,depth,p))return 1;
                retinal[eye]=o.left+p.u*(o.right-o.left);
            }
            if(std::abs((retinal[0]-retinal[1])-6.4F/depth)>2e-5F){
                std::cerr<<"scoped asymmetric retinal disparity does not match physical reference\n";return 1;
            }
        }
        CameraWeaponReticle marker{};marker.active=true;
        for(unsigned eye=0;eye<2;++eye){
            CameraProbeFrustum f{};
            if(!BuildCameraProbeFrustum(scoped[eye].fov,1,500,f)||
                !ProjectWorldAimRayToEyeReticle(ray.origin,ray.direction,
                    ApplyCameraEyeOffset({},scene_basis,scoped[eye].eye_to_head.position),scene_basis,f,475,marker.eyes[eye]))return 1;
        }
        std::array<cojvr::runtime::ScopeLens,2> lenses{};
        if(!BuildCoJScopeLenses(physical,marker,lenses)||
            !Near(lenses[0].center_u,marker.eyes[0].u)||Near(lenses[0].center_u,lenses[1].center_u)){
            std::cerr<<"scope lenses did not retain each asymmetric optical centre\n";return 1;
        }
        for(unsigned eye=0;eye<2;++eye){
            const auto& f=physical[eye].fov;
            const float horizontal=lenses[eye].radius_u*(std::tan(f.angle_right)-std::tan(f.angle_left));
            const float vertical=lenses[eye].radius_v*(std::tan(f.angle_up)-std::tan(f.angle_down));
            const float other=lenses[1-eye].radius_u*(std::tan(physical[1-eye].fov.angle_right)-std::tan(physical[1-eye].fov.angle_left));
            if(!Near(horizontal,vertical)||!Near(horizontal,other)){
                std::cerr<<"scope aperture has unequal angular radii across axes/eyes\n";return 1;
            }
        }
        marker.eyes[1].valid=false;
        if(BuildCoJScopeLenses(physical,marker,lenses)||lenses[0].valid||lenses[1].valid)return 1;
        for(auto bad:std::array<CameraProbeWeaponRay,3>{{
            {{},{0,0,0},true,true,true},{{},{std::numeric_limits<float>::infinity(),0,-1},true,true,true},{{},{0,0,-1},true,true,false}}}){
            if(BuildCoJScopedRifleRenderEyes(native_identity,{},head,0,physical,zoomed,bad,1,500,scoped,scene_head)){
                std::cerr<<"unverified or invalid scope ray admitted\n";return 1;
            }
        }
        head.position_valid=false;
        if(BuildCoJScopedRifleRenderEyes(native_identity,{},head,0,physical,zoomed,ray,1,500,scoped,scene_head))return 1;
    }
    const CameraProbeVector head_position{10.0F, 20.0F, 30.0F};
    const CameraProbeVector left_eye = ApplyCameraEyeOffset(
        head_position, native_identity, {-0.032F, 0.0F, 0.0F});
    const CameraProbeVector right_eye = ApplyCameraEyeOffset(
        head_position, native_identity, {0.032F, 0.0F, 0.0F});
    const CameraProbeVector forward_eye = ApplyCameraEyeOffset(
        head_position, native_identity, {0.0F, 0.0F, -0.01F});
    const CameraProbeVector tracked_head = ApplyCameraTrackingOffset(
        head_position, native_identity, {0.10F, 0.20F, -0.30F});
    if (!Near(kGameUnitsPerMeter, 100.0F) ||
        !NearVector(left_eye, {6.8F, 20.0F, 30.0F}) ||
        !NearVector(right_eye, {13.2F, 20.0F, 30.0F}) ||
        !NearVector(forward_eye, {10.0F, 20.0F, 29.0F}) ||
        !NearVector(tracked_head, {20.0F, 40.0F, 0.0F})) {
        std::cerr << "metre eye/head translation did not map into CoJ centimetres\n";
        return 1;
    }

    // A physical step towards a visible forward reticle must approach its
    // world target. Camera and skeleton must share the same horizontal map.
    const auto physical_forward = ApplyCameraTrackingOffset(
        {}, native_identity, {0, 0, -0.2F});
    bool body_valid = false;
    const auto body_forward = BuildCoJVisualBodyOffset(
        {0, -0.4F, -0.2F}, {1, 0, 0}, {0, 0, 1}, 100, body_valid);
    if (!body_valid || physical_forward.z >= 0 ||
        !NearVector(physical_forward, {body_forward.x, body_forward.y, body_forward.z})) {
        std::cerr << "physical forward camera motion disagreed with visible aim/body space\n";
        return 1;
    }
    // Replay the live natural basis. Native body yaw must be removed once
    // from translation just as it is from arm targets, even after a head turn.
    const auto roomscale_reference = BuildTrackingReferenceBasis(
        {-0.5515F, 0, -0.8342F}, {0, 1, 0}, -39.4842F);
    const auto turned_camera = ApplyCameraTrackingOffset(
        {}, roomscale_reference, {-0.035350F, 0, -0.134281F});
    const auto turned_body = BuildCoJVisualBodyOffset(
        {-0.035350F, -0.401934F, -0.134281F},
        {roomscale_reference.right.x, roomscale_reference.right.y, roomscale_reference.right.z},
        {roomscale_reference.forward.x, roomscale_reference.forward.y, roomscale_reference.forward.z},
        100, body_valid);
    if (!body_valid || !NearVector(
            turned_camera, {turned_body.x, turned_body.y, turned_body.z})) {
        std::cerr << "camera and pelvis diverged after body-owned yaw\n";
        return 1;
    }

    cojvr::runtime::Pose unresolved_actor_pose{};
    unresolved_actor_pose.orientation = AxisAngle(0.0F, 1.0F, 0.0F, 25.0F);
    unresolved_actor_pose.orientation_valid = true;
    unresolved_actor_pose.position = {0.12F, -0.04F, 0.33F};
    unresolved_actor_pose.position_valid = true;
    const auto rotation_only_pose = SuppressPhysicalTrackingTranslation(unresolved_actor_pose);
    if (!rotation_only_pose.orientation_valid || !rotation_only_pose.position_valid ||
        !Near(rotation_only_pose.orientation.x, unresolved_actor_pose.orientation.x) ||
        !Near(rotation_only_pose.orientation.y, unresolved_actor_pose.orientation.y) ||
        !Near(rotation_only_pose.orientation.z, unresolved_actor_pose.orientation.z) ||
        !Near(rotation_only_pose.orientation.w, unresolved_actor_pose.orientation.w) ||
        !Near(rotation_only_pose.position.x, 0.0F) ||
        !Near(rotation_only_pose.position.y, 0.0F) ||
        !Near(rotation_only_pose.position.z, 0.0F)) {
        std::cerr << "rotation-only fallback did not suppress physical translation cleanly\n";
        return 1;
    }

    for (int yaw = -60; yaw <= 60; yaw += 10) {
        for (int pitch = -45; pitch <= 45; pitch += 5) {
            const CameraProbeBasis swept = ApplyCameraProbeOrientation(
                native_identity.forward,
                native_identity.up,
                static_cast<float>(yaw),
                static_cast<float>(pitch));
            const CameraProbeVector native_right = Cross(swept.up, swept.forward);
            if (!IsCameraProbeBasisRigidRightHanded(swept) ||
                !Near(CameraProbeBasisDeterminant(swept), 1.0F, 0.002F) ||
                !NearVector(swept.right, native_right, 0.002F)) {
                std::cerr << "camera orientation sweep violated native right/up/forward contract\n";
                return 1;
            }
        }
    }


    cojvr::runtime::RelativePoseTracker tracker;
    tracker.SetEnabled(true);
    cojvr::runtime::PoseSample base{};
    base.sequence = 1;
    base.pose.orientation = {};
    base.pose.orientation_valid = true;
    base.pose.position = {1.0F, 2.0F, 3.0F};
    base.pose.position_valid = true;
    if (!tracker.Update(base) || tracker.last_recenter_sequence() != 1) {
        std::cerr << "initial HMD pose was not captured as recenter base\n";
        return 1;
    }

    cojvr::runtime::Pose relative{};
    if (!tracker.CurrentPose(relative) || !relative.position_valid ||
        !Near(relative.position.x, 0.0F) || !Near(relative.position.y, 0.0F) ||
        !Near(relative.position.z, 0.0F)) {
        std::cerr << "recentered pose was not available\n";
        return 1;
    }

    // Recenter is a standing-space calibration. Pitch/roll at the instant the
    // user presses recenter must not tilt the tracking coordinate system away
    // from gravity. Returning the head to level at the same yaw should therefore
    // produce a level relative orientation and keep vertical motion vertical.
    cojvr::runtime::RelativePoseTracker tilted_recenter_tracker;
    tilted_recenter_tracker.SetEnabled(true);
    cojvr::runtime::PoseSample tilted_base{};
    tilted_base.sequence = 10;
    const auto yaw_30 = AxisAngle(0.0F, 1.0F, 0.0F, 30.0F);
    tilted_base.pose.orientation = MultiplyQuaternion(
        yaw_30,
        MultiplyQuaternion(
            AxisAngle(1.0F, 0.0F, 0.0F, -25.0F),
            AxisAngle(0.0F, 0.0F, 1.0F, 15.0F)));
    tilted_base.pose.orientation_valid = true;
    tilted_base.pose.position = {4.0F, 1.5F, -2.0F};
    tilted_base.pose.position_valid = true;
    if (!tilted_recenter_tracker.Update(tilted_base)) {
        std::cerr << "tilted recenter pose was rejected\n";
        return 1;
    }
    cojvr::runtime::PoseSample leveled_after_recenter = tilted_base;
    leveled_after_recenter.sequence = 11;
    leveled_after_recenter.pose.orientation = yaw_30;
    leveled_after_recenter.pose.position = {4.0F, 1.7F, -2.0F};
    if (!tilted_recenter_tracker.Update(leveled_after_recenter) ||
        !tilted_recenter_tracker.CurrentPose(relative) ||
        !relative.orientation_valid || !relative.position_valid) {
        std::cerr << "leveled pose after tilted recenter was rejected\n";
        return 1;
    }
    const CameraProbeBasis level_after_tilted_recenter = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (!NearVector(level_after_tilted_recenter.forward, {0.0F, 0.0F, 1.0F}, 0.002F) ||
        !NearVector(level_after_tilted_recenter.up, {0.0F, 1.0F, 0.0F}, 0.002F) ||
        !Near(relative.position.x, 0.0F, 0.002F) ||
        !Near(relative.position.y, 0.20F, 0.002F) ||
        !Near(relative.position.z, 0.0F, 0.002F)) {
        std::cerr << "tilted recenter leaked pitch/roll into relative orientation or gravity axes\n";
        return 1;
    }
    cojvr::runtime::Pose absolute_controller{};
    absolute_controller.orientation = {};
    absolute_controller.orientation_valid = true;
    absolute_controller.position = {1.30F, 1.80F, 2.60F};
    absolute_controller.position_valid = true;
    cojvr::runtime::Pose relative_controller{};
    if (!tracker.TransformPose(absolute_controller, relative_controller) ||
        !relative_controller.orientation_valid || !relative_controller.position_valid ||
        !Near(relative_controller.position.x, 0.30F) ||
        !Near(relative_controller.position.y, -0.20F) ||
        !Near(relative_controller.position.z, -0.40F)) {
        std::cerr << "controller pose did not share the HMD recenter space\n";
        return 1;
    }
    const CameraProbeBasis centered = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (!Near(centered.forward.x, 0.0F) || !Near(centered.forward.y, 0.0F) ||
        !Near(centered.forward.z, 1.0F) || !Near(centered.right.x, 1.0F) ||
        !Near(centered.right.y, 0.0F) || !Near(centered.right.z, 0.0F) ||
        !Near(CameraProbeBasisDeterminant(centered), 1.0F) ||
        !IsCameraProbeBasisRigidRightHanded(centered)) {
        std::cerr << "recenter did not produce identity camera orientation\n";
        return 1;
    }

    const CameraProbeBasis naturally_tilted = ApplyCameraProbeOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, 25.0F, 20.0F);
    const CameraProbeBasis vr_leveled = ApplyCameraPoseOrientation(
        naturally_tilted.forward, naturally_tilted.up, {});
    const float expected_yaw_x = std::sin(25.0F * 3.14159265358979323846F / 180.0F);
    const float expected_yaw_z = std::cos(25.0F * 3.14159265358979323846F / 180.0F);
    if (!Near(vr_leveled.forward.x, expected_yaw_x, 0.002F) ||
        !Near(vr_leveled.forward.y, 0.0F, 0.002F) ||
        !Near(vr_leveled.forward.z, expected_yaw_z, 0.002F) ||
        !NearVector(vr_leveled.up, {0.0F, 1.0F, 0.0F}, 0.002F) ||
        !IsCameraProbeBasisRigidRightHanded(vr_leveled)) {
        std::cerr << "VR recenter inherited natural camera pitch/roll instead of a level yaw base\n";
        return 1;
    }

    cojvr::runtime::PoseSample translated_sample = base;
    translated_sample.sequence = 2;
    translated_sample.pose.position = {1.10F, 2.20F, 2.70F};
    if (!tracker.Update(translated_sample) || !tracker.CurrentPose(relative) ||
        !relative.position_valid || !Near(relative.position.x, 0.10F) ||
        !Near(relative.position.y, 0.20F) || !Near(relative.position.z, -0.30F)) {
        std::cerr << "relative HMD position did not preserve recentered metre axes\n";
        return 1;
    }

    cojvr::runtime::PoseSample yaw_sample = base;
    yaw_sample.sequence = 3;
    yaw_sample.pose.orientation = AxisAngle(0.0F, 1.0F, 0.0F, -20.0F);
    if (!tracker.Update(yaw_sample) || !tracker.CurrentPose(relative)) {
        std::cerr << "yaw HMD pose was rejected\n";
        return 1;
    }
    const CameraProbeBasis hmd_yaw = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    constexpr float kSin20 = 0.3420201433F;
    constexpr float kCos20 = 0.9396926208F;
    if (!Near(hmd_yaw.forward.x, -kSin20) || !Near(hmd_yaw.forward.z, kCos20) ||
        !Near(Length(hmd_yaw.forward), 1.0F) || !Near(Dot(hmd_yaw.forward, hmd_yaw.up), 0.0F) ||
        !Near(CameraProbeBasisDeterminant(hmd_yaw), 1.0F) ||
        !IsCameraProbeBasisRigidRightHanded(hmd_yaw) || hmd_yaw.right.x <= 0.9F) {
        std::cerr << "HMD yaw did not preserve the live-observed CoJ horizontal direction mapping\n";
        return 1;
    }

    const CameraProbeBasis actor_owned_yaw = ApplyCameraProbeOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, -20.0F, 0.0F);
    const CameraProbeBasis stable_tracking_basis = BuildTrackingReferenceBasis(
        actor_owned_yaw.forward,
        actor_owned_yaw.up,
        -20.0F);
    if (!NearVector(stable_tracking_basis.forward, {0.0F, 0.0F, 1.0F}, 0.002F) ||
        !NearVector(stable_tracking_basis.up, {0.0F, 1.0F, 0.0F}, 0.002F) ||
        !NearVector(stable_tracking_basis.right, {1.0F, 0.0F, 0.0F}, 0.002F) ||
        !IsCameraProbeBasisRigidRightHanded(stable_tracking_basis)) {
        std::cerr << "actor-owned yaw leaked into the stable controller/body reference\n";
        return 1;
    }
    const CameraProbeBasis compensated_hmd_yaw = ApplyCameraPoseOrientation(
        actor_owned_yaw.forward,
        actor_owned_yaw.up,
        relative.orientation,
        -20.0F);
    if (!NearVector(compensated_hmd_yaw.forward, actor_owned_yaw.forward, 0.002F) ||
        !NearVector(compensated_hmd_yaw.up, actor_owned_yaw.up, 0.002F) ||
        !IsCameraProbeBasisRigidRightHanded(compensated_hmd_yaw)) {
        std::cerr << "actor-owned HMD yaw was applied to the VR camera a second time\n";
        return 1;
    }

    cojvr::runtime::PoseSample pitch_sample = base;
    pitch_sample.sequence = 4;
    pitch_sample.pose.orientation = AxisAngle(1.0F, 0.0F, 0.0F, 10.0F);
    if (!tracker.Update(pitch_sample) || !tracker.CurrentPose(relative)) {
        std::cerr << "pitch HMD pose was rejected\n";
        return 1;
    }
    const CameraProbeBasis hmd_pitch = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (hmd_pitch.forward.y >= -0.17F || hmd_pitch.forward.z <= 0.98F ||
        !Near(CameraProbeBasisDeterminant(hmd_pitch), 1.0F) ||
        !IsCameraProbeBasisRigidRightHanded(hmd_pitch)) {
        std::cerr << "HMD pitch direction or scale is incorrect\n";
        return 1;
    }

    cojvr::runtime::PoseSample roll_sample = base;
    roll_sample.sequence = 5;
    roll_sample.pose.orientation = AxisAngle(0.0F, 0.0F, -1.0F, 15.0F);
    if (!tracker.Update(roll_sample) || !tracker.CurrentPose(relative)) return 1;
    const CameraProbeBasis hmd_roll = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (!Near(hmd_roll.forward.x, 0.0F) || !Near(hmd_roll.forward.y, 0.0F) ||
        !Near(hmd_roll.forward.z, 1.0F) || hmd_roll.up.x < 0.25F ||
        !Near(hmd_roll.up.y, 0.9659258F) ||
        !Near(hmd_roll.right.x, 0.9659258F) || hmd_roll.right.y > -0.25F ||
        !Near(CameraProbeBasisDeterminant(hmd_roll), 1.0F) ||
        !IsCameraProbeBasisRigidRightHanded(hmd_roll)) {
        std::cerr << "HMD roll did not rotate the native camera basis consistently\n";
        return 1;
    }

    cojvr::runtime::PoseSample origin_again = base;
    origin_again.sequence = 6;
    if (!tracker.Update(origin_again) || !tracker.CurrentPose(relative)) return 1;
    const CameraProbeBasis returned = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (!Near(returned.forward.x, 0.0F) || !Near(returned.forward.y, 0.0F) ||
        !Near(returned.forward.z, 1.0F)) {
        std::cerr << "returning to the physical origin accumulated camera rotation\n";
        return 1;
    }

    tracker.RequestRecenter();
    yaw_sample.sequence = 7;
    if (!tracker.Update(yaw_sample) || !tracker.CurrentPose(relative)) return 1;
    const CameraProbeBasis recentered = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (!Near(recentered.forward.x, 0.0F) || !Near(recentered.forward.z, 1.0F) ||
        !relative.position_valid || tracker.last_recenter_sequence() != 7) {
        std::cerr << "explicit recenter did not reset relative orientation\n";
        return 1;
    }

    cojvr::runtime::PoseSample invalid{};
    invalid.sequence = 8;
    if (tracker.Update(invalid) || tracker.CurrentPose(relative)) {
        std::cerr << "invalid tracking pose did not force passthrough\n";
        return 1;
    }
    tracker.SetEnabled(false);
    if (tracker.CurrentPose(relative)) {
        std::cerr << "disabled tracking did not clear relative orientation\n";
        return 1;
    }

    cojvr::runtime::Pose gaze_pose{};
    gaze_pose.orientation_valid = gaze_pose.position_valid = true;
    gaze_pose.orientation = AxisAngle(0, 1, 0, -20);
    gaze_pose.position = {0.1F, -0.3F, -0.2F};
    const CameraProbeVector camera_position{100, 170, 200};
    const auto gaze = BuildTrackedInteractionRay(camera_position, actor_owned_yaw,
        gaze_pose, -20);
    if (!gaze.valid || !NearVector(gaze.origin, {110, 140, 180}, 0.002F) ||
        !NearVector(gaze.direction, {kSin20, 0, -kCos20}, 0.002F)) {
        std::cerr << "Interaction gaze missed the central translated/crouched HMD camera\n";
        return 1;
    }
    // An invalid pose/basis cannot retain a usable target ray.
    gaze_pose.orientation_valid = false;
    if (BuildTrackedInteractionRay(camera_position, actor_owned_yaw, gaze_pose, -20).valid)
        return 1;
    gaze_pose.orientation_valid = true;
    gaze_pose.position.x = std::numeric_limits<float>::quiet_NaN();
    if (BuildTrackedInteractionRay(camera_position, actor_owned_yaw, gaze_pose, -20).valid)
        return 1;
    gaze_pose.position = {};
    if (BuildTrackedInteractionRay(camera_position, {}, gaze_pose, 0).valid)
        return 1;

    // L1 selection follows the left aim, regardless of HMD orientation. Room-scale
    // collision/height correction shifts both rendered head and hand equally.
    {
    auto tracked_head=cojvr::runtime::Pose{};
    tracked_head.position_valid=tracked_head.orientation_valid=true;
    tracked_head.position={.1F,-.3F,-.2F};
    tracked_head.orientation=AxisAngle(0,1,0,55);
    auto render_head=tracked_head;render_head.position={.05F,-.25F,-.1F};
    auto hand=tracked_head;hand.position={-.2F,-.5F,-.4F};
    hand.orientation=AxisAngle(0,1,0,-20);
    const auto hand_ray=BuildTrackedHandInteractionRay(camera_position,actor_owned_yaw,
        tracked_head,render_head,hand,-20);
    if(!hand_ray.valid || !NearVector(hand_ray.origin,{75,125,170},.002F) ||
        !NearVector(hand_ray.direction,{kSin20,0,-kCos20},.002F)) {
        std::cerr << "L1 interaction did not originate at corrected left aim independently of HMD\n";
        return 1;
    }
    tracked_head.orientation=AxisAngle(1,0,0,35);
    const auto looking_away=BuildTrackedHandInteractionRay(camera_position,actor_owned_yaw,
        tracked_head,render_head,hand,-20);
    if(!looking_away.valid || !NearVector(looking_away.origin,hand_ray.origin,.002F) ||
        !NearVector(looking_away.direction,hand_ray.direction,.002F))return 1;
    hand.position_valid=false;
    if(BuildTrackedHandInteractionRay(camera_position,actor_owned_yaw,
        tracked_head,render_head,hand,-20).valid)return 1;
    hand.position_valid=true;render_head.position.x=std::numeric_limits<float>::quiet_NaN();
    if(BuildTrackedHandInteractionRay(camera_position,actor_owned_yaw,
        tracked_head,render_head,hand,-20).valid)return 1;
    CoJInteractionActionGate action_gate;
    if(action_gate.Update(true,true,true,true)) {
        std::cerr << "L1 acquisition accepted a held action without observed release\n";return 1;
    }
    if(action_gate.Update(true,true,false,false) || !action_gate.Update(true,true,true,true))return 1;
    if(action_gate.Update(false,true,true,true) || action_gate.Update(true,true,true,true)) {
        std::cerr << "lost hand ray allowed held L1 to retarget on recovery\n";return 1;
    }
    if(action_gate.Update(true,false,false,false) || action_gate.Update(true,true,true,true))return 1;
    if(action_gate.Update(true,true,false,false) || !action_gate.Update(true,true,true,true))return 1;
    action_gate.Cancel();
    if(action_gate.Update(true,true,true,true))return 1;
    (void)action_gate.Update(true,true,false,false);
    if(!action_gate.Update(true,true,true,true))return 1;
    }

    std::cout << "PASS - camera controls, stereo frustum/eye mapping, HMD 6DOF and recenter\n";
    return 0;
}
