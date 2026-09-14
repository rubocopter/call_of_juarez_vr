#include "backends/openxr/d3d11_swapchains.hpp"

#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace cojvr::backends::openxr {
namespace {

std::size_t EyeIndex(runtime::Eye eye) noexcept {
    return eye == runtime::Eye::left ? 0U : 1U;
}

XrPosef ToXrPose(const runtime::Pose& pose) noexcept {
    XrPosef xr_pose{};
    xr_pose.orientation = XrQuaternionf{
        pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w};
    xr_pose.position = XrVector3f{pose.position.x, pose.position.y, pose.position.z};
    return xr_pose;
}

XrFovf ToXrFov(const runtime::EyeFov& fov) noexcept {
    return XrFovf{fov.angle_left, fov.angle_right, fov.angle_up, fov.angle_down};
}

} // namespace

struct D3D11StereoSwapchains::Impl {
    runtime::OpenXrRuntime* runtime = nullptr;
    std::array<runtime::OpenXrSwapchainHandle, 2> swapchains{};
    std::array<std::vector<XrSwapchainImageD3D11KHR>, 2> images{};
    std::array<bool, 2> acquired{false, false};
    std::array<std::uint32_t, 2> widths{};
    std::array<std::uint32_t, 2> heights{};
    std::int64_t format = 0;
    std::array<XrCompositionLayerProjectionView, 2> projection_views{{
        XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
        XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
    }};
    XrCompositionLayerProjection projection_layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    std::string last_error;
};

D3D11StereoSwapchains::D3D11StereoSwapchains() : impl_(std::make_unique<Impl>()) {}
D3D11StereoSwapchains::~D3D11StereoSwapchains() { Shutdown(); }
D3D11StereoSwapchains::D3D11StereoSwapchains(D3D11StereoSwapchains&&) noexcept = default;
D3D11StereoSwapchains& D3D11StereoSwapchains::operator=(D3D11StereoSwapchains&&) noexcept = default;

bool D3D11StereoSwapchains::Initialize(
    runtime::OpenXrRuntime& xr_runtime, D3D11SessionBridge& session) noexcept {
    if (!impl_) return false;
    if (impl_->runtime) {
        impl_->last_error = "D3D11 OpenXR swapchains are already initialized";
        return false;
    }
    if (!session.device() || xr_runtime.status() != runtime::OpenXrStatus::session_ready) {
        impl_->last_error = "OpenXR D3D11 session must be ready before swapchain creation";
        return false;
    }

    std::uint32_t format_count = 0;
    if (!xr_runtime.EnumerateSwapchainFormats({}, format_count) || format_count == 0) {
        impl_->last_error = std::string(xr_runtime.last_error());
        return false;
    }
    std::vector<std::int64_t> formats(format_count);
    if (!xr_runtime.EnumerateSwapchainFormats(formats, format_count)) {
        impl_->last_error = std::string(xr_runtime.last_error());
        return false;
    }
    formats.resize(format_count);

    constexpr std::array<DXGI_FORMAT, 4> preferred_formats{{
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_B8G8R8A8_UNORM,
    }};
    for (const DXGI_FORMAT preferred : preferred_formats) {
        if (std::find(formats.begin(), formats.end(), static_cast<std::int64_t>(preferred)) != formats.end()) {
            impl_->format = static_cast<std::int64_t>(preferred);
            break;
        }
    }
    if (impl_->format == 0) {
        impl_->last_error = "OpenXR runtime exposed no supported D3D11 RGBA/BGRA color format";
        return false;
    }

    impl_->runtime = &xr_runtime;
    const auto& recommended = xr_runtime.recommended_views();
    for (std::size_t eye = 0; eye < impl_->swapchains.size(); ++eye) {
        impl_->widths[eye] = recommended[eye].width;
        impl_->heights[eye] = recommended[eye].height;
        if (!xr_runtime.CreateColorSwapchain(
                impl_->format, impl_->widths[eye], impl_->heights[eye], 1, impl_->swapchains[eye])) {
            impl_->last_error = std::string(xr_runtime.last_error());
            Shutdown();
            return false;
        }

        std::uint32_t image_count = 0;
        if (!xr_runtime.EnumerateSwapchainImages(impl_->swapchains[eye], 0, image_count, nullptr) ||
            image_count == 0) {
            impl_->last_error = std::string(xr_runtime.last_error());
            Shutdown();
            return false;
        }
        impl_->images[eye].resize(image_count);
        for (auto& image : impl_->images[eye]) {
            image = XrSwapchainImageD3D11KHR{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR};
        }
        if (!xr_runtime.EnumerateSwapchainImages(
                impl_->swapchains[eye], image_count, image_count, impl_->images[eye].data())) {
            impl_->last_error = std::string(xr_runtime.last_error());
            Shutdown();
            return false;
        }
    }

    impl_->projection_layer.space = reinterpret_cast<XrSpace>(xr_runtime.native_reference_space());
    impl_->projection_layer.viewCount = static_cast<std::uint32_t>(impl_->projection_views.size());
    impl_->projection_layer.views = impl_->projection_views.data();
    impl_->last_error.clear();
    return true;
}

