#include "backends/d3d9/frame_mailbox.hpp"

#include <chrono>
#include <utility>

namespace cojvr::backends::d3d9 {

bool FrameMailbox::Publish(StereoCpuFrame frame) noexcept {
    try {
        if (frame.capture_sequence == 0) return false;
        std::lock_guard lock(mutex_);
        if (stopped_) return false;
        if (frame.capture_sequence <= last_published_sequence_) {
            ++stats_.rejected_stale;
            return false;
        }
        if (has_pending_) ++stats_.replaced_pending;
        last_published_sequence_ = frame.capture_sequence;
        pending_ = std::move(frame);
        has_pending_ = true;
        ++stats_.published;
        wake_.notify_one();
        return true;
    } catch (...) {
        return false;
    }
}

bool FrameMailbox::WaitConsumeLatest(
    StereoCpuFrame& frame,
    const std::uint32_t timeout_ms) noexcept {
    try {
        std::unique_lock lock(mutex_);
        wake_.wait_for(
            lock,
            std::chrono::milliseconds(timeout_ms),
            [this] { return has_pending_ || stopped_; });
        if (!has_pending_) return false;
        frame = std::move(pending_);
        pending_ = {};
        has_pending_ = false;
        ++stats_.consumed;
        return true;
    } catch (...) {
        return false;
    }
}

void FrameMailbox::Stop() noexcept {
    try {
        std::lock_guard lock(mutex_);
        stopped_ = true;
        wake_.notify_all();
    } catch (...) {
    }
}

void FrameMailbox::Reset() noexcept {
    try {
        std::lock_guard lock(mutex_);
        pending_ = {};
        has_pending_ = false;
        stopped_ = false;
        last_published_sequence_ = 0;
        stats_ = {};
    } catch (...) {
    }
}

FrameMailboxStats FrameMailbox::stats() const noexcept {
    try {
        std::lock_guard lock(mutex_);
        return stats_;
    } catch (...) {
        return {};
    }
}

bool FrameMailbox::stopped() const noexcept {
    try {
        std::lock_guard lock(mutex_);
        return stopped_;
    } catch (...) {
        return true;
    }
}

} // namespace cojvr::backends::d3d9
