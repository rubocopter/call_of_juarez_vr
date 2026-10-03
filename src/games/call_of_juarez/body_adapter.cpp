#include "games/call_of_juarez/body_adapter.hpp"
#include "runtime/vr_math.hpp"

#include <algorithm>
#include <array>
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

bool NormalizeBasis(
    cojvr::runtime::Vec3& up,
    cojvr::runtime::Vec3& forward) noexcept {
    up = Normalize(up);
    if (Length(up) <= 0.99F) return false;
    forward = Subtract(forward, Scale(up, Dot(forward, up)));
    forward = Normalize(forward);
    if (Length(forward) <= 0.99F) return false;
    const cojvr::runtime::Vec3 x_axis = Normalize(Cross(up, forward));
    if (Length(x_axis) <= 0.99F) return false;
    forward = Normalize(Cross(x_axis, up));
    return Finite(up) && Finite(forward) &&
        std::fabs(Dot(up, forward)) <= 0.001F;
}

bool NormalizeCameraBasis(
    cojvr::runtime::Vec3& right,
    cojvr::runtime::Vec3& up,
    cojvr::runtime::Vec3& forward) noexcept {
    right = Normalize(right);
    up = Normalize(up);
    forward = Normalize(forward);
    return Finite(right) && Finite(up) && Finite(forward) &&
        Length(right) > 0.99F && Length(up) > 0.99F && Length(forward) > 0.99F &&
        std::fabs(Dot(right, up)) < 0.01F &&
        std::fabs(Dot(right, forward)) < 0.01F &&
        std::fabs(Dot(up, forward)) < 0.01F;
}

bool NormalizeQuaternionChecked(
    const cojvr::runtime::Quaternion input,
    cojvr::runtime::Quaternion& output) noexcept {
    const float length_squared = input.x * input.x + input.y * input.y +
        input.z * input.z + input.w * input.w;
    if (!std::isfinite(length_squared) || length_squared <= 1.0e-10F) return false;
    output = cojvr::runtime::NormalizeQuaternion(input);
    return std::isfinite(output.x) && std::isfinite(output.y) &&
        std::isfinite(output.z) && std::isfinite(output.w);
}

cojvr::runtime::Quaternion Conjugate(
    const cojvr::runtime::Quaternion value) noexcept {
    return {-value.x, -value.y, -value.z, value.w};
}

cojvr::runtime::Quaternion Multiply(
    const cojvr::runtime::Quaternion left,
    const cojvr::runtime::Quaternion right) noexcept {
    return {
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
    };
}

cojvr::runtime::Vec3 WorldToBasis(
    const cojvr::runtime::Vec3 value,
    const cojvr::runtime::Vec3 right,
    const cojvr::runtime::Vec3 up,
    const cojvr::runtime::Vec3 forward) noexcept {
    return {Dot(value, right), Dot(value, up), Dot(value, forward)};
}

cojvr::runtime::Vec3 BasisToWorld(
    const cojvr::runtime::Vec3 value,
    const cojvr::runtime::Vec3 right,
    const cojvr::runtime::Vec3 up,
    const cojvr::runtime::Vec3 forward) noexcept {
    return Add(Add(Scale(right, value.x), Scale(up, value.y)), Scale(forward, value.z));
}

cojvr::runtime::Vec3 RotateAroundAxis(
    const cojvr::runtime::Vec3 value,
    cojvr::runtime::Vec3 axis,
    const float angle_degrees) noexcept {
    axis = Normalize(axis);
    if (Length(axis) <= 0.99F || !std::isfinite(angle_degrees)) return value;
    constexpr float kDegreesToRadians = 0.017453292519943295F;
    const float radians = angle_degrees * kDegreesToRadians;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return Add(
        Add(Scale(value, cosine), Scale(Cross(axis, value), sine)),
        Scale(axis, Dot(axis, value) * (1.0F - cosine)));
}

BoneRotationDelta BuildBasisRotationDelta(
    cojvr::runtime::Vec3 source_up,
    cojvr::runtime::Vec3 source_forward,
    cojvr::runtime::Vec3 target_up,
    cojvr::runtime::Vec3 target_forward) noexcept {
    BoneRotationDelta result{};
    if (!NormalizeBasis(source_up, source_forward) ||
        !NormalizeBasis(target_up, target_forward)) {
        return result;
    }

    const cojvr::runtime::Vec3 source_x = Normalize(Cross(source_up, source_forward));
    const cojvr::runtime::Vec3 target_x = Normalize(Cross(target_up, target_forward));
    const std::array<float, 12> matrix{
        target_x.x * source_x.x + target_up.x * source_up.x +
            target_forward.x * source_forward.x,
        target_x.x * source_x.y + target_up.x * source_up.y +
            target_forward.x * source_forward.y,
        target_x.x * source_x.z + target_up.x * source_up.z +
            target_forward.x * source_forward.z,
        0.0F,
        target_x.y * source_x.x + target_up.y * source_up.x +
            target_forward.y * source_forward.x,
        target_x.y * source_x.y + target_up.y * source_up.y +
            target_forward.y * source_forward.y,
        target_x.y * source_x.z + target_up.y * source_up.z +
            target_forward.y * source_forward.z,
        0.0F,
        target_x.z * source_x.x + target_up.z * source_up.x +
            target_forward.z * source_forward.x,
        target_x.z * source_x.y + target_up.z * source_up.y +
            target_forward.z * source_forward.y,
        target_x.z * source_x.z + target_up.z * source_up.z +
            target_forward.z * source_forward.z,
        0.0F,
    };
    const cojvr::runtime::Pose rotation_pose =
        cojvr::runtime::PoseFromRigidTransform3x4(matrix);
    if (!rotation_pose.orientation_valid) return result;

    cojvr::runtime::Quaternion rotation{};
    if (!NormalizeQuaternionChecked(rotation_pose.orientation, rotation)) return result;
    if (rotation.w < 0.0F) {
        rotation.x = -rotation.x;
        rotation.y = -rotation.y;
        rotation.z = -rotation.z;
        rotation.w = -rotation.w;
    }
    constexpr float kRadiansToDegrees = 57.29577951308232F;
    const float half_sine = std::sqrt(std::max(0.0F, 1.0F - rotation.w * rotation.w));
    const float angle = 2.0F * std::acos(std::clamp(rotation.w, -1.0F, 1.0F));
    if (angle <= 1.0e-5F || half_sine <= 1.0e-5F) {
        result.axis = target_forward;
        result.angle_degrees = 0.0F;
        result.no_op = true;
        result.valid = true;
        return result;
    }
    result.axis = Normalize({
        rotation.x / half_sine,
        rotation.y / half_sine,
        rotation.z / half_sine,
    });
    result.angle_degrees = angle * kRadiansToDegrees;
    result.valid = Finite(result.axis) && Length(result.axis) > 0.99F &&
        std::isfinite(result.angle_degrees) && result.angle_degrees <= 180.0001F;
    return result;
}

