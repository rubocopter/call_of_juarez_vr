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
    std::string last_error;
    std::int32_t last_result_code = 0;
};

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
OpenVrRuntime& OpenVrRuntime::operator=(OpenVrRuntime&&) noexcept = default;

bool OpenVrRuntime::Initialize(const std::string_view application_name) noexcept {
    impl_->last_error.clear();
    impl_->last_result_code = 0;
    if (initialized()) {
        impl_->last_error = "OpenVR is already initialized";
        impl_->last_result_code = -1;
        return false;
    }

    const std::string application_name_copy(application_name);
    vr::EVRInitError init_error = vr::VRInitError_None;
    vr::IVRSystem* system = vr::VR_Init(&init_error, vr::VRApplication_Scene, application_name_copy.c_str());
    if (init_error != vr::VRInitError_None || system == nullptr) {
        impl_->last_result_code = static_cast<std::int32_t>(init_error);
        impl_->last_error = InitializationError(init_error);
        if (system != nullptr) vr::VR_Shutdown();
        return false;
    }

    vr::IVRCompositor* compositor = vr::VRCompositor();
    if (compositor == nullptr) {
        impl_->last_error = "OpenVR compositor interface is unavailable";
        impl_->last_result_code = -1;
        vr::VR_Shutdown();
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
        return false;
    }

    impl_->system = system;
    impl_->compositor = compositor;
    impl_->system_info = info;
    return true;
}

void OpenVrRuntime::Shutdown() noexcept {
    if (!impl_ || impl_->system == nullptr) return;
    vr::VR_Shutdown();
    impl_->system = nullptr;
    impl_->compositor = nullptr;
    impl_->input = nullptr;
    impl_->global_action_set = vr::k_ulInvalidActionSetHandle;
    impl_->recenter_action = vr::k_ulInvalidActionHandle;
    impl_->recenter_edge.Reset();
    impl_->system_info = {};
}

bool OpenVrRuntime::ReadEyeConfiguration(std::array<EyeView, 2>& eyes) noexcept {
    impl_->last_error.clear();
    eyes = {};
    if (!initialized()) {
        impl_->last_error = "OpenVR is not initialized";
        return false;
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
        eye.pose = PoseFromRigidTransform3x4(Flatten(impl_->system->GetEyeToHeadTransform(native_eyes[index])));
        eye.fov = OpenVrProjectionRawToEyeFov(left, right, top, bottom);
        eye.width = impl_->system_info.recommended_width;
        eye.height = impl_->system_info.recommended_height;
    }
    return true;
}

bool OpenVrRuntime::WaitForHmdPose(Pose& pose) noexcept {
    impl_->last_error.clear();
    impl_->last_result_code = 0;
    pose = {};
    if (!initialized()) {
        impl_->last_error = "OpenVR is not initialized";
        impl_->last_result_code = -1;
        return false;
    }

    std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> poses{};
    const vr::EVRCompositorError error = impl_->compositor->WaitGetPoses(
        poses.data(), static_cast<std::uint32_t>(poses.size()), nullptr, 0);
    if (error != vr::VRCompositorError_None) {
        impl_->last_result_code = static_cast<std::int32_t>(error);
        impl_->last_error = "OpenVR WaitGetPoses failed with code " + std::to_string(static_cast<int>(error));
        return false;
    }

    const vr::TrackedDevicePose_t& hmd = poses[vr::k_unTrackedDeviceIndex_Hmd];
    pose = PoseFromRigidTransform3x4(Flatten(hmd.mDeviceToAbsoluteTracking));
    pose.orientation_valid = hmd.bPoseIsValid && pose.orientation_valid;
    pose.position_valid = hmd.bPoseIsValid && pose.position_valid;
    return true;
}

