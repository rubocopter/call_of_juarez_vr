#include "runtime/openvr_runtime.hpp"
#include "runtime/vr_math.hpp"
#include <openvr.h>
#include <algorithm>
#include <array>
#include <cmath>
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
    vr::VRActionHandle_t ui_accept_action = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t ui_back_action = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t pause_action = vr::k_ulInvalidActionHandle;
    std::array<vr::VRActionHandle_t, 2> ui_pointer_actions{};
    std::array<OpenVrDigitalActionEdge, 2> ui_pointer_edges{};
    vr::VRActionHandle_t left_hand_grip_pose_action = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t right_hand_grip_pose_action = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t left_hand_aim_pose_action = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t right_hand_aim_pose_action = vr::k_ulInvalidActionHandle;
    std::array<vr::VRActionHandle_t, 2> hand_skeleton_actions{};
    std::array<vr::VRActionHandle_t, 2> hand_haptic_actions{};
    std::array<vr::VRInputValueHandle_t, 2> hand_haptic_sources{};
    UiHapticOutputGate haptic_output;
    std::array<vr::VRActionHandle_t, 30> gameplay_actions{};
    OpenVrDigitalActionEdge recenter_edge{};
    OpenVrDigitalActionEdge ui_select_left_edge{};
    OpenVrDigitalActionEdge ui_select_right_edge{};
    OpenVrDigitalActionEdge ui_accept_edge{};
    OpenVrDigitalActionEdge ui_back_edge{};
    OpenVrDigitalActionEdge pause_edge{};
    OpenVrSystemInfo system_info{};
    OpenVrStateTracker state_tracker{};
    bool owns_process_runtime = false;
    std::string last_error;
    std::int32_t last_result_code = 0;
    mutable std::mutex events_mutex; // Serializes ProcessEvents calls

    void ResetActionEdges() noexcept {
        for (auto& edge : ui_pointer_edges) edge.Reset();
        recenter_edge.Reset();
        ui_select_left_edge.Reset(); ui_select_right_edge.Reset();
        ui_accept_edge.Reset(); ui_back_edge.Reset(); pause_edge.Reset();
    }
    // Failed/partial polls invalidate all global edges, including those read
    // before the failing API call. No output or fresh press survives the gap.
    struct InputPollTransaction {
        Impl& owner;
        OpenVrGlobalActions& globals;
        GameplayInputState* gameplay = nullptr;
        OpenVrHandPoses* poses = nullptr;
        bool committed = false;
        ~InputPollTransaction() noexcept {
            if (committed) return;
            owner.ResetActionEdges(); globals = {};
            if (gameplay) *gameplay = {};
            if (poses) *poses = {};
        }
    };
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
    if (!active) { Reset(); return false; }
    if (!pressed) { armed_ = true; pressed_ = false; return false; }
    const bool rising = armed_ && !pressed_;
    pressed_ = true;
    return rising;
}

void OpenVrDigitalActionEdge::Reset() noexcept { armed_ = pressed_ = false; }

void OpenVrRuntime::InvalidateActionEdges() noexcept {
    if (impl_) impl_->ResetActionEdges();
}

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
    impl_->ui_accept_action = vr::k_ulInvalidActionHandle;
    impl_->ui_back_action = vr::k_ulInvalidActionHandle;
    impl_->pause_action = vr::k_ulInvalidActionHandle;
    impl_->ui_pointer_actions.fill(vr::k_ulInvalidActionHandle);
    for (auto& edge : impl_->ui_pointer_edges) edge.Reset();
    impl_->left_hand_grip_pose_action = vr::k_ulInvalidActionHandle;
    impl_->right_hand_grip_pose_action = vr::k_ulInvalidActionHandle;
    impl_->left_hand_aim_pose_action = vr::k_ulInvalidActionHandle;
    impl_->right_hand_aim_pose_action = vr::k_ulInvalidActionHandle;
    impl_->hand_skeleton_actions.fill(vr::k_ulInvalidActionHandle);
    impl_->hand_haptic_actions.fill(vr::k_ulInvalidActionHandle);
    impl_->hand_haptic_sources.fill(vr::k_ulInvalidInputValueHandle);
    impl_->haptic_output = {};
    impl_->gameplay_actions.fill(vr::k_ulInvalidActionHandle);
    impl_->recenter_edge.Reset();
    impl_->ui_select_left_edge.Reset();
    impl_->ui_select_right_edge.Reset();
    impl_->ui_accept_edge.Reset();
    impl_->ui_back_edge.Reset();
    impl_->pause_edge.Reset();
    impl_->system_info = {};
    impl_->owns_process_runtime = false;
    impl_->state_tracker.ShutdownComplete();
}

