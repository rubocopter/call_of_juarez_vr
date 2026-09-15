#include "runtime/vr_math.hpp"

#include <algorithm>
#include <cmath>

namespace cojvr::runtime {

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

    pose.orientation = NormalizeQuaternion(q);
    pose.orientation_valid = true;
    pose.position_valid = true;
    return pose;
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

} // namespace cojvr::runtime
