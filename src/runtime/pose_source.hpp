#pragma once

#include "runtime/vr_types.hpp"

#include <cstdint>

namespace cojvr::runtime {

struct PoseSample {
    Pose pose{};
    std::uint64_t sequence = 0;
};

class PoseSource {
public:
    virtual ~PoseSource() = default;
    [[nodiscard]] virtual bool TryGetLatestPose(PoseSample& sample) noexcept = 0;
};

// Game-neutral recenter policy. Pose sources remain absolute; consumers receive
// a relative orientation whose identity is the captured physical forward pose.
class RelativePoseTracker final {
public:
    void SetEnabled(bool enabled) noexcept;
    void RequestRecenter() noexcept;
    [[nodiscard]] bool Update(const PoseSample& sample) noexcept;
    [[nodiscard]] bool CurrentPose(Pose& pose) const noexcept;

    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] std::uint64_t last_sample_sequence() const noexcept {
        return last_sample_sequence_;
    }
    [[nodiscard]] std::uint64_t last_recenter_sequence() const noexcept {
        return last_recenter_sequence_;
    }

private:
    bool enabled_ = false;
    bool recenter_pending_ = false;
    bool base_valid_ = false;
    Vec3 base_x_{1.0F, 0.0F, 0.0F};
    Vec3 base_y_{0.0F, 1.0F, 0.0F};
    Vec3 base_z_{0.0F, 0.0F, 1.0F};
    Pose relative_pose_{};
    std::uint64_t last_sample_sequence_ = 0;
    std::uint64_t last_recenter_sequence_ = 0;
};

} // namespace cojvr::runtime
