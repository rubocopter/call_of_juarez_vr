#pragma once
#include "backends/openvr/temporal_guides.hpp"
#include <memory>

namespace cojvr::backends::openvr::dlss {
struct PairIdentity {
    runtime::TemporalFrameIdentity frame{};
    std::uint64_t settings_revision=0,history_epoch=0,input_context=0;
};
struct PairEyeSource {
    PairIdentity identity{};
    // Nonzero immutable capture-lease/content identity, not a texture address.
    // Change it whenever contents or source semantics change, including reuse
    // of the same allocation. Tokens need not agree between physical eyes.
    std::uint64_t input_token=0;
    unsigned eye=0;
    GuideSource guides{};
    runtime::TemporalMatrix unjittered_view_projection=runtime::TemporalIdentity();
};
struct DepthMaskPolicy {
    bool enabled=false;
    float relative_tolerance=0;
    unsigned erosion_radius=0;
    std::array<bool,2> infinite_far{};
    bool operator==(const DepthMaskPolicy&) const = default;
};
[[nodiscard]] inline bool ValidDepthMaskPolicy(const DepthMaskPolicy& policy) noexcept {
    return std::isfinite(policy.relative_tolerance)&&policy.relative_tolerance>=0&&
        policy.relative_tolerance<=1&&policy.erosion_radius<=2;
}
struct PairFrame {
    PairIdentity identity{};
    std::array<PairEyeSource,2> eyes{};
    DepthMaskPolicy depth_mask{}; // Explicit experiment, supplied mask remains default.
};

// GPU/IPC dependency boundary. The default implementation uses Client and
// TemporalGuides. Alternate executors must enforce actual texture/context
// ownership, formats, extents, completed GPU work and separate eye features.
// Success never qualifies game source semantics. No presentation work belongs
// here. This interface permits CPU fault tests without starting a helper/GPU.
class PairExecutor {
public:
    virtual ~PairExecutor()=default;
    virtual bool Start(ID3D11Device*,const std::filesystem::path&) noexcept=0;
    virtual bool Build(unsigned eye,const runtime::TemporalFrameIdentity&) noexcept=0;
    virtual bool Generate(ID3D11Device*,ID3D11DeviceContext*,const std::array<GuideSource,2>&,
                          std::array<Inputs,2>&) noexcept=0;
    virtual bool Evaluate(ID3D11DeviceContext*,unsigned eye,const Inputs&) noexcept=0;
    // Prepare may replace bias planes. Derived history/clip transforms in frame
    // are session-owned. Opted-in executors must implement both phases; commit
    // is called only after both eyes complete, before pair publication.
    virtual bool PrepareHistoryMask(ID3D11Device*,ID3D11DeviceContext*,const PairFrame& frame,
                                    std::array<Inputs,2>&) noexcept {return !frame.depth_mask.enabled;}
    virtual bool CommitHistoryMask(ID3D11DeviceContext*) noexcept {return false;}
    virtual void InvalidateHistoryMask() noexcept {}
    [[nodiscard]] virtual ID3D11Texture2D* Output(unsigned eye) const noexcept=0;
    [[nodiscard]] virtual std::string_view last_error() const noexcept=0;
    virtual void Stop() noexcept=0;
};

// Small synchronous, serialized worker-owned laboratory session. Client IPC
// can block for SECONDS, including Start/Stop: never run this on the OpenVR
// presentation thread. Production scheduling/cancellation and final current
// identity checks belong to the caller's background worker/delivery boundary.
// Retain immutable input leases for the whole Reconstruct call. No native
// fallback pixels are written and no D3D9 resources/leases are owned here.
class PairSession final {
public:
    PairSession();
    explicit PairSession(std::unique_ptr<PairExecutor>);
    ~PairSession();
    PairSession(const PairSession&)=delete;
    PairSession& operator=(const PairSession&)=delete;
    [[nodiscard]] bool Start(ID3D11Device*,const std::filesystem::path& helper) noexcept;
    // frame.device is the actual Start device pointer encoded as uintptr_t.
    // Both eye stamps must exactly match identity; revisions/context/tokens
    // must be nonzero. guides.history/clip_to_previous are derived internally.
    // Reports whether a successful pair reused history in both eyes.
    [[nodiscard]] bool Reconstruct(ID3D11DeviceContext*,const PairFrame&,bool* history_reused=nullptr) noexcept;
    // Complete pair or two nulls, never a per-eye publication. Borrowed until
    // next Reconstruct/Invalidate/Stop. A mismatched current snapshot permanently
    // revokes this publication; querying an older snapshot cannot resurrect it.
    // Caller must also fence the completed result against current native state
    // when transporting it from this worker to a separate presentation thread.
    [[nodiscard]] std::array<ID3D11Texture2D*,2> Outputs(const PairFrame& current) noexcept;
    // Cancels publication/history, retaining frame/generation ordering fences.
    // Like all other methods, serialized with worker access (no thread safety).
    void Invalidate() noexcept;
    void Stop() noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
