#pragma once

#include "runtime/vr_types.hpp"

#include <array>
#include <cstdint>

namespace cojvr::runtime {

[[nodiscard]] Pose PoseFromRigidTransform3x4(
    const std::array<float, 12>& matrix) noexcept;

[[nodiscard]] bool RigidTransform3x4FromPose(
    const Pose& pose,
    std::array<float, 12>& matrix) noexcept;

[[nodiscard]] Quaternion NormalizeQuaternion(Quaternion value) noexcept;
[[nodiscard]] Vec3 RotateVector(Quaternion rotation, Vec3 value) noexcept;

struct FlatTheaterPointerProjection {
    bool hit = false;
    float u = 0.0F;
    float v = 0.0F;
    std::uint32_t pixel_x = 0;
    std::uint32_t pixel_y = 0;
    float ray_distance_m = 0.0F;
};

struct FlatTheaterEyePlacement {
    std::uint32_t left = 0;
    std::uint32_t top = 0;
};

// Projects the head-centered flat-theater screen onto one runtime eye. The
// source image keeps its native pixel extent inside the larger eye texture,
// while eye-to-head translation and asymmetric FOV move its center to the
// location corresponding to a finite plane in front of the anchored HMD.
[[nodiscard]] bool ComputeFlatTheaterEyePlacement(
    const EyeView& eye,
    std::uint32_t source_width,
    std::uint32_t source_height,
    std::uint32_t texture_width,
    std::uint32_t texture_height,
    FlatTheaterEyePlacement& placement,
    float plane_distance_m = 1.5F) noexcept;

// Intersects a tracked controller aim ray with the virtual flat-theater plane.
// The plane is fixed relative to the HMD pose captured when flat presentation
// is anchored. The centered source rectangle matches the black-border layout
// used by the OpenVR presenter, so returned UV/pixel coordinates address the
// original 2D game backbuffer rather than the larger compositor texture.
[[nodiscard]] bool ProjectFlatTheaterPointer(
    const Pose& anchor_pose,
    const Pose& aim_pose,
    EyeFov left_fov,
    EyeFov right_fov,
    std::uint32_t source_width,
    std::uint32_t source_height,
    std::uint32_t texture_width,
    std::uint32_t texture_height,
    FlatTheaterPointerProjection& projection,
    float plane_distance_m = 1.5F) noexcept;

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