float OpenVrRuntime::ReadDisplayFrequency() noexcept {
    if (!initialized() || !impl_->system->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd)) {
        return 0.0F;
    }
    vr::ETrackedPropertyError error = vr::TrackedProp_Success;
    const float hz = impl_->system->GetFloatTrackedDeviceProperty(
        vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float, &error);
    return error == vr::TrackedProp_Success ? hz : 0.0F;
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
        impl_->ui_accept_action = vr::k_ulInvalidActionHandle;
        impl_->ui_back_action = vr::k_ulInvalidActionHandle;
        impl_->pause_action = vr::k_ulInvalidActionHandle;
        impl_->ui_pointer_actions.fill(vr::k_ulInvalidActionHandle);
        for (auto& edge : impl_->ui_pointer_edges) edge.Reset();
        impl_->left_hand_grip_pose_action = vr::k_ulInvalidActionHandle;
        impl_->right_hand_grip_pose_action = vr::k_ulInvalidActionHandle;
        impl_->left_hand_aim_pose_action = vr::k_ulInvalidActionHandle;
        impl_->right_hand_aim_pose_action = vr::k_ulInvalidActionHandle;
        impl_->hand_skeleton_actions.fill(vr::k_ulInvalidActionHandle);
        impl_->hand_haptic_actions.fill(vr::k_ulInvalidActionHandle);
        impl_->hand_haptic_sources.fill(vr::k_ulInvalidInputValueHandle);
        impl_->haptic_output = {};
        impl_->gameplay_actions.fill(vr::k_ulInvalidActionHandle);
        impl_->recenter_edge.Reset();
        impl_->ui_select_left_edge.Reset();
        impl_->ui_select_right_edge.Reset();
        impl_->ui_accept_edge.Reset();
        impl_->ui_back_edge.Reset();
        impl_->pause_edge.Reset();
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
        vr::VRActionHandle_t ui_accept_action = vr::k_ulInvalidActionHandle;
        error = input->GetActionHandle(
            "/actions/global/in/ui_accept", &ui_accept_action);
        if (error != vr::VRInputError_None ||
            ui_accept_action == vr::k_ulInvalidActionHandle) {
            return FailNoThrow(
                impl_.get(), "OpenVR UI-accept action resolution failed",
                static_cast<std::int32_t>(error));
        }
        vr::VRActionHandle_t ui_back_action = vr::k_ulInvalidActionHandle;
        error = input->GetActionHandle(
            "/actions/global/in/ui_back", &ui_back_action);
        if (error != vr::VRInputError_None ||
            ui_back_action == vr::k_ulInvalidActionHandle) {
            return FailNoThrow(
                impl_.get(), "OpenVR UI-back action resolution failed",
                static_cast<std::int32_t>(error));
        }

        vr::VRActionHandle_t pause_action = vr::k_ulInvalidActionHandle;
        error = input->GetActionHandle("/actions/global/in/pause", &pause_action);
        if (error != vr::VRInputError_None || pause_action == vr::k_ulInvalidActionHandle) {
            return FailNoThrow(impl_.get(), "OpenVR pause action resolution failed",
                static_cast<std::int32_t>(error));
        }
        std::array<vr::VRActionHandle_t, 2> ui_pointer_actions{};
        constexpr const char* pointer_names[] = {
            "/actions/global/in/ui_pointer_left", "/actions/global/in/ui_pointer_right"};
        for (std::size_t index = 0; index < ui_pointer_actions.size(); ++index) {
            error = input->GetActionHandle(pointer_names[index], &ui_pointer_actions[index]);
            if (error != vr::VRInputError_None || ui_pointer_actions[index] == vr::k_ulInvalidActionHandle)
                return FailNoThrow(impl_.get(), "OpenVR UI-pointer action resolution failed",
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

        // Missing skeletal bindings must never disable tracking or gameplay.
        std::array<vr::VRActionHandle_t, 2> hand_skeleton_actions{};
        constexpr const char* skeleton_names[] = {
            "/actions/global/in/left_hand_skeleton", "/actions/global/in/right_hand_skeleton"};
        for (std::size_t index = 0; index < hand_skeleton_actions.size(); ++index) {
            if (input->GetActionHandle(skeleton_names[index], &hand_skeleton_actions[index]) !=
                vr::VRInputError_None) hand_skeleton_actions[index] = vr::k_ulInvalidActionHandle;
        }

        vr::VRActionSetHandle_t gameplay_action_set = vr::k_ulInvalidActionSetHandle;
        error = input->GetActionSetHandle("/actions/gameplay", &gameplay_action_set);
        if (error != vr::VRInputError_None ||
            gameplay_action_set == vr::k_ulInvalidActionSetHandle) {
            return FailNoThrow(
                impl_.get(), "OpenVR gameplay action set resolution failed",
                static_cast<std::int32_t>(error));
        }
        static constexpr std::array<const char*, 30> kGameplayActionNames{
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
            "/actions/gameplay/in/walk",
            "/actions/gameplay/in/focus",
            "/actions/gameplay/in/alternate_fire",
            "/actions/gameplay/in/hands",
            "/actions/gameplay/in/discard_weapon",
            "/actions/gameplay/in/objectives",
            "/actions/gameplay/in/logs",
            "/actions/gameplay/in/quick_save",
            "/actions/gameplay/in/quick_load",
            "/actions/gameplay/in/lean_left",
            "/actions/gameplay/in/lean_right",
            "/actions/gameplay/in/equipment_1",
            "/actions/gameplay/in/equipment_2",
            "/actions/gameplay/in/equipment_3",
            "/actions/gameplay/in/equipment_4",
            "/actions/gameplay/in/equipment_5",
            "/actions/gameplay/in/equipment_6",
            "/actions/gameplay/in/weapon_radial",
        };
        std::array<vr::VRActionHandle_t, 30> gameplay_actions{};
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
        impl_->ui_accept_action = ui_accept_action;
        impl_->ui_back_action = ui_back_action;
        impl_->pause_action = pause_action;
        impl_->ui_pointer_actions = ui_pointer_actions;
        impl_->left_hand_grip_pose_action = left_hand_grip_pose_action;
        impl_->right_hand_grip_pose_action = right_hand_grip_pose_action;
        impl_->left_hand_aim_pose_action = left_hand_aim_pose_action;
        impl_->right_hand_aim_pose_action = right_hand_aim_pose_action;
        impl_->hand_skeleton_actions = hand_skeleton_actions;
        impl_->gameplay_actions = gameplay_actions;
        // Optional per-hand vibration outputs, matching pinned SDK samples.
        // Resolution failure affects only feedback, after required input setup.
        constexpr const char* haptic_names[] = {
            "/actions/global/out/haptic_left", "/actions/global/out/haptic_right"};
        constexpr const char* source_names[] = {"/user/hand/left", "/user/hand/right"};
        for (std::size_t index = 0; index < impl_->hand_haptic_actions.size(); ++index) {
            auto& action = impl_->hand_haptic_actions[index];
            auto& source = impl_->hand_haptic_sources[index];
            bool ready = false;
            try {
                ready = input->GetActionHandle(haptic_names[index], &action) == vr::VRInputError_None &&
                    input->GetInputSourceHandle(source_names[index], &source) == vr::VRInputError_None &&
                    action != vr::k_ulInvalidActionHandle && source != vr::k_ulInvalidInputValueHandle;
            } catch (...) {}
            if (!ready) {
                action = vr::k_ulInvalidActionHandle; source = vr::k_ulInvalidInputValueHandle;
            } else impl_->haptic_output.Configure(static_cast<UiHapticHand>(index), true);
        }
        return true;
    } catch (...) {
        return FailNoThrow(impl_.get(), "OpenVR global-action initialization raised an exception");
    }
}

