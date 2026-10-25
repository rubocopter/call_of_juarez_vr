#include "runtime/hmd_frame_pacing.hpp"
#include "backends/d3d9/vr_present_policy.hpp"

#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using cojvr::runtime::HmdFramePacer;
    for (const int hz : {72,80,90,120,144}) {
        HmdFramePacer rate;
        const auto period=static_cast<std::int64_t>(1000000000.0/hz);
        for (int frame=0;frame<=hz;++frame) {
            const auto now=frame*period;
            if (rate.Deadline(now,static_cast<float>(hz))!=now) {
                std::cerr << "HMD rate introduced a second producer wait\n";
                return 1;
            }
        }
        if (std::abs(hz*period-1000000000LL)>hz) return 1;
    }
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
    using cojvr::backends::d3d9::NativeStereoAdmissionTick;
    using cojvr::backends::d3d9::NativeStereoFrameFresh;
    // Immersive movie admission must retain a successfully queued pair when
    // the next camera-only render cannot acquire a capture-ring slot.
    std::uint64_t movie_queued = 1000;
    for (const std::uint64_t now : {1001ULL, 1100ULL, 1249ULL, 1250ULL, 1500ULL}) {
        movie_queued = NativeStereoAdmissionTick(movie_queued, now, false, true);
        if (movie_queued != 1000 || NativeStereoFrameFresh(movie_queued, now) != (now < 1250)) {
            std::cerr << "immersive movie capture pressure cleared/renewed its queued pair\n";
            return 1;
        }
    }
    // A queued passive/settings pair survives transient ring backpressure, but
    // repeated admission must not extend its original 250 ms lease indefinitely.
    std::uint64_t last_queued = 1000;
    for (const std::uint64_t now : {1001ULL, 1100ULL, 1249ULL, 1250ULL, 1500ULL}) {
        last_queued = NativeStereoAdmissionTick(last_queued, now, true, false);
        if (last_queued != 1000 || NativeStereoFrameFresh(last_queued, now) != (now < 1250)) {
            std::cerr << "passive capture backpressure cleared/renewed the queued frame lease\n";
            return 1;
        }
    }
    last_queued = 1501; // A later successful queue restores presentation.
    if (!NativeStereoFrameFresh(last_queued, 1502) ||
        NativeStereoAdmissionTick(0, 1502, true, false) != 0 ||
        NativeStereoFrameFresh(0, 1502) || NativeStereoFrameFresh(1502, 1501) ||
        NativeStereoAdmissionTick(last_queued, 1502, true, true, true) != 0 ||
        NativeStereoAdmissionTick(last_queued, 1502, false, true, true) != 0 ||
        NativeStereoAdmissionTick(last_queued, 1502, false, false) != 1502) return 1;
    cojvr::backends::d3d9::NativeStereoQueuePolicy queue;
    auto epoch = queue.ObserveOwner(101); // Ordinary player/owner.
    if (!queue.BindCapture(epoch, 10) || queue.CaptureEpoch(10) != epoch) return 2;
    if (queue.Fresh(1000) || !queue.Queued(epoch, 10, 1000) || !queue.Accept(10)) return 2;
    epoch = queue.ObserveOwner(102); // Same player entering immersive movie.
    if (queue.Fresh(1001) || queue.Accept(10) || queue.CaptureEpoch(10) ||
        !queue.BindCapture(epoch, 11) || queue.CaptureEpoch(10) || queue.BindCapture(epoch, 10)) {
        std::cerr << "ordinary queued frame crossed immersive movie entry\n"; return 2;
    }
    if (!queue.Queued(epoch, 11, 1002) || queue.Fresh(1003)) {
        std::cerr << "current pending fence exposed previous owner's displayed pair\n"; return 2;
    }
    if (!queue.Accept(11) || !queue.Published(epoch)) return 2;
    for (const auto now : {1003ULL, 1100ULL, 1251ULL, 1252ULL, 2000ULL}) {
        queue.Admit(queue.ObserveOwner(102), now, false, true);
        if (queue.Fresh(now) != (now < 1252)) {
            std::cerr << "movie admission cleared/extended successful queue freshness\n"; return 2;
        }
    }
    if (!queue.Queued(epoch, 12, 2001) || !queue.Fresh(2002)) return 2;
    const auto movie_epoch = epoch;
    queue.ObserveOwner(0); // Death, pause, observation loss or theater preference.
    if (queue.Fresh(2002) || queue.Accept(12)) return 2;
    epoch = queue.ObserveOwner(101);
    if (queue.Accept(11) || queue.Accept(12) || queue.Fresh(2003)) return 2;
    if (queue.Queued(movie_epoch, 13, 2003)) return 2;
    if (!queue.Queued(epoch, 14, 2004) || queue.Accept(13) || !queue.Published(epoch) ||
        queue.Queued(movie_epoch, 99, 2005) || queue.Accept(99) || !queue.Accept(14) || !queue.Fresh(2005)) {
        std::cerr << "in-flight old-mode fence escaped owner retirement\n"; return 2;
    }
    queue.Invalidate(); // Failed native restoration: immediate, not a 250 ms wait.
    if (queue.Fresh(2005) || queue.Accept(14) || queue.Queued(epoch, 15, 2005)) return 2;
    epoch = queue.ObserveOwner(102);
    if (!queue.Queued(epoch, 16, 2006) || !queue.Accept(16)) return 2;
    epoch = queue.ObserveOwner(202); // Replacement player, same movie mode.
    if (queue.Fresh(2007) || queue.Accept(16)) return 2;
    // Abandoned readback/reset failures never strand registry capacity. Only
    // the newest three records can still correspond to producer slots.
    for (std::uint64_t seq = 17; seq <= 25; ++seq)
        if (!queue.Queued(epoch, seq, 2008)) return 2;
    if (queue.Accept(17) || !queue.Accept(25) || !queue.Published(epoch) || !queue.Fresh(2009)) {
        std::cerr << "abandoned collections prevented healthy same-owner recovery\n"; return 2;
    }
    // Settings capture uses the same bounded successful-queue lease.
    if (!queue.Queued(epoch, 26, 2008)) return 2;
    queue.Admit(epoch, 2258, true, false);
    if (queue.Fresh(2258)) return 2;
    queue.FlatPublished();
    if (!queue.Queued(epoch, 27, 2259) || queue.Fresh(2260) ||
        !queue.Accept(27) || !queue.Published(epoch) || !queue.Fresh(2260)) {
        std::cerr << "recovery fence skipped replacing the displayed flat pair\n"; return 2;
    }
    std::cout << "HMD refresh pacing, change, late/unavailable rate and mirror policy passed\n";
}
