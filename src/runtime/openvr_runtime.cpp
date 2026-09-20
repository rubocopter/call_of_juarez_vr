#include "runtime/openvr_runtime.hpp"
#include "runtime/vr_math.hpp"
#include <openvr.h>
#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <windows.h>

namespace cojvr::runtime {
namespace {

std::array<float, 12> Flatten(const vr::HmdMatrix34_t& matrix) noexcept {
    std::array<float, 12> values{};
    std::copy_n(&matrix.m[0][0], values.size(), values.begin());
    return values;
}

std::string InitializationError(const vr::EVRInitError error_code) {
    const char* description = vr::VR_GetVRInitErrorAsEnglishDescription(error_code);
    if (description == nullptr || description[0] == '\0') {
        return "OpenVR initialization failed with code " + std::to_string(static_cast<int>(error_code));
    }
    return std::string("OpenVR initialization failed: ") + description;
}

OpenVrProcessOwnerGate g_openvr_owner_gate;

bool ClaimProcessOwner(void* owner) noexcept {
    return g_openvr_owner_gate.TryClaim(owner);
}

void ReleaseProcessOwner(void* owner) noexcept {
    g_openvr_owner_gate.Release(owner);
}

} // namespace

EyeFov OpenVrProjectionRawToEyeFov(
    const float left,
    const float right,
    const float top,
    const float bottom) noexcept {
    return FovFromTangents(left, right, -top, -bottom);
}

struct OpenVrRuntime::Impl {
    vr::IVRSystem* system = nullptr;
    vr::IVRCompositor* compositor = nullptr;
    vr::IVRInput* input = nullptr;
    vr::VRActionSetHandle_t global_action_set = vr::k_ulInvalidActionSetHandle;
    vr::VRActionSetHandle_t gameplay_action_set = vr::k_ulInvalidActionSetHandle;
    vr::VRActionHandle_t recenter_action = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t ui_select_left_action = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t ui_select_right_action = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t left_hand_grip_pose_action = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t right_hand_grip_pose_action = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t left_hand_aim_pose_action = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t right_hand_aim_pose_action = vr::k_ulInvalidActionHandle;
    std::array<vr::VRActionHandle_t, 12> gameplay_actions{};
    OpenVrDigitalActionEdge recenter_edge{};
    OpenVrDigitalActionEdge ui_select_left_edge{};
    OpenVrDigitalActionEdge ui_select_right_edge{};
    OpenVrSystemInfo system_info{};
    OpenVrStateTracker state_tracker{};
    bool owns_process_runtime = false;
    std::string last_error;
    std::int32_t last_result_code = 0;
    mutable std::mutex events_mutex; // Serializes ProcessEvents calls
};

template <typename ImplT>
bool FailNoThrow(
    ImplT* impl,
    const char* message,
    const std::int32_t result_code = -1) noexcept {
    if (!impl) return false;
    impl->last_result_code = result_code;
    try {
        impl->last_error = message ? message : "OpenVR operation failed";
    } catch (...) {
        try {
            impl->last_error.clear();
        } catch (...) {
        }
    }
    return false;
}

Pose PoseFromTrackedDevice(const vr::TrackedDevicePose_t& tracked) noexcept {
    Pose pose = PoseFromRigidTransform3x4(Flatten(tracked.mDeviceToAbsoluteTracking));
    const bool valid = tracked.bDeviceIsConnected && tracked.bPoseIsValid;
    pose.orientation_valid = valid && pose.orientation_valid;
    pose.position_valid = valid && pose.position_valid;
    return pose;
}

void PopulateControllerRolePoses(
    vr::IVRSystem* system,
    const std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount>& device_poses,
    OpenVrTrackedPoses& poses) noexcept {
    if (!system) return;

    const auto populate = [&](const vr::ETrackedControllerRole role, Pose& target, bool& connected) {
        const vr::TrackedDeviceIndex_t index = system->GetTrackedDeviceIndexForControllerRole(role);
        connected = index != vr::k_unTrackedDeviceIndexInvalid &&
            index < device_poses.size() && system->IsTrackedDeviceConnected(index);
        if (!connected) {
            target = {};
            return;
        }
        target = PoseFromTrackedDevice(device_poses[index]);
    };
    populate(vr::TrackedControllerRole_LeftHand, poses.left_controller,
             poses.left_controller_connected);
    populate(vr::TrackedControllerRole_RightHand, poses.right_controller,
             poses.right_controller_connected);
}

bool OpenVrDigitalActionEdge::Update(const bool active, const bool pressed) noexcept {
    const bool current = active && pressed;
    const bool rising = current && !pressed_;
    pressed_ = current;
    return rising;
}

void OpenVrDigitalActionEdge::Reset() noexcept { pressed_ = false; }

OpenVrRuntime::OpenVrRuntime() : impl_(std::make_unique<Impl>()) {}
OpenVrRuntime::~OpenVrRuntime() { Shutdown(); }
OpenVrRuntime::OpenVrRuntime(OpenVrRuntime&& other) noexcept
    : impl_(std::move(other.impl_)) {}

OpenVrRuntime& OpenVrRuntime::operator=(OpenVrRuntime&& other) noexcept {
    if (this == &other) return *this;
    Shutdown();
    impl_ = std::move(other.impl_);
    // Invalidate ownership in moved-from object to prevent double shutdown
    if (other.impl_) {
        other.impl_->owns_process_runtime = false;
    }
    return *this;
}

bool OpenVrRuntime::Initialize(const std::string_view application_name) noexcept {
    if (!impl_) return false;
    try {
        impl_->last_error.clear();
        impl_->last_result_code = 0;
        if (initialized()) {
            impl_->last_error = "OpenVR is already initialized";
            impl_->last_result_code = -1;
            return false;
        }
        impl_->state_tracker.BeginInitialize();

        const std::string application_name_copy(application_name);
        if (!ClaimProcessOwner(impl_.get())) {
            impl_->last_error = "OpenVR process runtime is already owned by another OpenVrRuntime";
            impl_->last_result_code = -1;
            impl_->state_tracker.InitializationFailed();
            return false;
        }
        impl_->owns_process_runtime = true;

        vr::EVRInitError init_error = vr::VRInitError_None;
        vr::IVRSystem* system = vr::VR_Init(
            &init_error, vr::VRApplication_Scene, application_name_copy.c_str());
        if (init_error != vr::VRInitError_None || system == nullptr) {
            impl_->last_result_code = static_cast<std::int32_t>(init_error);
            impl_->last_error = InitializationError(init_error);
            if (system != nullptr) vr::VR_Shutdown();
            ReleaseProcessOwner(impl_.get());
            impl_->owns_process_runtime = false;
            impl_->state_tracker.InitializationFailed();
            return false;
        }

        vr::IVRCompositor* compositor = vr::VRCompositor();
        if (compositor == nullptr) {
            impl_->last_error = "OpenVR compositor interface is unavailable";
            impl_->last_result_code = -1;
            vr::VR_Shutdown();
            ReleaseProcessOwner(impl_.get());
            impl_->owns_process_runtime = false;
            impl_->state_tracker.InitializationFailed();
            return false;
        }
        compositor->SetTrackingSpace(vr::TrackingUniverseStanding);

        OpenVrSystemInfo info{};
        system->GetRecommendedRenderTargetSize(&info.recommended_width, &info.recommended_height);
        system->GetDXGIOutputInfo(&info.dxgi_adapter_index);
        if (info.recommended_width == 0 || info.recommended_height == 0) {
            impl_->last_error = "OpenVR returned an empty recommended render-target size";
            impl_->last_result_code = -1;
            vr::VR_Shutdown();
            ReleaseProcessOwner(impl_.get());
            impl_->owns_process_runtime = false;
            impl_->state_tracker.InitializationFailed();
            return false;
        }

        impl_->system = system;
        impl_->compositor = compositor;
        impl_->system_info = info;
        impl_->state_tracker.InitializationSucceeded(
            system->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd));
        (void)ProcessEvents();
        return true;
    } catch (...) {
        if (impl_->owns_process_runtime) {
            if (impl_->system != nullptr) vr::VR_Shutdown();
            ReleaseProcessOwner(impl_.get());
        }
        impl_->system = nullptr;
        impl_->compositor = nullptr;
        impl_->owns_process_runtime = false;
        impl_->state_tracker.InitializationFailed();
        impl_->last_result_code = -1;
        try {
            impl_->last_error = "OpenVR initialization raised an exception";
        } catch (...) {
        }
        return false;
    }
}

