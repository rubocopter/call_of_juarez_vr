#pragma once
#include <d3d9.h>
#include "runtime/hmd_frame_pacing.hpp"

namespace cojvr::backends::d3d9 {
// The desktop display mode stays game-owned. Only remove its vsync wait
// when a live VR owner supplies a valid headset cadence.
inline void ApplyVrPresentPolicy(D3DPRESENT_PARAMETERS& parameters, bool vr_paced) noexcept {
    if (vr_paced) parameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
}
// Keep a single timing sample paired with each game-owned device transition.
// Only a successful transition commits both cadence and interval ownership.
class VrPresentPolicy final {
public:
    bool Begin(D3DPRESENT_PARAMETERS& parameters, float reported_hz, bool reset) noexcept {
        pending_requested_ = parameters.PresentationInterval;
        if (reset && injected_ && pending_requested_ == D3DPRESENT_INTERVAL_IMMEDIATE)
            pending_requested_ = requested_;
        pending_hz_ = runtime::HmdFramePacer::ValidRefresh(reported_hz) ? reported_hz : 0.0F;
        pending_ = true;
        parameters.PresentationInterval = pending_requested_;
        ApplyVrPresentPolicy(parameters, pending_hz_ > 0.0F);
        return pending_hz_ > 0.0F;
    }
    void Complete(bool success, D3DPRESENT_PARAMETERS& parameters, runtime::VrFrameCadence& cadence) noexcept {
        if (pending_ && success) {
            requested_ = pending_requested_;
            injected_ = pending_hz_ > 0.0F && parameters.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE;
            (void)cadence.Target(pending_hz_, parameters.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE);
        } else if (pending_) {
            // A game may retry this same structure after a failed Create/Reset.
            parameters.PresentationInterval = pending_requested_;
        }
        pending_ = false;
    }
private:
    DWORD requested_ = D3DPRESENT_INTERVAL_DEFAULT;
    DWORD pending_requested_ = D3DPRESENT_INTERVAL_DEFAULT;
    float pending_hz_ = 0.0F;
    bool injected_ = false;
    bool pending_ = false;
};
}