bool OpenVrRuntime::ReadHmdPose(Pose& pose) noexcept {
    impl_->last_error.clear();
    impl_->last_result_code = 0;
    pose = {};
    if (!initialized()) {
        impl_->last_error = "OpenVR is not initialized";
        impl_->last_result_code = -1;
        return false;
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
    return true;
}

void OpenVrRuntime::PostPresentHandoff() noexcept {
    if (!initialized()) return;
    impl_->compositor->PostPresentHandoff();
}

bool OpenVrRuntime::ReadPresentationState(OpenVrPresentationState& state) noexcept {
    state = {};
    if (!initialized()) return false;

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
}

bool OpenVrRuntime::InitializeGlobalActions(
    const std::string_view absolute_manifest_path) noexcept {
    impl_->last_error.clear();
    impl_->last_result_code = 0;
    impl_->input = nullptr;
    impl_->global_action_set = vr::k_ulInvalidActionSetHandle;
    impl_->recenter_action = vr::k_ulInvalidActionHandle;
    impl_->recenter_edge.Reset();
    if (!initialized()) {
        impl_->last_error = "OpenVR is not initialized";
        impl_->last_result_code = -1;
        return false;
    }
    if (absolute_manifest_path.empty()) {
        impl_->last_error = "OpenVR action manifest path is empty";
        impl_->last_result_code = -1;
        return false;
    }

    vr::IVRInput* input = vr::VRInput();
    if (input == nullptr) {
        impl_->last_error = "OpenVR input interface is unavailable";
        impl_->last_result_code = -1;
        return false;
    }

    const std::string manifest_path(absolute_manifest_path);
    vr::EVRInputError error = input->SetActionManifestPath(manifest_path.c_str());
    if (error != vr::VRInputError_None) {
        impl_->last_result_code = static_cast<std::int32_t>(error);
        impl_->last_error = "OpenVR SetActionManifestPath failed with code " +
            std::to_string(static_cast<int>(error));
        return false;
    }

    vr::VRActionSetHandle_t global_action_set = vr::k_ulInvalidActionSetHandle;
    error = input->GetActionSetHandle("/actions/global", &global_action_set);
    if (error != vr::VRInputError_None ||
        global_action_set == vr::k_ulInvalidActionSetHandle) {
        impl_->last_result_code = static_cast<std::int32_t>(error);
        impl_->last_error = "OpenVR global action set resolution failed with code " +
            std::to_string(static_cast<int>(error));
        return false;
    }

    vr::VRActionHandle_t recenter_action = vr::k_ulInvalidActionHandle;
    error = input->GetActionHandle("/actions/global/in/recenter", &recenter_action);
    if (error != vr::VRInputError_None || recenter_action == vr::k_ulInvalidActionHandle) {
        impl_->last_result_code = static_cast<std::int32_t>(error);
        impl_->last_error = "OpenVR recenter action resolution failed with code " +
            std::to_string(static_cast<int>(error));
        return false;
    }

    impl_->input = input;
    impl_->global_action_set = global_action_set;
    impl_->recenter_action = recenter_action;
    return true;
}

bool OpenVrRuntime::PollGlobalActions(OpenVrGlobalActions& actions) noexcept {
    impl_->last_error.clear();
    impl_->last_result_code = 0;
    actions = {};
    if (!global_actions_initialized()) {
        impl_->last_error = "OpenVR global actions are not initialized";
        impl_->last_result_code = -1;
        return false;
    }

    vr::VRActiveActionSet_t active_set{};
    active_set.ulActionSet = impl_->global_action_set;
    const vr::EVRInputError update_error = impl_->input->UpdateActionState(
        &active_set, sizeof(active_set), 1);
    if (update_error != vr::VRInputError_None) {
        impl_->last_result_code = static_cast<std::int32_t>(update_error);
        impl_->last_error = "OpenVR UpdateActionState failed with code " +
            std::to_string(static_cast<int>(update_error));
        return false;
    }

    vr::InputDigitalActionData_t data{};
    const vr::EVRInputError digital_error = impl_->input->GetDigitalActionData(
        impl_->recenter_action, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
    if (digital_error != vr::VRInputError_None) {
        impl_->last_result_code = static_cast<std::int32_t>(digital_error);
        impl_->last_error = "OpenVR GetDigitalActionData(recenter) failed with code " +
            std::to_string(static_cast<int>(digital_error));
        return false;
    }

    actions.recenter_active = data.bActive;
    actions.recenter_requested = impl_->recenter_edge.Update(data.bActive, data.bState);
    return true;
}

bool OpenVrRuntime::initialized() const noexcept {
    return impl_ && impl_->system != nullptr && impl_->compositor != nullptr;
}

bool OpenVrRuntime::global_actions_initialized() const noexcept {
    return initialized() && impl_->input != nullptr &&
        impl_->global_action_set != vr::k_ulInvalidActionSetHandle &&
        impl_->recenter_action != vr::k_ulInvalidActionHandle;
}

const OpenVrSystemInfo& OpenVrRuntime::system_info() const noexcept { return impl_->system_info; }
std::string_view OpenVrRuntime::last_error() const noexcept { return impl_->last_error; }
std::int32_t OpenVrRuntime::last_result_code() const noexcept {
    return impl_->last_result_code;
}
void* OpenVrRuntime::native_system() const noexcept { return impl_->system; }
void* OpenVrRuntime::native_compositor() const noexcept { return impl_->compositor; }

} // namespace cojvr::runtime