void OpenVrRuntime::Shutdown() noexcept {
    if (!impl_) return;
    if (impl_->system != nullptr) impl_->state_tracker.ShutdownRequested();
    if (impl_->owns_process_runtime) vr::VR_Shutdown();
    if (impl_->owns_process_runtime) ReleaseProcessOwner(impl_.get());
    impl_->system = nullptr;
    impl_->compositor = nullptr;
    impl_->input = nullptr;
    impl_->global_action_set = vr::k_ulInvalidActionSetHandle;
    impl_->gameplay_action_set = vr::k_ulInvalidActionSetHandle;
    impl_->recenter_action = vr::k_ulInvalidActionHandle;
    impl_->ui_select_left_action = vr::k_ulInvalidActionHandle;
    impl_->ui_select_right_action = vr::k_ulInvalidActionHandle;
    impl_->left_hand_grip_pose_action = vr::k_ulInvalidActionHandle;
    impl_->right_hand_grip_pose_action = vr::k_ulInvalidActionHandle;
    impl_->left_hand_aim_pose_action = vr::k_ulInvalidActionHandle;
    impl_->right_hand_aim_pose_action = vr::k_ulInvalidActionHandle;
    impl_->gameplay_actions.fill(vr::k_ulInvalidActionHandle);
    impl_->recenter_edge.Reset();
    impl_->ui_select_left_edge.Reset();
    impl_->ui_select_right_edge.Reset();
    impl_->system_info = {};
    impl_->owns_process_runtime = false;
    impl_->state_tracker.ShutdownComplete();
}