bool OpenVrRuntime::SubmitUiHapticPulse(const UiHapticPulse& pulse) noexcept {
    if (!impl_ || !global_actions_initialized() || !UiHapticPulseValid(pulse) ||
        !impl_->haptic_output.Available(pulse.hand)) return false;
    try {
        // Recheck actual focus/dashboard/input immediately before output rather
        // than relying on the presenter's less frequent diagnostic state sample.
        OpenVrPresentationState presentation{};
        if (!ReadPresentationState(presentation) || !presentation.can_render_scene ||
            presentation.scene_focus_process_id != presentation.process_id ||
            !presentation.input_available || presentation.dashboard_visible || presentation.should_pause ||
            state().shutdown_requested) return false;
        auto* overlay = vr::VROverlay();
        if (!overlay || overlay->IsDashboardVisible()) return false;
        const auto role = pulse.hand == UiHapticHand::left
            ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand;
        const auto device = impl_->system->GetTrackedDeviceIndexForControllerRole(role);
        if (device == vr::k_unTrackedDeviceIndexInvalid || !impl_->system->IsTrackedDeviceConnected(device)) {
            impl_->haptic_output.Failed(pulse.hand); return false;
        }
        const auto index = static_cast<std::size_t>(pulse.hand);
        return impl_->haptic_output.Dispatch(pulse, [&] {
            return impl_->input->TriggerHapticVibrationAction(
                impl_->hand_haptic_actions[index], 0.0F, pulse.duration_seconds,
                pulse.frequency_hz, pulse.amplitude, impl_->hand_haptic_sources[index]) == vr::VRInputError_None;
        });
    } catch (...) {
        impl_->haptic_output.Failed(pulse.hand);
        return false;
    }
}

