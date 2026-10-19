#pragma once
#include "games/call_of_juarez/hud_boundary_probe.hpp"
#include "games/call_of_juarez/presentation_owner.hpp"
#include "games/call_of_juarez/ui_language.hpp"

#include "runtime/pose_source.hpp"
#include "runtime/hud_text.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace cojvr::games::call_of_juarez {

enum class CoJUiDispatchRoute;

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

struct CameraProbeReticlePoint {
    bool valid = false;
    float u = 0.0F;
    float v = 0.0F;
};

struct CameraWeaponReticle {
    bool active = false;
    bool no_shoot = false;
    std::array<CameraProbeReticlePoint, 2> eyes{};
};

struct CameraStereoReticleOverlay {
    std::uint64_t frame_sequence = 0;
    // Stable hand order: right, left. Each marker has a complete eye pair.
    std::array<CameraWeaponReticle, 2> weapons{};
};

struct CameraProbeCommand {
    bool enabled = false;
    bool override_fov = false;
    float fov_degrees = 0.0F;
    float yaw_degrees = 0.0F;
    float pitch_degrees = 0.0F;
    bool tracking_enabled = false;
    bool body_ik_enabled = false;
    bool independent_hands_enabled = false;
    bool recenter = false;
    // Diagnostic controls are opt-in and preserve the production path when
    // omitted from the control file.
    bool movement_trace_enabled = false;
    bool reload_trace_enabled = false;
    bool reload_wait_probe_installed = false; // Archive identity marker; observation only.
    bool manual_reload_installed = false; // Explicit exact archive variant; default off.
    std::string movement_trace_phase = "off";
    bool vr_gameplay_input_enabled = true;
    bool capture_readback_enabled = true;
    bool second_eye_render_enabled = true;
    bool render_resource_probe_enabled = false;
    bool frame_luma_probe_enabled = false; // Startup-only read-only flash diagnosis.
    // Read once by the renderer at startup; 0 disables, width fixed at 2560.
    std::uint32_t independent_eye_height = 0;
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
    cojvr::runtime::Quaternion relative_orientation,
    float actor_yaw_compensation_degrees = 0.0F) noexcept;

// Produces the level, recenter-space basis used for controllers and body
// targets. Natural camera yaw already includes actor-owned HMD rotation, so
// remove that owned component before mapping fixed tracking-space poses.
[[nodiscard]] CameraProbeBasis BuildTrackingReferenceBasis(
    CameraProbeVector forward,
    CameraProbeVector up,
    float actor_owned_yaw_degrees) noexcept;

[[nodiscard]] float CameraProbeBasisDeterminant(CameraProbeBasis basis) noexcept;

[[nodiscard]] bool IsCameraProbeBasisRigidRightHanded(CameraProbeBasis basis) noexcept;

// Movement diagnostics become valid only after the renderer owns a fully
// formed natural camera. This prevents loading/menu player objects from being
// published as gameplay actors before the camera transition has completed.
[[nodiscard]] bool IsMovementTraceCameraReady(
    CameraProbeBasis basis,
    bool renderer_camera_match,
    bool source_world_homogeneous_layout,
    bool source_view_homogeneous_layout) noexcept;

// A loss of ODE ground contact is only a jump candidate. The exact game keeps
// IsJumping true for the complete accepted jump, including its landing settle,
// so diagnostics require that native confirmation before emitting an event.
enum class CoJMovementJumpPhase {
    grounded_stable,
    awaiting_native_confirmation,
    airborne_ascending,
    airborne_descending,
    landed_waiting_release,
};

struct CoJMovementJumpTransition {
    CoJMovementJumpPhase phase = CoJMovementJumpPhase::grounded_stable;
    bool begin_candidate = false;
    bool confirm_jump = false;
    bool reject_candidate = false;
    bool reached_apex = false;
    bool complete_jump = false;
};

