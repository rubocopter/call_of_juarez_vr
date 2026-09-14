#include "runtime/openvr_runtime.hpp"
#include "runtime/vr_math.hpp"
#include <openvr.h>
#include <algorithm>
#include <array>
#include <string>
#include <utility>

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

struct OpenVrRuntime::Impl {
    vr::IVRSystem* system = nullptr;
    vr::IVRCompositor* compositor = nullptr;
    OpenVrSystemInfo system_info{};
    std::string last_error;
};

OpenVrRuntime::OpenVrRuntime() : impl_(std::make_unique<Impl>()) {}
OpenVrRuntime::~OpenVrRuntime() { Shutdown(); }
OpenVrRuntime::OpenVrRuntime(OpenVrRuntime&&) noexcept = default;
OpenVrRuntime& OpenVrRuntime::operator=(OpenVrRuntime&&) noexcept = default;

bool OpenVrRuntime::Initialize(const std::string_view application_name) noexcept {
    impl_->last_error.clear();
    if (initialized()) {
        impl_->last_error = "OpenVR is already initialized";
        return false;
    }

    const std::string application_name_copy(application_name);
    vr::EVRInitError init_error = vr::VRInitError_None;
    vr::IVRSystem* system = vr::VR_Init(&init_error, vr::VRApplication_Scene, application_name_copy.c_str());
    if (init_error != vr::VRInitError_None || system == nullptr) {
        impl_->last_error = InitializationError(init_error);
        if (system != nullptr) vr::VR_Shutdown();
        return false;
    }

    vr::IVRCompositor* compositor = vr::VRCompositor();
    if (compositor == nullptr) {
        impl_->last_error = "OpenVR compositor interface is unavailable";
        vr::VR_Shutdown();
        return false;
    }
    compositor->SetTrackingSpace(vr::TrackingUniverseStanding);

    OpenVrSystemInfo info{};
    system->GetRecommendedRenderTargetSize(&info.recommended_width, &info.recommended_height);
    system->GetDXGIOutputInfo(&info.dxgi_adapter_index);
    if (info.recommended_width == 0 || info.recommended_height == 0) {
        impl_->last_error = "OpenVR returned an empty recommended render-target size";
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
        eye.fov = FovFromTangents(left, right, top, bottom);
        eye.width = impl_->system_info.recommended_width;
        eye.height = impl_->system_info.recommended_height;
    }
    return true;
}

bool OpenVrRuntime::WaitForHmdPose(Pose& pose) noexcept {
    impl_->last_error.clear();
    pose = {};
    if (!initialized()) {
        impl_->last_error = "OpenVR is not initialized";
        return false;
    }

    std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> poses{};
    const vr::EVRCompositorError error = impl_->compositor->WaitGetPoses(
        poses.data(), static_cast<std::uint32_t>(poses.size()), nullptr, 0);
    if (error != vr::VRCompositorError_None) {
        impl_->last_error = "OpenVR WaitGetPoses failed with code " + std::to_string(static_cast<int>(error));
        return false;
    }

    const vr::TrackedDevicePose_t& hmd = poses[vr::k_unTrackedDeviceIndex_Hmd];
    pose = PoseFromRigidTransform3x4(Flatten(hmd.mDeviceToAbsoluteTracking));
    pose.orientation_valid = hmd.bPoseIsValid;
    pose.position_valid = hmd.bPoseIsValid;
    return true;
}

bool OpenVrRuntime::initialized() const noexcept {
    return impl_ && impl_->system != nullptr && impl_->compositor != nullptr;
}

const OpenVrSystemInfo& OpenVrRuntime::system_info() const noexcept { return impl_->system_info; }
std::string_view OpenVrRuntime::last_error() const noexcept { return impl_->last_error; }
void* OpenVrRuntime::native_system() const noexcept { return impl_->system; }
void* OpenVrRuntime::native_compositor() const noexcept { return impl_->compositor; }

} // namespace cojvr::runtime
