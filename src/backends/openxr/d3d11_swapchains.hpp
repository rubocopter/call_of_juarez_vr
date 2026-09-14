#pragma once

#include "backends/openxr/d3d11_session.hpp"
#include "runtime/openxr_runtime.hpp"

#include <d3d11.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

namespace cojvr::backends::openxr {

struct AcquiredEyeImage {
    runtime::Eye eye = runtime::Eye::left;
    std::uint32_t image_index = 0;
    ID3D11Texture2D* texture = nullptr;
};

class D3D11StereoSwapchains final {
public:
    D3D11StereoSwapchains();
    ~D3D11StereoSwapchains();

    D3D11StereoSwapchains(const D3D11StereoSwapchains&) = delete;
    D3D11StereoSwapchains& operator=(const D3D11StereoSwapchains&) = delete;
    D3D11StereoSwapchains(D3D11StereoSwapchains&&) noexcept;
    D3D11StereoSwapchains& operator=(D3D11StereoSwapchains&&) noexcept;

    bool Initialize(runtime::OpenXrRuntime& xr_runtime, D3D11SessionBridge& session) noexcept;
    void Shutdown() noexcept;

    bool Acquire(runtime::Eye eye, AcquiredEyeImage& image) noexcept;
    bool Release(runtime::Eye eye) noexcept;
    runtime::OpenXrCompositionLayer BuildProjectionLayer(
        const std::array<runtime::EyeView, 2>& views) noexcept;

    [[nodiscard]] std::int64_t format() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cojvr::backends::openxr