bool OpenVrRuntime::PollGlobalActions(OpenVrGlobalActions& actions) noexcept {
    actions = {};
    if (!impl_) return false;
    Impl::InputPollTransaction poll{*impl_, actions};
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
        actions.recenter_held = data.bActive && data.bState;
        actions.recenter_requested = impl_->recenter_edge.Update(data.bActive, data.bState);
        const auto read_ui_select = [&](const vr::VRActionHandle_t action,
                                        bool& value,
                                        bool& pressed,
                                        OpenVrDigitalActionEdge& edge,
                                        bool* active = nullptr) noexcept {
            vr::InputDigitalActionData_t select{};
            const vr::EVRInputError select_error = impl_->input->GetDigitalActionData(
                action, &select, sizeof(select), vr::k_ulInvalidInputValueHandle);
            if (select_error != vr::VRInputError_None) return false;
            value = select.bActive && select.bState;
            if (active) *active = select.bActive;
            pressed = edge.Update(select.bActive, select.bState);
            return true;
        };
        bool pointer_pressed = false;
        if (!read_ui_select(impl_->ui_pointer_actions[0], actions.ui_pointer_left,
                pointer_pressed, impl_->ui_pointer_edges[0], &actions.ui_pointer_left_active) ||
            !read_ui_select(impl_->ui_pointer_actions[1], actions.ui_pointer_right,
                pointer_pressed, impl_->ui_pointer_edges[1], &actions.ui_pointer_right_active) ||
            !read_ui_select(
                impl_->ui_select_left_action,
                actions.ui_select_left,
                actions.ui_select_left_pressed,
                impl_->ui_select_left_edge) ||
            !read_ui_select(
                impl_->ui_select_right_action,
                actions.ui_select_right,
                actions.ui_select_right_pressed,
                impl_->ui_select_right_edge) ||
            !read_ui_select(
                impl_->ui_accept_action,
                actions.ui_accept,
                actions.ui_accept_pressed,
                impl_->ui_accept_edge) ||
            !read_ui_select(
                impl_->ui_back_action,
                actions.ui_back,
                actions.ui_back_pressed,
                impl_->ui_back_edge) ||
            !read_ui_select(impl_->pause_action, actions.pause,
                actions.pause_pressed, impl_->pause_edge)) {
            actions = {};
            return FailNoThrow(impl_.get(), "OpenVR UI-select action read failed");
        }
        poll.committed = true;
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
    Impl::InputPollTransaction poll{*impl_, global_actions, &gameplay_actions, &hand_poses};
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
        global_actions.recenter_held = recenter.bActive && recenter.bState;
        global_actions.recenter_requested =
            impl_->recenter_edge.Update(recenter.bActive, recenter.bState);

        const auto read_global_digital = [&](const vr::VRActionHandle_t action,
                                             bool& value,
                                             bool& pressed,
                                             OpenVrDigitalActionEdge& edge,
                                             bool* active = nullptr) noexcept {
            vr::InputDigitalActionData_t data{};
            const vr::EVRInputError digital_error = impl_->input->GetDigitalActionData(
                action, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
            if (digital_error != vr::VRInputError_None) return false;
            value = data.bActive && data.bState;
            if (active) *active = data.bActive;
            pressed = edge.Update(data.bActive, data.bState);
            return true;
        };
        bool pointer_pressed = false;
        if (!read_global_digital(impl_->ui_pointer_actions[0], global_actions.ui_pointer_left,
                pointer_pressed, impl_->ui_pointer_edges[0], &global_actions.ui_pointer_left_active) ||
            !read_global_digital(impl_->ui_pointer_actions[1], global_actions.ui_pointer_right,
                pointer_pressed, impl_->ui_pointer_edges[1], &global_actions.ui_pointer_right_active) ||
            !read_global_digital(
                impl_->ui_select_left_action,
                global_actions.ui_select_left,
                global_actions.ui_select_left_pressed,
                impl_->ui_select_left_edge) ||
            !read_global_digital(
                impl_->ui_select_right_action,
                global_actions.ui_select_right,
                global_actions.ui_select_right_pressed,
                impl_->ui_select_right_edge) ||
            !read_global_digital(
                impl_->ui_accept_action,
                global_actions.ui_accept,
                global_actions.ui_accept_pressed,
                impl_->ui_accept_edge) ||
            !read_global_digital(
                impl_->ui_back_action,
                global_actions.ui_back,
                global_actions.ui_back_pressed,
                impl_->ui_back_edge) ||
            !read_global_digital(impl_->pause_action, global_actions.pause,
                global_actions.pause_pressed, impl_->pause_edge)) {
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

        const auto read_fingers = [&](const std::size_t index, FingerTrackingState& fingers) noexcept {
            fingers = {};
            const auto action = impl_->hand_skeleton_actions[index];
            if (action == vr::k_ulInvalidActionHandle) return;
            vr::InputSkeletalActionData_t data{};
            vr::EVRSkeletalTrackingLevel level{};
            vr::VRSkeletalSummaryData_t summary{};
            if (impl_->input->GetSkeletalActionData(action, &data, sizeof(data)) != vr::VRInputError_None ||
                !data.bActive ||
                impl_->input->GetSkeletalTrackingLevel(action, &level) != vr::VRInputError_None ||
                impl_->input->GetSkeletalSummaryData(action, vr::VRSummaryType_FromDevice, &summary) !=
                    vr::VRInputError_None) return;
            switch (level) {
            case vr::VRSkeletalTracking_Estimated: fingers.quality = FingerTrackingQuality::estimated; break;
            case vr::VRSkeletalTracking_Partial: fingers.quality = FingerTrackingQuality::partial; break;
            case vr::VRSkeletalTracking_Full: fingers.quality = FingerTrackingQuality::full; break;
            default: return;
            }
            for (std::size_t finger = 0; finger < fingers.curls.size(); ++finger) {
                const float curl = summary.flFingerCurl[finger];
                if (!std::isfinite(curl) || curl < 0.0F || curl > 1.0F) {
                    fingers = {}; return;
                }
                fingers.curls[finger] = curl;
            }
            fingers.available = true;
        };
        read_fingers(0, hand_poses.left_fingers);
        read_fingers(1, hand_poses.right_fingers);

        const auto read_analog = [&](const std::size_t index, Vec2& value) noexcept {
            vr::InputAnalogActionData_t data{};
            const vr::EVRInputError analog_error = impl_->input->GetAnalogActionData(
                impl_->gameplay_actions[index],
                &data,
                sizeof(data),
                vr::k_ulInvalidInputValueHandle);
            if (analog_error != vr::VRInputError_None) return false;
            if (index == 0) gameplay_actions.move_available = data.bActive;
            else gameplay_actions.turn_available = data.bActive;
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
            if (index == 29) gameplay_actions.radial_available = data.bActive;
            else if (!data.bActive) gameplay_actions.digital_available &= ~(1U << (index - 2));
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
            !read_digital(11, gameplay_actions.kick) ||
            !read_digital(12, gameplay_actions.walk) ||
            !read_digital(13, gameplay_actions.focus) ||
            !read_digital(14, gameplay_actions.alternate_fire) ||
            !read_digital(15, gameplay_actions.hands) ||
            !read_digital(16, gameplay_actions.discard_weapon) ||
            !read_digital(17, gameplay_actions.objectives) ||
            !read_digital(18, gameplay_actions.logs) ||
            !read_digital(19, gameplay_actions.quick_save) ||
            !read_digital(20, gameplay_actions.quick_load) ||
            !read_digital(21, gameplay_actions.lean_left) ||
            !read_digital(22, gameplay_actions.lean_right) ||
            !read_digital(23, gameplay_actions.equipment_select[0]) ||
            !read_digital(24, gameplay_actions.equipment_select[1]) ||
            !read_digital(25, gameplay_actions.equipment_select[2]) ||
            !read_digital(26, gameplay_actions.equipment_select[3]) ||
            !read_digital(27, gameplay_actions.equipment_select[4]) ||
            !read_digital(28, gameplay_actions.equipment_select[5]) ||
            !read_digital(29, gameplay_actions.weapon_radial)) {
            gameplay_actions = {};
            return FailNoThrow(impl_.get(), "OpenVR gameplay action read failed");
        }
        gameplay_actions.active = true;
        poll.committed = true;
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
        impl_->ui_accept_action != vr::k_ulInvalidActionHandle &&
        impl_->ui_back_action != vr::k_ulInvalidActionHandle &&
        impl_->pause_action != vr::k_ulInvalidActionHandle &&
        impl_->ui_pointer_actions[0] != vr::k_ulInvalidActionHandle &&
        impl_->ui_pointer_actions[1] != vr::k_ulInvalidActionHandle &&
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