bool OpenVrRuntime::ReadEyeConfiguration(std::array<EyeView, 2>& eyes) noexcept {
    eyes = {};
    if (!impl_) return false;
    try {
        impl_->last_error.clear();
        if (!initialized()) {
            return FailNoThrow(impl_.get(), "OpenVR is not initialized");
        }

        constexpr std::array<vr::EVREye, 2> native_eyes{vr::Eye_Left, vr::Eye_Right};
        constexpr std::array<Eye, 2> logical_eyes{Eye::left, Eye::right};
        for (std::size_t index = 0; index < native_eyes.size(); ++index) {
            float left = 0.0F;
            float right = 0.0F;
            float top = 0.0F;
            float bottom = 0.0F;
            impl_->system->GetProjectionRaw(native_eyes[index], &left, &right, &top, &bottom);

            EyeView& eye = eyes[index];
            eye.eye = logical_eyes[index];
            eye.eye_to_head = PoseFromRigidTransform3x4(
                Flatten(impl_->system->GetEyeToHeadTransform(native_eyes[index])));
            eye.fov = OpenVrProjectionRawToEyeFov(left, right, top, bottom);
            eye.width = impl_->system_info.recommended_width;
            eye.height = impl_->system_info.recommended_height;
            if (!eye.eye_to_head.orientation_valid || !eye.eye_to_head.position_valid ||
                !IsValidEyeFov(eye.fov)) {
                eyes = {};
                return FailNoThrow(
                    impl_.get(),
                    "OpenVR returned invalid/non-finite eye optics");
            }
        }
        return true;
    } catch (...) {
        return FailNoThrow(impl_.get(), "OpenVR eye-configuration read raised an exception");
    }
}

bool OpenVrRuntime::WaitForHmdPose(Pose& pose) noexcept {
    OpenVrTrackedPoses poses{};
    const bool result = WaitForTrackedPoses(poses);
    pose = poses.hmd;
    return result;
}

bool OpenVrRuntime::ReadHmdPose(Pose& pose) noexcept {
    OpenVrTrackedPoses poses{};
    const bool result = ReadTrackedPoses(poses);
    pose = poses.hmd;
    return result;
}

bool OpenVrRuntime::WaitForTrackedPoses(OpenVrTrackedPoses& poses) noexcept {
    poses = {};
    if (!impl_) return false;
    try {
        impl_->last_error.clear();
        impl_->last_result_code = 0;
        if (!initialized()) return FailNoThrow(impl_.get(), "OpenVR is not initialized");
        (void)ProcessEvents();
        if (impl_->state_tracker.state().shutdown_requested ||
            !impl_->state_tracker.state().connected) {
            impl_->state_tracker.TrackingChanged(false);
            return FailNoThrow(
                impl_.get(),
                impl_->state_tracker.state().shutdown_requested
                    ? "OpenVR runtime requested shutdown"
                    : "OpenVR HMD is disconnected");
        }

        std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> device_poses{};
        const vr::EVRCompositorError error = impl_->compositor->WaitGetPoses(
            device_poses.data(), static_cast<std::uint32_t>(device_poses.size()), nullptr, 0);
        if (error != vr::VRCompositorError_None) {
            impl_->state_tracker.TrackingChanged(false);
            return FailNoThrow(
                impl_.get(), "OpenVR WaitGetPoses failed", static_cast<std::int32_t>(error));
        }

        poses.hmd = PoseFromTrackedDevice(device_poses[vr::k_unTrackedDeviceIndex_Hmd]);
        PopulateControllerRolePoses(impl_->system, device_poses, poses);
        impl_->state_tracker.TrackingChanged(
            poses.hmd.orientation_valid && poses.hmd.position_valid);
        return true;
    } catch (...) {
        impl_->state_tracker.TrackingChanged(false);
        return FailNoThrow(impl_.get(), "OpenVR tracked-pose wait raised an exception");
    }
}

