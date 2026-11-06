#pragma once
#include "backends/d3d9/sampled_depth_capture.hpp"
#include "backends/d3d9/stereo_frame.hpp"
#include <array>
#include <memory>

namespace cojvr::backends::openvr::dlss {

// Source identity only, never permission for temporal reconstruction.
struct SceneDepthStamp {
    std::uintptr_t device=0;
    std::uint64_t generation=0,frame=0,pose=0,owner=0,integrity=0,epoch=0;
    bool operator==(const SceneDepthStamp&) const = default;
};
struct SceneDepthEye {
    SceneDepthStamp stamp{};
    unsigned eye=0;
    d3d9::D3D9SampledDepthSnapshot snapshot;
    std::chrono::steady_clock::time_point capture_time{};
};
struct SceneDepthWork {
    SceneDepthStamp stamp{};
    std::array<std::uint64_t,2> input_tokens{};
    std::chrono::steady_clock::time_point capture_time{};
    // No D3D9 COM objects/immutable snapshot references cross to the worker.
    // Each frame has two eye handles and its own GPU-completion lease.
    // Bridge generation is the monotonic capture-resource epoch; the native
    // device generation remains in stamp. Recreated pools must invalidate caches
    // even when the game keeps the same native device/generation.
    std::unique_ptr<d3d9::StereoCpuFrame> color,depth;
    // Worker calls only after BOTH copy operations were accepted. GPU
    // completion is still independently required by their producer leases.
    void AcceptCopies() noexcept { if(copies_accepted_)copies_accepted_->store(true,std::memory_order_release); }
private:
    std::shared_ptr<std::atomic_bool> copies_accepted_;
    friend class SceneDepthTransfer;
};
enum class SceneDepthDelivery { idle,pending,ready,discarded };

// Construct/use/destroy on the D3D9 owner. Queue and Poll never wait for GPU
// work. Worker moves the two frame packets through existing shared-copy bridges;
// those release tokens only after proven copy completion (or abandon on error).
// Invalidation revokes publication but retains snapshots until BOTH retire.
// Destruction with unretired work quarantines one retained pair for process
// lifetime; do not replace/restart that failed owner to bypass backpressure.
class SceneDepthTransfer final {
public:
    SceneDepthTransfer();
    ~SceneDepthTransfer();
    SceneDepthTransfer(const SceneDepthTransfer&)=delete;
    SceneDepthTransfer& operator=(const SceneDepthTransfer&)=delete;
    [[nodiscard]] bool Queue(const std::array<SceneDepthEye,2>& eyes,SceneDepthWork& work) noexcept;
    void Invalidate() noexcept;
    // Pass current exact source identity. A stale completed result is consumed
    // and cannot be resurrected with an older current stamp. Only ready writes
    // completed; this admits transport, not game source or DLSS output.
    [[nodiscard]] SceneDepthDelivery Poll(const SceneDepthStamp& current,
        SceneDepthStamp& completed) noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace cojvr::backends::openvr::dlss
