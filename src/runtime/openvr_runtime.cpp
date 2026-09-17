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
    vr::VRActionHandle_t recenter_action = vr::k_ulInvalidActionHandle;
    OpenVrDigitalActionEdge recenter_edge{};
    OpenVrSystemInfo system_info{};
    OpenVrStateTracker state_tracker{};
    bool owns_process_runtime = false;
    std::string last_error;
    std::int32_t last_result_code = 0;
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

bool OpenVrDigitalActionEdge::Update(const bool active, const bool pressed) noexcept {
    const bool current = active && pressed;
    const bool rising = current && !pressed_;
    pressed_ = current;
    return rising;
}

void OpenVrDigitalActionEdge::Reset() noexcept { pressed_ = false; }

OpenVrRuntime::OpenVrRuntime() : impl_(std::make_unique<Impl>()) {}
OpenVrRuntime::~OpenVrRuntime() { Shutdown(); }
OpenVrRuntime::OpenVrRuntime(OpenVrRuntime&&) noexcept = default;
OpenVrRuntime& OpenVrRuntime::operator=(OpenVrRuntime&& other) noexcept {
    if (this == &other) return *this;
    Shutdown();
    impl_ = std::move(other.impl_);
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
    impl_->recenter_action = vr::k_ulInvalidActionHandle;
    impl_->recenter_edge.Reset();
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
            eye.pose = PoseFromRigidTransform3x4(
                Flatten(impl_->system->GetEyeToHeadTransform(native_eyes[index])));
            eye.fov = OpenVrProjectionRawToEyeFov(left, right, top, bottom);
            eye.width = impl_->system_info.recommended_width;
            eye.height = impl_->system_info.recommended_height;
        }
        return true;
    } catch (...) {
        return FailNoThrow(impl_.get(), "OpenVR eye-configuration read raised an exception");
    }
}

bool OpenVrRuntime::WaitForHmdPose(Pose& pose) noexcept {
    pose = {};
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

        std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> poses{};
        const vr::EVRCompositorError error = impl_->compositor->WaitGetPoses(
            poses.data(), static_cast<std::uint32_t>(poses.size()), nullptr, 0);
        if (error != vr::VRCompositorError_None) {
            impl_->state_tracker.TrackingChanged(false);
            return FailNoThrow(
                impl_.get(), "OpenVR WaitGetPoses failed", static_cast<std::int32_t>(error));
        }

        const vr::TrackedDevicePose_t& hmd = poses[vr::k_unTrackedDeviceIndex_Hmd];
        pose = PoseFromRigidTransform3x4(Flatten(hmd.mDeviceToAbsoluteTracking));
        pose.orientation_valid = hmd.bPoseIsValid && pose.orientation_valid;
        pose.position_valid = hmd.bPoseIsValid && pose.position_valid;
        impl_->state_tracker.TrackingChanged(pose.orientation_valid && pose.position_valid);
        return true;
    } catch (...) {
        impl_->state_tracker.TrackingChanged(false);
        return FailNoThrow(impl_.get(), "OpenVR pose wait raised an exception");
    }
}

bool OpenVrRuntime::ReadHmdPose(Pose& pose) noexcept {
    pose = {};
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

        std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> poses{};
        impl_->system->GetDeviceToAbsoluteTrackingPose(
            vr::TrackingUniverseStanding,
            0.0F,
            poses.data(),
            static_cast<std::uint32_t>(poses.size()));

        const vr::TrackedDevicePose_t& hmd = poses[vr::k_unTrackedDeviceIndex_Hmd];
        pose = PoseFromRigidTransform3x4(Flatten(hmd.mDeviceToAbsoluteTracking));
        pose.orientation_valid = hmd.bPoseIsValid && pose.orientation_valid;
        pose.position_valid = hmd.bPoseIsValid && pose.position_valid;
        impl_->state_tracker.TrackingChanged(pose.orientation_valid && pose.position_valid);
        return true;
    } catch (...) {
        impl_->state_tracker.TrackingChanged(false);
        return FailNoThrow(impl_.get(), "OpenVR pose read raised an exception");
    }
}

bool OpenVrRuntime::ProcessEvents() noexcept {
    if (!initialized()) return false;
    try {
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
        impl_->recenter_action = vr::k_ulInvalidActionHandle;
        impl_->recenter_edge.Reset();
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

        impl_->input = input;
        impl_->global_action_set = global_action_set;
        impl_->recenter_action = recenter_action;
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
        return true;
    } catch (...) {
        actions = {};
        return FailNoThrow(impl_.get(), "OpenVR global-action poll raised an exception");
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
        impl_->recenter_action != vr::k_ulInvalidActionHandle;
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