bool OpenVrRuntime::ReadTrackedPoses(OpenVrTrackedPoses& poses) noexcept {
    poses = {};
    if (!impl_) return false;
    try {
        impl_->last_error.clear();
        impl_->last_result_code = 0;
        if (!initialized()) return FailNoThrow(impl_.get(), "OpenVR is not initialized");
        (void)ProcessEvents();
        if (impl_->state_tracker.state().shutdown_requested ||
            !impl_->state_tracker.state().connected) {
            impl_->state_tracker.TrackingChanged(false);
            return FailNoThrow(
                impl_.get(),
                impl_->state_tracker.state().shutdown_requested
                    ? "OpenVR runtime requested shutdown"
                    : "OpenVR HMD is disconnected");
        }

        std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> device_poses{};
        impl_->system->GetDeviceToAbsoluteTrackingPose(
            vr::TrackingUniverseStanding,
            0.0F,
            device_poses.data(),
            static_cast<std::uint32_t>(device_poses.size()));
        poses.hmd = PoseFromTrackedDevice(device_poses[vr::k_unTrackedDeviceIndex_Hmd]);
        PopulateControllerRolePoses(impl_->system, device_poses, poses);
        impl_->state_tracker.TrackingChanged(
            poses.hmd.orientation_valid && poses.hmd.position_valid);
        return true;
    } catch (...) {
        impl_->state_tracker.TrackingChanged(false);
        return FailNoThrow(impl_.get(), "OpenVR tracked-pose read raised an exception");
    }
}

bool OpenVrRuntime::ProcessEvents() noexcept {
    if (!initialized()) return false;
    try {
        std::lock_guard lock(impl_->events_mutex);
        vr::VREvent_t event{};
        while (impl_->system->PollNextEvent(&event, sizeof(event))) {
            switch (event.eventType) {
            case vr::VREvent_TrackedDeviceActivated:
            case vr::VREvent_TrackedDeviceUpdated:
            case vr::VREvent_TrackedDeviceDeactivated:
                if (event.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd) {
                    impl_->state_tracker.ConnectionChanged(
                        impl_->system->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd));
                }
                break;
            case vr::VREvent_Quit:
            case vr::VREvent_ProcessQuit:
            case vr::VREvent_DriverRequestedQuit:
                impl_->state_tracker.ShutdownRequested();
                break;
            default:
                break;
            }
        }

        impl_->state_tracker.ConnectionChanged(
            impl_->system->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd));
        const bool focused = impl_->compositor->CanRenderScene() &&
            impl_->compositor->GetCurrentSceneFocusProcess() ==
                static_cast<std::uint32_t>(GetCurrentProcessId());
        impl_->state_tracker.FocusChanged(focused);
        return true;
    } catch (...) {
        impl_->last_result_code = -1;
        try {
            impl_->last_error = "OpenVR event processing raised an exception";
        } catch (...) {
        }
        return false;
    }
}

void OpenVrRuntime::PostPresentHandoff() noexcept {
    if (!initialized()) return;
    impl_->compositor->PostPresentHandoff();
}

bool OpenVrRuntime::ReadPresentationState(OpenVrPresentationState& state) noexcept {
    state = {};
    if (!initialized()) return false;
    try {
        (void)ProcessEvents();

        state.process_id = static_cast<std::uint32_t>(GetCurrentProcessId());
        state.scene_focus_process_id = impl_->compositor->GetCurrentSceneFocusProcess();
        state.can_render_scene = impl_->compositor->CanRenderScene();
        state.input_available = impl_->system->IsInputAvailable();
        state.should_pause = impl_->system->ShouldApplicationPause();
        state.should_reduce_rendering_work = impl_->system->ShouldApplicationReduceRenderingWork();
        if (vr::IVROverlay* overlay = vr::VROverlay(); overlay != nullptr) {
            state.dashboard_visible = overlay->IsDashboardVisible();
        }
        return true;
    } catch (...) {
        state = {};
        return FailNoThrow(impl_.get(), "OpenVR presentation-state read raised an exception");
    }
}

void OpenVrRuntime::RecordEyeSubmission(const Eye eye, const bool succeeded) noexcept {
    if (!impl_) return;
    impl_->state_tracker.EyeSubmitResult(eye, succeeded);
}

