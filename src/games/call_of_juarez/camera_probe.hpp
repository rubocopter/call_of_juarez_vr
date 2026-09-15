#pragma once

#include "runtime/pose_source.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace cojvr::games::call_of_juarez {

inline constexpr std::string_view kInspectedChromeEngine3Sha256 =
    "DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8";

struct CameraProbeVector {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct CameraProbeBasis {
    CameraProbeVector forward{};
    CameraProbeVector up{};
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

[[nodiscard]] bool IsSupportedChromeEngineHash(std::string_view sha256) noexcept;

using CameraProbeEventCallback = void (*)(
    const char* event,
    const char* result,
    const char* detail) noexcept;

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
};

[[nodiscard]] CameraProbeInstallStatus InitializeCameraProbe(
    const std::filesystem::path& control_path,
    CameraProbeEventCallback callback,
    cojvr::runtime::PoseSource* pose_source = nullptr) noexcept;

[[nodiscard]] CameraProbeHookState InspectCameraProbe() noexcept;

void ShutdownCameraProbe() noexcept;

[[nodiscard]] const char* CameraProbeInstallStatusName(CameraProbeInstallStatus status) noexcept;

} // namespace cojvr::games::call_of_juarez
