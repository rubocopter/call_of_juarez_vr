#pragma once
#include "backends/openvr/scene_depth_transfer.hpp"
#include "backends/openvr/dlss_pair_session.hpp"
#include <chrono>
#include <vector>

namespace cojvr::backends::openvr::dlss {
struct SceneWorkerAuthority {
    std::uint64_t settings_revision=0,input_context=0,history_epoch=0;
    bool available=false,tracking_valid=false;
    bool operator==(const SceneWorkerAuthority&) const = default;
};
struct SceneWorkerGuides {
    bool source_qualified=false;
    float near_metres=0,far_metres=0,jitter_x=0,jitter_y=0;
    runtime::TemporalMatrix view_projection=runtime::TemporalIdentity();
    std::vector<std::uint8_t> current_color_mask;
};
struct SceneWorkerRequest {
    SceneWorkerAuthority authority{};
    std::chrono::steady_clock::time_point capture_time{};
    // Must equal the oldest source-eye capture time; never re-stamp at submit.
    std::chrono::milliseconds max_age{0}; // Caller must explicitly supply a bound.
    bool reconstruct=false;
    unsigned output_width=0,output_height=0;
    Quality quality=Quality::quality;
    std::array<SceneWorkerGuides,2> guides;
};
struct SceneWorkerOptions {
    std::filesystem::path helper; // Empty: copy-audit only; no inference helper.
    unsigned gpu_timeout_ms=1000;
};
enum class SceneWorkerPhase { stopped,starting,idle,busy,completed,faulted };
enum class SceneWorkerOutcome { copied,reconstructed,cancelled,expired,failed };
struct SceneWorkerReport {
    SceneDepthStamp source{};
    SceneWorkerAuthority authority{};
    SceneWorkerOutcome outcome=SceneWorkerOutcome::failed;
    unsigned width=0,height=0;
    std::uint32_t worker_thread=0;
    // CPU wall-clock diagnostics, not GPU timestamps or game frame-rate proof.
    double copy_ms=0,processing_ms=0,age_ms=0;
    std::array<std::array<std::uint16_t,4>,2> center_rgba16f{};
    // Packed input-depth center samples for copy-audit reports only.
    std::array<std::array<std::uint16_t,4>,2> center_depth_rgba16f{};
    std::array<char,192> error{};
};
struct SceneWorkerStatus {
    SceneWorkerPhase phase=SceneWorkerPhase::stopped;
    std::uint64_t submitted=0,completed=0,source_pairs_retired=0;
    std::uint32_t worker_thread=0;
    std::array<char,192> error{};
};

// Producer owns this service and its D3D9 snapshots. Only value packets and
// completion tokens cross to one persistent D3D11/NGX thread. Start is async;
// Submit/Poll/SetAuthority never wait on GPU or IPC. Stop joins explicitly and
// belongs outside render/presenter hot paths. Unproven source drain quarantines
// the retained pair through SceneDepthTransfer; do not restart a failed owner.
// Reports are diagnostics ONLY: no image/COM output reaches presentation here.
class SceneDepthWorker final {
public:
    explicit SceneDepthWorker(std::unique_ptr<PairExecutor> executor={});
    ~SceneDepthWorker();
    SceneDepthWorker(const SceneDepthWorker&)=delete;
    SceneDepthWorker& operator=(const SceneDepthWorker&)=delete;
    [[nodiscard]] bool Start(IDirect3DDevice9*,SceneWorkerOptions={}) noexcept;
    [[nodiscard]] bool SetAuthority(SceneWorkerAuthority) noexcept;
    [[nodiscard]] bool Submit(const std::array<SceneDepthEye,2>&,SceneWorkerRequest) noexcept;
    // Newer frame/pose within the same source domain is allowed within max_age.
    // Domain, authority or age rejection consumes the result permanently.
    [[nodiscard]] bool Poll(const SceneDepthStamp& current,SceneWorkerReport&) noexcept;
    [[nodiscard]] SceneWorkerStatus status() const noexcept;
    [[nodiscard]] bool Stop() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace cojvr::backends::openvr::dlss
