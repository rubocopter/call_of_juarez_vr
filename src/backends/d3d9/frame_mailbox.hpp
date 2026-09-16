#pragma once

#include "backends/d3d9/stereo_frame.hpp"

#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace cojvr::backends::d3d9 {

struct FrameMailboxStats {
    std::uint64_t published = 0;
    std::uint64_t consumed = 0;
    std::uint64_t replaced_pending = 0;
    std::uint64_t rejected_stale = 0;
};

// Latest-frame mailbox. Ownership of pixel storage moves across the boundary;
// no D3D9 COM object is allowed into this type.
class FrameMailbox final {
public:
    [[nodiscard]] bool Publish(StereoCpuFrame frame) noexcept;
    [[nodiscard]] bool WaitConsumeLatest(
        StereoCpuFrame& frame,
        std::uint32_t timeout_ms) noexcept;
    void Stop() noexcept;
    void Reset() noexcept;

    [[nodiscard]] FrameMailboxStats stats() const noexcept;
    [[nodiscard]] bool stopped() const noexcept;

private:
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    StereoCpuFrame pending_{};
    bool has_pending_ = false;
    bool stopped_ = false;
    std::uint64_t last_published_sequence_ = 0;
    FrameMailboxStats stats_{};
};

} // namespace cojvr::backends::d3d9
