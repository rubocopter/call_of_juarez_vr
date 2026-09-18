#include "games/call_of_juarez/body_adapter.hpp"
#include "runtime/vr_math.hpp"

#include <algorithm>
#include <cmath>

namespace cojvr::games::call_of_juarez {
namespace {

bool Finite(const cojvr::runtime::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

cojvr::runtime::Vec3 Add(
    const cojvr::runtime::Vec3 left,
    const cojvr::runtime::Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

cojvr::runtime::Vec3 Subtract(
    const cojvr::runtime::Vec3 left,
    const cojvr::runtime::Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

cojvr::runtime::Vec3 Scale(const cojvr::runtime::Vec3 value, const float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

float Dot(const cojvr::runtime::Vec3 left, const cojvr::runtime::Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

cojvr::runtime::Vec3 Cross(
    const cojvr::runtime::Vec3 left,
    const cojvr::runtime::Vec3 right) noexcept {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

float Length(const cojvr::runtime::Vec3 value) noexcept {
    return std::sqrt(Dot(value, value));
}

cojvr::runtime::Vec3 Normalize(
    const cojvr::runtime::Vec3 value,
    const cojvr::runtime::Vec3 fallback = {}) noexcept {
    const float length = Length(value);
    if (!std::isfinite(length) || length <= 1.0e-5F) return fallback;
    return Scale(value, 1.0F / length);
}

cojvr::runtime::Vec3 StablePerpendicular(const cojvr::runtime::Vec3 direction) noexcept {
    const cojvr::runtime::Vec3 axis = std::fabs(direction.y) < 0.9F
        ? cojvr::runtime::Vec3{0.0F, 1.0F, 0.0F}
        : cojvr::runtime::Vec3{1.0F, 0.0F, 0.0F};
    return Normalize(Cross(direction, axis), {0.0F, 0.0F, 1.0F});
}

cojvr::runtime::Vec3 RotateFromTo(
    const cojvr::runtime::Vec3 value,
    const cojvr::runtime::Vec3 from,
    const cojvr::runtime::Vec3 to) noexcept {
    const cojvr::runtime::Vec3 source = Normalize(from);
    const cojvr::runtime::Vec3 target = Normalize(to);
    if (Length(source) <= 1.0e-5F || Length(target) <= 1.0e-5F) return value;

    const float cosine = std::clamp(Dot(source, target), -1.0F, 1.0F);
    cojvr::runtime::Vec3 axis = Cross(source, target);
    const float sine = Length(axis);
    if (sine <= 1.0e-5F) {
        if (cosine > 0.0F) return value;
        axis = StablePerpendicular(source);
        return Subtract(Scale(axis, 2.0F * Dot(axis, value)), value);
    }
    axis = Scale(axis, 1.0F / sine);
    return Add(
        Add(Scale(value, cosine), Scale(Cross(axis, value), sine)),
        Scale(axis, Dot(axis, value) * (1.0F - cosine)));
}

BoneRotationDelta BuildRotationDelta(
    const cojvr::runtime::Vec3 from,
    const cojvr::runtime::Vec3 to,
    const cojvr::runtime::Vec3 preferred_axis) noexcept {
    BoneRotationDelta result{};
    const cojvr::runtime::Vec3 source = Normalize(from);
    const cojvr::runtime::Vec3 target = Normalize(to);
    if (Length(source) <= 1.0e-5F || Length(target) <= 1.0e-5F) return result;

    const float cosine = std::clamp(Dot(source, target), -1.0F, 1.0F);
    cojvr::runtime::Vec3 axis = Cross(source, target);
    const float sine = Length(axis);
    constexpr float kRadiansToDegrees = 57.29577951308232F;
    if (sine <= 1.0e-5F) {
        if (cosine > 0.99999F) {
            result.axis = Normalize(preferred_axis, StablePerpendicular(source));
            result.angle_degrees = 0.0F;
            result.no_op = true;
            result.valid = Finite(result.axis) && Length(result.axis) > 0.99F;
            return result;
        }
        axis = Subtract(preferred_axis, Scale(source, Dot(preferred_axis, source)));
        axis = Normalize(axis, StablePerpendicular(source));
        result.axis = axis;
        result.angle_degrees = 180.0F;
        result.valid = Finite(axis) && Length(axis) > 0.99F;
        return result;
    }

    axis = Scale(axis, 1.0F / sine);
    result.axis = axis;
    result.angle_degrees = std::atan2(sine, cosine) * kRadiansToDegrees;
    result.valid = Finite(axis) && std::isfinite(result.angle_degrees) &&
        Length(axis) > 0.99F && result.angle_degrees >= 0.0F &&
        result.angle_degrees <= 180.0001F;
    return result;
}

ElementWorldBasisTarget BuildBasisTarget(
    const cojvr::runtime::Vec3 position,
    const cojvr::runtime::Vec3 current_segment,
    const cojvr::runtime::Vec3 desired_segment,
    const cojvr::runtime::Vec3 current_up,
    const cojvr::runtime::Vec3 current_forward) noexcept {
    ElementWorldBasisTarget result{};
    if (!Finite(position) || !Finite(current_segment) || !Finite(desired_segment) ||
        !Finite(current_up) || !Finite(current_forward)) {
        return result;
    }
    result.position = position;
    result.up = Normalize(RotateFromTo(current_up, current_segment, desired_segment));
    cojvr::runtime::Vec3 forward =
        RotateFromTo(current_forward, current_segment, desired_segment);
    forward = Subtract(forward, Scale(result.up, Dot(forward, result.up)));
    result.forward = Normalize(forward, StablePerpendicular(result.up));
    result.valid = Finite(result.up) && Finite(result.forward) &&
        Length(result.up) > 0.99F && Length(result.forward) > 0.99F &&
        std::fabs(Dot(result.up, result.forward)) < 0.01F;
    return result;
}

ElementWorldBasisTarget BuildPivotedBasisTarget(
    const cojvr::runtime::Vec3 source_pivot,
    const cojvr::runtime::Vec3 target_pivot,
    const cojvr::runtime::Vec3 element_position,
    const cojvr::runtime::Vec3 current_segment,
    const cojvr::runtime::Vec3 desired_segment,
    const cojvr::runtime::Vec3 current_up,
    const cojvr::runtime::Vec3 current_forward) noexcept {
    ElementWorldBasisTarget result{};
    if (!Finite(source_pivot) || !Finite(target_pivot) || !Finite(element_position) ||
        !Finite(current_segment) || !Finite(desired_segment) || !Finite(current_up) ||
        !Finite(current_forward)) {
        return result;
    }

    const cojvr::runtime::Vec3 native_offset = Subtract(element_position, source_pivot);
    result.position = Add(
        target_pivot,
        RotateFromTo(native_offset, current_segment, desired_segment));
    result.up = Normalize(RotateFromTo(current_up, current_segment, desired_segment));
    cojvr::runtime::Vec3 forward =
        RotateFromTo(current_forward, current_segment, desired_segment);
    forward = Subtract(forward, Scale(result.up, Dot(forward, result.up)));
    result.forward = Normalize(forward, StablePerpendicular(result.up));
    result.valid = Finite(result.position) && Finite(result.up) && Finite(result.forward) &&
        Length(result.up) > 0.99F && Length(result.forward) > 0.99F &&
        std::fabs(Dot(result.up, result.forward)) < 0.01F;
    return result;
}

} // namespace

ArmGeometryRestoreCheck CheckArmGeometryRestored(
    const ArmGeometrySample& natural,
    const ArmGeometrySample& restored) noexcept {
    ArmGeometryRestoreCheck result{};
    const auto distance = [](const cojvr::runtime::Vec3 left,
                             const cojvr::runtime::Vec3 right) noexcept {
        return Length(Subtract(left, right));
    };
    result.max_joint_position_error = std::max(
        distance(natural.elbow, restored.elbow),
        distance(natural.wrist, restored.wrist));
    result.max_element_position_error = std::max(
        distance(natural.upper_element_position, restored.upper_element_position),
        distance(natural.forearm_element_position, restored.forearm_element_position));
    result.max_axis_error = std::max({
        distance(natural.upper_element_up, restored.upper_element_up),
        distance(natural.upper_element_forward, restored.upper_element_forward),
        distance(natural.forearm_element_up, restored.forearm_element_up),
        distance(natural.forearm_element_forward, restored.forearm_element_forward),
    });

    constexpr float kPositionToleranceGameUnits = 0.02F;
    constexpr float kUnitAxisTolerance = 0.001F;
    result.matches =
        std::isfinite(result.max_joint_position_error) &&
        std::isfinite(result.max_element_position_error) &&
        std::isfinite(result.max_axis_error) &&
        result.max_joint_position_error <= kPositionToleranceGameUnits &&
        result.max_element_position_error <= kPositionToleranceGameUnits &&
        result.max_axis_error <= kUnitAxisTolerance;
    return result;
}

SkeletonBinding ExactGameSkeletonBinding() noexcept {
    // EBones.class constants in the shipped code.pak:
    // pelvis=0, spine=1, spine1=2, spine2=3, neck=4, head=5,
    // L upper/forearm/hand=7/8/10, R upper/forearm/hand=12/13/15,
    // thighs=16/17, calves=20/21, feet=22/23.
    return {
        .pelvis = 0,
        .spine = 1,
        .spine1 = 2,
        .chest = 3,
        .neck = 4,
        .head = 5,
        .left_upper_arm = 7,
        .left_forearm = 8,
        .left_hand = 10,
        .right_upper_arm = 12,
        .right_forearm = 13,
        .right_hand = 15,
        .left_thigh = 16,
        .left_shin = 20,
        .left_foot = 22,
        .right_thigh = 17,
        .right_shin = 21,
        .right_foot = 23,
    };
}

UpperBodyTrackingOffsets UpperBodyOffsetsFromHeadPose(
    const cojvr::runtime::Pose& relative_head_pose) noexcept {
    UpperBodyTrackingOffsets result{};
    if (!relative_head_pose.orientation_valid) return result;

    const cojvr::runtime::Vec3 tracked_forward = cojvr::runtime::RotateVector(
        relative_head_pose.orientation, {0.0F, 0.0F, -1.0F});
    if (!std::isfinite(tracked_forward.x) || !std::isfinite(tracked_forward.y) ||
        !std::isfinite(tracked_forward.z)) {
        return result;
    }

    constexpr float kRadiansToDegrees = 57.29577951308232F;
    const float local_x = tracked_forward.x;
    const float local_y = tracked_forward.y;
    const float local_z = -tracked_forward.z;
    const float physical_yaw = std::atan2(local_x, local_z) * kRadiansToDegrees;
    const float physical_pitch =
        std::atan2(local_y, std::sqrt(local_x * local_x + local_z * local_z)) *
        kRadiansToDegrees;
    if (!std::isfinite(physical_yaw) || !std::isfinite(physical_pitch)) return result;

    // The current body gate keeps a human-scale neck range. Body-yaw following
    // will absorb turns beyond this range once actor orientation ownership is
    // live-proven; allowing an unrestricted 180-degree head twist here would
    // only create an invalid intermediate skeleton.
    const float game_yaw = std::clamp(-physical_yaw, -90.0F, 90.0F);
    const float game_pitch = std::clamp(physical_pitch, -60.0F, 90.0F);
    result.head_horizontal_degrees = game_yaw;
    result.spine_horizontal_degrees = game_yaw * (2.0F / 3.0F);
    result.head_vertical_degrees = game_pitch;
    result.valid = true;
    return result;
}

PlayerSpaceReconciliation ReconcilePlayerSpace(
    const cojvr::runtime::Vec3 current_actor_position,
    cojvr::runtime::Vec3 camera_right,
    cojvr::runtime::Vec3 camera_forward,
    const cojvr::runtime::Vec3 relative_head_position,
    const cojvr::runtime::Vec3 player_space_position,
    cojvr::runtime::Vec3 previous_world_offset,
    cojvr::runtime::Vec3 previous_tracking_offset,
    const bool previous_applied,
    const bool recentered,
    const float game_units_per_meter) noexcept {
    PlayerSpaceReconciliation result{};
    const auto finite = [](const cojvr::runtime::Vec3 value) noexcept {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    if (!finite(current_actor_position) || !finite(camera_right) || !finite(camera_forward) ||
        !finite(relative_head_position) || !finite(player_space_position) ||
        !std::isfinite(game_units_per_meter) || game_units_per_meter <= 0.0F) {
        return result;
    }

    // Player/capsule motion is horizontal. Head pitch must never turn a lean
    // into vertical actor motion, so derive a stable XZ basis from the game's
    // natural camera axes.
    camera_right.y = 0.0F;
    camera_forward.y = 0.0F;
    const float right_length = std::sqrt(
        camera_right.x * camera_right.x + camera_right.z * camera_right.z);
    const float forward_length = std::sqrt(
        camera_forward.x * camera_forward.x + camera_forward.z * camera_forward.z);
    if (right_length < 0.0001F || forward_length < 0.0001F) return result;
    camera_right.x /= right_length;
    camera_right.z /= right_length;
    camera_forward.x /= forward_length;
    camera_forward.z /= forward_length;

    if (!previous_applied || recentered) {
        previous_world_offset = {};
        previous_tracking_offset = {};
    }

    const cojvr::runtime::Vec3 tracking_offset{
        player_space_position.x,
        0.0F,
        player_space_position.z,
    };
    const cojvr::runtime::Vec3 world_offset{
        (camera_right.x * tracking_offset.x - camera_forward.x * tracking_offset.z) *
            game_units_per_meter,
        0.0F,
        (camera_right.z * tracking_offset.x - camera_forward.z * tracking_offset.z) *
            game_units_per_meter,
    };

    // Remove the room-scale offset that was already written last frame. What
    // remains is native game locomotion/physics, which must be preserved.
    const cojvr::runtime::Vec3 locomotion_base{
        current_actor_position.x - previous_world_offset.x,
        current_actor_position.y - previous_world_offset.y,
        current_actor_position.z - previous_world_offset.z,
    };
    result.desired_actor_position = {
        locomotion_base.x + world_offset.x,
        locomotion_base.y,
        locomotion_base.z + world_offset.z,
    };
    result.applied_world_offset = world_offset;
    result.applied_tracking_offset = tracking_offset;
    result.render_head_position = {
        relative_head_position.x - previous_tracking_offset.x,
        relative_head_position.y,
        relative_head_position.z - previous_tracking_offset.z,
    };
    result.valid = finite(result.desired_actor_position) && finite(result.render_head_position);
    return result;
}

ArmIkPlan BuildArmIkPlan(
    const ArmGeometrySample& geometry,
    const cojvr::runtime::Vec3 controller_target) noexcept {
    ArmIkPlan result{};
    if (!Finite(geometry.shoulder) || !Finite(geometry.elbow) || !Finite(geometry.wrist) ||
        !Finite(geometry.upper_element_position) || !Finite(geometry.upper_element_up) ||
        !Finite(geometry.upper_element_forward) || !Finite(geometry.forearm_element_position) ||
        !Finite(geometry.forearm_element_up) || !Finite(geometry.forearm_element_forward) ||
        !Finite(controller_target)) {
        return result;
    }

    const cojvr::runtime::Vec3 upper_segment =
        Subtract(geometry.elbow, geometry.shoulder);
    const cojvr::runtime::Vec3 lower_segment =
        Subtract(geometry.wrist, geometry.elbow);
    result.upper_length = Length(upper_segment);
    result.lower_length = Length(lower_segment);
    if (!std::isfinite(result.upper_length) || !std::isfinite(result.lower_length) ||
        result.upper_length <= 0.1F || result.lower_length <= 0.1F) {
        return result;
    }

    const cojvr::runtime::Vec3 target_axis =
        Normalize(Subtract(controller_target, geometry.shoulder));
    if (Length(target_axis) <= 1.0e-5F) return result;
    const cojvr::runtime::Vec3 current_elbow =
        Subtract(geometry.elbow, geometry.shoulder);
    cojvr::runtime::Vec3 pole =
        Subtract(current_elbow, Scale(target_axis, Dot(current_elbow, target_axis)));
    if (Length(pole) <= 1.0e-4F) pole = geometry.upper_element_forward;

    const auto solved = cojvr::runtime::SolveTwoBoneIK(
        geometry.shoulder,
        controller_target,
        pole,
        result.upper_length,
        result.lower_length);
    if (!solved.valid) return result;

    const cojvr::runtime::Vec3 desired_upper =
        Subtract(solved.joint, geometry.shoulder);
    const cojvr::runtime::Vec3 desired_lower = Subtract(solved.end, solved.joint);
    result.upper_arm = BuildPivotedBasisTarget(
        geometry.shoulder,
        geometry.shoulder,
        geometry.upper_element_position,
        upper_segment,
        desired_upper,
        geometry.upper_element_up,
        geometry.upper_element_forward);
    result.forearm = BuildPivotedBasisTarget(
        geometry.elbow,
        solved.joint,
        geometry.forearm_element_position,
        lower_segment,
        desired_lower,
        geometry.forearm_element_up,
        geometry.forearm_element_forward);
    result.elbow_target = solved.joint;
    result.wrist_target = solved.end;
    result.target_clamped = solved.target_clamped;
    result.valid = result.upper_arm.valid && result.forearm.valid;
    return result;
}

ArmBoneRotationPlan BuildArmBoneRotationPlan(
    const ArmGeometrySample& geometry,
    const ArmIkPlan& plan) noexcept {
    ArmBoneRotationPlan result{};
    if (!plan.valid || !Finite(geometry.shoulder) || !Finite(geometry.elbow) ||
        !Finite(geometry.wrist) || !Finite(geometry.upper_element_forward) ||
        !Finite(geometry.forearm_element_forward) || !Finite(plan.elbow_target) ||
        !Finite(plan.wrist_target)) {
        return result;
    }

    const cojvr::runtime::Vec3 natural_upper =
        Subtract(geometry.elbow, geometry.shoulder);
    const cojvr::runtime::Vec3 desired_upper =
        Subtract(plan.elbow_target, geometry.shoulder);
    const cojvr::runtime::Vec3 natural_lower =
        Subtract(geometry.wrist, geometry.elbow);
    const cojvr::runtime::Vec3 desired_lower =
        Subtract(plan.wrist_target, plan.elbow_target);
    if (Length(natural_upper) <= 0.1F || Length(desired_upper) <= 0.1F ||
        Length(natural_lower) <= 0.1F || Length(desired_lower) <= 0.1F) {
        return result;
    }

    result.upper_arm = BuildRotationDelta(
        natural_upper, desired_upper, geometry.upper_element_forward);
    if (!result.upper_arm.valid) return result;

    const cojvr::runtime::Vec3 lower_after_parent = result.upper_arm.no_op
        ? natural_lower
        : RotateFromTo(natural_lower, natural_upper, desired_upper);
    result.forearm = BuildRotationDelta(
        lower_after_parent, desired_lower, geometry.forearm_element_forward);
    result.valid = result.forearm.valid;
    return result;
}

BoneRotationDelta ConvertWorldRotationToElementLocal(
    const BoneRotationDelta& world_rotation,
    const cojvr::runtime::Vec3 element_up,
    const cojvr::runtime::Vec3 element_forward) noexcept {
    BoneRotationDelta result{};
    if (!world_rotation.valid || !Finite(world_rotation.axis) ||
        !std::isfinite(world_rotation.angle_degrees) || !Finite(element_up) ||
        !Finite(element_forward)) {
        return result;
    }

    const cojvr::runtime::Vec3 up = Normalize(element_up);
    cojvr::runtime::Vec3 forward = Subtract(
        element_forward, Scale(up, Dot(element_forward, up)));
    forward = Normalize(forward);
    const cojvr::runtime::Vec3 element_x = Normalize(Cross(up, forward));
    forward = Normalize(Cross(element_x, up));
    if (Length(up) <= 0.99F || Length(forward) <= 0.99F ||
        Length(element_x) <= 0.99F) {
        return result;
    }

    result = world_rotation;
    result.axis = Normalize({
        Dot(world_rotation.axis, element_x),
        Dot(world_rotation.axis, up),
        Dot(world_rotation.axis, forward),
    });
    result.valid = Finite(result.axis) && Length(result.axis) > 0.99F &&
        std::fabs(result.angle_degrees) <= 180.0001F;
    return result;
}

cojvr::runtime::Vec3 BuildTrackedHandTarget(
    const cojvr::runtime::Vec3 head_world_target,
    const cojvr::runtime::Vec3 camera_right,
    const cojvr::runtime::Vec3 camera_up,
    const cojvr::runtime::Vec3 camera_forward,
    const cojvr::runtime::Vec3 tracked_head,
    const cojvr::runtime::Vec3 tracked_hand,
    const float game_units_per_meter,
    bool& valid) noexcept {
    valid = false;
    if (!Finite(head_world_target) || !Finite(camera_right) || !Finite(camera_up) ||
        !Finite(camera_forward) || !Finite(tracked_head) || !Finite(tracked_hand) ||
        !std::isfinite(game_units_per_meter) || game_units_per_meter <= 0.0F) {
        return {};
    }

    const cojvr::runtime::Vec3 right = Normalize(camera_right);
    const cojvr::runtime::Vec3 up = Normalize(camera_up);
    const cojvr::runtime::Vec3 forward = Normalize(camera_forward);
    if (Length(right) <= 0.99F || Length(up) <= 0.99F || Length(forward) <= 0.99F ||
        std::fabs(Dot(right, up)) > 0.01F || std::fabs(Dot(right, forward)) > 0.01F ||
        std::fabs(Dot(up, forward)) > 0.01F) {
        return {};
    }

    const cojvr::runtime::Vec3 tracking_offset = Subtract(tracked_hand, tracked_head);
    const cojvr::runtime::Vec3 game_offset = Scale(tracking_offset, game_units_per_meter);
    const cojvr::runtime::Vec3 target{
        head_world_target.x + right.x * game_offset.x + up.x * game_offset.y +
            forward.x * game_offset.z,
        head_world_target.y + right.y * game_offset.x + up.y * game_offset.y +
            forward.y * game_offset.z,
        head_world_target.z + right.z * game_offset.x + up.z * game_offset.y +
            forward.z * game_offset.z,
    };
    valid = Finite(target);
    return target;
}

PelvisLocomotionAnchor BuildPelvisLocomotionAnchor(
    const cojvr::runtime::Vec3 actor_position,
    const cojvr::runtime::Vec3 pelvis_joint) noexcept {
    PelvisLocomotionAnchor result{};
    if (!Finite(actor_position) || !Finite(pelvis_joint)) return result;

    result.actor_position = actor_position;
    result.pelvis_offset = Subtract(pelvis_joint, actor_position);
    result.world_target = Add(actor_position, result.pelvis_offset);
    result.valid = Finite(result.pelvis_offset) && Finite(result.world_target);
    return result;
}

cojvr::runtime::Vec3 BuildTrackedFootTarget(
    const cojvr::runtime::Vec3 pelvis_world_target,
    const cojvr::runtime::Vec3 camera_right,
    const cojvr::runtime::Vec3 camera_up,
    const cojvr::runtime::Vec3 camera_forward,
    const cojvr::runtime::Vec3 tracked_pelvis,
    const cojvr::runtime::Vec3 tracked_foot,
    const float game_units_per_meter,
    bool& valid) noexcept {
    valid = false;
    if (!Finite(pelvis_world_target) || !Finite(camera_right) || !Finite(camera_up) ||
        !Finite(camera_forward) || !Finite(tracked_pelvis) || !Finite(tracked_foot) ||
        !std::isfinite(game_units_per_meter) || game_units_per_meter <= 0.0F) {
        return {};
    }

    const cojvr::runtime::Vec3 right = Normalize(camera_right);
    const cojvr::runtime::Vec3 up = Normalize(camera_up);
    const cojvr::runtime::Vec3 forward = Normalize(camera_forward);
    if (Length(right) <= 0.99F || Length(up) <= 0.99F || Length(forward) <= 0.99F ||
        std::fabs(Dot(right, up)) > 0.01F || std::fabs(Dot(right, forward)) > 0.01F ||
        std::fabs(Dot(up, forward)) > 0.01F) {
        return {};
    }

    const cojvr::runtime::Vec3 tracking_offset = Subtract(tracked_foot, tracked_pelvis);
    const cojvr::runtime::Vec3 game_offset = Scale(tracking_offset, game_units_per_meter);
    const cojvr::runtime::Vec3 target{
        pelvis_world_target.x + right.x * game_offset.x + up.x * game_offset.y -
            forward.x * game_offset.z,
        pelvis_world_target.y + right.y * game_offset.x + up.y * game_offset.y -
            forward.y * game_offset.z,
        pelvis_world_target.z + right.z * game_offset.x + up.z * game_offset.y -
            forward.z * game_offset.z,
    };
    valid = Finite(target);
    return target;
}

LegIkPlan BuildLegIkPlan(
    const LegGeometrySample& geometry,
    const cojvr::runtime::Vec3 foot_target) noexcept {
    LegIkPlan result{};
    if (!Finite(geometry.hip) || !Finite(geometry.knee) || !Finite(geometry.ankle) ||
        !Finite(geometry.thigh_up) || !Finite(geometry.thigh_forward) ||
        !Finite(geometry.shin_up) || !Finite(geometry.shin_forward) ||
        !Finite(geometry.foot_up) || !Finite(geometry.foot_forward) ||
        !Finite(foot_target)) {
        return result;
    }

    const cojvr::runtime::Vec3 thigh_segment = Subtract(geometry.knee, geometry.hip);
    const cojvr::runtime::Vec3 shin_segment = Subtract(geometry.ankle, geometry.knee);
    result.thigh_length = Length(thigh_segment);
    result.shin_length = Length(shin_segment);
    if (!std::isfinite(result.thigh_length) || !std::isfinite(result.shin_length) ||
        result.thigh_length <= 0.1F || result.shin_length <= 0.1F) {
        return result;
    }

    const cojvr::runtime::Vec3 target_axis = Normalize(Subtract(foot_target, geometry.hip));
    if (Length(target_axis) <= 1.0e-5F) return result;

    const cojvr::runtime::Vec3 current_knee = Subtract(geometry.knee, geometry.hip);
    cojvr::runtime::Vec3 pole =
        Subtract(current_knee, Scale(target_axis, Dot(current_knee, target_axis)));
    if (Length(pole) <= 1.0e-4F) {
        pole = Subtract(
            geometry.thigh_forward,
            Scale(target_axis, Dot(geometry.thigh_forward, target_axis)));
    }
    result.knee_plane_valid = Length(pole) > 1.0e-4F;
    if (!result.knee_plane_valid) pole = StablePerpendicular(target_axis);

    const auto solved = cojvr::runtime::SolveTwoBoneIK(
        geometry.hip,
        foot_target,
        pole,
        result.thigh_length,
        result.shin_length);
    if (!solved.valid) return result;

    const cojvr::runtime::Vec3 desired_thigh = Subtract(solved.joint, geometry.hip);
    const cojvr::runtime::Vec3 desired_shin = Subtract(solved.end, solved.joint);
    result.thigh = BuildBasisTarget(
        geometry.hip,
        thigh_segment,
        desired_thigh,
        geometry.thigh_up,
        geometry.thigh_forward);
    result.shin = BuildBasisTarget(
        solved.joint,
        shin_segment,
        desired_shin,
        geometry.shin_up,
        geometry.shin_forward);

    result.foot.position = solved.end;
    result.foot.up = Normalize(geometry.foot_up);
    cojvr::runtime::Vec3 foot_forward = Subtract(
        geometry.foot_forward,
        Scale(result.foot.up, Dot(geometry.foot_forward, result.foot.up)));
    result.foot.forward = Normalize(foot_forward, StablePerpendicular(result.foot.up));
    result.foot.valid = Finite(result.foot.up) && Finite(result.foot.forward) &&
        Length(result.foot.up) > 0.99F && Length(result.foot.forward) > 0.99F &&
        std::fabs(Dot(result.foot.up, result.foot.forward)) < 0.01F;
    result.ankle_target = solved.end;
    result.target_clamped = solved.target_clamped;
    result.valid = result.thigh.valid && result.shin.valid && result.foot.valid;
    return result;
}

void BodyAdapter::SetSkeletonState(const BodySkeletonState& state) noexcept {
    state_ = state;
}

void BodyAdapter::SetSkeletonBinding(const SkeletonBinding& binding) noexcept {
    binding_ = binding;
}

void BodyAdapter::SetTransformWriter(BoneTransformWriter writer) noexcept {
    writer_ = writer;
}

void BodyAdapter::SetPlayerCapsuleWriter(PlayerCapsuleWriter writer) noexcept {
    player_capsule_writer_ = writer;
}

void BodyAdapter::SetSkeletonDiscovery(SkeletonDiscovery discovery) noexcept {
    discovery_ = discovery;
}

bool BodyAdapter::DiscoverSkeleton() noexcept {
    if (!discovery_ || !state_.actor) return false;

    SkeletonBinding discovered{};
    if (!discovery_(state_.actor, discovered)) return false;

    binding_ = discovered;
    return true;
}

void BodyAdapter::Apply(const cojvr::runtime::IKBodyPose& pose) noexcept {
    if (!state_.available || state_.actor == nullptr || writer_ == nullptr) return;

    // CONFLICT WARNING: This method writes the SAME absolute pose to upper_arm,
    // forearm, and hand bones. This conflicts with the BoneRotate-based IK system
    // in camera_probe.cpp which applies relative hierarchical rotations.
    // Only ONE system should be enabled at a time. See body_adapter.hpp for details.
    const auto write = [this](int bone, const cojvr::runtime::Pose& target) noexcept {
        if (bone >= 0) writer_(state_.actor, bone, target);
    };

    write(binding_.pelvis, pose.pelvis);
    write(binding_.spine, pose.spine);
    write(binding_.chest, pose.chest);
    write(binding_.head, pose.head);
    write(binding_.left_hand, pose.left_arm);
    write(binding_.right_hand, pose.right_arm);
    write(binding_.left_upper_arm, pose.left_arm);
    write(binding_.left_forearm, pose.left_arm);
    write(binding_.right_upper_arm, pose.right_arm);
    write(binding_.right_forearm, pose.right_arm);
    write(binding_.left_thigh, pose.left_leg);
    write(binding_.left_shin, pose.left_leg);
    write(binding_.right_thigh, pose.right_leg);
    write(binding_.right_shin, pose.right_leg);
    write(binding_.left_foot, pose.left_foot);
    write(binding_.right_foot, pose.right_foot);
}

void BodyAdapter::ApplyPlayerSpace(const cojvr::runtime::Pose& player_space) noexcept {
    if (!state_.available || state_.actor == nullptr || player_capsule_writer_ == nullptr) return;
    if (!player_space.position_valid) return;

    // The render camera remains independent. This callback is reserved for the
    // native actor/capsule transform used for physical HMD displacement.
    player_capsule_writer_(state_.actor, player_space);
}

} // namespace cojvr::games::call_of_juarez
