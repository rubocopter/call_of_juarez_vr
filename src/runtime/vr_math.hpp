#pragma once

#include "runtime/vr_types.hpp"

#include <array>

namespace cojvr::runtime {

[[nodiscard]] Pose PoseFromRigidTransform3x4(
    const std::array<float, 12>& matrix) noexcept;

[[nodiscard]] Quaternion NormalizeQuaternion(Quaternion value) noexcept;
[[nodiscard]] Vec3 RotateVector(Quaternion rotation, Vec3 value) noexcept;

[[nodiscard]] EyeFov FovFromTangents(
    float left,
    float right,
    float top,
    float bottom) noexcept;

} // namespace cojvr::runtime
