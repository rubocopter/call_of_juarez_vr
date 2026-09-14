#include "runtime/vr_math.hpp"

#include <algorithm>
#include <cmath>

namespace cojvr::runtime {

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

    const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (length > 0.0F) {
        const float inverse = 1.0F / length;
        q.x *= inverse;
        q.y *= inverse;
        q.z *= inverse;
        q.w *= inverse;
    }

    pose.orientation = q;
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
