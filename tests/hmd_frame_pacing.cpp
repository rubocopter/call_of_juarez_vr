#include "runtime/hmd_frame_pacing.hpp"
#include "backends/d3d9/vr_present_policy.hpp"

#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using cojvr::runtime::HmdFramePacer;
    HmdFramePacer pacer;
    if (pacer.Deadline(0, 90.0F) != 0 ||
        pacer.Deadline(1000000, 90.0F) != 11111111 ||
        pacer.Deadline(12000000, 90.0F) != 22222222) {
        std::cerr << "90 Hz render pacing did not follow HMD intervals\n";
        return 1;
    }
    if (pacer.Deadline(30000000, 120.0F) != 30000000 ||
        pacer.Deadline(31000000, 120.0F) != 38333333) {
        std::cerr << "runtime refresh change did not rebase render pacing\n";
        return 1;
    }
    if (pacer.Deadline(200000000, 120.0F) != 200000000 ||
        pacer.Deadline(200000001, 120.0F) != 208333333) {
        std::cerr << "late frame attempted catch-up bursts\n";
        return 1;
    }
    for (float invalid : {0.0F, -1.0F, 1.0F, 100000.0F,
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()}) {
        if (pacer.Deadline(300000000, invalid) != 300000000 ||
            pacer.Deadline(300000001, 90.0F) != 300000001) {
            std::cerr << "unavailable refresh did not fail open/reset\n";
            return 1;
        }
    }
    pacer.Reset();
    if (pacer.Deadline(400000000, 90.0F) != 400000000) return 1;
    cojvr::runtime::VrFrameCadence cadence;
    if (cadence.Target(0, false) != 0 || cadence.Target(90, false) != 0 ||
        cadence.Target(90, true) != 90 || cadence.Target(0, true) != 90 ||
        cadence.Target(120, true) != 120 || cadence.Target(0, false) != 0) {
        std::cerr << "late/missing HMD timing lost desktop or fallback pacing ownership\n";
        return 1;
    }
    if (!cadence.PresentNeedsPacing()) return 1;
    cadence.RenderPaced();
    if (cadence.PresentNeedsPacing() || !cadence.PresentNeedsPacing()) {
        std::cerr << "flat/native transition double-paced or failed to pace\n";
        return 1;
    }
    D3DPRESENT_PARAMETERS parameters{};
    parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    parameters.FullScreen_RefreshRateInHz = 144;
    cojvr::backends::d3d9::ApplyVrPresentPolicy(parameters, false);
    if (parameters.PresentationInterval != D3DPRESENT_INTERVAL_ONE) return 1;
    cojvr::backends::d3d9::ApplyVrPresentPolicy(parameters, true);
    if (parameters.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE ||
        parameters.FullScreen_RefreshRateInHz != 144) {
        std::cerr << "VR mirror policy did not remove desktop vsync exclusively\n";
        return 1;
    }
    cojvr::backends::d3d9::VrPresentPolicy policy;
    cojvr::runtime::VrFrameCadence reset_cadence;
    parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    policy.Begin(parameters, 0, false);
    policy.Complete(true, parameters, reset_cadence);
    if (reset_cadence.Target(90, false) != 0) return 1;
    policy.Begin(parameters, 90, true);
    policy.Complete(true, parameters, reset_cadence);
    if (parameters.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE ||
        reset_cadence.Target(0, true) != 90) {
        std::cerr << "successful late-rate reset did not retain its sampled cadence\n";
        return 1;
    }
    parameters.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    policy.Begin(parameters, 120, true);
    policy.Complete(true, parameters, reset_cadence);
    policy.Begin(parameters, 0, true);
    if (parameters.PresentationInterval != D3DPRESENT_INTERVAL_TWO) {
        std::cerr << "reset lost the latest game-owned presentation interval\n";
        return 1;
    }
    policy.Complete(true, parameters, reset_cadence);
    parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    policy.Begin(parameters, 90, true);
    policy.Complete(false, parameters, reset_cadence);
    parameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    policy.Begin(parameters, 0, true);
    if (parameters.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE) return 1;
    // A failed reset must also preserve an existing injected interval's owner.
    parameters.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    policy.Begin(parameters, 120, true);
    policy.Complete(true, parameters, reset_cadence);
    parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    policy.Begin(parameters, 90, true);
    policy.Complete(false, parameters, reset_cadence);
    if (reset_cadence.Target(0, true) != 120) return 1;
    parameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    policy.Begin(parameters, 0, true);
    if (parameters.PresentationInterval != D3DPRESENT_INTERVAL_TWO) return 1;
    cojvr::backends::d3d9::VrPresentPolicy retry_policy;
    cojvr::runtime::VrFrameCadence retry_cadence;
    parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    retry_policy.Begin(parameters, 0, false);
    retry_policy.Complete(true, parameters, retry_cadence);
    retry_policy.Begin(parameters, 90, true);
    retry_policy.Complete(false, parameters, retry_cadence);
    // Retry the same game-owned structure without rewriting its interval.
    retry_policy.Begin(parameters, 0, true);
    if (parameters.PresentationInterval != D3DPRESENT_INTERVAL_ONE) {
        std::cerr << "failed transition left injected IMMEDIATE in retry parameters\n";
        return 1;
    }
    std::cout << "HMD refresh pacing, change, late/unavailable rate and mirror policy passed\n";
}
