#pragma once

#include "backends/d3d9/stereo_frame.hpp"

#include <d3d11.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace cojvr::backends::openvr {

struct D3D9SharedTextureCopyTiming {
    double open_ms = 0.0;
    double copy_queue_ms = 0.0;
    double consumer_wait_ms = 0.0;
    std::size_t pending_copy_fences = 0;
};

struct D3D9SharedTextureBridgeStats {
    std::uint64_t frames_copied = 0;
    std::uint64_t resources_opened = 0;
    std::uint64_t copy_fences_completed = 0;
    std::uint64_t copy_fence_poll_pending = 0;
    std::uint64_t open_failures = 0;
    std::uint64_t copy_failures = 0;
    std::uint64_t abandoned_on_shutdown = 0;
    std::size_t pending_copy_fences = 0;
    std::size_t pending_copy_fences_peak = 0;
    double last_copy_completion_ms = 0.0;
    double max_copy_completion_ms = 0.0;
};

// Presenter-thread bridge for D3D9Ex shared render targets. Shared handles are
// opened once and copied GPU-to-GPU into the presenter's normal D3D11 eye
// textures. A D3D11 event query holds each producer lease until the copy has
// actually retired; no wait is performed on the game thread or presenter.
class D3D9SharedTextureBridge final {
public:
    D3D9SharedTextureBridge();
    ~D3D9SharedTextureBridge();

    D3D9SharedTextureBridge(const D3D9SharedTextureBridge&) = delete;
    D3D9SharedTextureBridge& operator=(const D3D9SharedTextureBridge&) = delete;

    [[nodiscard]] bool CopyFrame(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        d3d9::StereoCpuFrame& frame,
        const std::array<ID3D11Texture2D*, 2>& destination,
        D3D9SharedTextureCopyTiming& timing) noexcept;
    void Poll(ID3D11DeviceContext* context) noexcept;
    void ResetOpenedResources() noexcept;
    void Shutdown(ID3D11DeviceContext* context) noexcept;

    [[nodiscard]] D3D9SharedTextureBridgeStats stats() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cojvr::backends::openvr