[[nodiscard]] CoJMovementJumpTransition AdvanceCoJMovementJumpPhase(
    CoJMovementJumpPhase phase,
    bool grounded,
    bool jumping,
    float native_vertical_speed) noexcept;

[[nodiscard]] bool BuildCameraProbeFrustum(
    cojvr::runtime::EyeFov fov,
    float near_plane,
    float far_plane,
    CameraProbeFrustum& frustum) noexcept;

struct CameraProbeWeaponRay {
    CameraProbeVector origin{};
    CameraProbeVector direction{};
    bool origin_valid = false;
    bool direction_valid = false;
    bool muzzle_verified = false;
    bool whip_aim_verified = false;
};

struct CameraProbeInteractionRay {
    CameraProbeVector origin{};
    CameraProbeVector direction{};
    bool valid = false;
};

// Uses the central render pose, including native crouch-height correction.
// The exact source forward axis points backwards from the visible gaze.
[[nodiscard]] CameraProbeInteractionRay BuildTrackedInteractionRay(
    CameraProbeVector camera_position, CameraProbeBasis natural_basis,
    const cojvr::runtime::Pose& render_head_pose,
    float actor_yaw_compensation_degrees) noexcept;

// Hand pose stays in the shared recentered tracking space. Apply only the
// render head's collision/height translation delta, never its look orientation.
[[nodiscard]] CameraProbeInteractionRay BuildTrackedHandInteractionRay(
    CameraProbeVector camera_position, CameraProbeBasis natural_basis,
    const cojvr::runtime::Pose& tracked_head, const cojvr::runtime::Pose& render_head,
    const cojvr::runtime::Pose& left_aim, float actor_yaw_compensation_degrees) noexcept;

// Missing publication/pose/button or owner loss requires a real L1 release.
// Prevent a held button from falling back to gaze or retargeting after recovery.
class CoJInteractionActionGate final {
public:
    [[nodiscard]] bool Update(bool ray_available,bool button_available,
        bool raw_held,bool mapped_held) noexcept;
    void Cancel() noexcept { release_required_=true; }
private:
    bool release_required_=true;
};

enum class CoJWeaponReticleHands { none, right, left, both };

// Tip tracking alone cannot establish weapon alignment. Select only a published
// verified muzzle or the owned procedural whip ray; the whip keeps its native
// two-trigger semantics. Both verified firearms remain visible independently
// of which trigger is held or which weapon last fired.
[[nodiscard]] CoJWeaponReticleHands SelectWeaponReticleHands(
    const CameraProbeWeaponRay& left,
    const CameraProbeWeaponRay& right) noexcept;

// Projects a world-space weapon ray onto a camera-facing plane at the supplied
// eye-space depth and then through the exact asymmetric eye frustum. Output UV
// coordinates use the presenter's top-left origin.
[[nodiscard]] bool ProjectWorldAimRayToEyeReticle(
    CameraProbeVector ray_origin,
    CameraProbeVector ray_direction,
    CameraProbeVector eye_position,
    CameraProbeBasis eye_basis,
    CameraProbeFrustum frustum,
    float projection_depth,
    CameraProbeReticlePoint& reticle) noexcept;

[[nodiscard]] CameraProbeVector ApplyCameraEyeOffset(
    CameraProbeVector head_position,
    CameraProbeBasis head_basis,
    cojvr::runtime::Vec3 eye_to_head_position) noexcept;

[[nodiscard]] CameraProbeVector ApplyCameraTrackingOffset(
    CameraProbeVector camera_position,
    CameraProbeBasis recenter_basis,
    cojvr::runtime::Vec3 relative_head_position) noexcept;

// When the native actor cannot be proven/reconciled, keep HMD orientation and
// pose validity but suppress room-scale translation. Stereo eye-to-head
// translation is applied later and remains active.
[[nodiscard]] cojvr::runtime::Pose SuppressPhysicalTrackingTranslation(
    cojvr::runtime::Pose pose) noexcept;

