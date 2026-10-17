#pragma once
#include <atomic>
#include <cstdint>

namespace cojvr::backends::d3d9 {
// A queued private pair has already replaced the desktop world with its mirror.
// Its first Present must preserve the original capture token while collection
// catches up, even though no native pair has been published yet. This is a
// single-Present exemption, never a renewal of the displayed image's freshness.
class SourceCaptureAdmission final {
public:
    void BeginFrame() noexcept { Invalidate(); }
    void Invalidate() noexcept { queued_.store(0, std::memory_order_release); }
    // Call only after both EndFrame and the source queue's Queued succeeded.
    void Queued(std::uint64_t sequence) noexcept { queued_.store(sequence, std::memory_order_release); }
    std::uint64_t queued_sequence() const noexcept { return queued_.load(std::memory_order_acquire); }
    bool PreserveForPresent(std::uint64_t sequence, bool owner_allowed,
        bool capture_current, bool source_current) noexcept {
        if (!sequence || !queued_.compare_exchange_strong(sequence, 0,
                std::memory_order_acq_rel, std::memory_order_acquire)) return false;
        return owner_allowed && capture_current && source_current;
    }
private:
    std::atomic_uint64_t queued_{0};
};
}
