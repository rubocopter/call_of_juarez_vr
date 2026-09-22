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
    std::uint64_t query_not_ready = 0;
    std::uint64_t query_flushes = 0;
    std::uint64_t shared_frames_published = 0;
    std::uint64_t cpu_fallback_frames = 0;
    std::uint64_t consumer_releases = 0;
    std::uint64_t fallback_activations = 0;
    std::uint32_t ring_depth = 0;
    std::uint32_t ring_depth_peak = 0;
};

// Producer side of the native-stereo transport. A D3D9Ex device keeps frames
// GPU-resident in shared DEFAULT-pool render-target textures. The classic-D3D9
// fallback retains deferred CPU readback. Both routes use event queries polled
// without waiting. A slot that has not yet been submitted gets one explicit
// D3DGETDATA_FLUSH poll; subsequent S_FALSE results remain nonblocking.
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
    // Flat/menu capture is intentionally immediate and owns no persistent
    // default-pool resource. This keeps classic D3D9 Reset legal while the
    // exact CoJ menu path has no pre-Reset lifecycle callback.
    [[nodiscard]] bool CaptureFlatFrameImmediate(
        IDirect3DDevice9* device,
        IDirect3DSurface9* source,
        std::uint64_t frame_sequence,
        std::uint64_t generation,
        const runtime::Pose& render_hmd_pose,
        std::uint64_t render_pose_sequence,
        StereoCpuFrame& frame) noexcept;
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
    [[nodiscard]] bool gpu_resident_active() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
    [[nodiscard]] std::string_view capture_description() const noexcept;
    [[nodiscard]] std::string_view collect_description() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cojvr::backends::d3d9
