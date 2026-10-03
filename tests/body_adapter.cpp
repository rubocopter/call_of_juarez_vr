#include "games/call_of_juarez/body_adapter.hpp"
#include "games/call_of_juarez/java_player_bridge.hpp"
#include "runtime/body_ik.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

struct WriteRecord {
    int bone = -1;
    cojvr::runtime::Pose pose{};
};

std::array<WriteRecord, 32> g_writes{};
std::size_t g_write_count = 0;

void CaptureWrite(void*, int bone, const cojvr::runtime::Pose& pose) noexcept {
    if (g_write_count < g_writes.size()) {
        g_writes[g_write_count++] = {.bone = bone, .pose = pose};
    }
}

const WriteRecord* FindWrite(int bone) {
    for (std::size_t index = 0; index < g_write_count; ++index) {
        if (g_writes[index].bone == bone) return &g_writes[index];
    }
    return nullptr;
}

bool SamePosition(const cojvr::runtime::Pose& pose, float x, float y, float z) {
    return pose.position_valid && pose.position.x == x && pose.position.y == y &&
        pose.position.z == z;
}

bool Near(const float left, const float right, const float tolerance = 0.01F) {
    return std::fabs(left - right) <= tolerance;
}

cojvr::runtime::Pose HeadPoseAtPhysicalYaw(const float degrees) {
    constexpr float kDegreesToHalfRadians = 0.008726646259971648F;
    const float half_yaw = degrees * kDegreesToHalfRadians;
    cojvr::runtime::Pose pose{};
    pose.orientation = {0.0F, -std::sin(half_yaw), 0.0F, std::cos(half_yaw)};
    pose.orientation_valid = true;
    return pose;
}

bool CheckGradualBodyYawFollow(
    const float initial_head_yaw,
    const float initial_owned_yaw,
    const std::array<float, 3>& turn_endpoints) {
    using cojvr::games::call_of_juarez::BuildBodyYawOwnershipUpdate;
    float head_yaw = initial_head_yaw;
    float owned_yaw = initial_owned_yaw;
    float max_actor_increment = 0.0F;
    for (const float endpoint : turn_endpoints) {
        const float head_increment = endpoint > head_yaw ? 0.5F : -0.5F;
        while (!Near(head_yaw, endpoint)) {
            head_yaw += head_increment;
            const auto update = BuildBodyYawOwnershipUpdate(
                HeadPoseAtPhysicalYaw(head_yaw), owned_yaw, true, false);
            const float actor_increment = std::fabs(update.actor_delta_degrees);
            if (!update.valid || actor_increment > std::fabs(head_increment) + 0.001F) {
                std::cerr << "gradual body yaw leapt at physical yaw " << head_yaw
                          << ": actor increment=" << actor_increment
                          << ", head increment=" << std::fabs(head_increment) << '\n';
                return false;
            }
            // Rendering uses the already committed actor yaw; the new target
            // becomes authoritative only after the current stereo frame.
            if (!Near(update.camera_compensation_degrees, owned_yaw) ||
                !Near(std::remainder(update.actor_target_degrees - owned_yaw, 360.0F),
                    update.actor_delta_degrees) ||
                std::fabs(std::remainder(-head_yaw - update.actor_target_degrees,
                    360.0F)) > 35.001F) {
                std::cerr << "gradual body yaw lost compensation or the comfort boundary\n";
                return false;
            }
            if (actor_increment > max_actor_increment) max_actor_increment = actor_increment;
            owned_yaw = update.actor_target_degrees;
        }
    }
    if (!Near(max_actor_increment, 0.5F, 0.001F)) {
        std::cerr << "wide gradual turns did not continuously advance actor yaw\n";
        return false;
    }
    return true;
}

float Distance(const cojvr::runtime::Vec3 left, const cojvr::runtime::Vec3 right) {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    const float z = left.z - right.z;
    return std::sqrt(x * x + y * y + z * z);
}

} // namespace

