#pragma once

#include "runtime/vr_types.hpp"

#include <array>

namespace cojvr::runtime {

[[nodiscard]] Pose PoseFromRigidTransform3x4(
    const std::array<float, 12>& matrix) noexcept;

[[nodiscard]] bool RigidTransform3x4FromPose(
    const Pose& pose,
    std::array<float, 12>& matrix) noexcept;

[[nodiscard]] Quaternion NormalizeQuaternion(Quaternion value) noexcept;
[[nodiscard]] Vec3 RotateVector(Quaternion rotation, Vec3 value) noexcept;

[[nodiscard]] EyeFov FovFromTangents(
    float left,
    float right,
    float top,
    float bottom) noexcept;

[[nodiscard]] bool IsValidEyeFov(EyeFov fov) noexcept;

// Row-major, column-vector reference projection for the neutral convention:
// right-handed +X/+Y/-Z view space and D3D-style NDC z in [0, 1]. This helper
// exists to lock the FOV semantic contract; renderer/game adapters may use a
// different native matrix layout after an explicit conversion.
[[nodiscard]] bool BuildReferenceProjectionMatrix(
    EyeFov fov,
    float near_plane,
    float far_plane,
    std::array<float, 16>& matrix) noexcept;

} // namespace cojvr::runtime