void D3D11StereoSwapchains::Shutdown() noexcept {
    if (!impl_) return;
    if (impl_->runtime) {
        for (std::size_t eye = 0; eye < impl_->swapchains.size(); ++eye) {
            if (impl_->acquired[eye]) {
                (void)impl_->runtime->ReleaseSwapchainImage(impl_->swapchains[eye]);
                impl_->acquired[eye] = false;
            }
            (void)impl_->runtime->DestroySwapchain(impl_->swapchains[eye]);
        }
    }
    for (auto& images : impl_->images) images.clear();
    impl_->runtime = nullptr;
    impl_->format = 0;
}

bool D3D11StereoSwapchains::Acquire(runtime::Eye eye, AcquiredEyeImage& image) noexcept {
    if (!impl_ || !impl_->runtime) return false;
    const std::size_t index = EyeIndex(eye);
    if (impl_->acquired[index]) {
        impl_->last_error = "OpenXR eye swapchain image is already acquired";
        return false;
    }

    std::uint32_t image_index = 0;
    if (!impl_->runtime->AcquireSwapchainImage(impl_->swapchains[index], image_index)) {
        impl_->last_error = std::string(impl_->runtime->last_error());
        return false;
    }
    if (!impl_->runtime->WaitSwapchainImage(impl_->swapchains[index])) {
        const std::string wait_error(impl_->runtime->last_error());
        (void)impl_->runtime->ReleaseSwapchainImage(impl_->swapchains[index]);
        impl_->last_error = wait_error;
        return false;
    }
    if (image_index >= impl_->images[index].size()) {
        (void)impl_->runtime->ReleaseSwapchainImage(impl_->swapchains[index]);
        impl_->last_error = "OpenXR returned an out-of-range D3D11 swapchain image index";
        return false;
    }

    impl_->acquired[index] = true;
    image = AcquiredEyeImage{eye, image_index, impl_->images[index][image_index].texture};
    impl_->last_error.clear();
    return true;
}

bool D3D11StereoSwapchains::Release(runtime::Eye eye) noexcept {
    if (!impl_ || !impl_->runtime) return false;
    const std::size_t index = EyeIndex(eye);
    if (!impl_->acquired[index]) {
        impl_->last_error = "OpenXR eye swapchain image is not acquired";
        return false;
    }
    if (!impl_->runtime->ReleaseSwapchainImage(impl_->swapchains[index])) {
        impl_->last_error = std::string(impl_->runtime->last_error());
        return false;
    }
    impl_->acquired[index] = false;
    impl_->last_error.clear();
    return true;
}

runtime::OpenXrCompositionLayer D3D11StereoSwapchains::BuildProjectionLayer(
    const std::array<runtime::EyeView, 2>& views) noexcept {
    if (!impl_ || !impl_->runtime) return {};

    for (std::size_t eye = 0; eye < impl_->projection_views.size(); ++eye) {
        auto& projection = impl_->projection_views[eye];
        projection.pose = ToXrPose(views[eye].pose);
        projection.fov = ToXrFov(views[eye].fov);
        projection.subImage.swapchain =
            reinterpret_cast<XrSwapchain>(impl_->swapchains[eye].native);
        projection.subImage.imageRect.offset = XrOffset2Di{0, 0};
        projection.subImage.imageRect.extent = XrExtent2Di{
            static_cast<std::int32_t>(impl_->widths[eye]),
            static_cast<std::int32_t>(impl_->heights[eye]),
        };
        projection.subImage.imageArrayIndex = 0;
    }
    return runtime::OpenXrCompositionLayer{&impl_->projection_layer};
}

std::int64_t D3D11StereoSwapchains::format() const noexcept {
    return impl_ ? impl_->format : 0;
}

std::string_view D3D11StereoSwapchains::last_error() const noexcept {
    return impl_ ? std::string_view(impl_->last_error)
                 : std::string_view("D3D11 OpenXR swapchains unavailable");
}

} // namespace cojvr::backends::openxr
