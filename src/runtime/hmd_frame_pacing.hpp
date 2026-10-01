#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace cojvr::runtime {
class HmdFramePacer final {
public:
    static bool ValidRefresh(float hz) noexcept {
        return std::isfinite(hz) && hz >= 20.0F && hz <= 1000.0F;
    }
    // One producer deadline per eye pair. Never catch up after a pause; no
    // device/compositor calls or GPU waits belong to this scheduling policy.
    std::int64_t Deadline(std::int64_t now_ns, float hz) noexcept {
        if (!ValidRefresh(hz)) { Reset(); return now_ns; }
        const auto period = static_cast<std::int64_t>(1000000000.0 / hz);
        if (!active_ || hz != refresh_hz_) {
            active_ = true;
            refresh_hz_ = hz;
            next_ns_ = now_ns + period;
            return now_ns;
        }
        const auto deadline = std::max(now_ns, next_ns_);
        next_ns_ = deadline + period;
        return deadline;
    }
    void Reset() noexcept { active_ = false; next_ns_ = 0; refresh_hz_ = 0.0F; }
private:
    bool active_ = false;
    std::int64_t next_ns_ = 0;
    float refresh_hz_ = 0.0F;
};
class VrFrameCadence final {
public:
    float Target(float reported_hz, bool desktop_vsync_disabled) noexcept {
        if (!desktop_vsync_disabled) { last_valid_hz_ = 0.0F; return 0.0F; }
        if (HmdFramePacer::ValidRefresh(reported_hz)) last_valid_hz_ = reported_hz;
        return last_valid_hz_;
    }
    bool PresentNeedsPacing() noexcept {
        const bool needed = !render_paced_;
        render_paced_ = false;
        return needed;
    }
    void RenderPaced() noexcept { render_paced_ = true; }
private:
    float last_valid_hz_ = 0.0F;
    bool render_paced_ = false;
};
}
