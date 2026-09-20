#pragma once

#include "runtime/openvr_state.hpp"
#include "runtime/vr_types.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

namespace cojvr::runtime {

struct OpenVrSystemInfo {
    std::uint32_t recommended_width = 0;
    std::uint32_t recommended_height = 0;
    std::int32_t dxgi_adapter_index = -1;
};

struct OpenVrGlobalActions {
    bool recenter_requested = false;
    bool recenter_active = false;
    bool ui_select_left = false;
    bool ui_select_right = false;
    bool ui_select_left_pressed = false;
    bool ui_select_right_pressed = false;
};

struct OpenVrHandPoses {
    Pose left_grip{};
    Pose right_grip{};
    Pose left_aim{};
    Pose right_aim{};
    bool left_grip_active = false;
    bool right_grip_active = false;
    bool left_aim_active = false;
    bool right_aim_active = false;
};

struct OpenVrTrackedPoses {
    Pose hmd{};
    Pose left_controller{};
    Pose right_controller{};
    bool left_controller_connected = false;
    bool right_controller_connected = false;
};

struct OpenVrPresentationState {
    std::uint32_t process_id = 0;
    std::uint32_t scene_focus_process_id = 0;
    bool can_render_scene = false;
    bool input_available = false;
    bool dashboard_visible = false;
    bool should_pause = false;
    bool should_reduce_rendering_work = false;
};

// Deterministic press-edge semantics, kept independent from IVRInput so this
// part can be host-tested without a running VR runtime or controller.
class OpenVrDigitalActionEdge final {
public:
    [[nodiscard]] bool Update(bool active, bool pressed) noexcept;
    void Reset() noexcept;

private:
    bool pressed_ = false;
};

// OpenVR GetProjectionRaw uses screen-space vertical signs: top is negative and
// bottom is positive. Convert that API-specific convention into the neutral
// EyeFov contract where up is positive and down is negative.
[[nodiscard]] EyeFov OpenVrProjectionRawToEyeFov(
    float left,
    float right,
    float top,
    float bottom) noexcept;

class OpenVrRuntime final {
public:
    OpenVrRuntime();
    ~OpenVrRuntime();

    OpenVrRuntime(const OpenVrRuntime&) = delete;
    OpenVrRuntime& operator=(const OpenVrRuntime&) = delete;
    OpenVrRuntime(OpenVrRuntime&&) noexcept;
    OpenVrRuntime& operator=(OpenVrRuntime&&) noexcept;

    [[nodiscard]] bool Initialize(
        std::string_view application_name = "Call of Juarez VR") noexcept;
    void Shutdown() noexcept;
    // OpenVR-specific static optics. EyeView::eye_to_head is the eye-to-head transform
    // returned by GetEyeToHeadTransform; it is not an absolute tracking pose.
    [[nodiscard]] bool ReadEyeConfiguration(std::array<EyeView, 2>& eyes) noexcept;
    // Non-blocking tracking sample that does not enter the compositor frame
    // lifecycle. Use this while a scene application has no frame ready to
    // submit so merely polling the HMD does not claim scene focus early.
    [[nodiscard]] bool ReadHmdPose(Pose& pose) noexcept;
    [[nodiscard]] bool WaitForHmdPose(Pose& pose) noexcept;
    // HMD and controller-role poses from one OpenVR tracking sample. Controller
    // poses remain neutral runtime poses; game integrations decide how they map
    // onto hands, weapons or other body anchors.
    [[nodiscard]] bool ReadTrackedPoses(OpenVrTrackedPoses& poses) noexcept;
    [[nodiscard]] bool WaitForTrackedPoses(OpenVrTrackedPoses& poses) noexcept;
    // Drain runtime events and refresh connection/focus state. A runtime quit
    // request is represented in state() and left to the owning thread to tear
    // down outside loader-lock-sensitive contexts.
    [[nodiscard]] bool ProcessEvents() noexcept;
    void PostPresentHandoff() noexcept;
    [[nodiscard]] bool ReadPresentationState(OpenVrPresentationState& state) noexcept;
    [[nodiscard]] bool InitializeGlobalActions(
        std::string_view absolute_manifest_path) noexcept;
    [[nodiscard]] bool PollGlobalActions(OpenVrGlobalActions& actions) noexcept;
    [[nodiscard]] bool PollActions(
        OpenVrGlobalActions& global_actions,
        GameplayInputState& gameplay_actions,
        OpenVrHandPoses& hand_poses) noexcept;
    [[nodiscard]] bool global_actions_initialized() const noexcept;
    void RecordEyeSubmission(Eye eye, bool succeeded) noexcept;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] OpenVrRuntimeState state() const noexcept;
    [[nodiscard]] const OpenVrSystemInfo& system_info() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
    [[nodiscard]] std::int32_t last_result_code() const noexcept;

    // Native handles are exposed only to renderer adapters that must bridge
    // runtime-owned tracking/session state to API-specific texture submission.
    [[nodiscard]] void* native_system() const noexcept;
    [[nodiscard]] void* native_compositor() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cojvr::runtime
