#pragma once

#include <d3d9.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace cojvr::backends::d3d9 {

struct IndependentEyeTextureDesc {
    UINT width = 0, height = 0;
    D3DFORMAT format = D3DFMT_UNKNOWN;
    DWORD usage = D3DUSAGE_RENDERTARGET;
    bool history = false;
};

enum class IndependentEyeSurface : std::size_t {
    final_color, final_depth, internal_color, internal_depth, additional_depth, count
};

struct IndependentEyeTargetConfig {
    UINT width = 0, height = 0;
    // Exact output dimensions, supplied by the game adapter after native rounding.
    // Twelve scratch entries and two histories: sixteen unique texture payloads.
    std::array<IndependentEyeTextureDesc, 14> textures{};
    // Explicit private allocations, without speculative deduplication. DEFAULT,
    // SURFACE, non-MSAA, usage RENDERTARGET/DEPTHSTENCIL must match each slot.
    std::array<D3DSURFACE_DESC, 5> surfaces{};
    std::uint64_t max_payload_bytes = 0;
};

class IndependentEyeTargets final {
    struct Impl;
public:
    // Source lifetime only, never a compositor/capture-slot lease. Keep this
    // token on the device owner thread. It may outlive the bank facade.
    class Lease final {
    public:
        Lease() noexcept = default;
        ~Lease();
        Lease(Lease&&) noexcept;
        Lease& operator=(Lease&&) noexcept;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        explicit operator bool() const noexcept { return bool(impl_); }
        void Reset() noexcept;
    private:
        friend class IndependentEyeTargets;
        std::shared_ptr<Impl> impl_;
    };
    IndependentEyeTargets();
    ~IndependentEyeTargets();
    IndependentEyeTargets(const IndependentEyeTargets&) = delete;
    IndependentEyeTargets& operator=(const IndependentEyeTargets&) = delete;

    // No device state mutation. Failed creation retains the previous bank.
    // The byte total is payload only; parent admission owns VA/VRAM headroom.
    [[nodiscard]] static bool EstimatePayload(const IndependentEyeTargetConfig&,
                                            std::uint64_t& bytes) noexcept;
    [[nodiscard]] bool Create(IDirect3DDevice9*, std::uint64_t generation,
                              const IndependentEyeTargetConfig&) noexcept;
    // Captures ALL state plus retained MRT/depth/viewport/scissor, then binds
    // private final color/depth. Not nestable. No native game fields are touched.
    [[nodiscard]] bool Begin(IDirect3DDevice9*, std::uint64_t generation) noexcept;
    // Failed restoration keeps the scope active and resources quarantined. Retry
    // End/RestoreAll on the owner thread; no new Begin/Create/reset is needed.
    // Keep the facade alive until recovery succeeds; destruction is best effort.
    [[nodiscard]] bool RestoreAll() noexcept;
    [[nodiscard]] bool End() noexcept;
    // Explicit owner/source invalidation only, never automatic per eye/scope.
    // Owner thread, inactive and healthy bank only. Zeros both histories for
    // both eyes without changing bindings or other scratch/output contents.
    [[nodiscard]] bool ClearHistories() noexcept;
    // Borrowed pointers, owner thread only. Copy to transport-owned resources on
    // this device's ordered queue before ending/releasing the bank's source lease.
    [[nodiscard]] IDirect3DTexture9* texture(std::size_t index, std::size_t eye) const noexcept;
    [[nodiscard]] IDirect3DSurface9* surface(IndependentEyeSurface) const noexcept;
    // Only after the whole eye pair and native field restoration, before sprites.
    // Uses private final color (the last/right eye), proportionally letterboxed.
    [[nodiscard]] bool MirrorRightTo(IDirect3DSurface9* original_default) noexcept;
    [[nodiscard]] Lease AcquireLease() noexcept;
    // Must run on the owner thread BEFORE native Reset/ResetEx. Refuses active
    // scopes/leases. No bank resources may be retained independently across Reset.
    [[nodiscard]] bool InvalidateBeforeReset() noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] std::uint64_t payload_bytes() const noexcept;
    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
#if defined(COJVR_INDEPENDENT_EYE_FAULT_TESTING)
    // Zero-based resource allocation ordinal; -1 disables. Test executable only.
    void FailAllocationForTest(int ordinal) noexcept;
    // Zero-based restore operation/readback ordinal, persistent across retries;
    // skips that GPU call/readback and reports failure. -1 disables.
    void FailRestoreForTest(int ordinal) noexcept;
    [[nodiscard]] std::uint32_t RestoreAttemptsForTest() const noexcept;
#endif

private:
    std::shared_ptr<Impl> impl_;
};

} // namespace cojvr::backends::d3d9
