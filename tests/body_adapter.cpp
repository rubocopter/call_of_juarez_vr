#include "games/call_of_juarez/body_adapter.hpp"
#include "games/call_of_juarez/java_player_bridge.hpp"
#include "runtime/body_ik.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>

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

    const SkeletonBinding binding = ExactGameSkeletonBinding();
    if (binding.pelvis != 0 || binding.spine != 1 || binding.spine1 != 2 ||
        binding.chest != 3 || binding.neck != 4 || binding.head != 5 ||
        binding.left_upper_arm != 7 ||
        binding.left_forearm != 8 || binding.left_hand != 10 ||
        binding.right_upper_arm != 12 || binding.right_forearm != 13 ||
        binding.right_hand != 15 || binding.left_thigh != 16 ||
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
        .wrist = {2.0F, 0.0F, 0.0F},
        .upper_up = {0.0F, 1.0F, 0.0F},
        .upper_forward = {0.0F, 0.0F, 1.0F},
        .forearm_up = {0.0F, 1.0F, 0.0F},
        .forearm_forward = {0.0F, 0.0F, 1.0F},
    };
    const ArmIkPlan arm_plan = BuildArmIkPlan(arm_geometry, {1.2F, 0.8F, 0.2F});
    if (!arm_plan.valid || arm_plan.target_clamped ||
        !Near(Distance(arm_plan.upper_arm.position, arm_plan.forearm.position), 1.0F) ||
        !Near(Distance(arm_plan.forearm.position, arm_plan.wrist_target), 1.0F) ||
        !Near(Distance(arm_plan.wrist_target, {1.2F, 0.8F, 0.2F}), 0.0F) ||
        !Near(Distance(arm_plan.upper_arm.up, {}), 1.0F) ||
        !Near(Distance(arm_plan.upper_arm.forward, {}), 1.0F)) {
        std::cerr << "CoJ arm plan did not preserve segment lengths/basis toward controller\n";
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

    // Host tests run without the game's JVM. Discovery and all dependent
    // operations must therefore fail closed without creating or loading one.
    JavaPlayerBridge bridge;
    std::string error;
    JavaPlayerPosition position{};
    JavaPlayerPosition joint{};
    JavaPlayerPosition direction{};
    JavaPlayerPosition perpendicular{};
    int element = -1;
    if (bridge.Refresh(&error) || bridge.player_available() || error.empty() ||
        bridge.TryGetPosition(position) ||
        bridge.TrySetPosition(position) ||
        bridge.TryGetMeshElement(0, element) ||
        bridge.TryGetBoneJointPosition(0, joint) ||
        bridge.TryGetBoneDirection(0, direction) ||
        bridge.TryGetBonePerpendicular(0, perpendicular) ||
        bridge.TrySetElementWorldBasis(0, {}, {}, {}) ||
        bridge.TryApplyUpperBodyTracking(0.0F, 0.0F, 0.0F)) {
        std::cerr << "Java player bridge did not fail closed without the game JVM\n";
        return 1;
    }

    std::cout << "PASS - CoJ bones, IK, player-space reconciliation and JNI fail-closed path\n";
    return 0;
}
