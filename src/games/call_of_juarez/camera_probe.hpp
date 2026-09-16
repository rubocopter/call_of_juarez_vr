#pragma once

#include "runtime/pose_source.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace cojvr::games::call_of_juarez {

inline constexpr std::string_view kInspectedChromeEngine3Sha256 =
    "DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8";

// Call of Juarez gameplay data documents movement in cm/s and cm/s^2.
// Shared XR/runtime poses stay in metres; conversion belongs in this game adapter.
inline constexpr float kGameUnitsPerMeter = 100.0F;

struct CameraProbeVector {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct CameraProbeBasis {
    CameraProbeVector forward{};
    CameraProbeVector up{};
    CameraProbeVector right{};
};

struct CameraProbeFrustum {
    float left = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;
    float top = 0.0F;
    float near_plane = 0.0F;
    float far_plane = 0.0F;
};

struct CameraProbeCommand {
    bool enabled = false;
    bool override_fov = false;
    float fov_degrees = 0.0F;
    float yaw_degrees = 0.0F;
    float pitch_degrees = 0.0F;
    bool tracking_enabled = false;
    bool recenter = false;
};

[[nodiscard]] bool ParseCameraProbeCommand(
    std::string_view text,
    CameraProbeCommand& command,
    std::string* error = nullptr) noexcept;

[[nodiscard]] CameraProbeBasis ApplyCameraProbeOrientation(
    CameraProbeVector forward,
    CameraProbeVector up,
    float yaw_degrees,
    float pitch_degrees) noexcept;

[[nodiscard]] CameraProbeBasis ApplyCameraPoseOrientation(
    CameraProbeVector forward,
    CameraProbeVector up,
    cojvr::runtime::Quaternion relative_orientation) noexcept;

[[nodiscard]] float CameraProbeBasisDeterminant(CameraProbeBasis basis) noexcept;

[[nodiscard]] bool IsCameraProbeBasisRigidRightHanded(CameraProbeBasis basis) noexcept;

[[nodiscard]] bool BuildCameraProbeFrustum(
    cojvr::runtime::EyeFov fov,
    float near_plane,
    float far_plane,
    CameraProbeFrustum& frustum) noexcept;

[[nodiscard]] CameraProbeVector ApplyCameraEyeOffset(
    CameraProbeVector head_position,
    CameraProbeBasis head_basis,
    cojvr::runtime::Vec3 eye_to_head_position) noexcept;

[[nodiscard]] bool IsSupportedChromeEngineHash(std::string_view sha256) noexcept;

using CameraProbeEventCallback = void (*)(
    const char* event,
    const char* result,
    const char* detail) noexcept;

struct CameraStereoFrameSample {
    cojvr::runtime::PoseSample hmd_pose{};
    std::array<cojvr::runtime::EyeView, 2> eyes{};
    bool recenter_requested = false;
};

// Runtime/transport boundary for the exact-build Chrome Engine integration.
// EyeView::pose is eye-to-head here; the game adapter never depends on an XR API.
struct CameraStereoRuntimeCallbacks {
    void* context = nullptr;
    bool (*begin_frame)(void* context, CameraStereoFrameSample& sample) noexcept = nullptr;
    bool (*capture_eye)(
        void* context,
        cojvr::runtime::Eye eye,
        std::uint64_t frame_sequence,
        std::uint64_t* content_hash) noexcept = nullptr;
    bool (*submit_frame)(
        void* context,
        std::uint64_t frame_sequence,
        const cojvr::runtime::PoseSample& render_hmd_pose) noexcept = nullptr;
};

enum class CameraProbeInstallStatus {
    installed,
    already_installed,
    host_rejected,
    engine_missing,
    engine_rejected,
    profile_mismatch,
    hook_failed,
};

struct CameraProbeHookState {
    bool initialized = false;
    bool installed = false;
    bool render_update_owned = false;
    bool set_fov_owned = false;
    bool render_view_owned = false;
};

[[nodiscard]] CameraProbeInstallStatus InitializeCameraProbe(
    const std::filesystem::path& control_path,
    CameraProbeEventCallback callback,
    cojvr::runtime::PoseSource* pose_source = nullptr,
    CameraStereoRuntimeCallbacks stereo_callbacks = {}) noexcept;

[[nodiscard]] CameraProbeHookState InspectCameraProbe() noexcept;

void ShutdownCameraProbe() noexcept;

[[nodiscard]] const char* CameraProbeInstallStatusName(CameraProbeInstallStatus status) noexcept;

} // namespace cojvr::games::call_of_juarez
