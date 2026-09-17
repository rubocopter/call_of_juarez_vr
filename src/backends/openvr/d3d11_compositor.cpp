#include "backends/openvr/d3d11_compositor.hpp"

#include "runtime/openvr_runtime.hpp"
#include "runtime/vr_math.hpp"

#include <d3d11.h>
#include <openvr.h>

#include <array>

namespace cojvr::backends::openvr {

namespace {

runtime::Eye RuntimeEye(const EyeSubmission eye) noexcept {
    return eye == EyeSubmission::Left ? runtime::Eye::left : runtime::Eye::right;
}

} // namespace

bool SubmitEyeD3D11(
    runtime::OpenVrRuntime& runtime,
    ID3D11Texture2D* texture,
    const EyeSubmission eye,
    int& runtime_result,
    std::string& error) noexcept {
    error.clear();
    runtime_result = static_cast<int>(vr::VRCompositorError_None);
    if (!runtime.initialized()) {
        error = "OpenVR runtime is not initialized";
        runtime_result = -1;
        return false;
    }
    if (!texture) {
        error = "OpenVR D3D11 submission requires an eye texture";
        runtime_result = -1;
        runtime.RecordEyeSubmission(RuntimeEye(eye), false);
        return false;
    }
    auto* compositor = static_cast<vr::IVRCompositor*>(runtime.native_compositor());
    if (!compositor) {
        error = "OpenVR compositor interface is unavailable";
        runtime_result = -1;
        runtime.RecordEyeSubmission(RuntimeEye(eye), false);
        return false;
    }

    vr::Texture_t native_texture{texture, vr::TextureType_DirectX, vr::ColorSpace_Auto};
    const vr::EVRCompositorError result = compositor->Submit(
        eye == EyeSubmission::Left ? vr::Eye_Left : vr::Eye_Right,
        &native_texture, nullptr, vr::Submit_Default);
    runtime_result = static_cast<int>(result);
    runtime.RecordEyeSubmission(RuntimeEye(eye), result == vr::VRCompositorError_None);
    if (result != vr::VRCompositorError_None) {
        error = std::string(eye == EyeSubmission::Left ? "Left" : "Right") +
            " eye OpenVR submission failed with code " + std::to_string(runtime_result);
        return false;
    }
    return true;
}

bool SubmitEyeD3D11WithPose(
    runtime::OpenVrRuntime& runtime,
    ID3D11Texture2D* texture,
    const EyeSubmission eye,
    const runtime::Pose& render_hmd_pose,
    int& runtime_result,
    std::string& error) noexcept {
    error.clear();
    runtime_result = static_cast<int>(vr::VRCompositorError_None);
    if (!runtime.initialized()) {
        error = "OpenVR runtime is not initialized";
        runtime_result = -1;
        return false;
    }
    if (!texture) {
        error = "OpenVR D3D11 submission requires an eye texture";
        runtime_result = -1;
        runtime.RecordEyeSubmission(RuntimeEye(eye), false);
        return false;
    }
    auto* compositor = static_cast<vr::IVRCompositor*>(runtime.native_compositor());
    if (!compositor) {
        error = "OpenVR compositor interface is unavailable";
        runtime_result = -1;
        runtime.RecordEyeSubmission(RuntimeEye(eye), false);
        return false;
    }

    std::array<float, 12> render_transform{};
    if (!runtime::RigidTransform3x4FromPose(render_hmd_pose, render_transform)) {
        error = "OpenVR explicit-pose submission requires a valid rigid HMD render pose";
        runtime_result = -1;
        runtime.RecordEyeSubmission(RuntimeEye(eye), false);
        return false;
    }

    vr::VRTextureWithPose_t native_texture{};
    native_texture.handle = texture;
    native_texture.eType = vr::TextureType_DirectX;
    native_texture.eColorSpace = vr::ColorSpace_Auto;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            native_texture.mDeviceToAbsoluteTracking.m[row][column] =
                render_transform[row * 4 + column];
        }
    }

    const vr::EVRCompositorError result = compositor->Submit(
        eye == EyeSubmission::Left ? vr::Eye_Left : vr::Eye_Right,
        &native_texture, nullptr, vr::Submit_TextureWithPose);
    runtime_result = static_cast<int>(result);
    runtime.RecordEyeSubmission(RuntimeEye(eye), result == vr::VRCompositorError_None);
    if (result != vr::VRCompositorError_None) {
        error = std::string(eye == EyeSubmission::Left ? "Left" : "Right") +
            " eye explicit-pose OpenVR submission failed with code " +
            std::to_string(runtime_result);
        return false;
    }
    return true;
}

bool SubmitStereoD3D11(
    runtime::OpenVrRuntime& runtime,
    ID3D11Texture2D* left_eye,
    ID3D11Texture2D* right_eye,
    std::string& error) noexcept {
    error.clear();
    if (left_eye == nullptr || right_eye == nullptr) {
        error = "OpenVR D3D11 submission requires both eye textures";
        return false;
    }

    const std::array<ID3D11Texture2D*, 2> textures{left_eye, right_eye};
    constexpr std::array<EyeSubmission, 2> eyes{EyeSubmission::Left, EyeSubmission::Right};
    for (std::size_t index = 0; index < eyes.size(); ++index) {
        int runtime_result = 0;
        if (!SubmitEyeD3D11(runtime, textures[index], eyes[index], runtime_result, error)) {
            return false;
        }
    }
    return true;
}

} // namespace cojvr::backends::openvr
