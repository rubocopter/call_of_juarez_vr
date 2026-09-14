#include "backends/openvr/d3d11_compositor.hpp"

#include "runtime/openvr_runtime.hpp"

#include <d3d11.h>
#include <openvr.h>

#include <array>

namespace cojvr::backends::openvr {

bool SubmitStereoD3D11(
    runtime::OpenVrRuntime& runtime,
    ID3D11Texture2D* left_eye,
    ID3D11Texture2D* right_eye,
    std::string& error) noexcept {
    error.clear();
    if (!runtime.initialized()) {
        error = "OpenVR runtime is not initialized";
        return false;
    }
    if (left_eye == nullptr || right_eye == nullptr) {
        error = "OpenVR D3D11 submission requires both eye textures";
        return false;
    }

    auto* compositor = static_cast<vr::IVRCompositor*>(runtime.native_compositor());
    if (compositor == nullptr) {
        error = "OpenVR compositor interface is unavailable";
        return false;
    }

    constexpr std::array<vr::EVREye, 2> eyes{vr::Eye_Left, vr::Eye_Right};
    const std::array<ID3D11Texture2D*, 2> textures{left_eye, right_eye};
    for (std::size_t index = 0; index < eyes.size(); ++index) {
        vr::Texture_t texture{
            textures[index],
            vr::TextureType_DirectX,
            vr::ColorSpace_Auto,
        };
        const vr::EVRCompositorError result = compositor->Submit(
            eyes[index], &texture, nullptr, vr::Submit_Default);
        if (result != vr::VRCompositorError_None) {
            error = std::string(index == 0 ? "Left" : "Right") +
                " eye OpenVR submission failed with code " +
                std::to_string(static_cast<int>(result));
            return false;
        }
    }
    return true;
}

} // namespace cojvr::backends::openvr