int main() {
    using namespace cojvr::games::call_of_juarez;
    using namespace cojvr::runtime;

    CoJPhysicalCrouchState physical_crouch;
    if (physical_crouch.Update(true, false, -0.10F)) {
        std::cerr << "physical crouch engaged above the crouch threshold\n";
        return 1;
    }
    if (!physical_crouch.Update(true, false, -0.27F) ||
        !physical_crouch.Update(true, false, -0.20F)) {
        std::cerr << "physical crouch did not engage/hold through hysteresis\n";
        return 1;
    }
    if (physical_crouch.Update(true, false, -0.14F)) {
        std::cerr << "physical crouch did not release above the release threshold\n";
        return 1;
    }
    if (!physical_crouch.Update(true, false, -0.30F) ||
        physical_crouch.Update(true, true, -0.30F) ||
        physical_crouch.Update(false, false, -0.30F)) {
        std::cerr << "physical crouch did not reset on recenter/tracking loss\n";
        return 1;
    }
    if (!ResolveCoJCrouchAction(false, true) ||
        !ResolveCoJCrouchAction(true, false) ||
        !ResolveCoJCrouchAction(true, true)) {
        std::cerr << "physical head-height crouch did not drive the native crouch action\n";
        return 1;
    }

    const Vec2 head_forward_right = RotateCoJMoveForHeadRelativeYaw({0.0F, 1.0F}, 90.0F);
    const Vec2 head_forward_left = RotateCoJMoveForHeadRelativeYaw({0.0F, 1.0F}, -90.0F);
    const Vec2 head_diagonal = RotateCoJMoveForHeadRelativeYaw({0.6F, 0.8F}, 35.0F);
    const Vec2 inactive_rotation = RotateCoJMoveForHeadRelativeYaw({0.25F, -0.5F}, 0.0F);
    if (!Near(head_forward_right.x, 1.0F, 0.001F) ||
        !Near(head_forward_right.y, 0.0F, 0.001F) ||
        !Near(head_forward_left.x, -1.0F, 0.001F) ||
        !Near(head_forward_left.y, 0.0F, 0.001F) ||
        !Near(std::sqrt(head_diagonal.x * head_diagonal.x +
                        head_diagonal.y * head_diagonal.y), 1.0F, 0.001F) ||
        !Near(inactive_rotation.x, 0.25F) || !Near(inactive_rotation.y, -0.5F)) {
        std::cerr << "head-relative locomotion did not preserve stick magnitude/direction\n";
        return 1;
    }

    if (std::string_view(CoJUiDispatchRouteName(CoJUiDispatchRoute::paused_hint)) !=
            "LawmanModule.m_GameMode.GetHintManager.DisableCurrentHint" ||
        std::string_view(CoJUiDispatchRouteName(CoJUiDispatchRoute::intro_skip)) !=
            "GameWithMenu.sm_cIntroModule.OnInputKey" ||
        std::string_view(CoJUiDispatchRouteName(CoJUiDispatchRoute::active_game_menu)) !=
            "LawmanGame.sm_cActiveGameModule.cMenu.GetCurrentUI.Enter" ||
        std::string_view(CoJUiDispatchRouteName(CoJUiDispatchRoute::active_game_module)) !=
            "LawmanGame.sm_cActiveGameModule.OnInputKey" ||
        std::string_view(CoJUiDispatchRouteName(CoJUiDispatchRoute::loading_ui)) !=
            "GameUILoading.OnInputKey") {
        std::cerr << "exact UI dispatch routes lost their telemetry identities\n";
        return 1;
    }

    const auto ui_back_available = BuildCoJUiBackDispatchPolicy(true);
    const auto ui_back_unavailable = BuildCoJUiBackDispatchPolicy(false);
    if (!ui_back_available.dispatch || ui_back_available.key_code != 1 ||
        !ui_back_available.press || !ui_back_available.release ||
        !ui_back_unavailable.dispatch || ui_back_unavailable.key_code != 1 ||
        !ui_back_unavailable.press || !ui_back_unavailable.release) {
        std::cerr << "UI back stopped providing Escape when no menu UI is open\n";
        return 1;
    }
    if (!ShouldFallbackCoJUiBack(CoJCurrentUiResolution::unavailable) ||
        ShouldFallbackCoJUiBack(CoJCurrentUiResolution::resolved) ||
        ShouldFallbackCoJUiBack(CoJCurrentUiResolution::error)) {
        std::cerr << "UI back fallback no longer fails closed on lookup errors\n";
        return 1;
    }
    if (std::string_view(CoJUiBackDispatchRouteName(CoJUiDispatchRoute::global_menu)) !=
            "GameWithMenu.sm_cMenuModule.GetCurrentUI.CallOnInputKeyGlobal(Escape)" ||
        std::string_view(CoJUiBackDispatchRouteName(CoJUiDispatchRoute::active_game_menu)) !=
            "LawmanGame.sm_cActiveGameModule.cMenu.GetCurrentUI.CallOnInputKeyGlobal(Escape)" ||
        std::string_view(CoJUiBackDispatchRouteName(CoJUiDispatchRoute::active_game_module)) !=
            "LawmanGame.sm_cActiveGameModule.OnInputKey(Escape)") {
        std::cerr << "UI back telemetry no longer names the shipped Escape route\n";
        return 1;
    }

    const auto cursor_without_current_ui = BuildCoJUiPointerDispatchPolicy(true, false);
    const auto cursor_with_current_ui = BuildCoJUiPointerDispatchPolicy(true, true);
    const auto no_menu_cursor = BuildCoJUiPointerDispatchPolicy(false, true);
    if (!cursor_without_current_ui.dispatch_cursor ||
        cursor_without_current_ui.dispatch_process_mouse ||
        !cursor_with_current_ui.dispatch_cursor ||
        !cursor_with_current_ui.dispatch_process_mouse ||
        no_menu_cursor.dispatch_cursor || no_menu_cursor.dispatch_process_mouse) {
        std::cerr << "flat UI cursor still depended on a current GameUserInterface object\n";
        return 1;
    }

    const SkeletonBinding binding = ExactGameSkeletonBinding();
    if (binding.pelvis != 0 || binding.spine != 1 || binding.spine1 != 2 ||
        binding.chest != 3 || binding.neck != 4 || binding.head != 5 ||
        binding.left_upper_arm != 7 ||
        binding.left_forearm != 8 || binding.left_foretwist != 9 ||
        binding.left_hand != 10 ||
        binding.right_upper_arm != 12 || binding.right_forearm != 13 ||
        binding.right_foretwist != 14 || binding.right_hand != 15 ||
        binding.left_thigh != 16 ||
        binding.right_thigh != 17 || binding.left_shin != 20 ||
        binding.right_shin != 21 || binding.left_foot != 22 ||
        binding.right_foot != 23) {
        std::cerr << "Call of Juarez EBones binding does not match code.pak\n";
        return 1;
    }

    Pose identity_head{};
    identity_head.orientation_valid = true;
    const auto identity_offsets = UpperBodyOffsetsFromHeadPose(identity_head);
    if (!identity_offsets.valid || identity_offsets.head_horizontal_degrees != 0.0F ||
        identity_offsets.spine_horizontal_degrees != 0.0F ||
        identity_offsets.head_vertical_degrees != 0.0F) {
        std::cerr << "identity HMD pose produced a body rotation offset\n";
        return 1;
    }

    constexpr float kPi = 3.14159265358979323846F;
    const float yaw_half = 15.0F * kPi / 180.0F;
    Pose right_turn_head{};
    // A -Y tracking-space rotation turns neutral -Z forward toward +X,
    // which HeadAngles/live camera evidence names positive physical yaw.
    right_turn_head.orientation = {0.0F, -std::sin(yaw_half), 0.0F, std::cos(yaw_half)};
    right_turn_head.orientation_valid = true;
    const auto yaw_offsets = UpperBodyOffsetsFromHeadPose(right_turn_head);
    if (!yaw_offsets.valid || !Near(yaw_offsets.head_horizontal_degrees, -30.0F) ||
        !Near(yaw_offsets.spine_horizontal_degrees, -20.0F) ||
        !Near(yaw_offsets.head_vertical_degrees, 0.0F)) {
        std::cerr << "HMD yaw did not map to the live-proven CoJ sign/distribution\n";
        return 1;
    }

    const auto first_body_yaw = BuildBodyYawOwnershipUpdate(
        right_turn_head, 0.0F, false, false);
    if (!first_body_yaw.valid ||
        !Near(first_body_yaw.actor_target_degrees, 0.0F) ||
        !Near(first_body_yaw.actor_delta_degrees, 0.0F) ||
        !Near(first_body_yaw.camera_compensation_degrees, 0.0F) ||
        !Near(first_body_yaw.head_residual_degrees, -30.0F) ||
        !Near(first_body_yaw.spine_residual_degrees, -20.0F)) {
        std::cerr << "body yaw comfort zone rotated the actor during an ordinary head turn\n";
        return 1;
    }

    // Half-degree physical steps cross both sides of the cone, reverse back
    // through free look, and cross HMD/owned-yaw +/-180 wrapping in both senses.
    if (!CheckGradualBodyYawFollow(0.0F, 0.0F, {60.0F, -60.0F, 0.0F}) ||
        !CheckGradualBodyYawFollow(0.0F, 0.0F, {-60.0F, 60.0F, 0.0F}) ||
        !CheckGradualBodyYawFollow(170.0F, -135.0F, {230.0F, 100.0F, 170.0F}) ||
        !CheckGradualBodyYawFollow(-170.0F, 135.0F, {-230.0F, -100.0F, -170.0F})) {
        return 1;
    }

    struct BodyYawCase {
        float physical_yaw;
        float previous_owned;
        bool previous_valid;
        bool recentered;
        float target;
        float delta;
        float compensation;
        float residual;
    };
    // Literal expectations distinguish pre-commit render residuals from the
    // next actor target, including a held pose and return into the free cone.
    constexpr std::array<BodyYawCase, 13> body_yaw_cases{{
        {35.0F, 0.0F, true, false, 0.0F, 0.0F, 0.0F, -35.0F},
        {35.5F, 0.0F, true, false, -0.5F, -0.5F, 0.0F, -35.5F},
        {35.5F, -0.5F, true, false, -0.5F, 0.0F, -0.5F, -35.0F},
        {35.0F, -0.5F, true, false, -0.5F, 0.0F, -0.5F, -34.5F},
        {-34.5F, -0.5F, true, false, -0.5F, 0.0F, -0.5F, 35.0F},
        {-35.0F, -0.5F, true, false, 0.0F, 0.5F, -0.5F, 35.5F},
        {181.0F, -144.0F, true, false, -146.0F, -2.0F, -144.0F, -37.0F},
        {-181.0F, 144.0F, true, false, 146.0F, 2.0F, 144.0F, 37.0F},
        {216.0F, -180.0F, true, false, 179.0F, -1.0F, -180.0F, -36.0F},
        {-216.0F, 180.0F, true, false, -179.0F, 1.0F, 180.0F, 36.0F},
        {170.0F, 225.0F, true, false, -135.0F, 0.0F, -135.0F, -35.0F},
        {45.0F, 123.0F, false, false, -10.0F, -10.0F, 0.0F, -45.0F},
        {216.0F, 179.0F, true, true, 0.0F, 0.0F, 0.0F, 0.0F},
    }};
    for (const auto& sample : body_yaw_cases) {
        const auto update = BuildBodyYawOwnershipUpdate(
            HeadPoseAtPhysicalYaw(sample.physical_yaw), sample.previous_owned,
            sample.previous_valid, sample.recentered);
        if (!update.valid || !Near(update.actor_target_degrees, sample.target) ||
            !Near(update.actor_delta_degrees, sample.delta) ||
            !Near(update.camera_compensation_degrees, sample.compensation) ||
            !Near(update.head_residual_degrees, sample.residual) ||
            !Near(update.spine_residual_degrees, sample.residual * (2.0F / 3.0F))) {
            std::cerr << "body yaw boundary/wrap/recenter case failed at physical yaw "
                      << sample.physical_yaw << '\n';
            return 1;
        }
    }

    const float wide_yaw_half = 22.5F * kPi / 180.0F;
    Pose wide_right_turn_head{};
    wide_right_turn_head.orientation = {
        0.0F, -std::sin(wide_yaw_half), 0.0F, std::cos(wide_yaw_half)};
    wide_right_turn_head.orientation_valid = true;
    const auto wide_body_yaw = BuildBodyYawOwnershipUpdate(
        wide_right_turn_head, 0.0F, false, false);
    if (!wide_body_yaw.valid ||
        !Near(wide_body_yaw.actor_target_degrees, -10.0F) ||
        !Near(wide_body_yaw.actor_delta_degrees, -10.0F) ||
        !Near(wide_body_yaw.camera_compensation_degrees, 0.0F) ||
        !Near(wide_body_yaw.head_residual_degrees, -45.0F)) {
        std::cerr << "wide HMD yaw did not move the actor toward the comfort-zone residual\n";
        return 1;
    }
    const auto settled_body_yaw = BuildBodyYawOwnershipUpdate(
        wide_right_turn_head, wide_body_yaw.actor_target_degrees, true, false);
    if (!settled_body_yaw.valid ||
        !Near(settled_body_yaw.actor_delta_degrees, 0.0F) ||
        !Near(settled_body_yaw.actor_target_degrees, -10.0F) ||
        !Near(settled_body_yaw.camera_compensation_degrees, -10.0F) ||
        !Near(settled_body_yaw.head_residual_degrees, -35.0F) ||
        !Near(settled_body_yaw.spine_residual_degrees, -23.333333F, 0.001F)) {
        std::cerr << "actor-owned HMD yaw was still being double-applied to the upper body\n";
        return 1;
    }
    const auto recentered_body_yaw = BuildBodyYawOwnershipUpdate(
        identity_head, settled_body_yaw.actor_target_degrees, true, true);
    if (!recentered_body_yaw.valid ||
        !Near(recentered_body_yaw.actor_target_degrees, 0.0F) ||
        !Near(recentered_body_yaw.actor_delta_degrees, 0.0F) ||
        !Near(recentered_body_yaw.camera_compensation_degrees, 0.0F) ||
        !Near(recentered_body_yaw.head_residual_degrees, 0.0F)) {
        std::cerr << "recenter did not adopt current actor yaw as the new VR baseline\n";
        return 1;
    }
    if (!CheckGradualBodyYawFollow(0.0F, recentered_body_yaw.actor_target_degrees,
            {60.0F, -60.0F, 0.0F})) {
        std::cerr << "recenter retained old actor ownership during later head turns\n";
        return 1;
    }

    const float pitch_half = 10.0F * kPi / 180.0F;
    Pose up_head{};
    up_head.orientation = {std::sin(pitch_half), 0.0F, 0.0F, std::cos(pitch_half)};
    up_head.orientation_valid = true;
    const auto pitch_offsets = UpperBodyOffsetsFromHeadPose(up_head);
    if (!pitch_offsets.valid || !Near(pitch_offsets.head_horizontal_degrees, 0.0F) ||
        !Near(pitch_offsets.spine_horizontal_degrees, 0.0F) ||
        !Near(pitch_offsets.head_vertical_degrees, 20.0F)) {
        std::cerr << "HMD pitch did not preserve the live-proven CoJ pitch sign\n";
        return 1;
    }

    BodyPose body{};
    body.pelvis.position = {1.0F, 2.0F, 3.0F};
    body.pelvis.position_valid = true;
    body.head.position = {4.0F, 5.0F, 6.0F};
    body.head.position_valid = true;
    body.chest.position = {2.0F, 3.0F, 4.0F};
    body.chest.position_valid = true;
    body.left_foot.position = {0.8F, 1.0F, 3.0F};
    body.left_foot.position_valid = true;
    body.right_foot.position = {1.2F, 1.0F, 3.0F};
    body.right_foot.position_valid = true;

    const IKBodyPose solved = BodyIKSolver{}.Solve(body);
    if (!SamePosition(solved.pelvis, 1.0F, 2.0F, 3.0F) ||
        !SamePosition(solved.head, 4.0F, 5.0F, 6.0F)) {
        std::cerr << "IK solver did not preserve pelvis/head authority\n";
        return 1;
    }

    const auto arm = SolveTwoBoneIK(
        {0.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F},
        1.0F,
        1.0F);
    if (!arm.valid || arm.target_clamped || !Near(arm.joint.x, 0.5F) ||
        !Near(arm.joint.y, 0.8660254F) || !Near(arm.joint.z, 0.0F) ||
        !Near(arm.end.x, 1.0F) || !Near(arm.end.y, 0.0F)) {
        std::cerr << "two-bone IK did not solve the reachable arm target\n";
        return 1;
    }

    const auto extended_arm = SolveTwoBoneIK(
        {0.0F, 0.0F, 0.0F},
        {3.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F},
        1.0F,
        1.0F);
    if (!extended_arm.valid || !extended_arm.target_clamped ||
        extended_arm.end.x < 1.99F || extended_arm.end.x >= 2.0F) {
        std::cerr << "two-bone IK did not clamp an unreachable arm target\n";
        return 1;
    }

    const ArmGeometrySample arm_geometry{
        .shoulder = {0.0F, 0.0F, 0.0F},
        .elbow = {1.0F, 0.0F, 0.0F},
        .wrist = {1.5F, 0.8660254F, 0.0F},
        .upper_element_position = {0.15F, 0.20F, -0.10F},
        .upper_element_up = {0.0F, 1.0F, 0.0F},
        .upper_element_forward = {0.0F, 0.0F, 1.0F},
        .forearm_element_position = {1.20F, -0.10F, 0.05F},
        .forearm_element_up = {0.0F, 1.0F, 0.0F},
        .forearm_element_forward = {0.0F, 0.0F, 1.0F},
        .foretwist_element_position = {1.35F, 0.10F, 0.02F},
        .foretwist_element_up = {0.0F, 1.0F, 0.0F},
        .foretwist_element_forward = {0.0F, 0.0F, 1.0F},
        .hand_element_position = {1.5F, 0.8660254F, 0.0F},
        .hand_element_up = {0.0F, 1.0F, 0.0F},
        .hand_element_forward = {0.0F, 0.0F, 1.0F},
    };
    const ArmIkPlan arm_plan = BuildArmIkPlan(arm_geometry, {1.2F, 0.8F, 0.2F});
    if (!arm_plan.valid || arm_plan.target_clamped ||
        !Near(
            Distance(arm_plan.upper_arm.position, arm_geometry.shoulder),
            Distance(arm_geometry.upper_element_position, arm_geometry.shoulder)) ||
        !Near(
            Distance(arm_plan.forearm.position, arm_plan.elbow_target),
            Distance(arm_geometry.forearm_element_position, arm_geometry.elbow)) ||
        !Near(Distance(arm_plan.wrist_target, {1.2F, 0.8F, 0.2F}), 0.0F) ||
        !Near(Distance(arm_plan.upper_arm.up, {}), 1.0F) ||
        !Near(Distance(arm_plan.upper_arm.forward, {}), 1.0F)) {
        std::cerr << "CoJ arm plan did not preserve native element pivots/basis toward controller\n";
        return 1;
    }

    const ArmIkPlan natural_arm_plan = BuildArmIkPlan(arm_geometry, arm_geometry.wrist);
    if (!natural_arm_plan.valid || natural_arm_plan.target_clamped ||
        !Near(Distance(natural_arm_plan.elbow_target, arm_geometry.elbow), 0.0F) ||
        !Near(
            Distance(natural_arm_plan.upper_arm.position, arm_geometry.upper_element_position),
            0.0F) ||
        !Near(
            Distance(
                natural_arm_plan.forearm.position,
                arm_geometry.forearm_element_position),
            0.0F) ||
        !Near(Distance(natural_arm_plan.upper_arm.up, arm_geometry.upper_element_up), 0.0F) ||
        !Near(
            Distance(
                natural_arm_plan.upper_arm.forward,
                arm_geometry.upper_element_forward),
            0.0F) ||
        !Near(
            Distance(natural_arm_plan.forearm.up, arm_geometry.forearm_element_up),
            0.0F) ||
        !Near(
            Distance(
                natural_arm_plan.forearm.forward,
                arm_geometry.forearm_element_forward),
            0.0F)) {
        std::cerr << "CoJ arm plan changed the native element frame for a no-op IK target\n";
        return 1;
    }

    const ArmIkPlan within_reach_plan = BuildArmIkPlan(arm_geometry, {1.90F, 0.0F, 0.0F});
    if (!within_reach_plan.valid || within_reach_plan.reach_adjusted ||
        within_reach_plan.target_clamped ||
        !Near(within_reach_plan.raw_target_distance, 1.90F, 0.001F) ||
        !Near(within_reach_plan.effective_target_distance, 1.90F, 0.001F)) {
        std::cerr << "arm reach policy changed a reachable controller target\n";
        return 1;
    }
    const ArmIkPlan soft_overreach_plan = BuildArmIkPlan(arm_geometry, {2.05F, 0.0F, 0.0F});
    if (!soft_overreach_plan.valid || soft_overreach_plan.reach_adjusted ||
        !soft_overreach_plan.target_clamped ||
        !Near(soft_overreach_plan.reach_adjustment, 0.0F) ||
        !Near(
            soft_overreach_plan.effective_target_distance,
            soft_overreach_plan.raw_target_distance,
            0.001F)) {
        std::cerr << "arm reach policy shortened a slightly unreachable controller target\n";
        return 1;
    }
    const ArmIkPlan extreme_overreach_plan = BuildArmIkPlan(arm_geometry, {4.0F, 0.0F, 0.0F});
    if (!extreme_overreach_plan.valid || extreme_overreach_plan.reach_adjusted ||
        !extreme_overreach_plan.target_clamped ||
        !Near(extreme_overreach_plan.reach_adjustment, 0.0F) ||
        !Near(
            extreme_overreach_plan.effective_target_distance,
            extreme_overreach_plan.raw_target_distance,
            0.001F)) {
        std::cerr << "arm reach policy shortened an extreme controller target before hard clamp\n";
        return 1;
    }
    if (!ShouldApplyCoJArmIk(within_reach_plan) ||
        !ShouldApplyCoJArmIk(soft_overreach_plan) ||
        !ShouldApplyCoJArmIk(extreme_overreach_plan) ||
        ShouldApplyCoJArmIk(ArmIkPlan{})) {
        std::cerr << "arm write ownership did not preserve valid hard-clamped IK or reject an invalid plan\n";
        return 1;
    }

    const ArmBoneRotationPlan natural_rotations =
        BuildArmBoneRotationPlan(arm_geometry, natural_arm_plan);
    if (!natural_rotations.valid || !natural_rotations.upper_arm.no_op ||
        !natural_rotations.forearm.no_op ||
        !Near(natural_rotations.upper_arm.angle_degrees, 0.0F) ||
        !Near(natural_rotations.forearm.angle_degrees, 0.0F)) {
        std::cerr << "natural arm pose produced a non-zero render-element delta\n";
        return 1;
    }

    // Parent rotation must be accounted for before deriving the forearm delta.
    // A straight +X chain rotated as a whole to +Y needs only the upper-arm
    // rotation; applying another +90 degrees to the forearm would reproduce
    // the reverse/contorted hierarchy seen in the headset.
    const ArmGeometrySample hierarchy_geometry{
        .shoulder = {0.0F, 0.0F, 0.0F},
        .elbow = {1.0F, 0.0F, 0.0F},
        .wrist = {2.0F, 0.0F, 0.0F},
        .upper_element_forward = {0.0F, 0.0F, 1.0F},
        .forearm_element_forward = {0.0F, 0.0F, 1.0F},
    };
    ArmIkPlan hierarchy_plan{};
    hierarchy_plan.elbow_target = {0.0F, 1.0F, 0.0F};
    hierarchy_plan.wrist_target = {0.0F, 2.0F, 0.0F};
    hierarchy_plan.valid = true;
    const ArmBoneRotationPlan hierarchy_rotations =
        BuildArmBoneRotationPlan(hierarchy_geometry, hierarchy_plan);
    if (!hierarchy_rotations.valid || hierarchy_rotations.upper_arm.no_op ||
        !Near(hierarchy_rotations.upper_arm.angle_degrees, 90.0F) ||
        !Near(hierarchy_rotations.upper_arm.axis.x, 0.0F) ||
        !Near(hierarchy_rotations.upper_arm.axis.y, 0.0F) ||
        !Near(hierarchy_rotations.upper_arm.axis.z, 1.0F) ||
        !hierarchy_rotations.forearm.no_op ||
        !Near(hierarchy_rotations.forearm.angle_degrees, 0.0F)) {
        std::cerr << "arm hierarchy rotation double-applied the parent delta\n";
        return 1;
    }

    // Run 20260918T165754Z-845101e7557b proved that the native handler
    // post-multiplies the element matrix: its Java axis is element-local, not
    // world-space. This exact left-upper-arm sample must map the recorded
    // shortest-arc world axis into the recorded live element frame.
    BoneRotationDelta live_world_rotation{};
    live_world_rotation.axis = {-0.967098F, -0.108973F, -0.229883F};
    live_world_rotation.angle_degrees = 110.533F;
    live_world_rotation.valid = true;
    const BoneRotationDelta live_local_rotation =
        ConvertWorldRotationToElementLocal(
            live_world_rotation,
            {0.947983F, 0.289677F, -0.131965F},
            {0.244177F, -0.395781F, 0.885288F});
    if (!live_local_rotation.valid || live_local_rotation.no_op ||
        !Near(live_local_rotation.axis.x, 0.0F, 0.001F) ||
        !Near(live_local_rotation.axis.y, -0.918023F, 0.001F) ||
        !Near(live_local_rotation.axis.z, -0.396526F, 0.001F) ||
        !Near(live_local_rotation.angle_degrees, 110.533F, 0.001F) ||
        Distance(live_local_rotation.axis, live_world_rotation.axis) < 0.5F) {
        std::cerr << "world arm axis was not converted into the live element-local frame\n";
        return 1;
    }

    const BoneRotationDelta invalid_local_rotation =
        ConvertWorldRotationToElementLocal(
            live_world_rotation,
            {0.0F, 1.0F, 0.0F},
            {0.0F, 2.0F, 0.0F});
    if (invalid_local_rotation.valid) {
        std::cerr << "degenerate element basis produced a native arm rotation\n";
        return 1;
    }

    // Around the live campaign's ~39,700-unit world coordinates, adjacent
    // float values are about 0.0039 game units apart. A one-ULP inverse-restore
    // discrepancy must not permanently disable the writer, while a residual
    // visible transform still must fail closed.
    ArmGeometrySample large_world_natural = arm_geometry;
    large_world_natural.elbow = {39700.0F, 3600.0F, 29400.0F};
    large_world_natural.wrist = {39725.0F, 3605.0F, 29410.0F};
    large_world_natural.upper_element_position = {39695.0F, 3602.0F, 29401.0F};
    large_world_natural.forearm_element_position = {39712.0F, 3604.0F, 29406.0F};
    large_world_natural.foretwist_element_position = {39718.0F, 3605.0F, 29408.0F};
    large_world_natural.hand_element_position = {39725.0F, 3605.0F, 29410.0F};

    // An arm transaction captures natural element frames before stereo rendering.
    // If native locomotion moves the actor before restore, writing those captured
    // world positions verbatim leaves the branch behind in world space. Rebase
    // only positions by the actor translation; orientation bases stay natural.
    const Vec3 captured_actor_position{39790.0F, 3538.0F, 29718.0F};
    const Vec3 current_actor_position{39688.0F, 3537.0F, 29681.0F};
    const Vec3 actor_delta{-102.0F, -1.0F, -37.0F};
    const ArmGeometrySample rebased_natural = RebaseArmGeometryForActorTranslation(
        large_world_natural, captured_actor_position, current_actor_position);
    if (!Near(Distance(rebased_natural.shoulder,
                       {large_world_natural.shoulder.x + actor_delta.x,
                        large_world_natural.shoulder.y + actor_delta.y,
                        large_world_natural.shoulder.z + actor_delta.z}), 0.0F) ||
        !Near(Distance(rebased_natural.elbow,
                       {large_world_natural.elbow.x + actor_delta.x,
                        large_world_natural.elbow.y + actor_delta.y,
                        large_world_natural.elbow.z + actor_delta.z}), 0.0F) ||
        !Near(Distance(rebased_natural.wrist,
                       {large_world_natural.wrist.x + actor_delta.x,
                        large_world_natural.wrist.y + actor_delta.y,
                        large_world_natural.wrist.z + actor_delta.z}), 0.0F) ||
        !Near(Distance(rebased_natural.upper_element_position,
                       {large_world_natural.upper_element_position.x + actor_delta.x,
                        large_world_natural.upper_element_position.y + actor_delta.y,
                        large_world_natural.upper_element_position.z + actor_delta.z}), 0.0F) ||
        !Near(Distance(rebased_natural.forearm_element_position,
                       {large_world_natural.forearm_element_position.x + actor_delta.x,
                        large_world_natural.forearm_element_position.y + actor_delta.y,
                        large_world_natural.forearm_element_position.z + actor_delta.z}), 0.0F) ||
        !Near(Distance(rebased_natural.foretwist_element_position,
                       {large_world_natural.foretwist_element_position.x + actor_delta.x,
                        large_world_natural.foretwist_element_position.y + actor_delta.y,
                        large_world_natural.foretwist_element_position.z + actor_delta.z}), 0.0F) ||
        !Near(Distance(rebased_natural.hand_element_position,
                       {large_world_natural.hand_element_position.x + actor_delta.x,
                        large_world_natural.hand_element_position.y + actor_delta.y,
                        large_world_natural.hand_element_position.z + actor_delta.z}), 0.0F) ||
        !Near(Distance(rebased_natural.upper_element_up,
                       large_world_natural.upper_element_up), 0.0F) ||
        !Near(Distance(rebased_natural.upper_element_forward,
                       large_world_natural.upper_element_forward), 0.0F) ||
        !Near(Distance(rebased_natural.hand_element_up,
                       large_world_natural.hand_element_up), 0.0F) ||
        !Near(Distance(rebased_natural.hand_element_forward,
                       large_world_natural.hand_element_forward), 0.0F)) {
        std::cerr << "arm restore geometry did not follow actor translation\n";
        return 1;
    }

    // The exact game can leave the restored arm branch world-fixed into the
    // next frame while native locomotion advances the actor. Detect that
    // continuity break from the complete positional chain and carry the stale
    // natural geometry with the actor before solving the next tracked pose.
    const ArmGeometryContinuityUpdate frozen_arm_update =
        StabilizeArmGeometryAcrossActorMotion(
            large_world_natural,
            current_actor_position,
            large_world_natural,
            captured_actor_position,
            true);
    if (!frozen_arm_update.valid || !frozen_arm_update.rebased_world_fixed ||
        !Near(Distance(frozen_arm_update.geometry.upper_element_position,
                       {large_world_natural.upper_element_position.x + actor_delta.x,
                        large_world_natural.upper_element_position.y + actor_delta.y,
                        large_world_natural.upper_element_position.z + actor_delta.z}), 0.0F) ||
        !Near(Distance(frozen_arm_update.geometry.wrist,
                       {large_world_natural.wrist.x + actor_delta.x,
                        large_world_natural.wrist.y + actor_delta.y,
                        large_world_natural.wrist.z + actor_delta.z}), 0.0F) ||
        !Near(Distance(frozen_arm_update.geometry.hand_element_forward,
                       large_world_natural.hand_element_forward), 0.0F)) {
        std::cerr << "world-fixed arm geometry did not follow actor motion\n";
        return 1;
    }

    const Vec3 next_actor_position{
        current_actor_position.x - 15.0F,
        current_actor_position.y,
        current_actor_position.z + 8.0F};
    const ArmGeometryContinuityUpdate repeated_frozen_arm_update =
        StabilizeArmGeometryAcrossActorMotion(
            frozen_arm_update.geometry,
            next_actor_position,
            frozen_arm_update.geometry,
            current_actor_position,
            true);
    if (!repeated_frozen_arm_update.valid ||
        !repeated_frozen_arm_update.rebased_world_fixed ||
        !Near(Distance(
            repeated_frozen_arm_update.geometry.wrist,
            {frozen_arm_update.geometry.wrist.x - 15.0F,
             frozen_arm_update.geometry.wrist.y,
             frozen_arm_update.geometry.wrist.z + 8.0F}), 0.0F)) {
        std::cerr << "repeated world-fixed arm geometry did not accumulate actor motion\n";
        return 1;
    }

    ArmGeometrySample animated_arm = large_world_natural;
    animated_arm.elbow.x += 2.0F;
    animated_arm.wrist.x += 3.0F;
    animated_arm.forearm_element_position.x += 2.0F;
    animated_arm.foretwist_element_position.x += 2.5F;
    animated_arm.hand_element_position.x += 3.0F;
    const ArmGeometryContinuityUpdate animated_arm_update =
        StabilizeArmGeometryAcrossActorMotion(
            animated_arm,
            current_actor_position,
            large_world_natural,
            captured_actor_position,
            true);
    if (!animated_arm_update.valid || animated_arm_update.rebased_world_fixed ||
        !Near(Distance(animated_arm_update.geometry.wrist, animated_arm.wrist), 0.0F)) {
        std::cerr << "live animated arm geometry was mistaken for a frozen branch\n";
        return 1;
    }
    ArmGeometrySample one_ulp_restored = large_world_natural;
    one_ulp_restored.elbow.x = std::nextafter(
        one_ulp_restored.elbow.x, std::numeric_limits<float>::infinity());
    one_ulp_restored.wrist.z = std::nextafter(
        one_ulp_restored.wrist.z, std::numeric_limits<float>::infinity());
    one_ulp_restored.upper_element_position.x = std::nextafter(
        one_ulp_restored.upper_element_position.x,
        std::numeric_limits<float>::infinity());
    one_ulp_restored.foretwist_element_position.x = std::nextafter(
        one_ulp_restored.foretwist_element_position.x,
        std::numeric_limits<float>::infinity());
    one_ulp_restored.forearm_element_up.x += 0.0002F;
    const ArmGeometryRestoreCheck one_ulp_check = CheckArmGeometryRestored(
        large_world_natural, one_ulp_restored);
    if (!one_ulp_check.matches || one_ulp_check.max_joint_position_error <= 0.001F ||
        one_ulp_check.max_element_position_error <= 0.001F) {
        std::cerr << "float-ULP arm restoration drift was rejected\n";
        return 1;
    }
    ArmGeometrySample residual_rotation = large_world_natural;
    residual_rotation.forearm_element_up.x += 0.01F;
    const ArmGeometryRestoreCheck residual_check = CheckArmGeometryRestored(
        large_world_natural, residual_rotation);
    if (residual_check.matches || residual_check.max_axis_error < 0.009F) {
        std::cerr << "visible residual arm rotation passed restoration validation\n";
        return 1;
    }

    // Mirrored T-pose arms must remain mirrored without introducing an
    // opposite-side correction when the solved target already matches each
    // natural chain.
    ArmGeometrySample left_t_pose = hierarchy_geometry;
    ArmIkPlan left_t_plan{};
    left_t_plan.elbow_target = left_t_pose.elbow;
    left_t_plan.wrist_target = left_t_pose.wrist;
    left_t_plan.valid = true;
    ArmGeometrySample right_t_pose = hierarchy_geometry;
    right_t_pose.elbow = {-1.0F, 0.0F, 0.0F};
    right_t_pose.wrist = {-2.0F, 0.0F, 0.0F};
    ArmIkPlan right_t_plan{};
    right_t_plan.elbow_target = right_t_pose.elbow;
    right_t_plan.wrist_target = right_t_pose.wrist;
    right_t_plan.valid = true;
    const ArmBoneRotationPlan left_t_rotations =
        BuildArmBoneRotationPlan(left_t_pose, left_t_plan);
    const ArmBoneRotationPlan right_t_rotations =
        BuildArmBoneRotationPlan(right_t_pose, right_t_plan);
    if (!left_t_rotations.valid || !right_t_rotations.valid ||
        !left_t_rotations.upper_arm.no_op || !left_t_rotations.forearm.no_op ||
        !right_t_rotations.upper_arm.no_op || !right_t_rotations.forearm.no_op) {
        std::cerr << "mirrored T-pose introduced an unintended arm rotation\n";
        return 1;
    }

    const HandOrientationReference hand_reference = BuildHandOrientationReference(
        {0.0F, 0.0F, 0.0F, 1.0F},
        {1.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F});
    const float half_sqrt = std::sqrt(0.5F);
    const HandOrientationTarget hand_orientation_target =
        BuildTrackedHandOrientationTarget(
            hand_reference,
            {half_sqrt, 0.0F, 0.0F, half_sqrt},
            {1.0F, 0.0F, 0.0F},
            {0.0F, 1.0F, 0.0F},
            {0.0F, 0.0F, 1.0F});
    if (!hand_reference.valid || !hand_orientation_target.valid ||
        !Near(Distance(hand_orientation_target.up, {0.0F, 0.0F, 1.0F}), 0.0F) ||
        !Near(Distance(hand_orientation_target.forward, {0.0F, -1.0F, 0.0F}), 0.0F)) {
        std::cerr << "controller orientation delta did not rotate the calibrated hand basis\n";
        return 1;
    }
    const HandOrientationRotationPlan hand_rotation_plan =
        BuildHandOrientationRotationPlan(
            {1.0F, 0.0F, 0.0F},
            {0.0F, 1.0F, 0.0F},
            {0.0F, 0.0F, 1.0F},
            hand_orientation_target);
    if (!hand_rotation_plan.valid || hand_rotation_plan.forearm_twist.no_op ||
        !Near(hand_rotation_plan.forearm_twist.angle_degrees, 90.0F) ||
        !Near(std::fabs(hand_rotation_plan.forearm_twist.axis.x), 1.0F) ||
        !hand_rotation_plan.hand.no_op ||
        !Near(hand_rotation_plan.hand.angle_degrees, 0.0F)) {
        std::cerr << "controller roll was not split into forearm twist plus residual hand rotation\n";
        return 1;
    }
    // The native FORETWIST hierarchy is authoritative after the twist write.
    // If its propagated hand basis differs from the ideal pre-write estimate,
    // the final residual must be recomputed from that observed basis.
    constexpr float cos_80 = 0.173648178F;
    constexpr float sin_80 = 0.984807753F;
    const BoneRotationDelta observed_residual = BuildHandResidualRotationDelta(
        {0.0F, cos_80, sin_80},
        {0.0F, -sin_80, cos_80},
        hand_orientation_target);
    if (!observed_residual.valid || observed_residual.no_op ||
        !Near(observed_residual.angle_degrees, 10.0F, 0.01F) ||
        !Near(std::fabs(observed_residual.axis.x), 1.0F, 0.001F)) {
        std::cerr << "observed post-FORETWIST hand basis did not produce the expected residual\n";
        return 1;
    }
    BoneRotationDelta excessive_twist{};
    excessive_twist.axis = {1.0F, 0.0F, 0.0F};
    excessive_twist.angle_degrees = 135.0F;
    excessive_twist.valid = true;
    const BoneRotationDelta limited_twist = LimitRotationMagnitude(excessive_twist, 100.0F);
    if (!limited_twist.valid || limited_twist.no_op ||
        !Near(limited_twist.angle_degrees, 100.0F) ||
        !Near(Distance(limited_twist.axis, excessive_twist.axis), 0.0F)) {
        std::cerr << "anatomical forearm twist limit did not preserve axis while bounding angle\n";
        return 1;
    }
    if (!Near(CoJArmControllerTwistLimitDegrees(), 0.0F)) {
        std::cerr << "current CoJ arm experiment still applies controller-driven axial roll\n";
        return 1;
    }
    if (!Near(CoJHandControllerResidualLimitDegrees(), 30.0F)) {
        std::cerr << "CoJ hand residual safety limit changed unexpectedly\n";
        return 1;
    }
    const BoneRotationDelta bounded_observed_residual = BuildSafeCoJHandResidual(
        observed_residual);
    const BoneRotationDelta bounded_excessive_hand = BuildSafeCoJHandResidual(
        excessive_twist);
    if (!bounded_observed_residual.valid || bounded_observed_residual.no_op ||
        !Near(bounded_observed_residual.angle_degrees, 10.0F, 0.01F) ||
        !bounded_excessive_hand.valid || !bounded_excessive_hand.no_op ||
        !Near(bounded_excessive_hand.angle_degrees, 0.0F)) {
        std::cerr << "CoJ hand residual policy did not preserve small corrections and reject implausible ones\n";
        return 1;
    }
    const HandOrientationReference invalid_hand_reference = BuildHandOrientationReference(
        {0.0F, 0.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F});
    if (invalid_hand_reference.valid) {
        std::cerr << "invalid controller orientation produced a hand calibration\n";
        return 1;
    }

    if (!CanRecoverArmWriterAfterRecenter(false, false, false) ||
        CanRecoverArmWriterAfterRecenter(false, true, true) ||
        CanRecoverArmWriterAfterRecenter(true, false, false) ||
        !CanRecoverArmWriterAfterRecenter(true, false, true)) {
        std::cerr << "recenter arm-recovery policy did not preserve fail-closed semantics\n";
        return 1;
    }

    GameplayInputState gameplay{};
    gameplay.active = true;
    gameplay.move = {0.8F, -0.6F};
    gameplay.turn = {-0.5F, 0.0F};
    gameplay.fire_left = true;
    gameplay.fire_right = true;
    gameplay.jump = true;
    gameplay.reload = true;
    gameplay.interact = true;
    gameplay.weapon_previous = true;
    const auto gameplay_actions = BuildCoJGameplayActionValues(gameplay);
    const auto action_value = [&](const int action) {
        for (const auto& item : gameplay_actions) {
            if (item.action == action) return item.value;
        }
        return -1.0F;
    };
    if (!Near(action_value(2), 0.0F) || !Near(action_value(3), 0.0F) ||
        !Near(action_value(4), 0.0F) || !Near(action_value(5), 7.0F / 12.0F) ||
        !Near(action_value(6), 19.0F / 24.0F) || !Near(action_value(7), 0.0F) ||
        !Near(action_value(9), 1.0F) || !Near(action_value(10), 1.0F) ||
        !Near(action_value(11), 1.0F) || !Near(action_value(30), 1.0F) ||
        !Near(action_value(31), 1.0F) || !Near(action_value(47), 1.0F)) {
        std::cerr << "VR gameplay semantics did not map to the exact CoJ action IDs\n";
        return 1;
    }
    CoJSnapTurnState snap_turn{};
    GameplayInputState snap_gameplay{};
    snap_gameplay.active = true;
    snap_gameplay.turn.x = 0.69F;
    if (!Near(snap_turn.Update(snap_gameplay), 0.0F)) {
        std::cerr << "snap turn fired below the engage threshold\n";
        return 1;
    }
    snap_gameplay.turn.x = 0.8F;
    if (!Near(snap_turn.Update(snap_gameplay), 90.0F) ||
        !Near(snap_turn.Update(snap_gameplay), 0.0F)) {
        std::cerr << "right-stick snap turn did not latch at 90 degrees\n";
        return 1;
    }
    snap_gameplay.turn.x = 0.2F;
    if (!Near(snap_turn.Update(snap_gameplay), 0.0F)) {
        std::cerr << "snap turn did not release near stick centre\n";
        return 1;
    }
    snap_gameplay.turn.x = -0.9F;
    if (!Near(snap_turn.Update(snap_gameplay), -90.0F)) {
        std::cerr << "left-stick snap turn did not produce minus 90 degrees\n";
        return 1;
    }
    snap_gameplay.active = false;
    (void)snap_turn.Update(snap_gameplay);
    snap_gameplay.active = true;
    snap_gameplay.turn.x = 0.9F;
    if (!Near(snap_turn.Update(snap_gameplay), 90.0F)) {
        std::cerr << "inactive gameplay did not reset the snap-turn latch\n";
        return 1;
    }

    // Recenter rebasing uses the last visible hand target as the new reference
    // rather than the animated natural hand. At the recenter frame this must
    // preserve the target exactly while accepting the new controller basis.
    const HandOrientationReference recentered_hand_reference =
        BuildHandOrientationReference(
            {0.0F, half_sqrt, 0.0F, half_sqrt},
            {1.0F, 0.0F, 0.0F},
            {0.0F, 1.0F, 0.0F},
            {0.0F, 0.0F, 1.0F},
            hand_orientation_target.up,
            hand_orientation_target.forward);
    const HandOrientationTarget recentered_hand_target =
        BuildTrackedHandOrientationTarget(
            recentered_hand_reference,
            {0.0F, half_sqrt, 0.0F, half_sqrt},
            {1.0F, 0.0F, 0.0F},
            {0.0F, 1.0F, 0.0F},
            {0.0F, 0.0F, 1.0F});
    if (!recentered_hand_target.valid ||
        !Near(Distance(recentered_hand_target.up, hand_orientation_target.up), 0.0F) ||
        !Near(Distance(recentered_hand_target.forward, hand_orientation_target.forward), 0.0F)) {
        std::cerr << "recenter hand-orientation rebase changed the visible target\n";
        return 1;
    }
    GameplayInputState deadzone_gameplay{};
    deadzone_gameplay.active = true;
    deadzone_gameplay.move = {0.03F, -0.03F};
    for (const auto& item : BuildCoJGameplayActionValues(deadzone_gameplay)) {
        if (!Near(item.value, 0.0F)) {
            std::cerr << "VR gameplay stick deadzone leaked into a CoJ action\n";
            return 1;
        }
    }
    GameplayInputState low_speed_gameplay{};
    low_speed_gameplay.active = true;
    low_speed_gameplay.move = {0.0F, 0.20F};
    const auto low_speed_actions = BuildCoJGameplayActionValues(low_speed_gameplay);
    float low_speed_forward = 0.0F;
    for (const auto& item : low_speed_actions) {
        if (item.action == 4) low_speed_forward = item.value;
    }
    if (!Near(low_speed_forward, 1.0F / 6.0F, 0.002F)) {
        std::cerr << "VR locomotion did not reproduce InputAnalog's 0.04 deadzone shaping\n";
        return 1;
    }
    GameplayInputState diagonal_gameplay{};
    diagonal_gameplay.active = true;
    diagonal_gameplay.move = {0.12F, 0.03F};
    const auto diagonal_actions = BuildCoJGameplayActionValues(diagonal_gameplay);
    float diagonal_forward = 0.0F;
    float diagonal_right = 0.0F;
    for (const auto& item : diagonal_actions) {
        if (item.action == 4) diagonal_forward = item.value;
        if (item.action == 6) diagonal_right = item.value;
    }
    if (!Near(diagonal_forward, 0.0F) ||
        !Near(diagonal_right, 1.0F / 12.0F, 0.002F)) {
        std::cerr << "VR locomotion did not preserve InputAnalog's per-axis constraints\n";
        return 1;
    }
    if (!UseDirectAnalogCoJLocomotion(4) || !UseDirectAnalogCoJLocomotion(5) ||
        !UseDirectAnalogCoJLocomotion(6) || !UseDirectAnalogCoJLocomotion(7) ||
        UseDirectAnalogCoJLocomotion(18) || UseDirectAnalogCoJLocomotion(9)) {
        std::cerr << "CoJ VR locomotion did not bypass desktop digital action semantics\n";
        return 1;
    }

    for (const int action : {4, 5, 6, 7, 11, 16, 18}) {
        const auto target = BuildCoJGameplayTargetSelection(action, 1, 3);
        if (!target.valid || target.index != 1) {
            std::cerr << "CoJ gameplay input did not select the shipped action target category\n";
            return 1;
        }
    }
    if (BuildCoJGameplayTargetSelection(4, -1, 3).valid ||
        BuildCoJGameplayTargetSelection(4, 3, 3).valid ||
        BuildCoJGameplayTargetSelection(4, 0, 0).valid) {
        std::cerr << "CoJ gameplay input accepted an out-of-range target category\n";
        return 1;
    }
    if (!ShouldYieldCoJArmIkForNativeReload(true, true) ||
        ShouldYieldCoJArmIkForNativeReload(true, false) ||
        ShouldYieldCoJArmIkForNativeReload(false, true)) {
        std::cerr << "CoJ arm IK reload ownership policy did not fail open on observation loss\n";
        return 1;
    }

    GameplayInputState inactive_gameplay = gameplay;
    inactive_gameplay.active = false;
    for (const auto& item : BuildCoJGameplayActionValues(inactive_gameplay)) {
        if (!Near(item.value, 0.0F)) {
            std::cerr << "inactive VR gameplay state did not release every CoJ action\n";
            return 1;
        }
    }

    CoJLoadingUiInputGate loading_ui_gate{};
    auto blocking_ui = loading_ui_gate.Update(
        true, true, false, false, false);
    if (!blocking_ui.suppress_fire || !blocking_ui.suppress_gameplay) {
        std::cerr << "frozen game timer did not immediately neutralize gameplay input\n";
        return 1;
    }
    loading_ui_gate.Reset();
    auto loading_ui = loading_ui_gate.Update(
        true, false, true, true, false);
    if (loading_ui.dispatch_select || loading_ui.suppress_fire ||
        loading_ui.suppress_gameplay) {
        std::cerr << "ordinary gameplay UI-select was consumed by the loading gate\n";
        return 1;
    }
    loading_ui = loading_ui_gate.Update(true, true, true, true, false);
    if (!loading_ui.dispatch_select || !loading_ui.suppress_fire ||
        !loading_ui.suppress_gameplay) {
        std::cerr << "frozen loading gate did not consume the Sense select press\n";
        return 1;
    }
    loading_ui = loading_ui_gate.Update(true, true, false, true, false);
    if (loading_ui.dispatch_select || !loading_ui.suppress_fire ||
        !loading_ui.suppress_gameplay) {
        std::cerr << "held trigger was not neutralized after loading UI dispatch\n";
        return 1;
    }
    loading_ui = loading_ui_gate.Update(true, false, false, true, false);
    if (loading_ui.dispatch_select || !loading_ui.suppress_fire ||
        loading_ui.suppress_gameplay) {
        std::cerr << "trigger leaked when the game timer resumed before release\n";
        return 1;
    }
    loading_ui = loading_ui_gate.Update(true, false, false, false, false);
    if (loading_ui.dispatch_select || loading_ui.suppress_fire ||
        loading_ui.suppress_gameplay) {
        std::cerr << "loading UI fire suppression did not clear after trigger release\n";
        return 1;
    }

    CoJUiSelectRetryState ui_select_retry{};
    ui_select_retry.Observe(true, true);
    if (!ui_select_retry.ShouldDispatch(true, false)) {
        std::cerr << "UI select edge was not retained for the active pointer\n";
        return 1;
    }
    ui_select_retry.Complete(false);
    ui_select_retry.Observe(false, true);
    if (!ui_select_retry.ShouldDispatch(true, false)) {
        std::cerr << "transient UI unavailability consumed a held select\n";
        return 1;
    }
    ui_select_retry.Complete(true);
    if (ui_select_retry.ShouldDispatch(true, false)) {
        std::cerr << "successful UI select was dispatched more than once\n";
        return 1;
    }
    ui_select_retry.Observe(true, true);
    ui_select_retry.Complete(false);
    ui_select_retry.Observe(false, false);
    if (ui_select_retry.ShouldDispatch(true, false)) {
        std::cerr << "released UI select remained queued after dispatch failure\n";
        return 1;
    }

    bool hand_target_valid = false;
    const Vec3 roomscale_aim_origin = BuildTrackedAimOrigin(
        {100, 105, 300}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
        {-0.035F, -0.4F, -0.13F}, {0.2F, -0.6F, -0.5F},
        50, 100, hand_target_valid);
    if (!hand_target_valid || !Near(Distance(roomscale_aim_origin, {120, 95, 250}), 0)) {
        std::cerr << "aim origin subtracted tracking from an untracked native camera\n";
        return 1;
    }
    (void)BuildTrackedAimOrigin({}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
        {}, {}, std::numeric_limits<float>::quiet_NaN(), 100, hand_target_valid);
    if (hand_target_valid) {
        std::cerr << "aim origin accepted a non-finite view correction\n";
        return 1;
    }
    CoJPhysicalViewHeightState physical_view;
    if (!Near(physical_view.Update(1155, 1000, false, false, true), 0) ||
        !Near(physical_view.Update(1105, 1000, true, false, false), 50) ||
        !Near(physical_view.Update(1205, 1100, true, false, false), 50) ||
        !Near(physical_view.Update(1230, 1100, false, false, false), 25) ||
        !Near(physical_view.Update(1255, 1100, false, false, false), 0)) {
        std::cerr << "physical crouch doubled native camera descent or lost actor vertical motion\n";
        return 1;
    }
    if (!Near(physical_view.Update(1205, 1100, false, true, false), 0) ||
        !Near(physical_view.Update(1205, 1100, false, false, false), 0) ||
        !Near(physical_view.Update(1255, 1100, false, false, false), 0) ||
        !Near(physical_view.Update(1205, 1100, true, false, false), 50) ||
        !Near(physical_view.Update(1205, 1100, true, false, true), 0)) {
        std::cerr << "physical view compensation interfered with explicit crouch/recenter\n";
        return 1;
    }
    if (!Near(physical_view.Update(
            std::numeric_limits<float>::quiet_NaN(), 1100, true, false, false), 0)) {
        std::cerr << "physical view compensation accepted a non-finite native camera\n";
        return 1;
    }
    // Body translation must share the demonstrated skeleton convention (-Z
    // forward), exclude physical height and rotate with the tracking reference.
    bool body_offset_valid = false;
    const Vec3 forward_body = BuildCoJVisualBodyOffset(
        {0.0F, -0.4F, -0.2F}, {1, 0, 0}, {0, 0, 1}, 100.0F, body_offset_valid);
    if (!body_offset_valid || !Near(Distance(forward_body, {0, 0, -20}), 0.0F)) {
        std::cerr << "physical forward body overlay used the camera's opposite Z convention\n";
        return 1;
    }
    const Vec3 turned_body = BuildCoJVisualBodyOffset(
        {0.1F, 0.4F, 0.2F}, {0, 0, -1}, {1, 0, 0}, 100.0F, body_offset_valid);
    if (!body_offset_valid || !Near(Distance(turned_body, {20, 0, -10}), 0.0F)) {
        std::cerr << "backward/lateral body overlay lost tracking yaw or changed height\n";
        return 1;
    }
    (void)BuildCoJVisualBodyOffset(
        {0, 0, 0}, {1, 0, 0}, {1, 0, 0}, 100.0F, body_offset_valid);
    if (body_offset_valid) {
        std::cerr << "visual body overlay accepted a degenerate tracking frame\n";
        return 1;
    }
    const Vec3 crouched_arm = BuildTrackedArmTarget(
        {100, 150, 300}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
        {0.1F, -0.4F, -0.3F}, {0.4F, -0.6F, -0.8F}, 100.0F, hand_target_valid);
    if (!hand_target_valid || !Near(Distance(crouched_arm, {130, 90, 250}), 0.0F)) {
        std::cerr << "crouched arm target cancelled physical controller descent\n";
        return 1;
    }
    const Vec3 tracked_hand_target = BuildTrackedHandTarget(
        {100.0F, 200.0F, 300.0F},
        {1.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        {0.10F, 0.20F, -0.30F},
        {0.40F, 0.00F, -0.80F},
        100.0F,
        hand_target_valid);
    if (!hand_target_valid ||
        !Near(Distance(tracked_hand_target, {130.0F, 180.0F, 250.0F}), 0.0F)) {
        std::cerr << "tracked forward hand did not map to negative native forward\n";
        return 1;
    }

    const Vec3 tracked_hand_behind = BuildTrackedHandTarget(
        {100.0F, 200.0F, 300.0F},
        {1.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        {0.10F, 0.20F, -0.30F},
        {0.40F, 0.00F, 0.20F},
        100.0F,
        hand_target_valid);
    if (!hand_target_valid ||
        !Near(Distance(tracked_hand_behind, {130.0F, 180.0F, 350.0F}), 0.0F)) {
        std::cerr << "tracked backward hand did not map to positive native forward\n";
        return 1;
    }

    bool aim_direction_valid = false;
    const Vec3 aim_direction = BuildTrackedAimDirection(
        {0.0F, 0.0F, 0.0F, 1.0F},
        {1.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        aim_direction_valid);
    if (!aim_direction_valid ||
        !Near(Distance(aim_direction, {0.0F, 0.0F, -1.0F}), 0.0F)) {
        std::cerr << "identity /pose/tip did not map tracking -Z to negative native forward\n";
        return 1;
    }
    (void)BuildTrackedAimDirection(
        {0.0F, 0.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        aim_direction_valid);
    if (aim_direction_valid) {
        std::cerr << "invalid /pose/tip orientation produced a CoJ aim direction\n";
        return 1;
    }

    const TrackedAimPoseDiagnostics aim_pose_diagnostics = BuildTrackedAimPoseDiagnostics(
        {0.0F, 0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, -0.10F});
    if (!aim_pose_diagnostics.valid ||
        !Near(aim_pose_diagnostics.grip_to_tip_distance_m, 0.10F) ||
        !Near(Distance(aim_pose_diagnostics.grip_to_tip_direction, {0.0F, 0.0F, -1.0F}), 0.0F) ||
        !Near(aim_pose_diagnostics.dot_negative_z, 1.0F) ||
        !Near(aim_pose_diagnostics.dot_positive_z, -1.0F) ||
        !Near(aim_pose_diagnostics.dot_positive_x, 0.0F) ||
        !Near(aim_pose_diagnostics.dot_positive_y, 0.0F)) {
        std::cerr << "tip-axis diagnostics did not identify tracking -Z from grip-to-tip geometry\n";
        return 1;
    }

    const PelvisLocomotionAnchor pelvis_anchor = BuildPelvisLocomotionAnchor(
        {100.0F, 20.0F, 200.0F},
        {102.0F, 110.0F, 205.0F});
    if (!pelvis_anchor.valid ||
        !Near(Distance(pelvis_anchor.pelvis_offset, {2.0F, 90.0F, 5.0F}), 0.0F) ||
        !Near(Distance(pelvis_anchor.world_target, {102.0F, 110.0F, 205.0F}), 0.0F)) {
        std::cerr << "pelvis locomotion anchor did not preserve the native actor offset\n";
        return 1;
    }

    bool foot_target_valid = false;
    const Vec3 tracked_foot_target = BuildTrackedFootTarget(
        pelvis_anchor.world_target,
        {1.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        {0.0F, -0.9F, 0.0F},
        {-0.12F, -1.8F, -0.05F},
        100.0F,
        foot_target_valid);
    if (!foot_target_valid ||
        !Near(Distance(tracked_foot_target, {90.0F, 20.0F, 210.0F}), 0.0F)) {
        std::cerr << "tracked foot target was not rooted in the native pelvis basis\n";
        return 1;
    }

    const LegGeometrySample leg_geometry{
        .hip = {0.0F, 2.0F, 0.0F},
        .knee = {0.0F, 1.0F, 0.2F},
        .ankle = {0.0F, 0.0F, 0.0F},
        .thigh_up = {0.0F, -0.9805807F, 0.1961161F},
        .thigh_forward = {0.0F, 0.0F, 1.0F},
        .shin_up = {0.0F, -0.9805807F, -0.1961161F},
        .shin_forward = {0.0F, 0.0F, 1.0F},
        .foot_up = {0.0F, 1.0F, 0.0F},
        .foot_forward = {0.0F, 0.0F, 1.0F},
    };
    const LegIkPlan leg_plan = BuildLegIkPlan(leg_geometry, {0.2F, 0.1F, 0.1F});
    if (!leg_plan.valid || !leg_plan.knee_plane_valid || leg_plan.target_clamped ||
        !Near(Distance(leg_plan.thigh.position, leg_plan.shin.position), leg_plan.thigh_length) ||
        !Near(Distance(leg_plan.shin.position, leg_plan.ankle_target), leg_plan.shin_length) ||
        !Near(Distance(leg_plan.ankle_target, {0.2F, 0.1F, 0.1F}), 0.0F) ||
        !Near(Distance(leg_plan.foot.up, {}), 1.0F) ||
        !Near(Distance(leg_plan.foot.forward, {}), 1.0F)) {
        std::cerr << "CoJ leg plan did not preserve measured lengths/knee plane/foot basis\n";
        return 1;
    }

    const LegIkPlan extended_leg = BuildLegIkPlan(leg_geometry, {0.0F, -5.0F, 0.0F});
    if (!extended_leg.valid || !extended_leg.target_clamped ||
        Distance(extended_leg.thigh.position, extended_leg.ankle_target) >=
            extended_leg.thigh_length + extended_leg.shin_length) {
        std::cerr << "CoJ leg plan did not clamp an unreachable foot target\n";
        return 1;
    }

    int actor = 0;
    BodyAdapter adapter;
    adapter.SetSkeletonState({.available = true, .actor = &actor});
    adapter.SetTransformWriter(&CaptureWrite);
    adapter.Apply(solved);

    const WriteRecord* pelvis = FindWrite(0);
    const WriteRecord* head = FindWrite(5);
    const WriteRecord* left_calf = FindWrite(20);
    const WriteRecord* right_calf = FindWrite(21);
    if (!pelvis || !head || !left_calf || !right_calf ||
        !SamePosition(pelvis->pose, 1.0F, 2.0F, 3.0F) ||
        !SamePosition(head->pose, 4.0F, 5.0F, 6.0F)) {
        std::cerr << "body adapter did not preserve semantic bone targets\n";
        return 1;
    }

    // First room-scale frame: the natural camera has not consumed the actor
    // move yet, so the render keeps the full 10 cm HMD displacement.
    const auto first = ReconcilePlayerSpace(
        {100.0F, 20.0F, 200.0F},
        {1.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        {0.10F, 0.20F, 0.0F},
        {0.10F, 0.0F, 0.0F},
        {}, {}, false, false, 100.0F);
    if (!first.valid || first.desired_actor_position.x != 110.0F ||
        first.desired_actor_position.y != 20.0F ||
        first.render_head_position.x != 0.10F) {
        std::cerr << "initial player-space reconciliation is incorrect\n";
        return 1;
    }

    // Next frame the actor already contains the old 10 cm offset and native
    // locomotion added 2 cm. Moving the HMD to 15 cm must preserve that 2 cm
    // and render only the new 5 cm residual.
    const auto second = ReconcilePlayerSpace(
        {112.0F, 20.0F, 200.0F},
        {1.0F, 0.3F, 0.0F},
        {0.0F, -0.4F, 1.0F},
        {0.15F, 0.25F, 0.0F},
        {0.15F, 0.0F, 0.0F},
        first.applied_world_offset,
        first.applied_tracking_offset,
        true, false, 100.0F);
    if (!second.valid || second.desired_actor_position.x != 117.0F ||
        second.desired_actor_position.y != 20.0F ||
        second.render_head_position.x < 0.049F ||
        second.render_head_position.x > 0.051F ||
        second.render_head_position.y != 0.25F) {
        std::cerr << "player-space reconciliation doubled motion or lost locomotion\n";
        return 1;
    }

    // Recenter establishes the current actor as the new room-scale origin and
    // must not undo an already accepted physical displacement.
    const auto centered_body = ReconcilePlayerSpace(
        second.desired_actor_position,
        {1.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        {}, {},
        second.applied_world_offset,
        second.applied_tracking_offset,
        true, true, 100.0F);
    if (!centered_body.valid ||
        centered_body.desired_actor_position.x != second.desired_actor_position.x ||
        centered_body.desired_actor_position.z != second.desired_actor_position.z) {
        std::cerr << "recenter moved the actor instead of establishing a new origin\n";
        return 1;
    }

    const auto camera_only_roomscale = BuildCollisionSafeRoomScaleTranslation(
        {0.034F, 0.12F, -0.027F});
    if (!camera_only_roomscale.valid || camera_only_roomscale.write_actor_position ||
        !Near(Distance(
            camera_only_roomscale.render_head_position,
            {0.034F, 0.12F, -0.027F}), 0.0F)) {
        std::cerr << "collision-safe room scale did not preserve HMD translation in the camera\n";
        return 1;
    }

    // Native replay of run 327dd proves FORETWIST is NOT under forearm and
    // hand is NOT under FORETWIST. With a 90-degree elbow bend the old twist-
    // only writer leaves the skinning frame behind, even at zero controller roll.
    ArmGeometrySample skin_natural{};
    skin_natural.foretwist_element_up = {0.0F, 1.0F, 0.0F};
    skin_natural.foretwist_element_forward = {0.0F, 0.0F, 1.0F};
    ArmBoneRotationPlan skin_rotations{};
    skin_rotations.valid = true;
    skin_rotations.upper_arm = {{1.0F, 0.0F, 0.0F}, 0.0F, true, true};
    skin_rotations.forearm = {{0.0F, 0.0F, 1.0F}, 90.0F, false, true};
    BoneRotationDelta skin_twist{{0.0F, 1.0F, 0.0F}, 0.0F, true, true};
    auto skin = BuildArmSkinningPlan(skin_natural, skin_rotations, skin_twist,
        {-1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F});
    if (!skin.valid || Distance(skin.foretwist.up, {-1.0F, 0.0F, 0.0F}) > 0.001F ||
        Distance(skin.hand.up, skin.foretwist.up) > 0.001F) {
        std::cerr << "FORETWIST sibling did not receive missing elbow swing\n";
        return 1;
    }
    for (float roll : {-90.0F, 90.0F}) {
        skin_twist.angle_degrees = roll;
        skin_twist.no_op = false;
        skin = BuildArmSkinningPlan(skin_natural, skin_rotations, skin_twist,
            {-1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F});
        const float sign = roll / 90.0F;
        if (!skin.valid || Distance(skin.foretwist.up, {0.0F, 0.0F, sign}) > 0.001F ||
            Distance(skin.foretwist.forward, {sign, 0.0F, 0.0F}) > 0.001F ||
            Distance(skin.hand.up, skin.foretwist.up) > 0.001F ||
            Distance(skin.hand.forward, skin.foretwist.forward) > 0.001F) {
            std::cerr << "Independent hand and skinning frame did not share signed roll\n";
            return 1;
        }
    }
    // A pre-existing native twist must survive the composed swing/roll.
    skin_natural.foretwist_element_up = {0.0F, 0.0F, 1.0F};
    skin_natural.foretwist_element_forward = {0.0F, -1.0F, 0.0F};
    skin_twist = {{0.0F, 1.0F, 0.0F}, 0.0F, true, true};
    skin = BuildArmSkinningPlan(skin_natural, skin_rotations, skin_twist,
        {-1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F});
    if (!skin.valid || Distance(skin.foretwist.up, {0.0F, 0.0F, 1.0F}) > 0.001F ||
        Distance(skin.foretwist.forward, {1.0F, 0.0F, 0.0F}) > 0.001F) {
        std::cerr << "Sibling correction destroyed native bind roll\n";
        return 1;
    }
    skin_rotations.forearm.axis.x = std::numeric_limits<float>::quiet_NaN();
    if (BuildArmSkinningPlan(skin_natural, skin_rotations, skin_twist, {}, {}).valid) {
        std::cerr << "Invalid sibling rotation accepted\n";
        return 1;
    }

    // The active CoJ arm overlay is written as complete world frames. A bent
    // lower arm must carry the sibling FORETWIST pivot to the solved elbow and
    // the independently parented hand pivot to the solved wrist without relying
    // on native child propagation.
    ArmGeometrySample frame_natural{};
    frame_natural.shoulder = {0.0F, 0.0F, 0.0F};
    frame_natural.elbow = {0.0F, 1.0F, 0.0F};
    frame_natural.wrist = {0.0F, 2.0F, 0.0F};
    frame_natural.foretwist_element_position = frame_natural.elbow;
    frame_natural.foretwist_element_up = {1.0F, 0.0F, 0.0F};
    frame_natural.foretwist_element_forward = {0.0F, 0.0F, 1.0F};
    frame_natural.hand_element_position = frame_natural.wrist;
    frame_natural.hand_element_up = {1.0F, 0.0F, 0.0F};
    frame_natural.hand_element_forward = {0.0F, 0.0F, 1.0F};
    frame_natural.forearm_element_up = {1.0F, 0.0F, 0.0F};
    frame_natural.forearm_element_forward = {0.0F, 0.0F, 1.0F};
    ArmIkPlan frame_ik{};
    frame_ik.upper_arm = {{0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F},
                          {0.0F, 0.0F, 1.0F}, true};
    frame_ik.forearm = {{0.0F, 1.0F, 0.0F}, {-1.0F, 0.0F, 0.0F},
                        {0.0F, 0.0F, 1.0F}, true};
    frame_ik.elbow_target = {0.0F, 1.0F, 0.0F};
    frame_ik.wrist_target = {-1.0F, 1.0F, 0.0F};
    frame_ik.valid = true;
    ArmBoneRotationPlan frame_rotations{};
    frame_rotations.upper_arm = {{1.0F, 0.0F, 0.0F}, 0.0F, true, true};
    frame_rotations.forearm = {{0.0F, 0.0F, 1.0F}, 90.0F, false, true};
    frame_rotations.valid = true;
    HandOrientationTarget frame_hand_target{};
    frame_hand_target.up = {0.0F, 1.0F, 0.0F};
    frame_hand_target.forward = {0.0F, 0.0F, 1.0F};
    frame_hand_target.valid = true;
    const auto frame_plan = BuildArmElementFramePlan(
        frame_natural, frame_ik, frame_rotations, frame_hand_target);
    if (!frame_plan.valid ||
        Distance(frame_plan.foretwist.position, frame_ik.elbow_target) > 0.001F ||
        Distance(frame_plan.hand.position, frame_ik.wrist_target) > 0.001F ||
        Distance(frame_plan.foretwist.up, {0.0F, 1.0F, 0.0F}) > 0.001F ||
        Distance(frame_plan.foretwist.forward, {0.0F, 0.0F, 1.0F}) > 0.001F ||
        Distance(frame_plan.hand.up, frame_hand_target.up) > 0.001F ||
        Distance(frame_plan.hand.forward, frame_hand_target.forward) > 0.001F) {
        std::cerr << "Absolute arm frame plan did not preserve solved pivots/bases\n";
        return 1;
    }

    // A direct lower-segment rotation and a parent+elbow composition reach
    // the same wrist but differ in axial roll. Every lower skinning element
    // must use the latter, or the mesh twists despite correct endpoints.
    frame_natural.upper_element_up = {1, 0, 0};
    frame_natural.upper_element_forward = {0, 0, 1};
    frame_ik.upper_arm = {{0, 0, 0}, {0, -1, 0}, {0, 0, 1}, true};
    frame_ik.forearm = {{1, 0, 0}, {1, 0, 0}, {0, -1, 0}, true};
    frame_ik.elbow_target = {1, 0, 0};
    frame_ik.wrist_target = {1, 0, 1};
    const auto composed_rotations = BuildArmBoneRotationPlan(frame_natural, frame_ik);
    const HandOrientationTarget composed_hand{{0, -1, 0}, {-1, 0, 0}, true};
    const auto composed_frame = BuildArmElementFramePlan(
        frame_natural, frame_ik, composed_rotations, composed_hand);
    if (!composed_frame.valid ||
        Distance(composed_frame.forearm.up, {0, -1, 0}) > 0.001F ||
        Distance(composed_frame.forearm.forward, {-1, 0, 0}) > 0.001F ||
        Distance(composed_frame.foretwist.up, {0, -1, 0}) > 0.001F ||
        Distance(composed_frame.hand.up, {0, -1, 0}) > 0.001F ||
        Distance(composed_frame.hand.position, {1, 0, 1}) > 0.001F) {
        std::cerr << "forearm used a different axial frame from FORETWIST and hand\n";
        return 1;
    }

    // Positional IK alone must not create a wrist residual on calibration.
    const auto solved_reference = BuildArmHandOrientationReference(
        {}, {1,0,0}, {0,1,0}, {0,0,1}, frame_natural, composed_rotations);
    const auto solved_hand_target = BuildTrackedHandOrientationTarget(
        solved_reference, {}, {1,0,0}, {0,1,0}, {0,0,1});
    const auto initially_calibrated = BuildArmElementFramePlan(
        frame_natural, frame_ik, composed_rotations, solved_hand_target);
    if (!solved_reference.valid || !initially_calibrated.valid ||
        !initially_calibrated.diagnostic_hand_residual.no_op ||
        Distance(initially_calibrated.hand.up, composed_hand.up) > 0.001F ||
        Distance(initially_calibrated.hand.forward, composed_hand.forward) > 0.001F) {
        std::cerr << "Unchanged controller required wrist compensation for the initial positional IK swing\n";
        return 1;
    }
    auto invalid_reference_rotations = composed_rotations;
    invalid_reference_rotations.forearm.valid = false;
    if (BuildArmHandOrientationReference({}, {1,0,0}, {0,1,0}, {0,0,1},
            frame_natural, invalid_reference_rotations).valid) {
        std::cerr << "Invalid positional swing calibrated the hand reference\n";
        return 1;
    }
    CoJArmSpanCalibration span_calibration;
    for (int i = 0; i < 40; ++i) {
        (void)span_calibration.Update({}, {-0.2F,-0.3F,-0.6F}, {0.2F,-0.3F,-0.6F}, 40, 100, true);
    }
    if (span_calibration.calibrated()) {
        std::cerr << "Ordinary aiming calibrated arm span\n"; return 1;
    }
    float fitted_scale = 1;
    for (int i = 0; i < 30; ++i) {
        fitted_scale = span_calibration.Update({}, {-0.85F,-0.25F,0}, {0.85F,-0.25F,0}, 40, 100, true);
    }
    if (!span_calibration.calibrated() || !Near(fitted_scale, 1.3F)) {
        std::cerr << "Stable measured span did not fit native arm ratios\n"; return 1;
    }
    const auto fitted_arm = BuildArmIkPlan(arm_geometry, {2.4F,0,0}, {}, fitted_scale);
    const auto fitted_rotation = BuildArmBoneRotationPlan(arm_geometry, fitted_arm);
    const auto fitted_frames = BuildArmElementFramePlan(arm_geometry, fitted_arm,
        fitted_rotation, {arm_geometry.hand_element_up, arm_geometry.hand_element_forward, true});
    if (!fitted_arm.valid || fitted_arm.target_clamped || !fitted_frames.valid ||
        Distance(fitted_frames.hand.position, fitted_arm.wrist_target) > 0.001F ||
        !Near(Distance(fitted_frames.foretwist.position, fitted_arm.elbow_target),
            Distance(arm_geometry.foretwist_element_position, arm_geometry.elbow))) {
        std::cerr << "Measured reach did not carry the skinning pivots to the longer chain\n"; return 1;
    }
    span_calibration.Reset();
    if (span_calibration.calibrated()) { std::cerr << "Span calibration survived generation reset\n"; return 1; }

    // The rejected headset T pose had a visor/shoulder-plane offset and was
    // later repeated after turning. Extension must not depend on recenter yaw.
    for (float yaw : {0.0F, 0.7F, 1.57F, 2.8F}) {
        CoJArmSpanCalibration observed_span;
        const auto rotate_yaw = [yaw](cojvr::runtime::Vec3 p) {
            return cojvr::runtime::Vec3{p.x*std::cos(yaw)+p.z*std::sin(yaw), p.y,
                -p.x*std::sin(yaw)+p.z*std::cos(yaw)};
        };
        const cojvr::runtime::Vec3 observed_left{-0.847036F,-0.130028F,0.301085F};
        const cojvr::runtime::Vec3 observed_right{0.805296F,-0.064709F,0.111916F};
        float observed_fit = 1;
        for (int i = 0; i < 30; ++i) {
            observed_fit = observed_span.Update({}, rotate_yaw(observed_left),
                rotate_yaw(observed_right), 26.4F, 100.0F, true);
        }
        const float expected_fit = (Distance(observed_left, observed_right)*100-26.4F)/100;
        if (!observed_span.calibrated() || !Near(observed_fit, expected_fit)) {
            std::cerr << "Extended bilateral span rejected visor offset or recenter yaw\n";
            return 1;
        }
    }
    for (int invalid_pose = 0; invalid_pose < 3; ++invalid_pose) {
        CoJArmSpanCalibration invalid_span;
        for (int i = 0; i < 40; ++i) {
            const auto left = invalid_pose == 0 ? cojvr::runtime::Vec3{-0.85F,0,-1}
                : cojvr::runtime::Vec3{-0.85F,0,0};
            const auto right = invalid_pose == 0 ? cojvr::runtime::Vec3{0.85F,0,-1}
                : cojvr::runtime::Vec3{0.85F,1,0};
            (void)invalid_span.Update({}, left, right, 40, 100, invalid_pose != 2);
        }
        if (invalid_span.calibrated()) {
            std::cerr << "Forward, nonhorizontal or lost-tracking pose calibrated span\n";
            return 1;
        }
    }

    // without collapsing its measured wrist-to-muzzle distance.
    const auto weapon_plan = BuildTrackedWeaponFrame(
        {{2, 0, 0}, {0, 1, 0}, {0, 0, 1}, true},
        {0, 0, 0}, {-20, 0, 0}, {-1, 0, 0},
        {10, 30, 40}, {0, 0, -1}, {0, 1, 0});
    if (!weapon_plan.valid ||
        Distance(weapon_plan.muzzle_origin, {10, 30, 20}) > 0.001F ||
        Distance(weapon_plan.muzzle_direction, {0, 0, -1}) > 0.001F ||
        Distance(weapon_plan.root.position, {10, 30, 42}) > 0.001F ||
        !Near(Distance(weapon_plan.root.position, {10, 30, 40}), 2.0F) ||
        BuildTrackedWeaponFrame({}, {}, {}, {}, {}, {}, {}).valid) {
        std::cerr << "Weapon rigid mapping changed grip/muzzle geometry or accepted invalid input\n";
        return 1;
    }
    auto changed_animation = arm_geometry;
    changed_animation.elbow = {0, 1, 0};
    changed_animation.wrist = {1, 1, 0};
    const auto stable_a = BuildArmIkPlan(arm_geometry, {1, 0, 0}, {0, -1, 0});
    const auto stable_b = BuildArmIkPlan(changed_animation, {1, 0, 0}, {0, -1, 0});
    if (!stable_a.valid || !stable_b.valid ||
        Distance(stable_a.elbow_target, stable_b.elbow_target) > 0.001F) {
        std::cerr << "Native animation changed the tracked elbow plane\n";
        return 1;
    }
    const ElementWorldBasisTarget weapon_element{{-6, 4, 2}, {0, 0, 1}, {1, 0, 0}, true};
    const auto moved_element = TransformWeaponElementFrame(
        {{2,0,0},{0,1,0},{0,0,1},true}, weapon_plan.root, weapon_element);
    const auto restored_element = TransformWeaponElementFrame(
        weapon_plan.root, {{2,0,0},{0,1,0},{0,0,1},true}, moved_element);
    if (!moved_element.valid || !restored_element.valid ||
        Distance(restored_element.position, weapon_element.position) > 0.001F ||
        Distance(restored_element.up, weapon_element.up) > 0.001F ||
        Distance(restored_element.forward, weapon_element.forward) > 0.001F) {
        std::cerr << "Weapon rigid map/restore changed an animated child element\n";
        return 1;
    }

    // Host tests run without the game's JVM. Discovery and all dependent
    // operations must therefore fail closed without creating or loading one.
    JavaPlayerBridge bridge;
    std::string error;
    JavaPlayerPosition position{};
    JavaPlayerPosition joint{};
    JavaPlayerPosition direction{};
    JavaPlayerPosition perpendicular{};
    JavaPlayerPosition element_position{};
    JavaPlayerPosition element_up{};
    JavaPlayerPosition element_forward{};
    JavaPlayerPosition barrel_origin{}, barrel_direction{};
    CoJWeaponAttackOrigins attack_origins{{1, 2, 3}, {4, 5, 6}, true, true};
    if (bridge.TryGetWeaponBarrel(-1, barrel_origin, barrel_direction) ||
        bridge.TryGetWeaponBarrel(0, barrel_origin, barrel_direction, nullptr, &attack_origins) ||
        attack_origins.ballistic_valid || attack_origins.visual_valid ||
        attack_origins.ballistic_origin.x != 0 || attack_origins.visual_origin.x != 0) {
        std::cerr << "weapon barrel observation did not fail closed without a player JVM\n";
        return 1;
    }
    bool game_timer_frozen = false;
    int element = -1;
    if (bridge.Refresh(&error) || bridge.player_available() ||
        bridge.single_player_fallback_used() || bridge.campaign_module_fallback_used() ||
        error.empty() ||
        bridge.TryGetPosition(position) ||
        bridge.TrySetPosition(position) ||
        bridge.TryGetMeshElement(0, element) ||
        bridge.TryGetBoneJointPosition(0, joint) ||
        bridge.TryGetBoneDirection(0, direction) ||
        bridge.TryGetBonePerpendicular(0, perpendicular) ||
        bridge.TryGetElementWorldBasis(
            0, element_position, element_up, element_forward) ||
        bridge.TrySetElementWorldBasis(0, {}, {}, {}) ||
        bridge.TryRotateElementWithChildren(0, {0.0F, 1.0F, 0.0F}, 10.0F) ||
        bridge.TryRotateHorizontally(15.0F) ||
        bridge.TryGetActiveGameTimerFrozen(game_timer_frozen) ||
        bridge.TryProcessUiPointer(0.0F, 0.0F) ||
        bridge.TrySetPerHandAimOrigin(0, {1.0F, 2.0F, 3.0F}) ||
        bridge.TryApplyUpperBodyTracking(0.0F, 0.0F, 0.0F)) {
        std::cerr << "Java player bridge did not fail closed without the game JVM\n";
        return 1;
    }

    std::cout << "PASS - CoJ bones, IK, player-space reconciliation and JNI fail-closed path\n";
    return 0;
}
