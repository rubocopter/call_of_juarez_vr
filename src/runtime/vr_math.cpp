#include "runtime/vr_math.hpp"

#include <algorithm>
#include <cmath>

namespace cojvr::runtime {
namespace {

bool IsFiniteVec3(const Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsFiniteQuaternion(const Quaternion value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z) && std::isfinite(value.w);
}

float QuaternionLengthSquared(const Quaternion value) noexcept {
    return value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w;
}

bool IsRigidRotation3x3(const std::array<float, 12>& matrix) noexcept {
    constexpr float kTolerance = 5.0e-3F;
    const Vec3 right{matrix[0], matrix[1], matrix[2]};
    const Vec3 up{matrix[4], matrix[5], matrix[6]};
    const Vec3 forward{matrix[8], matrix[9], matrix[10]};
    if (!IsFiniteVec3(right) || !IsFiniteVec3(up) || !IsFiniteVec3(forward)) return false;

    const auto dot = [](const Vec3 a, const Vec3 b) noexcept {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    const float right_length = dot(right, right);
    const float up_length = dot(up, up);
    const float forward_length = dot(forward, forward);
    if (std::fabs(right_length - 1.0F) > kTolerance ||
        std::fabs(up_length - 1.0F) > kTolerance ||
        std::fabs(forward_length - 1.0F) > kTolerance ||
        std::fabs(dot(right, up)) > kTolerance ||
        std::fabs(dot(right, forward)) > kTolerance ||
        std::fabs(dot(up, forward)) > kTolerance) {
        return false;
    }

    const float determinant =
        right.x * (up.y * forward.z - up.z * forward.y) -
        right.y * (up.x * forward.z - up.z * forward.x) +
        right.z * (up.x * forward.y - up.y * forward.x);
    return std::isfinite(determinant) && std::fabs(determinant - 1.0F) <= kTolerance;
}

} // namespace

Quaternion NormalizeQuaternion(Quaternion value) noexcept {
    const float length = std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w);
    if (!std::isfinite(length) || length <= 1.0e-6F) return {};
    const float inverse = 1.0F / length;
    value.x *= inverse;
    value.y *= inverse;
    value.z *= inverse;
    value.w *= inverse;
    return value;
}

Vec3 RotateVector(const Quaternion rotation, const Vec3 value) noexcept {
    const Quaternion q = NormalizeQuaternion(rotation);
    const Vec3 qv{q.x, q.y, q.z};
    const Vec3 first_cross{
        qv.y * value.z - qv.z * value.y,
        qv.z * value.x - qv.x * value.z,
        qv.x * value.y - qv.y * value.x,
    };
    const Vec3 t{2.0F * first_cross.x, 2.0F * first_cross.y, 2.0F * first_cross.z};
    const Vec3 second_cross{
        qv.y * t.z - qv.z * t.y,
        qv.z * t.x - qv.x * t.z,
        qv.x * t.y - qv.y * t.x,
    };
    return {
        value.x + q.w * t.x + second_cross.x,
        value.y + q.w * t.y + second_cross.y,
        value.z + q.w * t.z + second_cross.z,
    };
}

Pose PoseFromRigidTransform3x4(const std::array<float, 12>& matrix) noexcept {
    Pose pose{};
    pose.position = {matrix[3], matrix[7], matrix[11]};
    pose.position_valid = IsFiniteVec3(pose.position);
    if (!IsRigidRotation3x3(matrix)) return pose;

    const float m00 = matrix[0];
    const float m01 = matrix[1];
    const float m02 = matrix[2];
    const float m10 = matrix[4];
    const float m11 = matrix[5];
    const float m12 = matrix[6];
    const float m20 = matrix[8];
    const float m21 = matrix[9];
    const float m22 = matrix[10];

    Quaternion q{};
    const float trace = m00 + m11 + m22;
    if (trace > 0.0F) {
        const float s = 2.0F * std::sqrt(std::max(trace + 1.0F, 0.0F));
        if (s > 0.0F) {
            q.w = 0.25F * s;
            q.x = (m21 - m12) / s;
            q.y = (m02 - m20) / s;
            q.z = (m10 - m01) / s;
        }
    } else if (m00 > m11 && m00 > m22) {
        const float s = 2.0F * std::sqrt(std::max(1.0F + m00 - m11 - m22, 0.0F));
        if (s > 0.0F) {
            q.w = (m21 - m12) / s;
            q.x = 0.25F * s;
            q.y = (m01 + m10) / s;
            q.z = (m02 + m20) / s;
        }
    } else if (m11 > m22) {
        const float s = 2.0F * std::sqrt(std::max(1.0F + m11 - m00 - m22, 0.0F));
        if (s > 0.0F) {
            q.w = (m02 - m20) / s;
            q.x = (m01 + m10) / s;
            q.y = 0.25F * s;
            q.z = (m12 + m21) / s;
        }
    } else {
        const float s = 2.0F * std::sqrt(std::max(1.0F + m22 - m00 - m11, 0.0F));
        if (s > 0.0F) {
            q.w = (m10 - m01) / s;
            q.x = (m02 + m20) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25F * s;
        }
    }

    const float quaternion_length_squared =
        q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (!std::isfinite(quaternion_length_squared) ||
        quaternion_length_squared <= 1.0e-12F) {
        return pose;
    }
    pose.orientation = NormalizeQuaternion(q);
    pose.orientation_valid = true;
    return pose;
}

bool ProjectFlatTheaterPointer(
    const Pose& anchor_pose,
    const Pose& aim_pose,
    const EyeFov left_fov,
    const EyeFov right_fov,
    const std::uint32_t source_width,
    const std::uint32_t source_height,
    const std::uint32_t texture_width,
    const std::uint32_t texture_height,
    FlatTheaterPointerProjection& projection,
    const float plane_distance_m) noexcept {
    projection = {};
    if (!anchor_pose.orientation_valid || !anchor_pose.position_valid ||
        !aim_pose.orientation_valid || !aim_pose.position_valid ||
        !IsFiniteVec3(anchor_pose.position) || !IsFiniteVec3(aim_pose.position) ||
        !IsFiniteQuaternion(anchor_pose.orientation) ||
        !IsFiniteQuaternion(aim_pose.orientation) ||
        !IsValidEyeFov(left_fov) || !IsValidEyeFov(right_fov) ||
        source_width == 0 || source_height == 0 ||
        texture_width < source_width || texture_height < source_height ||
        !std::isfinite(plane_distance_m) || plane_distance_m <= 0.05F) {
        return false;
    }

    const float raw_anchor_length_sq = QuaternionLengthSquared(anchor_pose.orientation);
    const float raw_aim_length_sq = QuaternionLengthSquared(aim_pose.orientation);
    if (!std::isfinite(raw_anchor_length_sq) || !std::isfinite(raw_aim_length_sq) ||
        raw_anchor_length_sq <= 1.0e-6F || raw_aim_length_sq <= 1.0e-6F) {
        return false;
    }
    const Quaternion anchor = NormalizeQuaternion(anchor_pose.orientation);
    const Quaternion aim = NormalizeQuaternion(aim_pose.orientation);

    const Quaternion tracking_to_anchor{-anchor.x, -anchor.y, -anchor.z, anchor.w};
    const Vec3 tracking_origin{
        aim_pose.position.x - anchor_pose.position.x,
        aim_pose.position.y - anchor_pose.position.y,
        aim_pose.position.z - anchor_pose.position.z,
    };
    const Vec3 local_origin = RotateVector(tracking_to_anchor, tracking_origin);
    const Vec3 tracking_direction = RotateVector(aim, {0.0F, 0.0F, -1.0F});
    const Vec3 local_direction = RotateVector(tracking_to_anchor, tracking_direction);
    if (!IsFiniteVec3(local_origin) || !IsFiniteVec3(local_direction) ||
        local_direction.z >= -1.0e-5F) {
        return false;
    }

    const float ray_distance = (-plane_distance_m - local_origin.z) / local_direction.z;
    if (!std::isfinite(ray_distance) || ray_distance <= 0.0F) return false;
    const float hit_x = local_origin.x + local_direction.x * ray_distance;
    const float hit_y = local_origin.y + local_direction.y * ray_distance;

    const float tangent_left = 0.5F *
        (std::tan(left_fov.angle_left) + std::tan(right_fov.angle_left));
    const float tangent_right = 0.5F *
        (std::tan(left_fov.angle_right) + std::tan(right_fov.angle_right));
    const float tangent_up = 0.5F *
        (std::tan(left_fov.angle_up) + std::tan(right_fov.angle_up));
    const float tangent_down = 0.5F *
        (std::tan(left_fov.angle_down) + std::tan(right_fov.angle_down));
    const float x_min = tangent_left * plane_distance_m;
    const float x_max = tangent_right * plane_distance_m;
    const float y_min = tangent_down * plane_distance_m;
    const float y_max = tangent_up * plane_distance_m;
    const float x_span = x_max - x_min;
    const float y_span = y_max - y_min;
    if (!std::isfinite(x_span) || !std::isfinite(y_span) ||
        x_span <= 1.0e-5F || y_span <= 1.0e-5F) {
        return false;
    }

    const float full_u = (hit_x - x_min) / x_span;
    const float full_v = (y_max - hit_y) / y_span;
    if (!std::isfinite(full_u) || !std::isfinite(full_v)) return false;

    const float left_offset =
        static_cast<float>(texture_width - source_width) * 0.5F;
    const float top_offset =
        static_cast<float>(texture_height - source_height) * 0.5F;
    const float source_x = full_u * static_cast<float>(texture_width) - left_offset;
    const float source_y = full_v * static_cast<float>(texture_height) - top_offset;
    if (source_x < 0.0F || source_y < 0.0F ||
        source_x > static_cast<float>(source_width) ||
        source_y > static_cast<float>(source_height)) {
        return false;
    }

    projection.hit = true;
    projection.u = std::clamp(
        source_x / static_cast<float>(source_width), 0.0F, 1.0F);
    projection.v = std::clamp(
        source_y / static_cast<float>(source_height), 0.0F, 1.0F);
    projection.pixel_x = std::min(
        source_width - 1U,
        static_cast<std::uint32_t>(projection.u * static_cast<float>(source_width)));
    projection.pixel_y = std::min(
        source_height - 1U,
        static_cast<std::uint32_t>(projection.v * static_cast<float>(source_height)));
    projection.ray_distance_m = ray_distance;
    return true;
}

bool RigidTransform3x4FromPose(
    const Pose& pose,
    std::array<float, 12>& matrix) noexcept {
    matrix = {};
    if (!pose.orientation_valid || !pose.position_valid || !IsFiniteVec3(pose.position)) {
        return false;
    }

    const float length_squared =
        pose.orientation.x * pose.orientation.x +
        pose.orientation.y * pose.orientation.y +
        pose.orientation.z * pose.orientation.z +
        pose.orientation.w * pose.orientation.w;
    if (!std::isfinite(length_squared) || length_squared <= 1.0e-12F) return false;

    const Quaternion q = NormalizeQuaternion(pose.orientation);
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float xw = q.x * q.w;
    const float yw = q.y * q.w;
    const float zw = q.z * q.w;

    matrix = {
        1.0F - 2.0F * (yy + zz), 2.0F * (xy - zw), 2.0F * (xz + yw), pose.position.x,
        2.0F * (xy + zw), 1.0F - 2.0F * (xx + zz), 2.0F * (yz - xw), pose.position.y,
        2.0F * (xz - yw), 2.0F * (yz + xw), 1.0F - 2.0F * (xx + yy), pose.position.z,
    };
    return IsRigidRotation3x3(matrix);
}

EyeFov FovFromTangents(
    const float left,
    const float right,
    const float top,
    const float bottom) noexcept {
    return EyeFov{
        std::atan(left),
        std::atan(right),
        std::atan(top),
        std::atan(bottom),
    };
}

bool IsValidEyeFov(const EyeFov fov) noexcept {
    constexpr float kHalfPi = 1.57079632679F;
    return std::isfinite(fov.angle_left) &&
        std::isfinite(fov.angle_right) &&
        std::isfinite(fov.angle_up) &&
        std::isfinite(fov.angle_down) &&
        fov.angle_left < 0.0F && fov.angle_right > 0.0F &&
        fov.angle_down < 0.0F && fov.angle_up > 0.0F &&
        fov.angle_left > -kHalfPi && fov.angle_right < kHalfPi &&
        fov.angle_down > -kHalfPi && fov.angle_up < kHalfPi;
}

bool BuildReferenceProjectionMatrix(
    const EyeFov fov,
    const float near_plane,
    const float far_plane,
    std::array<float, 16>& matrix) noexcept {
    matrix = {};
    if (!IsValidEyeFov(fov) || !std::isfinite(near_plane) ||
        !std::isfinite(far_plane) || near_plane <= 0.0F ||
        far_plane <= near_plane) {
        return false;
    }

    const float left = std::tan(fov.angle_left) * near_plane;
    const float right = std::tan(fov.angle_right) * near_plane;
    const float bottom = std::tan(fov.angle_down) * near_plane;
    const float top = std::tan(fov.angle_up) * near_plane;
    const float width = right - left;
    const float height = top - bottom;
    const float depth = far_plane - near_plane;
    if (!std::isfinite(left) || !std::isfinite(right) ||
        !std::isfinite(bottom) || !std::isfinite(top) ||
        width <= 0.0F || height <= 0.0F || !std::isfinite(depth)) {
        return false;
    }

    matrix = {
        2.0F * near_plane / width, 0.0F, (right + left) / width, 0.0F,
        0.0F, 2.0F * near_plane / height, (top + bottom) / height, 0.0F,
        0.0F, 0.0F, -far_plane / depth, -(far_plane * near_plane) / depth,
        0.0F, 0.0F, -1.0F, 0.0F,
    };
    return std::all_of(matrix.begin(), matrix.end(), [](const float value) noexcept {
        return std::isfinite(value);
    });
}

} // namespace cojvr::runtime
