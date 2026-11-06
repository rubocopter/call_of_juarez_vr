#pragma once

#include <d3d9.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace cojvr::backends::d3d9 {

struct D3D9SampledDepthConfig {
    UINT width = 0, height = 0;
    D3DFORMAT native_color_format = D3DFMT_A8R8G8B8;
    // Private INTZ storage accepts native D24S8 and D24X8 depth layouts.
    // A D24X8 caller must preserve its existing stencil-disabled contract.
    D3DFORMAT native_depth_format = D3DFMT_D24S8;
    D3DMULTISAMPLE_TYPE multisample = D3DMULTISAMPLE_NONE;
    DWORD multisample_quality = 0;
    std::uint64_t generation = 0;
    // Optional private full-size FP16 color copy, fenced with the packed depth.
    // Default-off; requires native_color_format == A16B16G16R16F.
    bool capture_color = false;
};

struct D3D9SampledDepthPreview {
    static constexpr UINT max_extent = 512;
    UINT width = 0, height = 0;
    std::vector<std::uint16_t> color_rgba16f, depth_rgba16f;
};
struct D3D9SampledDepthPoint {
    std::array<std::uint16_t,4> color{},depth{};
};

// A copyable immutable capture lease, including the producer allocation/device.
// Retain this object until ALL D3D11 consumer work has completed (prove that with
// a consumer event query). Opening a handle is not a substitute for that lease.
// The consumer must treat the shared image as read-only. Legacy shared handles
// are borrowed, not NT handles: never CloseHandle them. No source texture is shared.
class D3D9SampledDepthSnapshot final {
public:
    D3D9SampledDepthSnapshot() noexcept = default;
    [[nodiscard]] bool valid() const noexcept;
    // Producer thread only; one nonblocking event-query poll, one initial FLUSH.
    // S_FALSE means pending. A failure is permanent for this capture/slot.
    [[nodiscard]] HRESULT PollReady() const noexcept;
    // Null until one producer event query proves BOTH optional color and depth
    // copies complete. Color is also null when capture_color was not requested.
    // These accessors perform no producer GPU calls and may be read by a worker
    // while its immutable lease is retained. Final release stays on the owner.
    [[nodiscard]] HANDLE shared_handle() const noexcept;
    [[nodiscard]] HANDLE color_shared_handle() const noexcept;
    // Bounded developer readback after producer readiness, outside the scene.
    // May wait for the 8x8 copy. Never use in the reconstruction hot path.
    [[nodiscard]] HRESULT ReadDiagnosticGrid(std::array<std::uint32_t,64>& output) const noexcept;
    // Optional joint point-sampled preview, at most 512 pixels on either axis.
    // Raw color and packed depth are immutable copies from the same Snapshot.
    // Readback requires readiness and the producer thread, outside the scene.
    [[nodiscard]] HRESULT ReadDiagnosticPreview(D3D9SampledDepthPreview& output) const noexcept;
    // Exact center pixel of BOTH private full-size planes. Owner thread only,
    // outside scene after readiness; diagnostic readback, never a hot-path input.
    [[nodiscard]] HRESULT ReadDiagnosticPoint(D3D9SampledDepthPoint& output) const noexcept;
    [[nodiscard]] UINT width() const noexcept;
    [[nodiscard]] UINT height() const noexcept;
    [[nodiscard]] std::uint64_t sequence() const noexcept;
    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] std::uintptr_t producer_device() const noexcept;
    [[nodiscard]] std::uint32_t producer_thread() const noexcept;
    // D3DFMT_A16B16G16R16F / DXGI_FORMAT_R16G16B16A16_FLOAT, packed_device_z_rgb24:
    // n = floor(saturate(deviceZ)*16777215 + .5); RGB hold INTEGER bytes
    // (n>>16, (n>>8)&255, n&255), NOT normalized bytes. Integers 0..255 are exact
    // in binary16. Decode Z = (R*65536 + G*256 + B)/16777215. Alpha is only an
    // approximate raw Z and MUST NOT be used as qualified depth. No linearization,
    // reversal, projection inference, or stencil transport is performed.
    static constexpr D3DFORMAT format = D3DFMT_A16B16G16R16F;

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
    friend class D3D9SampledDepthCapture;
};

// Renderer infrastructure only: this does not qualify any Call of Juarez source,
// material coverage, eye/pose association, or native build. All GPU operations,
// creation, producer destruction and final lease release belong to the original
// D3D9 owner thread. Not reentrant; do not call while recording a state block.
class D3D9SampledDepthCapture final {
public:
    static constexpr std::size_t slot_count = 2;
    static constexpr UINT max_extent = 4096;
    static constexpr std::uint64_t max_payload_bytes = 256ULL * 1024 * 1024;

    D3D9SampledDepthCapture();
    ~D3D9SampledDepthCapture();
    D3D9SampledDepthCapture(const D3D9SampledDepthCapture&) = delete;
    D3D9SampledDepthCapture& operator=(const D3D9SampledDepthCapture&) = delete;

    // Capability checks only. Initialize additionally proves actual allocations,
    // sharing-handle creation and shader creation. D3D9Ex is mandatory.
    [[nodiscard]] static HRESULT CheckSupport(
        IDirect3DDevice9*, const D3D9SampledDepthConfig&) noexcept;
    [[nodiscard]] HRESULT Initialize(
        IDirect3DDevice9*, const D3D9SampledDepthConfig&) noexcept;
    // Borrowed. The caller explicitly binds this surface BEFORE its native depth
    // clear/render and keeps native color/MSAA/layout compatible. Initialize and
    // this accessor NEVER bind or replace a native surface.
    [[nodiscard]] IDirect3DSurface9* native_depth_surface() const noexcept;
    // Inside the caller's active BeginScene/EndScene pair, AFTER native depth
    // clear/render, with our depth surface still bound. No BeginScene/EndScene,
    // clear, resolve or automatic game-depth replacement occurs. capture_color
    // copies the bound native FP16 target into the slot's private full-size
    // shared texture. Its copy and depth draw use the SAME completion query.
    // The optional diagnostic preview adds bounded private StretchRect copies.
    // Sequences are nonzero/strictly increasing; generation must match Initialize.
    // The output must be empty; on failure it stays empty. ERROR_BUSY means all
    // bounded slots are leased/pending. No waiting or allocation growth occurs.
    // All state is restored before success. Any restore error blocks capture and
    // retains the original preimage for RetryRestore; no partial output escapes.
    [[nodiscard]] HRESULT Snapshot(std::uint64_t sequence, std::uint64_t generation,
        D3D9SampledDepthSnapshot& output, bool diagnostic_preview = false) noexcept;
    [[nodiscard]] HRESULT RetryRestore() noexcept;
    [[nodiscard]] const char* last_error() const noexcept;
#if defined(COJVR_SAMPLED_DEPTH_FAULT_TESTING)
    // Standalone fixture only. Fail one restore operation, still attempt the rest.
    void FailNextRestoreForTest(unsigned operation) noexcept;
#endif

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cojvr::backends::d3d9