// A failed restore keeps its captured natural basis active so later frames and
// shutdown can retry the transaction instead of abandoning a shifted skeleton.
[[nodiscard]] bool ShouldRetainCoJBodyRestoreTransaction(bool restored) noexcept;

[[nodiscard]] bool IsSupportedChromeEngineHash(std::string_view sha256) noexcept;
struct CoJPostprocessResourceSnapshot;
[[nodiscard]] CoJPostprocessResourceSnapshot ObserveInstalledCoJPostprocessResources() noexcept;

using CameraProbeEventCallback = void (*)(
    const char* event,
    const char* result,
    const char* detail) noexcept;

struct CameraStereoFrameSample {
    cojvr::runtime::PoseSample hmd_pose{};
    // Active native gameplay availability/epoch, independent of texture uploads.
    std::uint64_t native_gameplay_context_token = 0;
    cojvr::runtime::Pose left_controller{};
    cojvr::runtime::Pose right_controller{};
    cojvr::runtime::Pose left_aim{};
    cojvr::runtime::Pose right_aim{};
    cojvr::runtime::FingerTrackingState left_fingers{};
    cojvr::runtime::FingerTrackingState right_fingers{};
    cojvr::runtime::GameplayInputState gameplay{};
    std::array<cojvr::runtime::EyeView, 2> eyes{};
    bool recenter_requested = false;
    bool objectives_requested = false;
    bool ui_select_left = false;
    bool ui_select_right = false;
    bool ui_select_left_pressed = false;
    bool ui_select_right_pressed = false;
    bool ui_accept = false;
    bool ui_back = false;
    bool ui_accept_pressed = false;
    bool ui_back_pressed = false;
    bool pause_pressed = false;
    // Runtime settings opt in only; the owner thread rechecks native eligibility.
    bool cinematic_immersive = false;
    bool settings_input_captured = false;
};

struct CameraStereoDiagnosticCounters {
    std::uint64_t frames_fenced = 0;
    std::uint64_t frames_collected = 0;
    std::uint64_t frames_uploaded = 0;
    std::uint64_t new_submissions = 0;
    std::uint64_t repeat_submissions = 0;
};

// Exact-game UI input seams. Cross/global accept keeps the shipped Enter
// helper. Ray-trigger selection uses the current native mouse context.
[[nodiscard]] bool DispatchCameraUiSelectPress(
    bool require_loading_ui,
    bool* current_ui_is_loading = nullptr,
    std::string* error = nullptr,
    bool* paused_hint_dismissed = nullptr,
    CoJUiDispatchRoute* route = nullptr) noexcept;
[[nodiscard]] bool DispatchCameraUiPointerSelectPress(
    bool require_loading_ui,
    bool* current_ui_is_loading = nullptr,
    std::string* error = nullptr,
    bool* paused_hint_dismissed = nullptr,
    CoJUiDispatchRoute* route = nullptr) noexcept;
[[nodiscard]] bool DispatchCameraUiPointerMotion(
    float pixel_x,
    float pixel_y,
    std::string* error = nullptr,
    bool* input_consumed = nullptr) noexcept;
// Positive existing global MenuMain observation on the game thread.
[[nodiscard]] bool ObserveCameraMainMenuActive(std::uint32_t* owner = nullptr) noexcept;
// Read CoJ's logical menu cursor on the game thread, without delivering input.
// Unavailable/failed observations clear position.
[[nodiscard]] bool ObserveCameraUiPointerPosition(
    CameraProbeVector& position,
    std::string* error = nullptr,
    std::int32_t* ui_index = nullptr) noexcept;
[[nodiscard]] bool DispatchCameraUiBackPress(
    std::string* error = nullptr,
    CoJUiDispatchRoute* route = nullptr) noexcept;
[[nodiscard]] bool ObserveCameraGameTimerFrozen(
    bool& frozen,
    std::string* error = nullptr) noexcept;
[[nodiscard]] bool ObserveCameraGamePresentationBlocked(
    bool& blocked,
    std::string* error = nullptr) noexcept;