BoneRotationDelta BuildTwistDelta(
    cojvr::runtime::Vec3 axis,
    cojvr::runtime::Vec3 current_reference,
    cojvr::runtime::Vec3 target_reference,
    cojvr::runtime::Vec3 current_fallback,
    cojvr::runtime::Vec3 target_fallback) noexcept {
    BoneRotationDelta result{};
    axis = Normalize(axis);
    if (Length(axis) <= 0.99F) return result;
    const auto projected = [&](const cojvr::runtime::Vec3 value) noexcept {
        return Normalize(Subtract(value, Scale(axis, Dot(value, axis))));
    };
    current_reference = projected(current_reference);
    target_reference = projected(target_reference);
    if (Length(current_reference) <= 0.99F || Length(target_reference) <= 0.99F) {
        current_reference = projected(current_fallback);
        target_reference = projected(target_fallback);
    }
    if (Length(current_reference) <= 0.99F || Length(target_reference) <= 0.99F) {
        return result;
    }
    const float cosine = std::clamp(Dot(current_reference, target_reference), -1.0F, 1.0F);
    const float sine = Dot(axis, Cross(current_reference, target_reference));
    constexpr float kRadiansToDegrees = 57.29577951308232F;
    float angle = std::atan2(sine, cosine) * kRadiansToDegrees;
    if (!std::isfinite(angle)) return result;
    if (std::fabs(angle) <= 0.001F) {
        result.axis = axis;
        result.angle_degrees = 0.0F;
        result.no_op = true;
        result.valid = true;
        return result;
    }
    if (angle < 0.0F) {
        axis = Scale(axis, -1.0F);
        angle = -angle;
    }
    result.axis = axis;
    result.angle_degrees = angle;
    result.valid = angle <= 180.0001F;
    return result;
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

ArmGeometrySample RebaseArmGeometryForActorTranslation(
    const ArmGeometrySample& natural,
    const cojvr::runtime::Vec3 captured_actor_position,
    const cojvr::runtime::Vec3 current_actor_position) noexcept {
    if (!Finite(captured_actor_position) || !Finite(current_actor_position)) return natural;

    const cojvr::runtime::Vec3 delta = Subtract(current_actor_position, captured_actor_position);
    ArmGeometrySample result = natural;
    result.shoulder = Add(result.shoulder, delta);
    result.elbow = Add(result.elbow, delta);
    result.wrist = Add(result.wrist, delta);
    result.upper_element_position = Add(result.upper_element_position, delta);
    result.forearm_element_position = Add(result.forearm_element_position, delta);
    result.foretwist_element_position = Add(result.foretwist_element_position, delta);
    result.hand_element_position = Add(result.hand_element_position, delta);
    return result;
}

ArmGeometryContinuityUpdate StabilizeArmGeometryAcrossActorMotion(
    const ArmGeometrySample& current,
    const cojvr::runtime::Vec3 current_actor_position,
    const ArmGeometrySample& previous,
    const cojvr::runtime::Vec3 previous_actor_position,
    const bool previous_valid) noexcept {
    ArmGeometryContinuityUpdate result{};
    result.geometry = current;

    const auto geometry_finite = [](const ArmGeometrySample& sample) noexcept {
        return Finite(sample.shoulder) && Finite(sample.elbow) && Finite(sample.wrist) &&
            Finite(sample.upper_element_position) && Finite(sample.upper_element_up) &&
            Finite(sample.upper_element_forward) && Finite(sample.forearm_element_position) &&
            Finite(sample.forearm_element_up) && Finite(sample.forearm_element_forward) &&
            Finite(sample.foretwist_element_position) && Finite(sample.foretwist_element_up) &&
            Finite(sample.foretwist_element_forward) && Finite(sample.hand_element_position) &&
            Finite(sample.hand_element_up) && Finite(sample.hand_element_forward);
    };
    if (!geometry_finite(current) || !Finite(current_actor_position)) return result;
    result.valid = true;
    if (!previous_valid || !geometry_finite(previous) || !Finite(previous_actor_position)) {
        return result;
    }

    constexpr float kActorMotionThresholdGameUnits = 0.25F;
    constexpr float kWorldFixedPositionToleranceGameUnits = 0.02F;
    const float actor_motion = Length(Subtract(current_actor_position, previous_actor_position));
    if (!std::isfinite(actor_motion) || actor_motion <= kActorMotionThresholdGameUnits) {
        return result;
    }

    const auto position_unchanged = [&](const cojvr::runtime::Vec3 current_position,
                                        const cojvr::runtime::Vec3 previous_position) noexcept {
        const float distance = Length(Subtract(current_position, previous_position));
        return std::isfinite(distance) &&
            distance <= kWorldFixedPositionToleranceGameUnits;
    };
    const bool complete_branch_world_fixed =
        position_unchanged(current.shoulder, previous.shoulder) &&
        position_unchanged(current.elbow, previous.elbow) &&
        position_unchanged(current.wrist, previous.wrist) &&
        position_unchanged(current.upper_element_position, previous.upper_element_position) &&
        position_unchanged(current.forearm_element_position, previous.forearm_element_position) &&
        position_unchanged(current.foretwist_element_position, previous.foretwist_element_position) &&
        position_unchanged(current.hand_element_position, previous.hand_element_position);
    if (!complete_branch_world_fixed) return result;

    result.geometry = RebaseArmGeometryForActorTranslation(
        current, previous_actor_position, current_actor_position);
    result.rebased_world_fixed = true;
    return result;
}

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
    result.max_element_position_error = std::max({
        distance(natural.upper_element_position, restored.upper_element_position),
        distance(natural.forearm_element_position, restored.forearm_element_position),
        distance(natural.foretwist_element_position, restored.foretwist_element_position),
        distance(natural.hand_element_position, restored.hand_element_position),
    });
    result.max_axis_error = std::max({
        distance(natural.upper_element_up, restored.upper_element_up),
        distance(natural.upper_element_forward, restored.upper_element_forward),
        distance(natural.forearm_element_up, restored.forearm_element_up),
        distance(natural.forearm_element_forward, restored.forearm_element_forward),
        distance(natural.foretwist_element_up, restored.foretwist_element_up),
        distance(natural.foretwist_element_forward, restored.foretwist_element_forward),
        distance(natural.hand_element_up, restored.hand_element_up),
        distance(natural.hand_element_forward, restored.hand_element_forward),
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

bool CanRecoverArmWriterAfterRecenter(
    const bool previous_fault,
    const bool transaction_active,
    const bool natural_geometry_verified) noexcept {
    if (transaction_active) return false;
    return !previous_fault || natural_geometry_verified;
}

SkeletonBinding ExactGameSkeletonBinding() noexcept {
    // EBones.class constants in the shipped code.pak:
    // pelvis=0, spine=1, spine1=2, spine2=3, neck=4, head=5,
    // L upper/forearm/foretwist/hand=7/8/9/10,
    // R upper/forearm/foretwist/hand=12/13/14/15,
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
        .left_foretwist = 9,
        .left_hand = 10,
        .right_upper_arm = 12,
        .right_forearm = 13,
        .right_foretwist = 14,
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

BodyYawOwnershipUpdate BuildBodyYawOwnershipUpdate(
    const cojvr::runtime::Pose& relative_head_pose,
    const float previously_owned_degrees,
    const bool previous_owned_valid,
    const bool recentered) noexcept {
    BodyYawOwnershipUpdate result{};
    if (!relative_head_pose.orientation_valid ||
        !std::isfinite(previously_owned_degrees)) {
        return result;
    }

    if (recentered) {
        result.valid = true;
        return result;
    }

    const cojvr::runtime::Vec3 tracked_forward = cojvr::runtime::RotateVector(
        relative_head_pose.orientation, {0.0F, 0.0F, -1.0F});
    if (!std::isfinite(tracked_forward.x) || !std::isfinite(tracked_forward.y) ||
        !std::isfinite(tracked_forward.z)) {
        return result;
    }

    constexpr float kRadiansToDegrees = 57.29577951308232F;
    const float local_x = tracked_forward.x;
    const float local_z = -tracked_forward.z;
    const float physical_yaw = std::atan2(local_x, local_z) * kRadiansToDegrees;
    if (!std::isfinite(physical_yaw)) return result;

    const float game_yaw = std::remainder(-physical_yaw, 360.0F);
    const float previous_owned = previous_owned_valid
        ? std::remainder(previously_owned_degrees, 360.0F)
        : 0.0F;
    const float residual = std::remainder(game_yaw - previous_owned, 360.0F);

    // Keep ordinary head motion in the neck/spine chain. Outside the comfort
    // cone, absorb only the excess so gradual HMD motion advances the actor
    // continuously along the boundary. Resetting to a smaller residual would
    // turn each boundary crossing into an actor jump. Returning inside the
    // cone leaves the committed actor yaw in place for free look.
    constexpr float kActorFollowComfortDegrees = 35.0F;
    float actor_delta = 0.0F;
    if (std::fabs(residual) > kActorFollowComfortDegrees) {
        actor_delta = residual - std::copysign(kActorFollowComfortDegrees, residual);
    }

    result.actor_target_degrees = std::remainder(previous_owned + actor_delta, 360.0F);
    result.actor_delta_degrees = actor_delta;
    result.camera_compensation_degrees = previous_owned;
    result.head_residual_degrees = std::clamp(residual, -90.0F, 90.0F);
    result.spine_residual_degrees = result.head_residual_degrees * (2.0F / 3.0F);
    result.valid = true;
    return result;
}

RoomScaleTranslationUpdate BuildCollisionSafeRoomScaleTranslation(
    const cojvr::runtime::Vec3 relative_head_position) noexcept {
    RoomScaleTranslationUpdate result{};
    if (!Finite(relative_head_position)) return result;
    result.render_head_position = relative_head_position;
    result.write_actor_position = false;
    result.valid = true;
    return result;
}

bool CoJPhysicalCrouchState::Update(
    const bool tracking_active,
    const bool recentered,
    const float relative_head_y_m) noexcept {
    constexpr float kEngageDropMetres = -0.25F;
    constexpr float kReleaseDropMetres = -0.17F;
    if (!tracking_active || recentered || !std::isfinite(relative_head_y_m)) {
        crouched_ = false;
        return false;
    }
    if (crouched_) {
        if (relative_head_y_m >= kReleaseDropMetres) crouched_ = false;
    } else if (relative_head_y_m <= kEngageDropMetres) {
        crouched_ = true;
    }
    return crouched_;
}

bool ResolveCoJCrouchAction(
    const bool requested_native_crouch,
    const bool physical_crouch_detected) noexcept {
    return requested_native_crouch || physical_crouch_detected;
}

cojvr::runtime::Vec2 RotateCoJMoveForHeadRelativeYaw(
    const cojvr::runtime::Vec2 move,
    const float head_yaw_degrees) noexcept {
    if (!std::isfinite(move.x) || !std::isfinite(move.y) ||
        !std::isfinite(head_yaw_degrees)) {
        return {};
    }
    constexpr float kDegreesToRadians = 0.017453292519943295F;
    const float radians = head_yaw_degrees * kDegreesToRadians;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        move.x * cosine + move.y * sine,
        -move.x * sine + move.y * cosine,
    };
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

ElementWorldBasisTarget TransformWeaponElementFrame(
    const ElementWorldBasisTarget& source_root,
    const ElementWorldBasisTarget& target_root,
    const ElementWorldBasisTarget& element) noexcept {
    ElementWorldBasisTarget result{};
    auto su = source_root.up, sf = source_root.forward;
    auto tu = target_root.up, tf = target_root.forward;
    if (!source_root.valid || !target_root.valid || !element.valid ||
        !Finite(source_root.position) || !Finite(target_root.position) ||
        !Finite(element.position) || !NormalizeBasis(su, sf) || !NormalizeBasis(tu, tf)) return result;
    const auto sx = Cross(su, sf), tx = Cross(tu, tf);
    const auto rotate = [&](const cojvr::runtime::Vec3 v) noexcept {
        return BasisToWorld(WorldToBasis(v, sx, su, sf), tx, tu, tf);
    };
    result.position = Add(target_root.position, rotate(Subtract(element.position, source_root.position)));
    result.up = rotate(element.up); result.forward = rotate(element.forward);
    result.valid = Finite(result.position) && NormalizeBasis(result.up, result.forward);
    return result;
}

TrackedWeaponFramePlan BuildTrackedWeaponFrame(
    const ElementWorldBasisTarget& natural_root,
    const cojvr::runtime::Vec3 natural_wrist,
    const cojvr::runtime::Vec3 natural_muzzle,
    const cojvr::runtime::Vec3 natural_barrel_direction,
    const cojvr::runtime::Vec3 tracked_grip,
    const cojvr::runtime::Vec3 tracked_direction,
    const cojvr::runtime::Vec3 tracked_up) noexcept {
    TrackedWeaponFramePlan result{};
    if (!natural_root.valid || !Finite(natural_root.position) ||
        !Finite(natural_wrist) || !Finite(natural_muzzle) || !Finite(tracked_grip) ||
        !Finite(natural_barrel_direction) || !Finite(tracked_direction) || !Finite(tracked_up))
        return result;
    auto root_up = natural_root.up;
    auto root_forward = natural_root.forward;
    if (!NormalizeBasis(root_up, root_forward)) return result;
    const auto source_forward = Normalize(natural_barrel_direction);
    const auto target_forward = Normalize(tracked_direction);
    const auto source_up = Normalize(Subtract(root_up, Scale(source_forward, Dot(root_up, source_forward))));
    const auto target_up = Normalize(Subtract(tracked_up, Scale(target_forward, Dot(tracked_up, target_forward))));
    if (Length(source_forward) < 0.99F || Length(target_forward) < 0.99F ||
        Length(source_up) < 0.99F || Length(target_up) < 0.99F) return result;
    const auto source_right = Cross(source_up, source_forward);
    const auto target_right = Cross(target_up, target_forward);
    const auto rotate = [&](const cojvr::runtime::Vec3 v) noexcept {
        return Add(Add(Scale(target_right, Dot(v, source_right)),
                       Scale(target_up, Dot(v, source_up))),
                   Scale(target_forward, Dot(v, source_forward)));
    };
    result.root.position = Add(tracked_grip, rotate(Subtract(natural_root.position, natural_wrist)));
    result.root.up = rotate(root_up);
    result.root.forward = rotate(root_forward);
    result.root.valid = NormalizeBasis(result.root.up, result.root.forward) && Finite(result.root.position);
    result.muzzle_origin = Add(tracked_grip, rotate(Subtract(natural_muzzle, natural_wrist)));
    result.muzzle_direction = target_forward;
    result.valid = result.root.valid && Finite(result.muzzle_origin);
    return result;
}

float CoJArmSpanCalibration::Update(
    const cojvr::runtime::Vec3 head, const cojvr::runtime::Vec3 left,
    const cojvr::runtime::Vec3 right, const float native_shoulder_span_cm,
    const float native_arm_sum_cm, const bool valid) noexcept {
    if (calibrated_) return scale_;
    const auto l = Subtract(left, head), r = Subtract(right, head);
    const bool t_pose = valid && Finite(head) && Finite(left) && Finite(right) &&
        std::isfinite(native_shoulder_span_cm) && native_shoulder_span_cm > 0 &&
        std::isfinite(native_arm_sum_cm) && native_arm_sum_cm > 0 &&
        l.x < 0 && r.x > 0 &&
        // The visor is above the shoulders even in a horizontal T pose.
        std::fabs(l.y) < -l.x * 0.5F && std::fabs(l.z) < -l.x * 0.25F &&
        std::fabs(r.y) < r.x * 0.5F && std::fabs(r.z) < r.x * 0.25F &&
        std::fabs(l.y-r.y) < (r.x-l.x) * 0.1F;
    if (!t_pose) { stable_samples_ = 0; candidate_span_cm_ = 0; return scale_; }
    const float span_cm = Length(Subtract(right, left)) * 100.0F;
    const float fit = (span_cm - native_shoulder_span_cm) / native_arm_sum_cm;
    // Reject tracking outliers rather than growing a chain without bound.
    if (!std::isfinite(fit) || fit < 1 || fit > 2) {
        stable_samples_ = 0; return scale_;
    }
    if (stable_samples_ == 0 || std::fabs(span_cm-candidate_span_cm_) > span_cm * 0.01F) {
        candidate_span_cm_ = span_cm; stable_samples_ = 1;
    } else {
        candidate_span_cm_ += (span_cm-candidate_span_cm_) / static_cast<float>(++stable_samples_);
    }
    if (stable_samples_ >= 30) {
        scale_ = (candidate_span_cm_ - native_shoulder_span_cm) / native_arm_sum_cm;
        calibrated_ = true;
    }
    return scale_;
}

ArmIkPlan BuildArmIkPlan(
    const ArmGeometrySample& geometry,
    const cojvr::runtime::Vec3 controller_target,
    const cojvr::runtime::Vec3 elbow_pole,
    const float measured_reach_scale) noexcept {
    ArmIkPlan result{};
    if (!Finite(geometry.shoulder) || !Finite(geometry.elbow) || !Finite(geometry.wrist) ||
        !Finite(geometry.upper_element_position) || !Finite(geometry.upper_element_up) ||
        !Finite(geometry.upper_element_forward) || !Finite(geometry.forearm_element_position) ||
        !Finite(geometry.forearm_element_up) || !Finite(geometry.forearm_element_forward) ||
        !Finite(controller_target) || !Finite(elbow_pole) ||
        !std::isfinite(measured_reach_scale) || measured_reach_scale < 1 || measured_reach_scale > 2) {
        return result;
    }

    const cojvr::runtime::Vec3 upper_segment =
        Subtract(geometry.elbow, geometry.shoulder);
    const cojvr::runtime::Vec3 lower_segment =
        Subtract(geometry.wrist, geometry.elbow);
    result.upper_length = Length(upper_segment) * measured_reach_scale;
    result.lower_length = Length(lower_segment) * measured_reach_scale;
    if (!std::isfinite(result.upper_length) || !std::isfinite(result.lower_length) ||
        result.upper_length <= 0.1F || result.lower_length <= 0.1F) {
        return result;
    }

    const cojvr::runtime::Vec3 raw_target_offset =
        Subtract(controller_target, geometry.shoulder);
    result.raw_target_distance = Length(raw_target_offset);
    const cojvr::runtime::Vec3 target_axis = Normalize(raw_target_offset);
    if (Length(target_axis) <= 1.0e-5F) return result;
    // Physical run 20260920T090448Z-b1f54e3cb38e showed that the old bounded
    // overreach policy made the rendered wrist lag the tracked controller by as
    // much as 12 game units. Preserve the native bone lengths and let the
    // two-bone solver perform the only reach clamp at the actual controller
    // target instead of shortening that target first.
    const cojvr::runtime::Vec3 effective_target = controller_target;
    result.effective_target_distance = result.raw_target_distance;
    const cojvr::runtime::Vec3 current_elbow = Length(elbow_pole) > 1.0e-4F
        ? elbow_pole : Subtract(geometry.elbow, geometry.shoulder);
    cojvr::runtime::Vec3 pole =
        Subtract(current_elbow, Scale(target_axis, Dot(current_elbow, target_axis)));
    if (Length(pole) <= 1.0e-4F) pole = Length(elbow_pole) > 1.0e-4F
        ? StablePerpendicular(target_axis) : geometry.upper_element_forward;

    const auto solved = cojvr::runtime::SolveTwoBoneIK(
        geometry.shoulder,
        effective_target,
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

bool ShouldApplyCoJArmIk(const ArmIkPlan& plan) noexcept {
    if (!plan.valid || !std::isfinite(plan.raw_target_distance) ||
        !std::isfinite(plan.upper_length) || !std::isfinite(plan.lower_length)) {
        return false;
    }
    const float native_reach = plan.upper_length + plan.lower_length;
    return std::isfinite(native_reach) && native_reach > 0.0F;
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

bool ShouldApplyCoJArmRotationPlan(const ArmBoneRotationPlan& plan) noexcept {
    constexpr float kMaximumOverlayRotationDegrees = 120.0F;
    if (!plan.valid || !plan.upper_arm.valid || !plan.forearm.valid ||
        !std::isfinite(plan.upper_arm.angle_degrees) ||
        !std::isfinite(plan.forearm.angle_degrees)) {
        return false;
    }
    return plan.upper_arm.angle_degrees <= kMaximumOverlayRotationDegrees &&
        plan.forearm.angle_degrees <= kMaximumOverlayRotationDegrees;
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

ArmSkinningPlan BuildArmSkinningPlan(
    const ArmGeometrySample& natural,
    const ArmBoneRotationPlan& rotations,
    const BoneRotationDelta& world_twist,
    const cojvr::runtime::Vec3 post_ik_hand_up,
    const cojvr::runtime::Vec3 post_ik_hand_forward) noexcept {
    ArmSkinningPlan result{};
    const auto valid_rotation = [](const BoneRotationDelta& r) noexcept {
        return r.valid && Finite(r.axis) && std::isfinite(r.angle_degrees) &&
            (r.no_op || Length(r.axis) > 0.99F);
    };
    if (!rotations.valid || !valid_rotation(rotations.upper_arm) ||
        !valid_rotation(rotations.forearm) || !valid_rotation(world_twist)) return result;
    const auto apply = [](cojvr::runtime::Vec3 v, const BoneRotationDelta& r) noexcept {
        return r.no_op ? v : RotateAroundAxis(v, r.axis, r.angle_degrees);
    };
    const auto skin_axis = [&](const cojvr::runtime::Vec3 v) noexcept {
        return apply(apply(apply(v, rotations.upper_arm), rotations.forearm), world_twist);
    };
    result.foretwist.up = skin_axis(natural.foretwist_element_up);
    result.foretwist.forward = skin_axis(natural.foretwist_element_forward);
    result.hand.up = apply(post_ik_hand_up, world_twist);
    result.hand.forward = apply(post_ik_hand_forward, world_twist);
    result.foretwist.valid = NormalizeBasis(result.foretwist.up, result.foretwist.forward);
    result.hand.valid = NormalizeBasis(result.hand.up, result.hand.forward);
    result.valid = result.foretwist.valid && result.hand.valid;
    return result;
}

HandOrientationReference BuildHandOrientationReference(
    const cojvr::runtime::Quaternion controller_orientation,
    cojvr::runtime::Vec3 camera_right,
    cojvr::runtime::Vec3 camera_up,
    cojvr::runtime::Vec3 camera_forward,
    cojvr::runtime::Vec3 hand_up_world,
    cojvr::runtime::Vec3 hand_forward_world) noexcept {
    HandOrientationReference result{};
    cojvr::runtime::Quaternion normalized_controller{};
    if (!NormalizeQuaternionChecked(controller_orientation, normalized_controller) ||
        !NormalizeCameraBasis(camera_right, camera_up, camera_forward) ||
        !NormalizeBasis(hand_up_world, hand_forward_world)) {
        return result;
    }

    result.controller_orientation = normalized_controller;
    result.hand_up_camera = WorldToBasis(
        hand_up_world, camera_right, camera_up, camera_forward);
    result.hand_forward_camera = WorldToBasis(
        hand_forward_world, camera_right, camera_up, camera_forward);
    if (!NormalizeBasis(result.hand_up_camera, result.hand_forward_camera)) return {};
    result.valid = true;
    return result;
}

HandOrientationTarget BuildTrackedHandOrientationTarget(
    const HandOrientationReference& reference,
    const cojvr::runtime::Quaternion controller_orientation,
    cojvr::runtime::Vec3 camera_right,
    cojvr::runtime::Vec3 camera_up,
    cojvr::runtime::Vec3 camera_forward) noexcept {
    HandOrientationTarget result{};
    cojvr::runtime::Quaternion current{};
    cojvr::runtime::Quaternion reference_orientation{};
    if (!reference.valid ||
        !NormalizeQuaternionChecked(controller_orientation, current) ||
        !NormalizeQuaternionChecked(reference.controller_orientation, reference_orientation) ||
        !NormalizeCameraBasis(camera_right, camera_up, camera_forward)) {
        return result;
    }

    cojvr::runtime::Quaternion delta = Multiply(current, Conjugate(reference_orientation));
    if (!NormalizeQuaternionChecked(delta, delta)) return result;
    cojvr::runtime::Vec3 target_up_camera = cojvr::runtime::RotateVector(
        delta, reference.hand_up_camera);
    cojvr::runtime::Vec3 target_forward_camera = cojvr::runtime::RotateVector(
        delta, reference.hand_forward_camera);
    result.up = BasisToWorld(
        target_up_camera, camera_right, camera_up, camera_forward);
    result.forward = BasisToWorld(
        target_forward_camera, camera_right, camera_up, camera_forward);
    if (!NormalizeBasis(result.up, result.forward)) return {};
    result.valid = true;
    return result;
}

HandOrientationRotationPlan BuildHandOrientationRotationPlan(
    const cojvr::runtime::Vec3 lower_arm_axis,
    cojvr::runtime::Vec3 current_hand_up,
    cojvr::runtime::Vec3 current_hand_forward,
    const HandOrientationTarget& target) noexcept {
    HandOrientationRotationPlan result{};
    cojvr::runtime::Vec3 target_up = target.up;
    cojvr::runtime::Vec3 target_forward = target.forward;
    if (!target.valid || !Finite(lower_arm_axis) ||
        !NormalizeBasis(current_hand_up, current_hand_forward) ||
        !NormalizeBasis(target_up, target_forward)) {
        return result;
    }

    result.forearm_twist = BuildTwistDelta(
        lower_arm_axis,
        current_hand_up,
        target_up,
        current_hand_forward,
        target_forward);
    if (!result.forearm_twist.valid) return result;

    const cojvr::runtime::Vec3 hand_up_after_twist = result.forearm_twist.no_op
        ? current_hand_up
        : RotateAroundAxis(
              current_hand_up,
              result.forearm_twist.axis,
              result.forearm_twist.angle_degrees);
    const cojvr::runtime::Vec3 hand_forward_after_twist = result.forearm_twist.no_op
        ? current_hand_forward
        : RotateAroundAxis(
              current_hand_forward,
              result.forearm_twist.axis,
              result.forearm_twist.angle_degrees);
    result.hand = BuildHandResidualRotationDelta(
        hand_up_after_twist, hand_forward_after_twist, target);
    result.valid = result.hand.valid;
    return result;
}

BoneRotationDelta BuildHandResidualRotationDelta(
    cojvr::runtime::Vec3 current_hand_up,
    cojvr::runtime::Vec3 current_hand_forward,
    const HandOrientationTarget& target) noexcept {
    cojvr::runtime::Vec3 target_up = target.up;
    cojvr::runtime::Vec3 target_forward = target.forward;
    if (!target.valid || !NormalizeBasis(current_hand_up, current_hand_forward) ||
        !NormalizeBasis(target_up, target_forward)) {
        return {};
    }
    return BuildBasisRotationDelta(
        current_hand_up, current_hand_forward, target_up, target_forward);
}

BoneRotationDelta LimitRotationMagnitude(
    BoneRotationDelta rotation,
    const float max_degrees) noexcept {
    if (!rotation.valid || !std::isfinite(rotation.angle_degrees) ||
        !std::isfinite(max_degrees) || max_degrees < 0.0F) {
        return {};
    }
    if (rotation.no_op || rotation.angle_degrees <= max_degrees) return rotation;
    rotation.angle_degrees = max_degrees;
    rotation.no_op = max_degrees <= 0.001F;
    return rotation;
}

float CoJArmControllerTwistLimitDegrees() noexcept {
    // Physical run 20260920T090448Z-b1f54e3cb38e still showed severe arm
    // corkscrewing while controller-driven axial roll routinely approached the
    // previous 100-degree limit. Keep controller orientation as diagnostic
    // telemetry for the next physical gate, but apply no axial roll. Positional
    // IK and the proven FORETWIST sibling swing remain active.
    return 0.0F;
}

float CoJHandControllerResidualLimitDegrees() noexcept {
    // FORETWIST controller roll remains disabled after the live corkscrewing
    // rejection.  A smaller correction on the hand element can recover wrist
    // orientation without rotating the upper/forearm chain. Physical evidence
    // showed that clipping a 120-158 degree mismatch to this limit still
    // deformed the wrist, so residuals beyond the limit now stay native.
    return 30.0F;
}

BoneRotationDelta BuildSafeCoJHandResidual(BoneRotationDelta residual) noexcept {
    if (!residual.valid || !std::isfinite(residual.angle_degrees)) return {};
    const float limit = CoJHandControllerResidualLimitDegrees();
    if (residual.no_op || residual.angle_degrees <= limit) return residual;
    residual.angle_degrees = 0.0F;
    residual.no_op = true;
    return residual;
}

ArmElementFramePlan BuildArmElementFramePlan(
    const ArmGeometrySample& natural,
    const ArmIkPlan& ik,
    const ArmBoneRotationPlan& rotations,
    const HandOrientationTarget& hand_target) noexcept {
    ArmElementFramePlan result{};
    const auto valid_rotation = [](const BoneRotationDelta& rotation) noexcept {
        return rotation.valid && Finite(rotation.axis) &&
            std::isfinite(rotation.angle_degrees) &&
            (rotation.no_op || Length(rotation.axis) > 0.99F);
    };
    if (!ik.valid || !rotations.valid || !hand_target.valid ||
        !ik.upper_arm.valid || !ik.forearm.valid ||
        !valid_rotation(rotations.upper_arm) || !valid_rotation(rotations.forearm) ||
        !Finite(natural.shoulder) || !Finite(natural.elbow) || !Finite(natural.wrist) ||
        !Finite(natural.foretwist_element_position) ||
        !Finite(natural.hand_element_position) ||
        !Finite(natural.hand_element_up) || !Finite(natural.hand_element_forward)) {
        return result;
    }

    const auto apply_rotation = [](cojvr::runtime::Vec3 value,
                                   const BoneRotationDelta& rotation) noexcept {
        return rotation.no_op
            ? value
            : RotateAroundAxis(value, rotation.axis, rotation.angle_degrees);
    };
    const auto rotate_point = [&](cojvr::runtime::Vec3 point,
                                  const cojvr::runtime::Vec3 pivot,
                                  const BoneRotationDelta& rotation) noexcept {
        return Add(pivot, apply_rotation(Subtract(point, pivot), rotation));
    };
    const auto apply_swing = [&](cojvr::runtime::Vec3 value) noexcept {
        return apply_rotation(apply_rotation(value, rotations.upper_arm), rotations.forearm);
    };

    result.upper_arm = ik.upper_arm;
    result.forearm = ik.forearm;
    // The direct natural-lower -> desired-lower basis in the positional plan
    // has a different axial roll from the parent+elbow composition used by
    // FORETWIST and hand. Preserve one composed skinning frame across all
    // three while retaining the positional solver's pivot.
    result.forearm.up = apply_swing(natural.forearm_element_up);
    result.forearm.forward = apply_swing(natural.forearm_element_forward);
    result.forearm.valid = Finite(result.forearm.position) &&
        NormalizeBasis(result.forearm.up, result.forearm.forward);
    if (!result.forearm.valid) return {};

    cojvr::runtime::Vec3 post_ik_hand_up = apply_swing(natural.hand_element_up);
    cojvr::runtime::Vec3 post_ik_hand_forward = apply_swing(natural.hand_element_forward);
    if (!NormalizeBasis(post_ik_hand_up, post_ik_hand_forward)) return {};

    const HandOrientationRotationPlan orientation_plan = BuildHandOrientationRotationPlan(
        Subtract(ik.wrist_target, ik.elbow_target),
        post_ik_hand_up,
        post_ik_hand_forward,
        hand_target);
    if (!orientation_plan.valid) return {};
    result.requested_forearm_twist_degrees = orientation_plan.forearm_twist.angle_degrees;
    result.forearm_twist = LimitRotationMagnitude(
        orientation_plan.forearm_twist, CoJArmControllerTwistLimitDegrees());
    if (!valid_rotation(result.forearm_twist)) return {};
    result.forearm_twist_limited =
        result.forearm_twist.angle_degrees + 0.001F <
        result.requested_forearm_twist_degrees;

    const ArmSkinningPlan skinning = BuildArmSkinningPlan(
        natural,
        rotations,
        result.forearm_twist,
        post_ik_hand_up,
        post_ik_hand_forward);
    if (!skinning.valid) return {};

    cojvr::runtime::Vec3 foretwist_position = Add(ik.elbow_target,
        apply_swing(Subtract(natural.foretwist_element_position, natural.elbow)));
    foretwist_position = rotate_point(
        foretwist_position, ik.elbow_target, result.forearm_twist);
    result.foretwist.position = foretwist_position;
    result.foretwist.up = skinning.foretwist.up;
    result.foretwist.forward = skinning.foretwist.forward;
    result.foretwist.valid = Finite(result.foretwist.position) &&
        NormalizeBasis(result.foretwist.up, result.foretwist.forward);
    if (!result.foretwist.valid) return {};

    cojvr::runtime::Vec3 hand_position = Add(ik.wrist_target,
        apply_swing(Subtract(natural.hand_element_position, natural.wrist)));
    hand_position = rotate_point(hand_position, ik.elbow_target, result.forearm_twist);

    result.diagnostic_hand_residual = BuildHandResidualRotationDelta(
        skinning.hand.up, skinning.hand.forward, hand_target);
    if (!valid_rotation(result.diagnostic_hand_residual)) return {};
    result.hand_residual = BuildSafeCoJHandResidual(result.diagnostic_hand_residual);
    if (!valid_rotation(result.hand_residual)) return {};

    result.hand.position = hand_position;
    result.hand.up = apply_rotation(skinning.hand.up, result.hand_residual);
    result.hand.forward = apply_rotation(skinning.hand.forward, result.hand_residual);
    result.hand.valid = Finite(result.hand.position) &&
        NormalizeBasis(result.hand.up, result.hand.forward);
    result.valid = result.upper_arm.valid && result.forearm.valid &&
        result.foretwist.valid && result.hand.valid;
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

float CoJPhysicalViewHeightState::Update(
    const float native_camera_y, const float actor_y, const bool physical_crouch,
    const bool controller_crouch, const bool reset) noexcept {
    if (reset) Reset();
    const float height = native_camera_y - actor_y;
    if (!std::isfinite(native_camera_y) || !std::isfinite(actor_y) ||
        !std::isfinite(height)) {
        Reset();
        return 0;
    }
    if (controller_crouch) {
        // Keep the existing standing reference while native crouch owns the
        // view; releasing the button must not calibrate against its low pose.
        compensating_ = false;
        controller_recovery_ = true;
        return 0;
    }
    if (!reference_valid_) {
        reference_height_ = height;
        reference_valid_ = true;
    }
    if (physical_crouch) {
        controller_recovery_ = false;
        compensating_ = true;
    } else if (controller_recovery_) {
        // Explicit crouch keeps its native transition after release as well.
        if (reference_height_ - height > 0.02F) return 0;
        controller_recovery_ = false;
    }
    if (compensating_) {
        const float correction = std::max(0.0F, reference_height_ - height);
        // Native pose recovery may lag the HMD/controller action release.
        // Retain compensation until the camera reaches its captured height.
        if (physical_crouch || correction > 0.02F) return correction;
        compensating_ = false;
    }
    reference_height_ = height;
    return 0;
}

cojvr::runtime::Vec3 BuildCoJVisualBodyOffset(
    const cojvr::runtime::Vec3 tracked_head,
    const cojvr::runtime::Vec3 camera_right,
    const cojvr::runtime::Vec3 camera_forward,
    const float game_units_per_meter,
    bool& valid) noexcept {
    if (!Finite(tracked_head)) { valid = false; return {}; }
    return BuildTrackedHandTarget({}, camera_right, {0, 1, 0}, camera_forward,
        {}, {tracked_head.x, 0, tracked_head.z}, game_units_per_meter, valid);
}

cojvr::runtime::Vec3 BuildTrackedArmTarget(
    const cojvr::runtime::Vec3 head_world_target,
    const cojvr::runtime::Vec3 camera_right,
    const cojvr::runtime::Vec3 camera_up,
    const cojvr::runtime::Vec3 camera_forward,
    const cojvr::runtime::Vec3 tracked_head,
    const cojvr::runtime::Vec3 tracked_hand,
    const float game_units_per_meter,
    bool& valid) noexcept {
    if (!Finite(tracked_head)) { valid = false; return {}; }
    return BuildTrackedHandTarget(head_world_target, camera_right, camera_up,
        camera_forward, {tracked_head.x, 0, tracked_head.z}, tracked_hand,
        game_units_per_meter, valid);
}

cojvr::runtime::Vec3 BuildTrackedAimOrigin(
    cojvr::runtime::Vec3 native_camera_position,
    const cojvr::runtime::Vec3 camera_right,
    const cojvr::runtime::Vec3 camera_up,
    const cojvr::runtime::Vec3 camera_forward,
    const cojvr::runtime::Vec3 tracked_head,
    const cojvr::runtime::Vec3 tracked_tip,
    const float physical_view_correction,
    const float game_units_per_meter, bool& valid) noexcept {
    valid = false;
    if (!Finite(tracked_head) || !std::isfinite(physical_view_correction)) return {};
    native_camera_position.y += physical_view_correction;
    return BuildTrackedHandTarget(native_camera_position, camera_right, camera_up,
        camera_forward, {}, tracked_tip, game_units_per_meter, valid);
}

cojvr::runtime::Vec3 BuildTrackedAimDirection(
    const cojvr::runtime::Quaternion tracked_orientation,
    cojvr::runtime::Vec3 camera_right,
    cojvr::runtime::Vec3 camera_up,
    cojvr::runtime::Vec3 camera_forward,
    bool& valid) noexcept {
    valid = false;
    cojvr::runtime::Quaternion orientation{};
    if (!NormalizeQuaternionChecked(tracked_orientation, orientation) ||
        !NormalizeCameraBasis(camera_right, camera_up, camera_forward)) {
        return {};
    }
    const cojvr::runtime::Vec3 tracking_forward = cojvr::runtime::RotateVector(
        orientation, {0.0F, 0.0F, -1.0F});
    if (!Finite(tracking_forward)) return {};
    cojvr::runtime::Vec3 world = BasisToWorld(
        tracking_forward, camera_right, camera_up, camera_forward);
    world = Normalize(world);
    valid = Finite(world) && Length(world) > 0.99F;
    return valid ? world : cojvr::runtime::Vec3{};
}

TrackedAimPoseDiagnostics BuildTrackedAimPoseDiagnostics(
    const cojvr::runtime::Quaternion tip_orientation,
    const cojvr::runtime::Vec3 grip_position,
    const cojvr::runtime::Vec3 tip_position) noexcept {
    TrackedAimPoseDiagnostics result{};
    cojvr::runtime::Quaternion orientation{};
    if (!NormalizeQuaternionChecked(tip_orientation, orientation) ||
        !Finite(grip_position) || !Finite(tip_position)) {
        return result;
    }

    const cojvr::runtime::Vec3 grip_to_tip = Subtract(tip_position, grip_position);
    result.grip_to_tip_distance_m = Length(grip_to_tip);
    if (!std::isfinite(result.grip_to_tip_distance_m) ||
        result.grip_to_tip_distance_m <= 0.0001F) {
        return result;
    }
    result.grip_to_tip_direction = Normalize(grip_to_tip);

    const cojvr::runtime::Vec3 axis_x =
        cojvr::runtime::RotateVector(orientation, {1.0F, 0.0F, 0.0F});
    const cojvr::runtime::Vec3 axis_y =
        cojvr::runtime::RotateVector(orientation, {0.0F, 1.0F, 0.0F});
    const cojvr::runtime::Vec3 axis_z =
        cojvr::runtime::RotateVector(orientation, {0.0F, 0.0F, 1.0F});
    if (!Finite(axis_x) || !Finite(axis_y) || !Finite(axis_z)) return result;

    result.dot_positive_x = Dot(result.grip_to_tip_direction, axis_x);
    result.dot_negative_x = -result.dot_positive_x;
    result.dot_positive_y = Dot(result.grip_to_tip_direction, axis_y);
    result.dot_negative_y = -result.dot_positive_y;
    result.dot_positive_z = Dot(result.grip_to_tip_direction, axis_z);
    result.dot_negative_z = -result.dot_positive_z;
    result.valid = std::isfinite(result.dot_positive_x) &&
        std::isfinite(result.dot_positive_y) && std::isfinite(result.dot_positive_z);
    return result;
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