bool OpenVrRuntime::InitializeGlobalActions(
    const std::string_view absolute_manifest_path) noexcept {
    if (!impl_) return false;
    try {
        impl_->last_error.clear();
        impl_->last_result_code = 0;
        impl_->input = nullptr;
        impl_->global_action_set = vr::k_ulInvalidActionSetHandle;
        impl_->gameplay_action_set = vr::k_ulInvalidActionSetHandle;
        impl_->recenter_action = vr::k_ulInvalidActionHandle;
        impl_->ui_select_left_action = vr::k_ulInvalidActionHandle;
        impl_->ui_select_right_action = vr::k_ulInvalidActionHandle;
        impl_->left_hand_grip_pose_action = vr::k_ulInvalidActionHandle;
        impl_->right_hand_grip_pose_action = vr::k_ulInvalidActionHandle;
        impl_->left_hand_aim_pose_action = vr::k_ulInvalidActionHandle;
        impl_->right_hand_aim_pose_action = vr::k_ulInvalidActionHandle;
        impl_->gameplay_actions.fill(vr::k_ulInvalidActionHandle);
        impl_->recenter_edge.Reset();
        impl_->ui_select_left_edge.Reset();
        impl_->ui_select_right_edge.Reset();
        if (!initialized()) return FailNoThrow(impl_.get(), "OpenVR is not initialized");
        if (absolute_manifest_path.empty()) {
            return FailNoThrow(impl_.get(), "OpenVR action manifest path is empty");
        }

        vr::IVRInput* input = vr::VRInput();
        if (input == nullptr) {
            return FailNoThrow(impl_.get(), "OpenVR input interface is unavailable");
        }

        const std::string manifest_path(absolute_manifest_path);
        vr::EVRInputError error = input->SetActionManifestPath(manifest_path.c_str());
        if (error != vr::VRInputError_None) {
            return FailNoThrow(
                impl_.get(), "OpenVR SetActionManifestPath failed",
                static_cast<std::int32_t>(error));
        }

        vr::VRActionSetHandle_t global_action_set = vr::k_ulInvalidActionSetHandle;
        error = input->GetActionSetHandle("/actions/global", &global_action_set);
        if (error != vr::VRInputError_None ||
            global_action_set == vr::k_ulInvalidActionSetHandle) {
            return FailNoThrow(
                impl_.get(), "OpenVR global action set resolution failed",
                static_cast<std::int32_t>(error));
        }

        vr::VRActionHandle_t recenter_action = vr::k_ulInvalidActionHandle;
        error = input->GetActionHandle("/actions/global/in/recenter", &recenter_action);
        if (error != vr::VRInputError_None || recenter_action == vr::k_ulInvalidActionHandle) {
            return FailNoThrow(
                impl_.get(), "OpenVR recenter action resolution failed",
                static_cast<std::int32_t>(error));
        }

        vr::VRActionHandle_t ui_select_left_action = vr::k_ulInvalidActionHandle;
        error = input->GetActionHandle(
            "/actions/global/in/ui_select_left", &ui_select_left_action);
        if (error != vr::VRInputError_None ||
            ui_select_left_action == vr::k_ulInvalidActionHandle) {
            return FailNoThrow(
                impl_.get(), "OpenVR left UI-select action resolution failed",
                static_cast<std::int32_t>(error));
        }
        vr::VRActionHandle_t ui_select_right_action = vr::k_ulInvalidActionHandle;
        error = input->GetActionHandle(
            "/actions/global/in/ui_select_right", &ui_select_right_action);
        if (error != vr::VRInputError_None ||
            ui_select_right_action == vr::k_ulInvalidActionHandle) {
            return FailNoThrow(
                impl_.get(), "OpenVR right UI-select action resolution failed",
                static_cast<std::int32_t>(error));
        }

        vr::VRActionHandle_t left_hand_grip_pose_action = vr::k_ulInvalidActionHandle;
        error = input->GetActionHandle(
            "/actions/global/in/left_hand_grip_pose", &left_hand_grip_pose_action);
        if (error != vr::VRInputError_None ||
            left_hand_grip_pose_action == vr::k_ulInvalidActionHandle) {
            return FailNoThrow(
                impl_.get(), "OpenVR left hand-grip pose action resolution failed",
                static_cast<std::int32_t>(error));
        }
        vr::VRActionHandle_t right_hand_grip_pose_action = vr::k_ulInvalidActionHandle;
        error = input->GetActionHandle(
            "/actions/global/in/right_hand_grip_pose", &right_hand_grip_pose_action);
        if (error != vr::VRInputError_None ||
            right_hand_grip_pose_action == vr::k_ulInvalidActionHandle) {
            return FailNoThrow(
                impl_.get(), "OpenVR right hand-grip pose action resolution failed",
                static_cast<std::int32_t>(error));
        }
        vr::VRActionHandle_t left_hand_aim_pose_action = vr::k_ulInvalidActionHandle;
        error = input->GetActionHandle(
            "/actions/global/in/left_hand_aim_pose", &left_hand_aim_pose_action);
        if (error != vr::VRInputError_None ||
            left_hand_aim_pose_action == vr::k_ulInvalidActionHandle) {
            return FailNoThrow(
                impl_.get(), "OpenVR left hand-aim pose action resolution failed",
                static_cast<std::int32_t>(error));
        }
        vr::VRActionHandle_t right_hand_aim_pose_action = vr::k_ulInvalidActionHandle;
        error = input->GetActionHandle(
            "/actions/global/in/right_hand_aim_pose", &right_hand_aim_pose_action);
        if (error != vr::VRInputError_None ||
            right_hand_aim_pose_action == vr::k_ulInvalidActionHandle) {
            return FailNoThrow(
                impl_.get(), "OpenVR right hand-aim pose action resolution failed",
                static_cast<std::int32_t>(error));
        }

        vr::VRActionSetHandle_t gameplay_action_set = vr::k_ulInvalidActionSetHandle;
        error = input->GetActionSetHandle("/actions/gameplay", &gameplay_action_set);
        if (error != vr::VRInputError_None ||
            gameplay_action_set == vr::k_ulInvalidActionSetHandle) {
            return FailNoThrow(
                impl_.get(), "OpenVR gameplay action set resolution failed",
                static_cast<std::int32_t>(error));
        }
        static constexpr std::array<const char*, 12> kGameplayActionNames{
            "/actions/gameplay/in/move",
            "/actions/gameplay/in/turn",
            "/actions/gameplay/in/fire_left",
            "/actions/gameplay/in/fire_right",
            "/actions/gameplay/in/jump",
            "/actions/gameplay/in/reload",
            "/actions/gameplay/in/run",
            "/actions/gameplay/in/crouch",
            "/actions/gameplay/in/interact",
            "/actions/gameplay/in/weapon_next",
            "/actions/gameplay/in/weapon_previous",
            "/actions/gameplay/in/kick",
        };
        std::array<vr::VRActionHandle_t, 12> gameplay_actions{};
        gameplay_actions.fill(vr::k_ulInvalidActionHandle);
        for (std::size_t index = 0; index < gameplay_actions.size(); ++index) {
            error = input->GetActionHandle(kGameplayActionNames[index], &gameplay_actions[index]);
            if (error != vr::VRInputError_None ||
                gameplay_actions[index] == vr::k_ulInvalidActionHandle) {
                return FailNoThrow(
                    impl_.get(), "OpenVR gameplay action resolution failed",
                    static_cast<std::int32_t>(error));
            }
        }

        impl_->input = input;
        impl_->global_action_set = global_action_set;
        impl_->gameplay_action_set = gameplay_action_set;
        impl_->recenter_action = recenter_action;
        impl_->ui_select_left_action = ui_select_left_action;
        impl_->ui_select_right_action = ui_select_right_action;
        impl_->left_hand_grip_pose_action = left_hand_grip_pose_action;
        impl_->right_hand_grip_pose_action = right_hand_grip_pose_action;
        impl_->left_hand_aim_pose_action = left_hand_aim_pose_action;
        impl_->right_hand_aim_pose_action = right_hand_aim_pose_action;
        impl_->gameplay_actions = gameplay_actions;
        return true;
    } catch (...) {
        return FailNoThrow(impl_.get(), "OpenVR global-action initialization raised an exception");
    }
}

