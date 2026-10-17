#pragma once

#include "backends/d3d9/frame_mailbox.hpp"
#include "backends/openvr/fixed_output_policy.hpp"

#include <mutex>
#include <utility>

namespace cojvr::backends::openvr {

// Exactly one sidecar for the existing latest-frame mailbox. All fixed-path
// publication/consumption passes through this lock, so replacement cannot pair
// one frame with another frame's authorization. No frame layout change needed.
class FixedOutputFrameMailbox final {
public:
    [[nodiscard]] bool Publish(d3d9::FrameMailbox& mailbox,
        const FixedOutputFrameAuthority& authority, d3d9::StereoCpuFrame frame,
        std::uint64_t captured_epoch) noexcept {
        try {
            std::lock_guard lock(mutex_);
            if (!authority.Allows(captured_epoch) || !mailbox.Publish(std::move(frame))) return false;
            pending_epoch_ = captured_epoch;
            return true;
        } catch (...) { return false; }
    }
    [[nodiscard]] bool Consume(d3d9::FrameMailbox& mailbox,
        d3d9::StereoCpuFrame& frame, std::uint64_t& captured_epoch) noexcept {
        captured_epoch = 0;
        try {
            std::lock_guard lock(mutex_);
            if (!mailbox.WaitConsumeLatest(frame, 0)) return false;
            captured_epoch = pending_epoch_;
            pending_epoch_ = 0;
            return true;
        } catch (...) { return false; }
    }
private:
    std::mutex mutex_;
    std::uint64_t pending_epoch_ = 0;
};

} // namespace cojvr::backends::openvr