[[nodiscard]] bool ObserveCameraGamePresentationOwner(
    CoJPresentationOwner& owner,
    std::string* error = nullptr) noexcept;
// Owner-thread cached observations; these never refresh native objects.
[[nodiscard]] CoJUiLanguage ObserveCameraUiLanguage() noexcept;
[[nodiscard]] std::uint64_t ObserveCameraPlayerGeneration() noexcept;

[[nodiscard]] const char* CoJSubtitleDiagnosticStateName(
    bool observation_available,
    bool subtitles_enabled,
    bool dialog_playing,
    bool dialog_subtitle_visible) noexcept;

// Runtime/transport boundary for the exact-build Chrome Engine integration.
// EyeView::eye_to_head is static optics; the game adapter never depends on an XR API.
struct CameraStereoRuntimeCallbacks {
    void* context = nullptr;
    // Failed native restoration invalidates a retained pair immediately;
    // transient capture backpressure alone may retain ordinary fresh output.
    void (*invalidate_frame)(void* context) noexcept = nullptr;
    bool (*begin_frame)(void* context, CameraStereoFrameSample& sample) noexcept = nullptr;
    bool (*capture_eye)(
        void* context,
        cojvr::runtime::Eye eye,
        std::uint64_t frame_sequence,
        std::uint64_t* content_hash) noexcept = nullptr;
    bool (*submit_frame)(
        void* context,
        std::uint64_t frame_sequence,
        const cojvr::runtime::PoseSample& render_hmd_pose,
        const CameraStereoReticleOverlay& gameplay_reticle,
        const runtime::StereoHudTextOverlay& hud_text) noexcept = nullptr;
    void (*set_capture_readback_enabled)(void* context, bool enabled) noexcept = nullptr;
    bool (*diagnostic_counters)(
        void* context,
        CameraStereoDiagnosticCounters& counters) noexcept = nullptr;
    void (*observe_hud_boundary)(void* context, const CoJHudBoundaryEvent& event,
        std::uint64_t frame_sequence) noexcept = nullptr;
    // Latest observed reload-owner epoch; 0 invalidates queued optional feedback.
    void (*publish_reload_feedback_owner)(void* context, std::uint64_t token) noexcept = nullptr;
    // Read-only renderer observation. No depth buffer ownership/admission.
    void (*observe_reload_depth)(void* context,runtime::Eye eye,std::uint64_t frame_sequence) noexcept = nullptr;
    // Diagnostic scope around the exact full wrapper only; no capture or fallback replay.
    void (*observe_render_pass)(void*, runtime::Eye, std::uint64_t, std::uintptr_t,
        bool begin, bool complete, const char* route) noexcept = nullptr;
    // Optional resource transaction around the full wrapper AND eye capture.
    // Install begin/end together. End is called even after a rejected begin;
    // False invalidates the pair. The pending query below separately decides
    // whether rollback must be retried with the same eye/frame before rendering.
    // Without that query, false conservatively retains restoration for retry.
    bool (*begin_eye_render)(void*, runtime::Eye, std::uint64_t frame) noexcept = nullptr;
    bool (*end_eye_render)(void*, runtime::Eye, std::uint64_t frame) noexcept = nullptr;
    // Called only after both eyes and camera/body/hand/weapon restoration,
    // before submission and the native caller's sprite pass. False invalidates
    // the pair and calls end(right,frame) to prove rollback before fallback.
    // A mirror-only callback must restore its own state even on failure.
    bool (*mirror_eye_pair)(void*, std::uint64_t frame) noexcept = nullptr;
    // Authoritative native/GPU quarantine, including failed Begin/Mirror.
    // True forbids natural world replay even if End reported success.
    bool (*eye_render_restoration_pending)(void*) noexcept = nullptr;
    // False revokes a desktop source; true follows a completed natural wrapper.
    void (*observe_natural_render)(void*,bool complete) noexcept = nullptr;
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
