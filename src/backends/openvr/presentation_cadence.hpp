#pragma once

#include <cstdint>

namespace cojvr::backends::openvr {

enum class PresentationContent : std::uint8_t {
    none,
    new_frame,
    repeated_frame,
};

// Small state machine for compositor cadence. A newly uploaded texture pair
// remains presentable while the game producer is paused; successful submits
// after the first are explicitly repetitions until another upload arrives.
// Dashboard visibility is handled by the presenter, not by the cadence.
class PresentationCadence final {
public:
    void FrameUploaded() noexcept {
        presentable_ = true;
        new_frame_pending_ = true;
    }

    void Invalidate() noexcept {
        presentable_ = false;
        new_frame_pending_ = false;
    }

    [[nodiscard]] bool presentable() const noexcept { return presentable_; }

    [[nodiscard]] bool scene_submission_allowed(
        const bool /*system_overlay_visible*/) const noexcept {
        return presentable_;
    }

    [[nodiscard]] PresentationContent pending_content() const noexcept {
        if (!presentable_) return PresentationContent::none;
        return new_frame_pending_
            ? PresentationContent::new_frame
            : PresentationContent::repeated_frame;
    }

    void SubmissionSucceeded() noexcept {
        if (presentable_) new_frame_pending_ = false;
    }

private:
    bool presentable_ = false;
    bool new_frame_pending_ = false;
};

} // namespace cojvr::backends::openvr
