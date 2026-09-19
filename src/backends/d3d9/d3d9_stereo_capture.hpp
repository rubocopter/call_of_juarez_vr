#pragma once

#include "backends/d3d9/stereo_frame.hpp"
#include "runtime/vr_types.hpp"

#include <d3d9.h>

#include <cstdint>
#include <memory>
#include <string_view>

namespace cojvr::backends::d3d9 {

struct D3D9StereoCaptureStats {
    std::uint64_t frames_fenced = 0;
    std::uint64_t frames_collected = 0;
    std::uint64_t frames_dropped_no_slot = 0;
    std::uint64_t frames_invalidated = 0;
};

// Classic-D3D9 producer side of the native-stereo transport. Eye capture queues
// GPU-to-GPU copies into a small ring. CPU readback is deferred until a later
// frame, so the engine no longer waits for each eye at its render-view boundary.
// An event query records whether the driver had already retired the copy; it is
// diagnostic only because classic-D3D9 drivers do not reliably expose that
// completion without a flushing GetData call.
class D3D9StereoCapture final {
public:
    D3D9StereoCapture();
    ~D3D9StereoCapture();

    D3D9StereoCapture(const D3D9StereoCapture&) = delete;
    D3D9StereoCapture& operator=(const D3D9StereoCapture&) = delete;

    [[nodiscard]] bool CaptureEye(
        IDirect3DDevice9* device,
        runtime::Eye eye,
        std::uint64_t frame_sequence,
        std::uint64_t generation) noexcept;
    [[nodiscard]] bool CaptureEyeSurface(
        IDirect3DDevice9* device,
        IDirect3DSurface9* source,
        runtime::Eye eye,
        std::uint64_t frame_sequence,
        std::uint64_t generation) noexcept;
    [[nodiscard]] bool EndFrame(
        std::uint64_t frame_sequence,
        const runtime::Pose& render_hmd_pose,
        std::uint64_t render_pose_sequence) noexcept;
    [[nodiscard]] bool TryCollectReady(StereoCpuFrame& frame) noexcept;
    // Releases device/default-pool resources while keeping the capture object
    // reusable. Owners with an explicit D3D9 lifecycle signal call this before
    // Reset/recreation; the exact CoJ native-stereo proof does not depend on a
    // Reset hook.
    void InvalidateResources() noexcept;
    void Shutdown() noexcept;

    [[nodiscard]] D3D9StereoCaptureStats stats() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
    [[nodiscard]] std::string_view capture_description() const noexcept;
    [[nodiscard]] std::string_view collect_description() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cojvr::backends::d3d9
