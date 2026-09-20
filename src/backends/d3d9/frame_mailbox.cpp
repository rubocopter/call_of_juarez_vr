#include "backends/d3d9/frame_mailbox.hpp"

#include <chrono>
#include <utility>

namespace cojvr::backends::d3d9 {

bool FrameMailbox::Publish(StereoCpuFrame frame) noexcept {
    try {
        if (frame.capture_sequence == 0) return false;
        // Validate render pose before accepting frame to fail fast
        if (frame.render_pose_sequence == 0 ||
            !frame.render_hmd_pose.orientation_valid ||
            !frame.render_hmd_pose.position_valid) {
            return false;
        }
        std::lock_guard lock(mutex_);
        if (stopped_) return false;
        const std::uint64_t ordering_sequence =
            frame.transport_sequence != 0 ? frame.transport_sequence : frame.capture_sequence;
        std::uint64_t expected = last_published_sequence_.load(std::memory_order_relaxed);
        while (ordering_sequence <= expected) {
            ++stats_.rejected_stale;
            return false;
        }
        if (!last_published_sequence_.compare_exchange_weak(
                expected, ordering_sequence,
                std::memory_order_acq_rel, std::memory_order_relaxed)) {
            ++stats_.rejected_stale;
            return false;
        }
        if (has_pending_) ++stats_.replaced_pending;
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

void FrameMailbox::Recycle(StereoCpuFrame frame) noexcept {
    try {
        std::lock_guard lock(mutex_);
        if (stopped_) return;
        recycled_ = std::move(frame);
        has_recycled_ = true;
    } catch (...) {
    }
}

bool FrameMailbox::TryAcquireRecycled(StereoCpuFrame& frame) noexcept {
    try {
        std::lock_guard lock(mutex_);
        if (stopped_ || !has_recycled_) return false;
        frame = std::move(recycled_);
        recycled_ = {};
        has_recycled_ = false;
        return true;
    } catch (...) {
        return false;
    }
}

void FrameMailbox::Stop() noexcept {
    try {
        std::lock_guard lock(mutex_);
        stopped_ = true;
        recycled_ = {};
        has_recycled_ = false;
        wake_.notify_all();
    } catch (...) {
    }
}

void FrameMailbox::Reset() noexcept {
    try {
        std::lock_guard lock(mutex_);
        pending_ = {};
        has_pending_ = false;
        recycled_ = {};
        has_recycled_ = false;
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
