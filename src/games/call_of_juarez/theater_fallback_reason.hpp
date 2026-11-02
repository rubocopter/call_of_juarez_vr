#pragma once

#include "games/call_of_juarez/presentation_owner.hpp"

namespace cojvr::games::call_of_juarez {

// Diagnostic only. Call after the stereo-preservation decision has failed;
// this classifier must never authorize a presentation or input route.
enum class CoJTheaterFallbackReason {
    owner_observation_failed,
    native_blocking_presentation,
    ui_release_barrier,
    settings_capture,
    native_stereo_stale,
};

[[nodiscard]] constexpr CoJTheaterFallbackReason ClassifyCoJTheaterFallback(
    const CoJPresentationOwner& owner, bool owner_observed,
    bool immersive_movie_allowed, bool settings_captured,
    bool ui_release_required) noexcept {
    if (!owner_observed || !owner.valid)
        return CoJTheaterFallbackReason::owner_observation_failed;
    if (owner.blocked() && !immersive_movie_allowed)
        return CoJTheaterFallbackReason::native_blocking_presentation;
    if (ui_release_required)
        return CoJTheaterFallbackReason::ui_release_barrier;
    if (settings_captured)
        return CoJTheaterFallbackReason::settings_capture;
    return CoJTheaterFallbackReason::native_stereo_stale;
}

[[nodiscard]] constexpr const char* CoJTheaterFallbackReasonName(
    CoJTheaterFallbackReason reason) noexcept {
    switch (reason) {
    case CoJTheaterFallbackReason::owner_observation_failed: return "owner_observation_failed";
    case CoJTheaterFallbackReason::native_blocking_presentation: return "native_blocking_presentation";
    case CoJTheaterFallbackReason::ui_release_barrier: return "ui_release_barrier";
    case CoJTheaterFallbackReason::settings_capture: return "settings_capture";
    case CoJTheaterFallbackReason::native_stereo_stale: return "native_stereo_stale";
    }
    return "unknown";
}

} // namespace cojvr::games::call_of_juarez