bool OpenVrRuntime::PollGlobalActions(OpenVrGlobalActions& actions) noexcept {
    actions = {};
    if (!impl_) return false;
    try {
        impl_->last_error.clear();
        impl_->last_result_code = 0;
        if (!global_actions_initialized()) {
            return FailNoThrow(impl_.get(), "OpenVR global actions are not initialized");
        }

        vr::VRActiveActionSet_t active_set{};
        active_set.ulActionSet = impl_->global_action_set;
        const vr::EVRInputError update_error = impl_->input->UpdateActionState(
            &active_set, sizeof(active_set), 1);
        if (update_error != vr::VRInputError_None) {
            return FailNoThrow(
                impl_.get(), "OpenVR UpdateActionState failed",
                static_cast<std::int32_t>(update_error));
        }

        vr::InputDigitalActionData_t data{};
        const vr::EVRInputError digital_error = impl_->input->GetDigitalActionData(
            impl_->recenter_action, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
        if (digital_error != vr::VRInputError_None) {
            return FailNoThrow(
                impl_.get(), "OpenVR GetDigitalActionData(recenter) failed",
                static_cast<std::int32_t>(digital_error));
        }

        actions.recenter_active = data.bActive;
        actions.recenter_requested = impl_->recenter_edge.Update(data.bActive, data.bState);
        const auto read_ui_select = [&](const vr::VRActionHandle_t action,
                                        bool& value,
                                        bool& pressed,
                                        OpenVrDigitalActionEdge& edge) noexcept {
            vr::InputDigitalActionData_t select{};
            const vr::EVRInputError select_error = impl_->input->GetDigitalActionData(
                action, &select, sizeof(select), vr::k_ulInvalidInputValueHandle);
            if (select_error != vr::VRInputError_None) return false;
            value = select.bActive && select.bState;
            pressed = edge.Update(select.bActive, select.bState);
            return true;
        };
        if (!read_ui_select(
                impl_->ui_select_left_action,
                actions.ui_select_left,
                actions.ui_select_left_pressed,
                impl_->ui_select_left_edge) ||
            !read_ui_select(
                impl_->ui_select_right_action,
                actions.ui_select_right,
                actions.ui_select_right_pressed,
                impl_->ui_select_right_edge)) {
            actions = {};
            return FailNoThrow(impl_.get(), "OpenVR UI-select action read failed");
        }
        return true;
    } catch (...) {
        actions = {};
        return FailNoThrow(impl_.get(), "OpenVR global-action poll raised an exception");
    }
}

bool OpenVrRuntime::PollActions(
    OpenVrGlobalActions& global_actions,
    GameplayInputState& gameplay_actions,
    OpenVrHandPoses& hand_poses) noexcept {
    global_actions = {};
    gameplay_actions = {};
    hand_poses = {};
    if (!impl_) return false;
    try {
        impl_->last_error.clear();
        impl_->last_result_code = 0;
        if (!global_actions_initialized()) {
            return FailNoThrow(impl_.get(), "OpenVR input actions are not initialized");
        }

        std::array<vr::VRActiveActionSet_t, 2> active_sets{};
        active_sets[0].ulActionSet = impl_->global_action_set;
        active_sets[1].ulActionSet = impl_->gameplay_action_set;
        const vr::EVRInputError update_error = impl_->input->UpdateActionState(
            active_sets.data(), sizeof(vr::VRActiveActionSet_t),
            static_cast<std::uint32_t>(active_sets.size()));
        if (update_error != vr::VRInputError_None) {
            return FailNoThrow(
                impl_.get(), "OpenVR UpdateActionState failed",
                static_cast<std::int32_t>(update_error));
        }

        vr::InputDigitalActionData_t recenter{};
        vr::EVRInputError error = impl_->input->GetDigitalActionData(
            impl_->recenter_action,
            &recenter,
            sizeof(recenter),
            vr::k_ulInvalidInputValueHandle);
        if (error != vr::VRInputError_None) {
            return FailNoThrow(
                impl_.get(), "OpenVR GetDigitalActionData(recenter) failed",
                static_cast<std::int32_t>(error));
        }
        global_actions.recenter_active = recenter.bActive;
        global_actions.recenter_requested =
            impl_->recenter_edge.Update(recenter.bActive, recenter.bState);

        const auto read_global_digital = [&](const vr::VRActionHandle_t action,
                                             bool& value,
                                             bool& pressed,
                                             OpenVrDigitalActionEdge& edge) noexcept {
            vr::InputDigitalActionData_t data{};
            const vr::EVRInputError digital_error = impl_->input->GetDigitalActionData(
                action, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
            if (digital_error != vr::VRInputError_None) return false;
            value = data.bActive && data.bState;
            pressed = edge.Update(data.bActive, data.bState);
            return true;
        };
        if (!read_global_digital(
                impl_->ui_select_left_action,
                global_actions.ui_select_left,
                global_actions.ui_select_left_pressed,
                impl_->ui_select_left_edge) ||
            !read_global_digital(
                impl_->ui_select_right_action,
                global_actions.ui_select_right,
                global_actions.ui_select_right_pressed,
                impl_->ui_select_right_edge)) {
            return FailNoThrow(impl_.get(), "OpenVR UI-select action read failed");
        }

        const auto read_pose = [&](const vr::VRActionHandle_t action,
                                   Pose& pose,
                                   bool& active) noexcept {
            vr::InputPoseActionData_t data{};
            const vr::EVRInputError pose_error = impl_->input->GetPoseActionDataForNextFrame(
                action,
                vr::TrackingUniverseStanding,
                &data,
                sizeof(data),
                vr::k_ulInvalidInputValueHandle);
            if (pose_error != vr::VRInputError_None) return false;
            active = data.bActive;
            if (active) pose = PoseFromTrackedDevice(data.pose);
            return true;
        };
        if (!read_pose(
                impl_->left_hand_grip_pose_action,
                hand_poses.left_grip,
                hand_poses.left_grip_active) ||
            !read_pose(
                impl_->right_hand_grip_pose_action,
                hand_poses.right_grip,
                hand_poses.right_grip_active) ||
            !read_pose(
                impl_->left_hand_aim_pose_action,
                hand_poses.left_aim,
                hand_poses.left_aim_active) ||
            !read_pose(
                impl_->right_hand_aim_pose_action,
                hand_poses.right_aim,
                hand_poses.right_aim_active)) {
            hand_poses = {};
            gameplay_actions = {};
            return FailNoThrow(impl_.get(), "OpenVR hand pose action read failed");
        }

        const auto read_analog = [&](const std::size_t index, Vec2& value) noexcept {
            vr::InputAnalogActionData_t data{};
            const vr::EVRInputError analog_error = impl_->input->GetAnalogActionData(
                impl_->gameplay_actions[index],
                &data,
                sizeof(data),
                vr::k_ulInvalidInputValueHandle);
            if (analog_error != vr::VRInputError_None) return false;
            if (data.bActive) value = {data.x, data.y};
            return true;
        };
        const auto read_digital = [&](const std::size_t index, bool& value) noexcept {
            vr::InputDigitalActionData_t data{};
            const vr::EVRInputError digital_error = impl_->input->GetDigitalActionData(
                impl_->gameplay_actions[index],
                &data,
                sizeof(data),
                vr::k_ulInvalidInputValueHandle);
            if (digital_error != vr::VRInputError_None) return false;
            value = data.bActive && data.bState;
            return true;
        };
        if (!read_analog(0, gameplay_actions.move) ||
            !read_analog(1, gameplay_actions.turn) ||
            !read_digital(2, gameplay_actions.fire_left) ||
            !read_digital(3, gameplay_actions.fire_right) ||
            !read_digital(4, gameplay_actions.jump) ||
            !read_digital(5, gameplay_actions.reload) ||
            !read_digital(6, gameplay_actions.run) ||
            !read_digital(7, gameplay_actions.crouch) ||
            !read_digital(8, gameplay_actions.interact) ||
            !read_digital(9, gameplay_actions.weapon_next) ||
            !read_digital(10, gameplay_actions.weapon_previous) ||
            !read_digital(11, gameplay_actions.kick)) {
            gameplay_actions = {};
            return FailNoThrow(impl_.get(), "OpenVR gameplay action read failed");
        }
        gameplay_actions.active = true;
        return true;
    } catch (...) {
        global_actions = {};
        gameplay_actions = {};
        hand_poses = {};
        return FailNoThrow(impl_.get(), "OpenVR input action poll raised an exception");
    }
}

bool OpenVrRuntime::initialized() const noexcept {
    return impl_ && impl_->system != nullptr && impl_->compositor != nullptr;
}

OpenVrRuntimeState OpenVrRuntime::state() const noexcept {
    return impl_ ? impl_->state_tracker.state() : OpenVrRuntimeState{};
}

bool OpenVrRuntime::global_actions_initialized() const noexcept {
    return impl_ && initialized() && impl_->input != nullptr &&
        impl_->global_action_set != vr::k_ulInvalidActionSetHandle &&
        impl_->gameplay_action_set != vr::k_ulInvalidActionSetHandle &&
        impl_->recenter_action != vr::k_ulInvalidActionHandle &&
        impl_->ui_select_left_action != vr::k_ulInvalidActionHandle &&
        impl_->ui_select_right_action != vr::k_ulInvalidActionHandle &&
        impl_->left_hand_grip_pose_action != vr::k_ulInvalidActionHandle &&
        impl_->right_hand_grip_pose_action != vr::k_ulInvalidActionHandle &&
        impl_->left_hand_aim_pose_action != vr::k_ulInvalidActionHandle &&
        impl_->right_hand_aim_pose_action != vr::k_ulInvalidActionHandle;
}

const OpenVrSystemInfo& OpenVrRuntime::system_info() const noexcept {
    static const OpenVrSystemInfo empty{};
    return impl_ ? impl_->system_info : empty;
}
std::string_view OpenVrRuntime::last_error() const noexcept {
    return impl_ ? std::string_view(impl_->last_error)
                 : std::string_view("OpenVR runtime object has no state");
}
std::int32_t OpenVrRuntime::last_result_code() const noexcept {
    return impl_ ? impl_->last_result_code : -1;
}
void* OpenVrRuntime::native_system() const noexcept { return impl_ ? impl_->system : nullptr; }
void* OpenVrRuntime::native_compositor() const noexcept {
    return impl_ ? impl_->compositor : nullptr;
}

} // namespace cojvr::runtime
